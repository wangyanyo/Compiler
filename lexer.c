#include"compiler.h"
#include<string.h>
#include<assert.h>
#include<ctype.h>
#include"helpers/vector.h"
#include"helpers/buffer.h"

#define LEX_GETC_IF(buffer, c, exp)         \
    for(c = peekc(); exp; c = peekc()) {    \
        buffer_write(buffer, c);            \
        nextc();                            \
    }

struct lex_process* lex_process;
struct token tmp_token;

struct token* read_next_token();
bool lex_in_expression();

static char peekc() {
    return lex_process->function->peek_char(lex_process);
}

static char nextc() {
    char c = lex_process->function->next_char(lex_process);
    if(lex_in_expression()) {
        buffer_write(lex_process->parentheses_buffer, c);
    }
    lex_process->pos.col += 1;
    if(c == '\n') {
        lex_process->pos.line += 1;
        lex_process->pos.col = 1;
    }
    return c;
}

static void pushc(char c) {
    lex_process->function->push_char(lex_process, c);
}

static char assert_next_char(char c) {
    char next_c = nextc();
    assert(next_c == c);
    return next_c;
}

static struct token* lexer_last_token() {
    return vector_back_or_null(lex_process->token_vec);
}

static struct token* lexer_pop_token() {
    vector_pop(lex_process->token_vec);
}

static struct token* handle_whitespace() {
    struct token* last_token = lexer_last_token();
    if(last_token) {
        last_token->whitespace = true;
    }

    nextc();
    return read_next_token();
}

static struct pos lex_file_position() {
    return lex_process->pos;
}

static struct token* token_create(struct token* _token) {
    memcpy(&tmp_token, _token, sizeof(struct token));
    tmp_token.pos = lex_file_position();
    if(lex_in_expression()) {
        tmp_token.between_brackets = buffer_ptr(lex_process->parentheses_buffer);
    }
    return &tmp_token;
}

static const char* read_number_str() {
    struct buffer* buffer = buffer_create();
    char c = peekc();
    LEX_GETC_IF(buffer, c, (c >= '0' && c <= '9'));
    buffer_write(buffer, 0x00);
    return buffer_ptr(buffer);
}

static struct token* lexer_number_type(char c) {
    // 这里没做double的处理，话说现在还没有处理浮点数的功能
    switch (c) {
        case 'L':
            return NUMBER_TYPE_LONG;
        break;

        case 'f':
            return NUMBER_TYPE_FLOAT;
        break;

        default:
            return NUMBER_TYPE_NORMAL;
        break;
    }
}

static struct token* token_make_number_for_value(unsigned long number, int len) {
    // 返回指针有两种方式，一种是在堆上开一块空间，返回其指针；一种是利用全局变量，返回其指针。
    // 该代码用的是后者，这样一切就能说的通了，抽丝剥茧般的抽象，思路很清晰。
    int number_type = lexer_number_type(peekc());
    if(number_type != NUMBER_TYPE_NORMAL) {
        nextc();
    }

    return token_create(&(struct token){.type = TOKEN_TYPE_NUMBER, .llnum = number, .num.type = number_type, .str_len = len});
}

static struct token* token_make_number() {
    const char* s = read_number_str();
    unsigned long number = atoll(s);
    int str_len = strlen(s);

    return token_make_number_for_value(number, str_len);
}

static struct token* token_make_string(char start_delim, char end_delim) {
    struct buffer* buffer = buffer_create();
    assert(nextc() == start_delim);
    char c = nextc();
    for(; c != end_delim && c != EOF; c = nextc()) {
        if(c == '\\') {
            // 处理转义字符
            continue;
        }
        buffer_write(buffer, c);
    }

    buffer_write(buffer, 0x00);
    return token_create(&(struct token){.type = TOKEN_TYPE_STRING, .sval = buffer_ptr(buffer)});
}

static bool op_treated_as_one(char op) {
    return op == '(' || op == '[' || op == ',' || op == '.' || op == '*' || op == '?';
}

