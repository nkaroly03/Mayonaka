#include <stdbool.h>

#include "../../hdrs/Allocator/Allocator.h"
#include "../../hdrs/Data_structure/Str_base.h"

#include "../../hdrs/Lang/Type_info.h"

static const char *TYPE_INFO_TAG_STRS[] = {
    [TYPE_INFO_TAG_NONE]  = "none",
    [TYPE_INFO_TAG_VOID]  = "void",
    [TYPE_INFO_TAG_BOOL]  = "bool",
    [TYPE_INFO_TAG_CHAR]  = "char",
    [TYPE_INFO_TAG_INT]   = "int",
    [TYPE_INFO_TAG_FLOAT] = "float",
    [TYPE_INFO_TAG_STR]   = "str",
};

Str_base_result type_info_to_str_base(Type_info type_info, Allocator alloc){
    Str_base result = {0};

    for (usize i = 0; i < type_info.m_dimensions; ++i)
        if (!str_base_append_raw(&result, alloc, "[]"))
            goto oom_error;

    if (!str_base_append_raw(&result, alloc, TYPE_INFO_TAG_STRS[type_info.m_tag]))
        goto oom_error;

    return (Str_base_result){.result = result, .success = true};

oom_error:
    str_base_deinit(&result, alloc);
    return (Str_base_result){0};
}

Type_info unary_op_result_type_info(enum Unary_op op, Type_info type_info){
    Type_info result = {0};

    if (type_info.m_dimensions == 0){
        switch (op){
            case UNARY_OP_NONE:
                break;
            case UNARY_OP_PLUS:
            case UNARY_OP_MINUS:
            case UNARY_OP_NOT:
                if (type_info.m_tag >= TYPE_INFO_TAG_BOOL && type_info.m_tag <= TYPE_INFO_TAG_FLOAT)
                    result = (Type_info){.m_tag = (op != UNARY_OP_NOT) ? type_info.m_tag : TYPE_INFO_TAG_BOOL, .m_dimensions = 0};
                break;
            case UNARY_OP_BNEG:
                if (type_info.m_tag >= TYPE_INFO_TAG_BOOL && type_info.m_tag <= TYPE_INFO_TAG_INT)
                    result = (Type_info){.m_tag = type_info.m_tag, .m_dimensions = 0};
                break;
        }
    }

    return result;
}

Type_info binary_op_result_type_info(enum Binary_op op, Type_info lhs, Type_info rhs){
    if (op == BINARY_OP_ASSIGNMENT && lhs.m_tag > TYPE_INFO_TAG_VOID && lhs.m_dimensions > 0 && rhs.m_tag == TYPE_INFO_TAG_VOID && rhs.m_dimensions == 1)
        return lhs;

    Type_info result = {0};

    if (lhs.m_tag > TYPE_INFO_TAG_VOID && rhs.m_tag > TYPE_INFO_TAG_VOID){
        switch (op){
            case BINARY_OP_NONE:
                break;

            case BINARY_OP_SUBSCRIPT:
                if (rhs.m_dimensions == 0 && rhs.m_tag >= TYPE_INFO_TAG_BOOL && rhs.m_tag <= TYPE_INFO_TAG_INT){
                    if (lhs.m_dimensions > 0)
                        result = (Type_info){.m_tag = lhs.m_tag, .m_dimensions = lhs.m_dimensions - 1};
                    else if (lhs.m_tag == TYPE_INFO_TAG_STR)
                        result = (Type_info){.m_tag = TYPE_INFO_TAG_CHAR, .m_dimensions = 0};
                }
                break;

            case BINARY_OP_ASSIGNMENT:
                if (lhs.m_dimensions > 0 || rhs.m_dimensions > 0){
                    if (lhs.m_dimensions == rhs.m_dimensions && lhs.m_tag == rhs.m_tag)
                        result = lhs;
                }
                else if (lhs.m_dimensions == 0 && rhs.m_dimensions == 0)
                    result = lhs;
                break;

            case BINARY_OP_EQ:
            case BINARY_OP_NEQ:
            case BINARY_OP_LE:
            case BINARY_OP_LEQ:
            case BINARY_OP_GE:
            case BINARY_OP_GEQ:
                if (
                    (lhs.m_dimensions == 0 && rhs.m_dimensions == 0) &&
                    ((lhs.m_tag == TYPE_INFO_TAG_STR && rhs.m_tag == TYPE_INFO_TAG_STR) || (lhs.m_tag != TYPE_INFO_TAG_STR && rhs.m_tag != TYPE_INFO_TAG_STR))
                )
                    result = (Type_info){.m_tag = TYPE_INFO_TAG_BOOL, .m_dimensions = 0};
                break;

            case BINARY_OP_ADD:
                if (lhs.m_dimensions == 0 && rhs.m_dimensions == 0){
                    if (lhs.m_tag == TYPE_INFO_TAG_STR || rhs.m_tag == TYPE_INFO_TAG_STR){
                        if (lhs.m_tag == rhs.m_tag || (lhs.m_tag == TYPE_INFO_TAG_CHAR || rhs.m_tag == TYPE_INFO_TAG_CHAR))
                            result = (Type_info){.m_tag = TYPE_INFO_TAG_STR, .m_dimensions = 0};
                    }
                    else
                        result = (Type_info){.m_tag = (lhs.m_tag > rhs.m_tag) ? lhs.m_tag : rhs.m_tag, .m_dimensions = 0};
                }
                break;
            case BINARY_OP_SUB:
            case BINARY_OP_MUL:
            case BINARY_OP_DIV:
            case BINARY_OP_MOD:
            case BINARY_OP_POW:
                if (lhs.m_dimensions == 0 && rhs.m_dimensions == 0 && lhs.m_tag <= TYPE_INFO_TAG_FLOAT && rhs.m_tag <= TYPE_INFO_TAG_FLOAT)
                    result = (Type_info){.m_tag = (lhs.m_tag > rhs.m_tag) ? lhs.m_tag : rhs.m_tag, .m_dimensions = 0};
                break;

            case BINARY_OP_SHL:
            case BINARY_OP_SHR:
            case BINARY_OP_BAND:
            case BINARY_OP_BOR:
            case BINARY_OP_XOR:
                if (lhs.m_dimensions == 0 && rhs.m_dimensions == 0 && lhs.m_tag <= TYPE_INFO_TAG_INT && rhs.m_tag <= TYPE_INFO_TAG_INT)
                    result = (Type_info){.m_tag = (lhs.m_tag > rhs.m_tag) ? lhs.m_tag : rhs.m_tag, .m_dimensions = 0};
                break;

            case BINARY_OP_AND:
            case BINARY_OP_OR:
                if (lhs.m_dimensions == 0 && rhs.m_dimensions == 0 && lhs.m_tag <= TYPE_INFO_TAG_FLOAT && rhs.m_tag <= TYPE_INFO_TAG_FLOAT)
                    result = (Type_info){.m_tag = TYPE_INFO_TAG_BOOL, .m_dimensions = 0};
                break;
        }
    }

    return result;
}
