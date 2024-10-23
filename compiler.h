#ifndef PEACHCOMPLIER_H
#define PEACHCOMPLIER_H
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

struct pos {
    int line;
    int col;
    const char* filename;
};

#define S_EQ(str1, str2) \
        (str1 && str2 && (strcmp(str1, str2) == 0))

#define NUMERIC_CASE \
    case '0':       \
    case '1':       \
    case '2':       \
    case '3':       \
    case '4':       \
    case '5':       \
    case '6':       \
    case '7':       \
    case '8':       \
    case '9'

#define OPERATOR_CASE_EXCLUDING_DIVISION \
    case '+':                           \
    case '-':                           \
    case '*':                           \
    case '>':                           \
    case '<':                           \
    case '^':                           \
    case '%':                           \
    case '!':                           \
    case '=':                           \
    case '~':                           \
    case '|':                           \
    case '&':                           \
    case '(':                           \
    case '[':                           \
    case ',':                           \
    case '.':                           \
    case '?'

#define SYMBOL_CASE \
    case '{':       \
    case '}':       \
    case ':':       \
    case ';':       \
    case '#':       \
    case '\\':      \
    case ')':       \
    case ']'


enum {
    LEXICAL_ANALYSIS_ALL_OK,
    LEXICAL_ANALYSIS_INPUT_ERROR
};

enum {
    TOKEN_TYPE_IDENTIFIER,
    TOKEN_TYPE_KEYWORD,
    TOKEN_TYPE_OPERATOR,
    TOKEN_TYPE_SYMBOL,
    TOKEN_TYPE_NUMBER,
    TOKEN_TYPE_STRING,
    TOKEN_TYPE_COMMENT,
    TOKEN_TYPE_NEWLINE
};

enum {
    NUMBER_TYPE_NORMAL,
    NUMBER_TYPE_LONG,
    NUMBER_TYPE_FLOAT,
    NUMBER_TYPE_DOUBLE
};

struct token {
    int type;
    int flags;
    // 对应数字字符串的长度，用于识别16进制数 0x
    int str_len;
    struct pos pos;

    union {
        char cval;
        const char* sval;
        unsigned int inum;
        unsigned long lnum;
        unsigned long long llnum;
        void* any;
    };

    // 数字这里确实麻烦，分 short int long ll float double 16进制 8进制 2进制
    struct token_number {
        int type;
    } num;

    // 代表该token和下一个token之间是否有空白符
    bool whitespace;

    // 括号内的token指向括号内字符串，为了debug方便？
    const char* between_brackets;
};

struct lex_process;
typedef char (*LEX_PROCESS_NEXT_CHAR)(struct lex_process* process);
typedef char (*LEX_PROCESS_PEEK_CHAR)(struct lex_process* process);
typedef void (*LEX_PROCESS_PUSH_CHAR)(struct lex_process* process, char c);
struct lex_process_functions {
    LEX_PROCESS_NEXT_CHAR next_char;
    LEX_PROCESS_PEEK_CHAR peek_char;
    LEX_PROCESS_PUSH_CHAR push_char;
};

struct lex_process {
    struct pos pos;
    struct vector* token_vec;
    struct compile_process* compiler;

    int current_expression_count;
    struct buffer* parentheses_buffer;
    // 这个function类似面向对象里的方法
    struct lex_process_functions* function;

    void* private; 
};
    
enum {
    COMPILER_FILE_COMPILED_OK,
    COMPILER_FAILED_WITH_ERRORS
};

struct compile_process {
    //如何编译此文件
    int flags;

    struct pos pos;
    //输入文件
    struct compile_process_input_file {
        FILE* fp;
        const char* abs_path;
    } cfile;

    struct vector* token_vec;

    struct vector* node_vec;
    struct vector* node_tree_vec;

    //输出文件
    FILE* ofile;
};

enum {
    NODE_TYPE_EXPRESSION,
    NODE_TYPE_EXPRESSION_PARENTHESES,
    NODE_TYPE_NUMBER,
    NODE_TYPE_IDENTIFIER,
    NODE_TYPE_STRING,
    NODE_TYPE_VARIABLE,
    NODE_TYPE_VARIABLE_LIST,
    NODE_TYPE_FUNCTION,
    NODE_TYPE_BODY,
    NODE_TYPE_STATEMENT_RETURN,
    NODE_TYPE_STATEMENT_IF,
    NODE_TYPE_STATEMENT_ELSE,
    NODE_TYPE_STATEMENT_WHILE,
    NODE_TYPE_STATEMENT_DO_WHILE,
    NODE_TYPE_STATEMENT_FOR,
    NODE_TYPE_STATEMENT_BREAK,
    NODE_TYPE_STATEMENT_CONTINUE,
    NODE_TYPE_STATEMENT_SWITCH,
    NODE_TYPE_STATEMENT_CASE,
    NODE_TYPE_STATEMENT_DEFAULT,
    NODE_TYPE_STATEMENT_GOTO,

    NODE_TYPE_UNARY,
    NODE_TYPE_TENARY,
    NODE_TYPE_LABEL,
    NODE_TYPE_STRUCT,
    NODE_TYPE_UNION,
    NODE_TYPE_BRACKET,
    NODE_TYPE_CAST,
    NODE_TYPE_BLANK
};

enum {
    PARSE_ALL_OK,
    PARSE_GENERAL_ERROR
};

enum {
    NODE_FLAG_INSIDE_EXPRESSION = 0b00000001
};

// node 中的信息表明了该node属于哪个 body 和 函数
struct node {
    int type;
    int flags;

    struct pos pos;

    struct node_binded {
        // Pointer to our body node
        struct node* owner;
        // Pointer to the function this node is in.
        struct node* function;
    } binded;

    union {
        struct exp {
            struct node* left;
            struct node* right;
            const char* op;
        } exp;
    };

    union {
        char cval;
        const char* sval;
        unsigned int inum;
        unsigned long lnum;
        unsigned long long llnum; 
    };
};

int compile_file(const char* filename, const char* out_filename, int flags);

void compile_error(struct compile_process* compiler, const char* msg, ...);
void compile_warning(struct compile_process* compiler, const char* msg, ...);

// 初始化编译进程
struct compile_process* compile_process_create(const char* filename, const char* filename_out, int flags);

char compile_process_next_char(struct lex_process* lex_process);
char compile_process_peek_char(struct lex_process* lex_process);
void compile_process_push_char(struct lex_process* lex_process, char c);

struct lex_process* lex_process_create(struct compile_process* compiler, struct lex_process_functions* functions, void* private);
void lex_process_free(struct lex_process* process);
void* lex_process_private(struct lex_process* process);
struct vector* lex_process_tokens(struct lex_process* process);

bool token_is_keyword(struct token* token, const char* value);

int lex(struct lex_process* process);

struct lex_process* tokens_build_for_string(struct compile_process* compiler, const char* str);

int parse(struct compile_process* process);

bool token_is_symbol(struct token* token, char c);
bool token_is_nl_or_comment_or_newline_seperator(struct token* token);

struct node* node_create(struct node* _node);
void node_set_vector(struct vector* vec, struct vector* root_vec);
void node_push(struct node* node);
struct node* node_peek_or_null();
struct node* node_peek();
struct node* node_pop();

bool node_is_expressionable(struct node* node);
struct node* node_peek_expressionable_or_null();
void make_exp_node(struct node* left_node, struct node* right_node, const char* op);

#endif