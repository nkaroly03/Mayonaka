#include <stdbool.h>

#include "../../hdrs/Utils/Utils.h"

#include "../../hdrs/Lang/Type_info.h"

// ------------------------------------------------------------------------------------------------

static bool type_info_tag_is_int_like(enum Type_info_tag tag){
    return tag >= TYPE_INFO_TAG_BOOL && tag <= TYPE_INFO_TAG_INT;
}
static bool type_info_tag_is_arithmetic(enum Type_info_tag tag){
    return tag >= TYPE_INFO_TAG_BOOL && tag <= TYPE_INFO_TAG_FLOAT;
}
static bool type_info_tag_for_bin_op_is_valid(enum Type_info_tag tag){
    return tag != TYPE_INFO_TAG_NONE && tag != TYPE_INFO_TAG_VOID;
}
static bool type_info_tag_is_type_id(enum Type_info_tag tag){
    return tag < TYPE_INFO_TAG_NONE || tag > TYPE_INFO_TAG_STR;
}

// ------------------------------------------------------------------------------------------------

const Type_info TYPE_INFO_VOID  = {.m_tag = TYPE_INFO_TAG_VOID,  .m_dimensions = 0};
const Type_info TYPE_INFO_BOOL  = {.m_tag = TYPE_INFO_TAG_BOOL,  .m_dimensions = 0};
const Type_info TYPE_INFO_CHAR  = {.m_tag = TYPE_INFO_TAG_CHAR,  .m_dimensions = 0};
const Type_info TYPE_INFO_INT   = {.m_tag = TYPE_INFO_TAG_INT,   .m_dimensions = 0};
const Type_info TYPE_INFO_FLOAT = {.m_tag = TYPE_INFO_TAG_FLOAT, .m_dimensions = 0};
const Type_info TYPE_INFO_STR   = {.m_tag = TYPE_INFO_TAG_STR,   .m_dimensions = 0};

Type_info unary_op_type_info_result(enum Unary_op op, Type_info type_info){
    Type_info result = {.m_tag = TYPE_INFO_TAG_NONE};

    if (type_info.m_dimensions == 0){
        switch (op){
            case UNARY_OP_NONE:
                break;
            case UNARY_OP_PLUS:
            case UNARY_OP_MINUS:
            case UNARY_OP_NOT:
                if (type_info_tag_is_arithmetic(type_info.m_tag))
                    result = (op != UNARY_OP_NOT) ? type_info : TYPE_INFO_BOOL;
                break;
            case UNARY_OP_BNEG:
                if (type_info_tag_is_int_like(type_info.m_tag))
                    result = type_info;
                break;
        }
    }

    return result;
}

