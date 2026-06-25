#ifndef LANG_TYPE_INFO_H
#define LANG_TYPE_INFO_H

#ifdef __cplusplus
extern "C"{
#endif

#include "../Utils/Num.h"

enum Type_info_tag{
    TYPE_INFO_TAG_NONE,
    TYPE_INFO_TAG_VOID,
    TYPE_INFO_TAG_BOOL,
    TYPE_INFO_TAG_CHAR,
    TYPE_INFO_TAG_INT,
    TYPE_INFO_TAG_FLOAT,
    TYPE_INFO_TAG_STR
};

typedef struct Type_info{
    enum Type_info_tag m_tag;
    usize m_dimensions;
} Type_info;

typedef struct Type_info_slice{
    usize m_size;
    const Type_info *m_data;
} Type_info_slice;

enum Unary_op{
    UNARY_OP_NONE,
    UNARY_OP_PLUS,
    UNARY_OP_MINUS,
    UNARY_OP_BNEG,
    UNARY_OP_NOT
};

Type_info unary_op_type_info_result(enum Unary_op op, Type_info type_info);

enum Binary_op{
    BINARY_OP_NONE,

    BINARY_OP_ASSIGNMENT,

    BINARY_OP_SUBSCRIPT,

    BINARY_OP_EQ,
    BINARY_OP_NEQ,
    BINARY_OP_LE,
    BINARY_OP_LEQ,
    BINARY_OP_GE,
    BINARY_OP_GEQ,

    BINARY_OP_ADD,
    BINARY_OP_SUB,
    BINARY_OP_MUL,
    BINARY_OP_DIV,
    BINARY_OP_MOD,
    BINARY_OP_POW,

    BINARY_OP_SHL,
    BINARY_OP_SHR,
    BINARY_OP_BAND,
    BINARY_OP_BOR,
    BINARY_OP_XOR,

    BINARY_OP_AND,
    BINARY_OP_OR
};

Type_info binary_op_type_info_result(enum Binary_op op, Type_info lhs, Type_info rhs);

#ifdef __cplusplus
}
#endif

#endif // LANG_TYPE_INFO_H