static bool op_vaild(const char* op) {
    return S_EQ(op, "+")  ||
            S_EQ(op, "-") ||
            S_EQ(op, "*") ||
            S_EQ(op, "/") ||
            S_EQ(op, "%%") ||
            S_EQ(op, "!") ||
            S_EQ(op, "^") ||
            S_EQ(op, "|") ||
            S_EQ(op, "&") ||
            S_EQ(op, "+=") ||
            S_EQ(op, "-=") ||
            S_EQ(op, "*=") ||
            S_EQ(op, "/=") ||
            S_EQ(op, "%%=") ||
            S_EQ(op, "!=") ||
            S_EQ(op, "^=") ||
            S_EQ(op, "|=") ||
            S_EQ(op, "&=") ||
            S_EQ(op, ">>") ||
            S_EQ(op, "<<") ||
            S_EQ(op, ">=") ||
            S_EQ(op, "<=") ||
            S_EQ(op, "==") ||
            S_EQ(op, "||") ||
            S_EQ(op, "&&") ||
            S_EQ(op, "++") ||
            S_EQ(op, "--") ||
            S_EQ(op, "=") ||
            S_EQ(op, "->") ||
            S_EQ(op, "(") ||
            S_EQ(op, "[") ||
            S_EQ(op, ",") ||
            S_EQ(op, ".") ||
            S_EQ(op, "...") ||
            S_EQ(op, "~") ||
            S_EQ(op, "?");
}

static bool is_single_operator(char op) {
    return op == '+' ||                           
            op == '-' ||                           
            op == '*' ||                          
            op == '>' ||                           
            op == '<' ||                           
            op == '^' ||                           
            op == '%' ||                           
            op == '!' ||                           
            op == '=' ||                           
            op == '~' ||                           
            op == '|' ||                           
            op == '&' ||                           
            op == '(' ||                           
            op == '[' ||                           
            op == ',' ||                           
            op == '.' ||
            op == '?';
}

static void read_op_flush_back_keep_first(struct buffer* buffer) {
    const char* data = buffer_ptr(buffer);
    int len = buffer->len;
    for(int i = len - 1; i >= 1; --i) {
        if(data[i] == 0x00) {
            continue;
        }
    
        pushc(data[i]);
    }
}

static const char* read_op() {
    bool single_operator = true;
    char op = nextc();
    struct buffer* buffer = buffer_create();
    buffer_write(buffer, op);

    if(!op_treated_as_one(op)) {
        op = peekc();
        // 要保证下一个字符是符号
        if(is_single_operator(op)) {
            buffer_write(buffer, op);
            nextc();
            single_operator = false;
        }
    }
    buffer_write(buffer, 0x00);

    char* ptr = buffer_ptr(buffer);
    if(!single_operator) {
        if(!op_vaild(ptr)) {
            read_op_flush_back_keep_first(buffer);
            ptr[1] = 0x00;
        }
    }
    else if(!op_vaild(ptr)) {
        compile_error(lex_process->compiler, "The operator %s is not valid\n", ptr);
    }
    
    return ptr;
}

static void lex_new_expression() {
    lex_process->current_expression_count++;
    if(lex_process->current_expression_count == 1) {
        lex_process->parentheses_buffer = buffer_create();
    }
}

bool lex_in_expression() {
    return lex_process->current_expression_count > 0;
}

static struct token* token_make_operator_or_string() {
    char c = peekc();
    if(c == '<') {
        struct token* last_token = lexer_last_token();
        if(token_is_keyword(last_token, "include")) {
            return token_make_string('<', '>');
        }
    }

    struct token* token = token_create(&(struct token){.type = TOKEN_TYPE_OPERATOR, .sval = read_op()});  

    if(c == '(') {
        lex_new_expression();
    }

    return token;
}

static struct token* lex_finish_expression() {
    lex_process->current_expression_count--;
    if(lex_process->current_expression_count < 0) {
        compile_error(lex_process->compiler, "You closed an expression that you never opened\n");
    }   
}

struct token* token_make_symbol() {
    char c = nextc();
    if(c == ')') {
        lex_finish_expression();
    }
    struct token* token = token_create(&(struct token){.type = TOKEN_TYPE_SYMBOL, .cval = c, .str_len = 0});
    return token;
}

static bool is_keyword(const char* str) {
    return S_EQ(str, "unsigned") ||
        S_EQ(str, "signed") ||
        S_EQ(str, "char") ||
        S_EQ(str, "short") ||
        S_EQ(str, "int") ||
        S_EQ(str, "long") ||
        S_EQ(str, "float") ||
        S_EQ(str, "double") ||
        S_EQ(str, "void") ||
        S_EQ(str, "struct") ||
        S_EQ(str, "union") ||
        S_EQ(str, "static") ||
        S_EQ(str, "__ignore_typecheck") ||
        S_EQ(str, "return") ||
        S_EQ(str, "include") ||
        S_EQ(str, "sizeof") ||
        S_EQ(str, "if") ||
        S_EQ(str, "else") ||
        S_EQ(str, "while") ||
        S_EQ(str, "for") ||
        S_EQ(str, "do") ||
        S_EQ(str, "break") ||
        S_EQ(str, "continue") ||
        S_EQ(str, "switch") ||
        S_EQ(str, "case") ||
        S_EQ(str, "default") ||
        S_EQ(str, "goto") ||
        S_EQ(str, "typedef") ||
        S_EQ(str, "const") ||
        S_EQ(str, "extern") ||
        S_EQ(str, "restrict");
}