Type_info binary_op_type_info_result(enum Binary_op op, Type_info lhs, Type_info rhs){
    Type_info result = {.m_tag = TYPE_INFO_TAG_NONE};

    if (type_info_tag_for_bin_op_is_valid(lhs.m_tag) && type_info_tag_for_bin_op_is_valid(rhs.m_tag)){
        switch (op){
            case BINARY_OP_NONE:
                break;

            case BINARY_OP_SUBSCRIPT:
                if (rhs.m_dimensions == 0 && type_info_tag_is_int_like(rhs.m_tag)){
                    if (lhs.m_dimensions > 0)
                        result = (Type_info){.m_tag = lhs.m_tag, .m_dimensions = lhs.m_dimensions - 1};
                    else if (lhs.m_tag == TYPE_INFO_TAG_STR)
                        result = TYPE_INFO_CHAR;
                }
                break;

            case BINARY_OP_POW:
                if (
                    lhs.m_dimensions == 0 && rhs.m_dimensions == 0 &&
                    type_info_tag_is_arithmetic(lhs.m_tag) && type_info_tag_is_arithmetic(rhs.m_tag) &&
                    (!type_info_tag_is_int_like(lhs.m_tag) || !type_info_tag_is_int_like(rhs.m_tag))
                )
                    result = TYPE_INFO_FLOAT;
                break;

            case BINARY_OP_AS:
                if (
                    lhs.m_dimensions == rhs.m_dimensions && (
                        lhs.m_tag == rhs.m_tag || (lhs.m_dimensions == 0 && !type_info_tag_is_type_id(lhs.m_tag) && !type_info_tag_is_type_id(rhs.m_tag))
                    )
                )
                    result = rhs;
                break;

            case BINARY_OP_MUL:
            case BINARY_OP_DIV:
            case BINARY_OP_REM:
            case BINARY_OP_SUB:
                if (lhs.m_dimensions == 0 && rhs.m_dimensions == 0 && type_info_tag_is_arithmetic(lhs.m_tag) && type_info_tag_is_arithmetic(rhs.m_tag))
                    result = (Type_info){.m_tag = max(lhs.m_tag, rhs.m_tag), .m_dimensions = 0};
                break;

            case BINARY_OP_ADD:
                if (lhs.m_dimensions == 0 && rhs.m_dimensions == 0){
                    if (lhs.m_tag == TYPE_INFO_TAG_STR || rhs.m_tag == TYPE_INFO_TAG_STR){
                        if (lhs.m_tag == rhs.m_tag || (lhs.m_tag == TYPE_INFO_TAG_CHAR || rhs.m_tag == TYPE_INFO_TAG_CHAR))
                            result = TYPE_INFO_STR;
                    }
                    else if (type_info_tag_is_arithmetic(lhs.m_tag) && type_info_tag_is_arithmetic(rhs.m_tag))
                        result = (Type_info){.m_tag = max(lhs.m_tag, rhs.m_tag), .m_dimensions = 0};
                }
                break;

            case BINARY_OP_SHL:
            case BINARY_OP_SHR:
            case BINARY_OP_BAND:
            case BINARY_OP_XOR:
            case BINARY_OP_BOR:
                if (lhs.m_dimensions == 0 && rhs.m_dimensions == 0 && type_info_tag_is_int_like(lhs.m_tag) && type_info_tag_is_int_like(rhs.m_tag))
                    result = (Type_info){.m_tag = max(lhs.m_tag, rhs.m_tag), .m_dimensions = 0};
                break;

            case BINARY_OP_CMP_LE:
            case BINARY_OP_CMP_LEQ:
            case BINARY_OP_CMP_GE:
            case BINARY_OP_CMP_GEQ:
            case BINARY_OP_CMP_EQ:
            case BINARY_OP_CMP_NEQ:
                if (
                    (lhs.m_dimensions == 0 && rhs.m_dimensions == 0) &&
                    ((lhs.m_tag == TYPE_INFO_TAG_STR && rhs.m_tag == TYPE_INFO_TAG_STR) || (type_info_tag_is_arithmetic(lhs.m_tag) && type_info_tag_is_arithmetic(rhs.m_tag)))
                )
                    result = TYPE_INFO_BOOL;
                break;

            case BINARY_OP_AND:
            case BINARY_OP_OR:
                if (lhs.m_dimensions == 0 && rhs.m_dimensions == 0 && type_info_tag_is_arithmetic(lhs.m_tag) && type_info_tag_is_arithmetic(rhs.m_tag))
                    result = TYPE_INFO_BOOL;
                break;

            case BINARY_OP_ASSIGNMENT:
                if (lhs.m_dimensions > 0 || rhs.m_dimensions > 0){
                    if (lhs.m_dimensions == rhs.m_dimensions && lhs.m_tag == rhs.m_tag)
                        result = lhs;
                }
                else if (type_info_tag_is_type_id(lhs.m_tag) || type_info_tag_is_type_id(rhs.m_tag)){
                    if (lhs.m_tag == rhs.m_tag)
                        result = lhs;
                }
                else
                    result = lhs;
                break;
        }
    }

    return result;
}
