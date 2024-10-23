#include<stdio.h>
#include<stdlib.h>
#include"compiler.h"
#include"helpers/vector.h"

// 初始化编译进程
struct compile_process* compile_process_create(const char* filename, const char* filename_out, int flags) {
    // 获取输入文件
    FILE* file = fopen(filename, "r");
    if(!file) {
        return NULL;
    }

    // 获取输出文件
    FILE* out_file = NULL;
    if(filename_out) {
        out_file = fopen(filename_out, "w");
        if(!out_file) {
            return NULL;
        }
    }

    // 创建编译进程实例
    struct compile_process* process = calloc(1, sizeof(struct compile_process));
    process->flags = flags;
    process->cfile.fp = file;
    process->ofile = out_file;
    process->pos.line = 1;
    process->pos.col = 1;
    process->pos.filename = filename;
    process->node_vec = vector_create(sizeof(struct node*));
    process->node_tree_vec = vector_create(sizeof(struct node*));

    return process;
}

// 在文件流里取出下一个字符并返回
char compile_process_next_char(struct lex_process* lex_process) {
    struct compile_process* compiler = lex_process->compiler;
    compiler->pos.col++;
    // 从文件流里读取下一个字符
    char c = getc(compiler->cfile.fp);
    if(c == '\n') {
        compiler->pos.line++;
        compiler->pos.col = 1;
    }

    return c;
}

// 只是想看文件流下一个字符是什么，但是不取出
char compile_process_peek_char(struct lex_process* lex_process) {
    struct compile_process* compiler = lex_process->compiler;
    char c = getc(compiler->cfile.fp);
    ungetc(c, compiler->cfile.fp);
    return c;
}

// 向文件流里放入一个字符
void compile_process_push_char(struct lex_process* lex_process, char c) {
    struct compile_process* compiler = lex_process->compiler;
    ungetc(c, compiler->cfile.fp);
}