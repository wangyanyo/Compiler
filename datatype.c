#include "compiler.h"

bool keyword_is_datatype(const char* val) {
    return S_EQ(val, "void") ||
            S_EQ(val, "short") ||
            S_EQ(val, "int") ||
            S_EQ(val, "long") ||
            S_EQ(val, "float") ||
            S_EQ(val, "double") ||
            S_EQ(val, "char") ||
            S_EQ(val, "struct") ||
            S_EQ(val, "union");
}