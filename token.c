#include "compiler.h"

#define PRIMITIVE_TYPES_TOTAL 7
const char* primitive_types[PRIMITIVE_TYPES_TOTAL] = {
    "void", "char", "short", "int", "long", "float", "double"
};

bool token_is_keyword(struct token* token, const char* value) {
    return token->type == TOKEN_TYPE_KEYWORD && S_EQ(token->sval, value);
}

bool token_is_symbol(struct token* token, char c) {
    return token->type == TOKEN_TYPE_SYMBOL && token->cval == c;
}

bool token_is_nl_or_comment_or_newline_seperator(struct token* token) {
    return token->type == TOKEN_TYPE_NEWLINE || token->type == TOKEN_TYPE_COMMENT || token_is_symbol(token, '\\');
}

void token_print(FILE* fp, struct token* token) {
    if(token->type == TOKEN_TYPE_COMMENT || 
        token->type == TOKEN_TYPE_IDENTIFIER ||
        token->type == TOKEN_TYPE_KEYWORD ||
        token->type == TOKEN_TYPE_OPERATOR ||
        token->type == TOKEN_TYPE_STRING) {

        fprintf(fp, "%s\n", token->sval);
    }
    else if(token->type == TOKEN_TYPE_NEWLINE) {
        fprintf(fp, "newline\n");
    }
    else if(token->type == TOKEN_TYPE_SYMBOL) {
        fprintf(fp, "%c\n",token->cval);
    }
    else {
        fprintf(fp, "(%lld %c)\n",token->llnum, token->cval);
    }
}

bool token_is_primitive_keyword(struct token* token) {
    if(token->type != TOKEN_TYPE_KEYWORD) {
        return false;
    }

    for(int i = 0; i < PRIMITIVE_TYPES_TOTAL; ++i) {
        if(S_EQ(token->sval, primitive_types[i])) {
            return true;
        }
    }
    return false;
}

bool token_is_operator(struct token* token, const char* op) {
    return token && token->type == TOKEN_TYPE_OPERATOR && S_EQ(token->sval, op);
}