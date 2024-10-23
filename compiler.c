#include"compiler.h"
#include<stdarg.h>
#include<stdlib.h>

struct lex_process_functions compile_lex_functions = {
    .next_char = compile_process_next_char,
    .peek_char = compile_process_peek_char,
    .push_char = compile_process_push_char
};

// 模仿GCC出错时的行为
void compile_error(struct compile_process* compiler, const char* msg, ...) {
    // 这一步是处理...的，把msg以及后面的内容都输出，可以把args看作指针
    va_list args;
    va_start(args, msg);
    vfprintf(stderr, msg, args);
    va_end(args);

    fprintf(stderr, " on line %i, col %i in file %s\n", compiler->pos.line, compiler->pos.col, compiler->pos.filename);
    exit(-1);
}

void compile_warning(struct compile_process* compiler, const char* msg, ...) {
    va_list args;
    va_start(args, msg);
    vfprintf(stderr, msg, args);
    va_end(args);

    fprintf(stderr, " on line %i, col %i in file %s\n", compiler->pos.line, compiler->pos.col, compiler->pos.filename);
}

int compile_file(const char* filename, const char* out_filename, int flags) {
    struct compile_process* process = compile_process_create(filename, out_filename, flags);
    if(!process) {
        return COMPILER_FAILED_WITH_ERRORS;
    }
    
    // Perform lexical analysis  词法分析
    struct lex_process* lex_process = lex_process_create(process, &compile_lex_functions, NULL);
    if(!lex_process) {
        return COMPILER_FAILED_WITH_ERRORS;
    }

    // LEXICAL_ANALYSIS_ALL_OK 是 lex函数的状态
    if(lex(lex_process) != LEXICAL_ANALYSIS_ALL_OK) {
        return COMPILER_FAILED_WITH_ERRORS;
    }

    process->token_vec = lex_process->token_vec;

    // Perform parsing  语法分析
    if(parse(process) != PARSE_ALL_OK) {
        return COMPILER_FAILED_WITH_ERRORS;
    }

    // Perform code generation  生成中间代码

    return COMPILER_FILE_COMPILED_OK;
}