static struct token* token_make_identifier_or_keyword() {
    struct buffer* buffer = buffer_create();
    char c = 0x00;
    LEX_GETC_IF(buffer, c, (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_');
    buffer_write(buffer, 0x00);

    // 判断是不是关键字
    if(is_keyword(buffer_ptr(buffer))) {
        return token_create(&(struct token){.type = TOKEN_TYPE_KEYWORD, .sval = buffer_ptr(buffer)});
    }

    return token_create(&(struct token){.type = TOKEN_TYPE_IDENTIFIER, .sval = buffer_ptr(buffer)});
}

static struct token* read_special_token() {
    char c = peekc();
    if(isalpha(c) || c == '_') {
        return token_make_identifier_or_keyword();
    }

    return NULL;
}

static struct token* token_make_newline() {
    nextc();
    return token_create(&(struct token){.type = TOKEN_TYPE_NEWLINE});
}

static char lex_get_escaped_char(char c) {
    char co = 0x00;
    switch(c) {
        case 'n':
            co = '\n';
        break;

        case 't':
            co = '\t';
        break;

        case '\\':
            co = '\t';
        break;

        case '\'':
            co = '\'';
        break;
        
        case '\"':
            co = '\"';
        break;
    }

    return co;
}

static struct token* token_make_quote() {
    // 可以尝试用 assert 拦截一些小错误？
    assert_next_char('\'');
    char c = nextc();
    
    // 处理 \n, \t 这种情况
    if(c == '\\') {
        c = nextc();
        c = lex_get_escaped_char(c);
    }
    
    // 字符后面必须是单引号闭
    if(nextc() != '\'') {
        compile_error(lex_process->compiler, "You opened a quote ' but did not close it with a ' character");
    }

    return token_create(&(struct token){.type = TOKEN_TYPE_NUMBER, .cval = c, .str_len = 0});
}


static struct token* token_make_one_line_comment() {
    struct buffer* buffer = buffer_create();
    char c = 0x00;
    LEX_GETC_IF(buffer, c, c != '\n' && c != EOF);
    return token_create(&(struct token){.type = TOKEN_TYPE_COMMENT, .sval = buffer_ptr(buffer)});
}

static struct token* token_make_multiline_comment() {
    struct buffer* buffer = buffer_create();
    char c = 0x00;
    // /******/ 遇到*要跳过，检查有没有*/，如果没有*/就到文件尾，说明是有问题的
    while(1) {
        LEX_GETC_IF(buffer, c, c != '*' && c != EOF);

        if(c == EOF) {
            compile_error(lex_process->compiler, "You did not close this multiline comment\n");
        }
        else if(c == '*') {
            nextc();
            if(peekc() == '/') {
                nextc();
                break;
            }
        }
    }
    return token_create(&(struct token){.type = TOKEN_TYPE_COMMENT, .sval = buffer_ptr(buffer)});
}

static struct token* handle_comment() {
    char c = peekc();

    if(c == '/') {
        nextc();

        if(peekc() == '/') {
            nextc();
            return token_make_one_line_comment();
        }
        else if(peekc() == '*') {
            nextc();
            return token_make_multiline_comment();
        }

        pushc('/');
        return token_make_operator_or_string();
    }
    
    return NULL;
}

static bool is_hex_char(char c) {
    c = tolower(c);
    
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
}

static const char* read_hex_number_str() {
    struct buffer* buffer = buffer_create();
    char c = 0x00;
    LEX_GETC_IF(buffer, c, is_hex_char(c));

    if(buffer->len == 0) {
        compile_error(lex_process->compiler, "empty hex number\n");
    }
    
    buffer_write(buffer, 0x00);
    return buffer_ptr(buffer);
}

static struct token* token_make_special_number_hexadecimal() {
    nextc();
    
    const char* number_str = read_hex_number_str();
    unsigned long number = strtol(number_str, 0, 16);
    return token_make_number_for_value(number, 0);
}

static const char* read_bin_number_str() {
    struct buffer* buffer = buffer_create();
    char c = 0x00;
    LEX_GETC_IF(buffer, c, c == '0' || c == '1');

    if(buffer->len == 0) {
        compile_error(lex_process->compiler, "This is not a valid binary number\n");
    }

    buffer_write(buffer, 0x00);
    return buffer_ptr(buffer);
}

static struct token* token_make_special_number_binary() {
    nextc();

    const char* number_str = read_bin_number_str();
    unsigned long number = strtol(number_str, 0, 2);
    return token_make_number_for_value(number, 0);
}

// 16进制数
static struct token* token_make_special_number() {
    struct token* token = NULL;
    struct token* last_token = lexer_last_token();

    char c = peekc();
    if(last_token && last_token->llnum == 0 && !last_token->whitespace && last_token->str_len == 1 && (c == 'x' || c == 'b')) {
        lexer_pop_token();
        if(c == 'x') {
            token = token_make_special_number_hexadecimal();
        }
        else if(c == 'b') {
            token = token_make_special_number_binary();
        }
    }
    else {
        token = token_make_identifier_or_keyword();
    }

    return token;
}

struct token* read_next_token() {
    struct token* token = NULL;
    char c = peekc();

    // read_next_token 函数只是返回一个token，所以完全可以在这个位置来处理，检测并处理一个注释
    token = handle_comment();
    if(token) {
        return token;
    }

    switch(c) {
        NUMERIC_CASE:
            token = token_make_number();
        break;

        OPERATOR_CASE_EXCLUDING_DIVISION:
            token = token_make_operator_or_string();
        break;

        SYMBOL_CASE:
            token = token_make_symbol();
        break;

        case 'b':
        case 'x':
            token = token_make_special_number();
        break;

        case '"':
            token = token_make_string('"', '"');
        break;

        // 处理单引号
        case '\'':
            token = token_make_quote();
        break;

        case ' ':
        case '\t':
            token = handle_whitespace();
        break;

        case '\n':
            token = token_make_newline();
        break;

        case EOF:
            // 结束
        break;
    
        default:
            token = read_special_token();
            if(!token) {
                compile_error(lex_process->compiler, "Unexpected token\n");
            }
    }

    return token;
}

int lex(struct lex_process* process) {
    process->current_expression_count = 0;
    process->parentheses_buffer = NULL;
    process->pos.filename = process->compiler->cfile.abs_path;
    lex_process = process;

    // 在最上层不是按照字符来遍历，而是按照token遍历，一层层抽丝剥茧
    struct token* token = read_next_token();
    while(token) {
        vector_push(process->token_vec, token);
        token = read_next_token();
    }

    // debug 用
    FILE* token_output_file = fopen("./file/token.txt", "w");
    for(int i = 0; i < lex_process->token_vec->count; ++i) {
        struct token* token = vector_at(lex_process->token_vec, i);
        if(token->type == TOKEN_TYPE_COMMENT || 
            token->type == TOKEN_TYPE_IDENTIFIER ||
            token->type == TOKEN_TYPE_KEYWORD ||
            token->type == TOKEN_TYPE_OPERATOR ||
            token->type == TOKEN_TYPE_STRING) {

            fprintf(token_output_file, "%s\n", token->sval);
        }
        else if(token->type == TOKEN_TYPE_NEWLINE) {
            fprintf(token_output_file, "newline\n");
        }
        else {
            fprintf(token_output_file, "%lld %c %d\n",token->llnum, token->cval, token->str_len);
        }
    }
    fclose(token_output_file);

    return LEXICAL_ANALYSIS_ALL_OK;
}

char lexer_string_buffer_nextc(struct lex_process* process) {
    struct buffer* buf = lex_process_private(process);
    return buffer_read(buf);
}

char lexer_string_buffer_peekc(struct lex_process* process) {
    struct buffer* buf = lex_process_private(process);
    return buffer_peek(buf);
}

char lexer_string_buffer_pushc(struct lex_process* process, char c) {
    struct buffer* buf = lex_process_private(process);
    buffer_push(buf, c);
}

struct lex_process_functions lexer_string_buffer_functions = {
    .next_char = lexer_string_buffer_nextc,
    .peek_char = lexer_string_buffer_peekc,
    .push_char = lexer_string_buffer_pushc
};

// 单独为一段字符串进行词法分析
struct lex_process* tokens_build_for_string(struct compile_process* compiler, const char* str) {
    struct buffer* buffer = buffer_create();
    buffer_printf(buffer, str);
    struct lex_process* lex_process = lex_process_create(compiler, &lexer_string_buffer_functions, buffer);
    if(!lex_process) {
        return NULL;
    }

    if(lex(lex_process) != LEXICAL_ANALYSIS_ALL_OK) {
        return NULL;
    }

    return lex_process; 
}