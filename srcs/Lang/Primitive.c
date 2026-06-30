#include <assert.h>
#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>

#include "../../hdrs/Allocator/Allocator.h"
#include "../../hdrs/Data_structure/Str_base.h"
#include "../../hdrs/Data_structure/Str_view.h"
#include "../../hdrs/Data_structure/Vec_base.h"
#include "../../hdrs/Utils/Cmp.h"
#include "../../hdrs/Utils/Num.h"

#include "../../hdrs/Lang/Primitive.h"

// ------------------------------------------------------------------------------------------------

static const Primitive_op_result OOM_ERROR = {.error = PRIMITIVE_OP_ERROR_OOM, .error_msg = "Out of memory"};

#define runtime_error(msg) (Primitive_op_result){.error = PRIMITIVE_OP_ERROR_RUNTIME, .error_msg = (msg)}

enum Cmp_bin_op{
    CMP_BIN_OP_EQ,
    CMP_BIN_OP_NEQ,
    CMP_BIN_OP_LE,
    CMP_BIN_OP_LEQ,
    CMP_BIN_OP_GE,
    CMP_BIN_OP_GEQ
};

static Primitive_op_result primitive_cmp(Primitive *self, Allocator alloc, const Primitive *other, enum Cmp_bin_op op){
    if (self->m_tag == PRIMITIVE_TAG_LIST || other->m_tag == PRIMITIVE_TAG_LIST)
        return runtime_error("Trying to use comparison on list(s)");

    if (self->m_tag == PRIMITIVE_TAG_STR || other->m_tag == PRIMITIVE_TAG_STR){
        if (self->m_tag != PRIMITIVE_TAG_STR || other->m_tag != PRIMITIVE_TAG_STR)
            return runtime_error("Trying to compare a <str> to a non-str");

        bool cmp;
        switch (op){
            case CMP_BIN_OP_EQ:  cmp = cmp_eq_Str_base (&self->m_str_data, &other->m_str_data); break;
            case CMP_BIN_OP_NEQ: cmp = cmp_neq_Str_base(&self->m_str_data, &other->m_str_data); break;
            case CMP_BIN_OP_LE:  cmp = cmp_le_Str_base (&self->m_str_data, &other->m_str_data); break;
            case CMP_BIN_OP_LEQ: cmp = cmp_leq_Str_base(&self->m_str_data, &other->m_str_data); break;
            case CMP_BIN_OP_GE:  cmp = cmp_ge_Str_base (&self->m_str_data, &other->m_str_data); break;
            case CMP_BIN_OP_GEQ: cmp = cmp_geq_Str_base(&self->m_str_data, &other->m_str_data); break;
        }
        str_base_deinit(&self->m_str_data, alloc);
        *self = (Primitive){.m_tag = PRIMITIVE_TAG_BOOL, .m_bool_data = cmp};
    }
    else{
        bool cmp;

        Primitive lhs_temp = *self;
        Primitive rhs_temp = *other;

        switch (lhs_temp.m_tag){
            case PRIMITIVE_TAG_BOOL:
            case PRIMITIVE_TAG_CHAR:
            case PRIMITIVE_TAG_INT:
                switch (rhs_temp.m_tag){
                    case PRIMITIVE_TAG_BOOL:
                    case PRIMITIVE_TAG_CHAR:
                    case PRIMITIVE_TAG_INT:
                        (void)primitive_to_int(&lhs_temp, alloc);
                        (void)primitive_to_int(&rhs_temp, alloc);
                        switch (op){
                            case CMP_BIN_OP_EQ:  cmp = cmp_eq_i64 (&lhs_temp.m_int_data, &rhs_temp.m_int_data); break;
                            case CMP_BIN_OP_NEQ: cmp = cmp_neq_i64(&lhs_temp.m_int_data, &rhs_temp.m_int_data); break;
                            case CMP_BIN_OP_LE:  cmp = cmp_le_i64 (&lhs_temp.m_int_data, &rhs_temp.m_int_data); break;
                            case CMP_BIN_OP_LEQ: cmp = cmp_leq_i64(&lhs_temp.m_int_data, &rhs_temp.m_int_data); break;
                            case CMP_BIN_OP_GE:  cmp = cmp_ge_i64 (&lhs_temp.m_int_data, &rhs_temp.m_int_data); break;
                            case CMP_BIN_OP_GEQ: cmp = cmp_geq_i64(&lhs_temp.m_int_data, &rhs_temp.m_int_data); break;
                        }
                        break;
                    case PRIMITIVE_TAG_FLOAT:
                        (void)primitive_to_float(&lhs_temp, alloc);
                        goto float_cmp;
                    case PRIMITIVE_TAG_STR:
                        break;
                    case PRIMITIVE_TAG_LIST:
                        break;
                }
                break;
            case PRIMITIVE_TAG_FLOAT:
                (void)primitive_to_float(&rhs_temp, alloc);
            float_cmp:
                switch (op){
                    case CMP_BIN_OP_EQ:  cmp = cmp_eq_f64 (&lhs_temp.m_float_data, &rhs_temp.m_float_data); break;
                    case CMP_BIN_OP_NEQ: cmp = cmp_neq_f64(&lhs_temp.m_float_data, &rhs_temp.m_float_data); break;
                    case CMP_BIN_OP_LE:  cmp = cmp_le_f64 (&lhs_temp.m_float_data, &rhs_temp.m_float_data); break;
                    case CMP_BIN_OP_LEQ: cmp = cmp_leq_f64(&lhs_temp.m_float_data, &rhs_temp.m_float_data); break;
                    case CMP_BIN_OP_GE:  cmp = cmp_ge_f64 (&lhs_temp.m_float_data, &rhs_temp.m_float_data); break;
                    case CMP_BIN_OP_GEQ: cmp = cmp_geq_f64(&lhs_temp.m_float_data, &rhs_temp.m_float_data); break;
                }
                break;
            case PRIMITIVE_TAG_STR:
            case PRIMITIVE_TAG_LIST:
                break;
        }

        *self = (Primitive){.m_tag = PRIMITIVE_TAG_BOOL, .m_bool_data = cmp};
    }

    return (Primitive_op_result){0};
}

enum Bin_op{
    BIN_OP_SUB,
    BIN_OP_MUL,
    BIN_OP_DIV,
    BIN_OP_MOD,
    BIN_OP_POW,
    
    BIN_OP_SHL,
    BIN_OP_SHR,
    BIN_OP_BAND,
    BIN_OP_BOR,
    BIN_OP_XOR,
};

static Primitive_op_result primitive_bin_op(Primitive *self, const Primitive *other, enum Bin_op op){
    if (self->m_tag == PRIMITIVE_TAG_LIST || other->m_tag == PRIMITIVE_TAG_LIST){
        switch (op){
            case BIN_OP_SUB:  return runtime_error("Trying to use subtration on <list>");
            case BIN_OP_MUL:  return runtime_error("Trying to use multiplication on <list>");
            case BIN_OP_DIV:  return runtime_error("Trying to use division on <list>");
            case BIN_OP_MOD:  return runtime_error("Trying to use modulus on <list>");
            case BIN_OP_POW:  return runtime_error("Trying to use exponentiation on <list>");
            case BIN_OP_SHL:  return runtime_error("Trying to use left shift on <list>");
            case BIN_OP_SHR:  return runtime_error("Trying to use right shift on <list>");
            case BIN_OP_BAND: return runtime_error("Trying to use bitwise and on <list>");
            case BIN_OP_BOR:  return runtime_error("Trying to use bitwise or on <list>");
            case BIN_OP_XOR:  return runtime_error("Trying to use xor on <list>");
        }
    }
    if (self->m_tag == PRIMITIVE_TAG_STR || other->m_tag == PRIMITIVE_TAG_STR){
        switch (op){
            case BIN_OP_SUB:  return runtime_error("Trying to use subtration on <str>");
            case BIN_OP_MUL:  return runtime_error("Trying to use multiplication on <str>");
            case BIN_OP_DIV:  return runtime_error("Trying to use division on <str>");
            case BIN_OP_MOD:  return runtime_error("Trying to use modulus on <str>");
            case BIN_OP_POW:  return runtime_error("Trying to use exponentiation on <str>");
            case BIN_OP_SHL:  return runtime_error("Trying to use left shift on <str>");
            case BIN_OP_SHR:  return runtime_error("Trying to use right shift on <str>");
            case BIN_OP_BAND: return runtime_error("Trying to use bitwise and on <str>");
            case BIN_OP_BOR:  return runtime_error("Trying to use bitwise or on <str>");
            case BIN_OP_XOR:  return runtime_error("Trying to use xor on <str>");
        }
    }

    Allocator alloc_placeholder = {0};

    Primitive lhs_temp = *self;
    Primitive rhs_temp = *other;

    switch (lhs_temp.m_tag){
        case PRIMITIVE_TAG_BOOL:
            switch (rhs_temp.m_tag){
                case PRIMITIVE_TAG_CHAR:
                    (void)primitive_to_char(&lhs_temp, alloc_placeholder);
                    break;
                case PRIMITIVE_TAG_INT:
                    (void)primitive_to_int(&lhs_temp, alloc_placeholder);
                    break;
                case PRIMITIVE_TAG_FLOAT:
                    (void)primitive_to_float(&lhs_temp, alloc_placeholder);
                    break;
                case PRIMITIVE_TAG_BOOL:
                case PRIMITIVE_TAG_STR:
                case PRIMITIVE_TAG_LIST:
                    break;
            }
            break;
        case PRIMITIVE_TAG_CHAR:
            switch (rhs_temp.m_tag){
                case PRIMITIVE_TAG_BOOL:
                    (void)primitive_to_char(&rhs_temp, alloc_placeholder);
                    break;
                case PRIMITIVE_TAG_INT:
                    (void)primitive_to_int(&lhs_temp, alloc_placeholder);
                    break;
                case PRIMITIVE_TAG_FLOAT:
                    (void)primitive_to_float(&lhs_temp, alloc_placeholder);
                    break;
                case PRIMITIVE_TAG_CHAR:
                case PRIMITIVE_TAG_STR:
                case PRIMITIVE_TAG_LIST:
                    break;
            }
            break;
        case PRIMITIVE_TAG_INT:
            switch (rhs_temp.m_tag){
                case PRIMITIVE_TAG_BOOL:
                case PRIMITIVE_TAG_CHAR:
                    (void)primitive_to_int(&rhs_temp, alloc_placeholder);
                    break;
                case PRIMITIVE_TAG_FLOAT:
                    (void)primitive_to_float(&lhs_temp, alloc_placeholder);
                    break;
                case PRIMITIVE_TAG_INT:
                case PRIMITIVE_TAG_STR:
                case PRIMITIVE_TAG_LIST:
                    break;
            }
            break;
        case PRIMITIVE_TAG_FLOAT:
            switch (rhs_temp.m_tag){
                case PRIMITIVE_TAG_BOOL:
                case PRIMITIVE_TAG_CHAR:
                case PRIMITIVE_TAG_INT:
                    (void)primitive_to_float(&rhs_temp, alloc_placeholder);
                    break;
                case PRIMITIVE_TAG_FLOAT:
                case PRIMITIVE_TAG_STR:
                case PRIMITIVE_TAG_LIST:
                    break;
            }
            break;
        case PRIMITIVE_TAG_STR:
            break;
        case PRIMITIVE_TAG_LIST:
            break;
    }

    switch (op){
        case BIN_OP_SUB:
            switch (lhs_temp.m_tag){
                case PRIMITIVE_TAG_BOOL:  lhs_temp.m_bool_data  = (lhs_temp.m_bool_data != rhs_temp.m_bool_data);    break;
                case PRIMITIVE_TAG_CHAR:  lhs_temp.m_char_data  = (u8)(lhs_temp.m_char_data - rhs_temp.m_char_data); break;
                case PRIMITIVE_TAG_INT:   lhs_temp.m_int_data   = (lhs_temp.m_int_data - rhs_temp.m_int_data);       break;
                case PRIMITIVE_TAG_FLOAT: lhs_temp.m_float_data = (lhs_temp.m_float_data - rhs_temp.m_float_data);   break;
                case PRIMITIVE_TAG_STR:
                case PRIMITIVE_TAG_LIST:
                    break;
            }
            break;
        case BIN_OP_MUL:
            switch (lhs_temp.m_tag){
                case PRIMITIVE_TAG_BOOL:  lhs_temp.m_bool_data  = (lhs_temp.m_bool_data && rhs_temp.m_bool_data);    break;
                case PRIMITIVE_TAG_CHAR:  lhs_temp.m_char_data  = (u8)(lhs_temp.m_char_data * rhs_temp.m_char_data); break;
                case PRIMITIVE_TAG_INT:   lhs_temp.m_int_data   = (lhs_temp.m_int_data * rhs_temp.m_int_data);       break;
                case PRIMITIVE_TAG_FLOAT: lhs_temp.m_float_data = (lhs_temp.m_float_data * rhs_temp.m_float_data);   break;
                case PRIMITIVE_TAG_STR:
                case PRIMITIVE_TAG_LIST:
                    break;
            }
            break;
        case BIN_OP_DIV:
            switch (lhs_temp.m_tag){
                case PRIMITIVE_TAG_BOOL:  lhs_temp.m_bool_data  = (lhs_temp.m_bool_data == rhs_temp.m_bool_data);    break;
                case PRIMITIVE_TAG_CHAR:  lhs_temp.m_char_data  = (u8)(lhs_temp.m_char_data / rhs_temp.m_char_data); break;
                case PRIMITIVE_TAG_INT:   lhs_temp.m_int_data   = (lhs_temp.m_int_data / rhs_temp.m_int_data);       break;
                case PRIMITIVE_TAG_FLOAT: lhs_temp.m_float_data = (lhs_temp.m_float_data / rhs_temp.m_float_data);   break;
                case PRIMITIVE_TAG_STR:
                case PRIMITIVE_TAG_LIST:
                    break;
            }
            break;
        case BIN_OP_MOD:
            switch (lhs_temp.m_tag){
                case PRIMITIVE_TAG_BOOL:  lhs_temp.m_bool_data  = false;                                             break;
                case PRIMITIVE_TAG_CHAR:  lhs_temp.m_char_data  = (u8)(lhs_temp.m_char_data % rhs_temp.m_char_data); break;
                case PRIMITIVE_TAG_INT:   lhs_temp.m_int_data   = (lhs_temp.m_int_data % rhs_temp.m_int_data);       break;
                case PRIMITIVE_TAG_FLOAT: lhs_temp.m_float_data = fmod(lhs_temp.m_float_data, rhs_temp.m_float_data);break;
                case PRIMITIVE_TAG_STR:
                case PRIMITIVE_TAG_LIST:
                    break;
            }
            break;
        case BIN_OP_POW:
            switch (lhs_temp.m_tag){
                case PRIMITIVE_TAG_BOOL:  lhs_temp.m_bool_data  = (bool)pow((f64)lhs_temp.m_bool_data, (f64)rhs_temp.m_bool_data); break;
                case PRIMITIVE_TAG_CHAR:  lhs_temp.m_char_data  = (u8)pow((f64)lhs_temp.m_char_data, (f64)rhs_temp.m_char_data);   break;
                case PRIMITIVE_TAG_INT:   lhs_temp.m_int_data   = (i64)pow((f64)lhs_temp.m_int_data, (f64)rhs_temp.m_int_data);    break;
                case PRIMITIVE_TAG_FLOAT: lhs_temp.m_float_data = pow(lhs_temp.m_float_data, rhs_temp.m_float_data);               break;
                case PRIMITIVE_TAG_STR:
                case PRIMITIVE_TAG_LIST:
                    break;
            }
            break;
        case BIN_OP_SHL:
            switch (lhs_temp.m_tag){
                case PRIMITIVE_TAG_BOOL:  lhs_temp.m_bool_data = (lhs_temp.m_bool_data && !rhs_temp.m_bool_data);    break;
                case PRIMITIVE_TAG_CHAR:  lhs_temp.m_char_data = (u8)(lhs_temp.m_char_data << rhs_temp.m_char_data); break;
                case PRIMITIVE_TAG_INT:   lhs_temp.m_int_data  = lhs_temp.m_int_data << rhs_temp.m_int_data;         break;
                case PRIMITIVE_TAG_FLOAT: return runtime_error("Trying to use left shift between non-int types");
                case PRIMITIVE_TAG_STR:
                case PRIMITIVE_TAG_LIST:
                    break;
            }
            break;
        case BIN_OP_SHR:
            switch (lhs_temp.m_tag){
                case PRIMITIVE_TAG_BOOL: lhs_temp.m_bool_data = (lhs_temp.m_bool_data && !rhs_temp.m_bool_data);    break;
                case PRIMITIVE_TAG_CHAR: lhs_temp.m_char_data = (u8)(lhs_temp.m_char_data >> rhs_temp.m_char_data); break;
                case PRIMITIVE_TAG_INT:
                    if (rhs_temp.m_int_data > 0){
                        bool is_negative = (lhs_temp.m_int_data < 0);
                        lhs_temp.m_int_data >>= rhs_temp.m_int_data;
                        if (is_negative)
                            lhs_temp.m_int_data |= (i64)~((U64_MSBIT >> ((u64)rhs_temp.m_int_data - 1)) - 1);
                    }
                    break;
                case PRIMITIVE_TAG_FLOAT:
                    return runtime_error("Trying to use right shift between non-int types");
                case PRIMITIVE_TAG_STR:
                case PRIMITIVE_TAG_LIST:
                    break;
            }
            break;
        case BIN_OP_BAND:
            switch (lhs_temp.m_tag){
                case PRIMITIVE_TAG_BOOL:  lhs_temp.m_bool_data = (lhs_temp.m_bool_data & rhs_temp.m_bool_data);     break;
                case PRIMITIVE_TAG_CHAR:  lhs_temp.m_char_data = (u8)(lhs_temp.m_char_data & rhs_temp.m_char_data); break;
                case PRIMITIVE_TAG_INT:   lhs_temp.m_int_data  = lhs_temp.m_int_data & rhs_temp.m_int_data;         break;
                case PRIMITIVE_TAG_FLOAT: return runtime_error("Trying to use bitwise and between non-int types");
                case PRIMITIVE_TAG_STR:
                case PRIMITIVE_TAG_LIST:
                    break;
            }
            break;
        case BIN_OP_BOR:
            switch (lhs_temp.m_tag){
                case PRIMITIVE_TAG_BOOL:  lhs_temp.m_bool_data = (lhs_temp.m_bool_data | rhs_temp.m_bool_data);     break;
                case PRIMITIVE_TAG_CHAR:  lhs_temp.m_char_data = (u8)(lhs_temp.m_char_data | rhs_temp.m_char_data); break;
                case PRIMITIVE_TAG_INT:   lhs_temp.m_int_data  = lhs_temp.m_int_data | rhs_temp.m_int_data;         break;
                case PRIMITIVE_TAG_FLOAT: return runtime_error("Trying to use bitwise or between non-int types");
                case PRIMITIVE_TAG_STR:
                case PRIMITIVE_TAG_LIST:
                    break;
            }
            break;
        case BIN_OP_XOR:
            switch (lhs_temp.m_tag){
                case PRIMITIVE_TAG_BOOL:  lhs_temp.m_bool_data = (lhs_temp.m_bool_data ^ rhs_temp.m_bool_data);     break;
                case PRIMITIVE_TAG_CHAR:  lhs_temp.m_char_data = (u8)(lhs_temp.m_char_data ^ rhs_temp.m_char_data); break;
                case PRIMITIVE_TAG_INT:   lhs_temp.m_int_data  = lhs_temp.m_int_data ^ rhs_temp.m_int_data;         break;
                case PRIMITIVE_TAG_FLOAT: return runtime_error("Trying to use xor between non-int types");
                case PRIMITIVE_TAG_STR:
                case PRIMITIVE_TAG_LIST:
                    break;
            }
            break;
    }

    *self = lhs_temp;

    return (Primitive_op_result){0};
}

// ------------------------------------------------------------------------------------------------

void primitive_deinit(const Primitive *self, Allocator alloc){
    assert(self && "<self> is never null");

    switch (self->m_tag){
        case PRIMITIVE_TAG_BOOL:
        case PRIMITIVE_TAG_CHAR:
        case PRIMITIVE_TAG_INT:
        case PRIMITIVE_TAG_FLOAT:
            break;
        case PRIMITIVE_TAG_STR:
            str_base_deinit(&self->m_str_data, alloc);
            break;
        case PRIMITIVE_TAG_LIST:
            if (--self->m_list_data->m_ref_count == 0){
                vec_base_for_each(self->m_list_data->m_data, it){
                    primitive_deinit(it, alloc);
                }
                allocator_free(alloc, self->m_list_data, 1);
            }
            break;
    }
}

Primitive_op_result primitive_to_bool(Primitive *self, Allocator alloc){
    assert(self && "<self> is never null");

    switch (self->m_tag){
        case PRIMITIVE_TAG_BOOL:
            break;
        case PRIMITIVE_TAG_CHAR:
            *self = (Primitive){.m_tag = PRIMITIVE_TAG_BOOL, .m_bool_data = (bool)self->m_char_data};
            break;
        case PRIMITIVE_TAG_INT:
            *self = (Primitive){.m_tag = PRIMITIVE_TAG_BOOL, .m_bool_data = (bool)self->m_int_data};
            break;
        case PRIMITIVE_TAG_FLOAT:
            *self = (Primitive){.m_tag = PRIMITIVE_TAG_BOOL, .m_bool_data = (bool)self->m_float_data};
            break;
        case PRIMITIVE_TAG_STR:
            {
                Str_view sv = str_view_trim_right_while(str_view_trim_left_while(str_base_to_str_view(&self->m_str_data), isspace), isspace);

                Str_view match;
                bool val;
                if (
                    (val = false, match = str_view_init("false"), cmp_eq_Str_view(&sv, &match)) ||
                    (val = true,  match = str_view_init("true" ), cmp_eq_Str_view(&sv, &match))
                ){
                    str_base_deinit(&self->m_str_data, alloc);
                    *self = (Primitive){.m_tag = PRIMITIVE_TAG_BOOL, .m_bool_data = val};
                }
                else
                    return runtime_error("Trying to convert <str> not containing \"false\" or \"true\" to <bool>");
            }
            break;
        case PRIMITIVE_TAG_LIST:
            return runtime_error("Trying to convert <list> to <bool>");
    }

    return (Primitive_op_result){0};
}
Primitive_op_result primitive_to_char(Primitive *self, Allocator alloc){
    assert(self && "<self> is never null");

    switch (self->m_tag){
        case PRIMITIVE_TAG_BOOL:
            *self = (Primitive){.m_tag = PRIMITIVE_TAG_CHAR, .m_char_data = self->m_bool_data};
            break;
        case PRIMITIVE_TAG_CHAR:
            break;
        case PRIMITIVE_TAG_INT:
            *self = (Primitive){.m_tag = PRIMITIVE_TAG_CHAR, .m_char_data = (u8)self->m_int_data};
            break;
        case PRIMITIVE_TAG_FLOAT:
            *self = (Primitive){.m_tag = PRIMITIVE_TAG_CHAR, .m_char_data = (u8)self->m_float_data};
            break;
        case PRIMITIVE_TAG_STR:
            if (str_base_size(&self->m_str_data) == 1){
                u8 c = (u8)str_base_data(&self->m_str_data)[0];
                str_base_deinit(&self->m_str_data, alloc);
                *self = (Primitive){.m_tag = PRIMITIVE_TAG_CHAR, .m_char_data = c};
            }
            else
                return runtime_error("Trying to convert <str> with size != 1 to <char>");
            break;
        case PRIMITIVE_TAG_LIST:
            return runtime_error("Trying to convert <list> to <char>");
    }

    return (Primitive_op_result){0};
}
Primitive_op_result primitive_to_int(Primitive *self, Allocator alloc){
    assert(self && "<self> is never null");

    switch (self->m_tag){
        case PRIMITIVE_TAG_BOOL:
            *self = (Primitive){.m_tag = PRIMITIVE_TAG_INT, .m_int_data = self->m_bool_data};
            break;
        case PRIMITIVE_TAG_CHAR:
            *self = (Primitive){.m_tag = PRIMITIVE_TAG_INT, .m_int_data = self->m_char_data};
            break;
        case PRIMITIVE_TAG_INT:
            break;
        case PRIMITIVE_TAG_FLOAT:
            *self = (Primitive){.m_tag = PRIMITIVE_TAG_INT, .m_int_data = (i64)self->m_float_data};
            break;
        case PRIMITIVE_TAG_STR:
            {
                char *data = str_base_data(&self->m_str_data);

                char *end;
                i64 val = (i64)strtoll(data, &end, 0);

                if (data == end)
                    goto str_to_int_conversion_error;
                else{
                    while (*end){
                        if (*end != ' '){
                        str_to_int_conversion_error:
                            return runtime_error("Failed to convert <str> to <int>");
                        }
                    }
                }

                str_base_deinit(&self->m_str_data, alloc);
                *self = (Primitive){.m_tag = PRIMITIVE_TAG_INT, .m_int_data = val};
            }
            break;
        case PRIMITIVE_TAG_LIST:
            return runtime_error("Trying to convert <list> to <int>");
    }

    return (Primitive_op_result){0};
}
Primitive_op_result primitive_to_float(Primitive *self, Allocator alloc){
    assert(self && "<self> is never null");

    switch (self->m_tag){
        case PRIMITIVE_TAG_BOOL:
            *self = (Primitive){.m_tag = PRIMITIVE_TAG_FLOAT, .m_float_data = (f64)self->m_bool_data};
            break;
        case PRIMITIVE_TAG_CHAR:
            *self = (Primitive){.m_tag = PRIMITIVE_TAG_FLOAT, .m_float_data = (f64)self->m_char_data};
            break;
        case PRIMITIVE_TAG_INT:
            *self = (Primitive){.m_tag = PRIMITIVE_TAG_FLOAT, .m_float_data = (f64)self->m_int_data};
            break;
        case PRIMITIVE_TAG_FLOAT:
            break;
        case PRIMITIVE_TAG_STR:
            {
                char *data = str_base_data(&self->m_str_data);

                char *end;
                f64 val = strtod(data, &end);

                if (data == end)
                    goto str_to_float_conversion_error;
                else{
                    while (*end){
                        if (*end != ' '){
                        str_to_float_conversion_error:
                            return runtime_error("Failed to convert <str> to <float>");
                        }
                    }
                }

                str_base_deinit(&self->m_str_data, alloc);
                *self = (Primitive){.m_tag = PRIMITIVE_TAG_FLOAT, .m_float_data = val};
            }
            break;
        case PRIMITIVE_TAG_LIST:
            return runtime_error("Trying to convert <list> to <float>");
    }

    return (Primitive_op_result){0};
}
Primitive_op_result primitive_to_str(Primitive *self, Allocator alloc){
    assert(self && "<self> is never null");

    Str_base_result str_result = {.success = true};

    switch (self->m_tag){
        case PRIMITIVE_TAG_BOOL:
            str_result = str_base_init_raw(alloc, (self->m_bool_data) ? "true" : "false");
            break;
        case PRIMITIVE_TAG_CHAR:
            str_result = str_base_init_raw(alloc, (char[]){(char)self->m_char_data, '\0'});
            break;
        case PRIMITIVE_TAG_INT:
            str_result = str_base_init_fmt(alloc, I64_PFMT, self->m_int_data);
            break;
        case PRIMITIVE_TAG_FLOAT:
            str_result = str_base_init_fmt(alloc, "%lf", self->m_float_data);
            break;
        case PRIMITIVE_TAG_STR:
            str_result.result = self->m_str_data;
            break;
        case PRIMITIVE_TAG_LIST:
            return runtime_error("Trying to convert <list> to <float>");
    }

    if (!str_result.success)
        return OOM_ERROR;

    *self = (Primitive){.m_tag = PRIMITIVE_TAG_STR, .m_str_data = str_result.result};

    return (Primitive_op_result){0};
}

Primitive_op_result primitive_neg(Primitive *self){
    assert(self && "<self> is never null");

    switch (self->m_tag){
        case PRIMITIVE_TAG_BOOL:  self->m_bool_data  = !self->m_bool_data;     break;
        case PRIMITIVE_TAG_CHAR:  self->m_char_data  = (u8)-self->m_char_data; break;
        case PRIMITIVE_TAG_INT:   self->m_int_data   = (i64)-self->m_int_data; break;
        case PRIMITIVE_TAG_FLOAT: self->m_float_data = -self->m_float_data;    break;
        case PRIMITIVE_TAG_STR:   return runtime_error("Trying to use negation on <str>");
        case PRIMITIVE_TAG_LIST:  return runtime_error("Trying to use negation on <list>");
    }

    return (Primitive_op_result){0};
}
Primitive_op_result primitive_bneg(Primitive *self){
    assert(self && "<self> is never null");

    switch (self->m_tag){
        case PRIMITIVE_TAG_BOOL:  self->m_bool_data = !self->m_bool_data;     break;
        case PRIMITIVE_TAG_CHAR:  self->m_char_data = (u8)~self->m_char_data; break;
        case PRIMITIVE_TAG_INT:   self->m_int_data  = (i64)~self->m_int_data; break;
        case PRIMITIVE_TAG_FLOAT: return runtime_error("Trying to use bitwise negation on <float>");
        case PRIMITIVE_TAG_STR:   return runtime_error("Trying to use bitwise negation on <str>");
        case PRIMITIVE_TAG_LIST:  return runtime_error("Trying to use bitwise negation on <list>");
    }

    return (Primitive_op_result){0};
}

Primitive_op_result primitive_deref(Primitive *self, Allocator alloc, const Primitive *other){
    assert(self && "<self> is never null");
    assert(other && "<other> is not nullable");

    i64 idx;
    switch (other->m_tag){
        case PRIMITIVE_TAG_BOOL: idx = other->m_bool_data; break;
        case PRIMITIVE_TAG_CHAR: idx = other->m_char_data; break;
        case PRIMITIVE_TAG_INT:  idx = other->m_int_data;  break;
        case PRIMITIVE_TAG_FLOAT:
        case PRIMITIVE_TAG_STR:
        case PRIMITIVE_TAG_LIST:
            return runtime_error("Trying to index with non-int type");
    }

    switch (self->m_tag){
        case PRIMITIVE_TAG_BOOL:  return runtime_error("Trying to index a <bool>");
        case PRIMITIVE_TAG_CHAR:  return runtime_error("Trying to index a <char>");
        case PRIMITIVE_TAG_INT:   return runtime_error("Trying to index an <int>");
        case PRIMITIVE_TAG_FLOAT: return runtime_error("Trying to index a <float>");
        case PRIMITIVE_TAG_STR:
            {
                if (idx >= (i64)str_base_size(&self->m_str_data))
                    return runtime_error("Idx out of range");
                u8 c = (u8)str_base_data(&self->m_str_data)[idx];
                str_base_deinit(&self->m_str_data, alloc);
                *self = (Primitive){.m_tag = PRIMITIVE_TAG_CHAR, .m_char_data = c};
            }
            break;
        case PRIMITIVE_TAG_LIST:
            {
                if (idx >= (i64)self->m_list_data->m_data.m_size)
                    return runtime_error("Idx out of range");
                Primitive *val = vec_base_at(&self->m_list_data->m_data, (usize)idx);
                Primitive new_val = {.m_tag = val->m_tag};
                switch (val->m_tag){
                    case PRIMITIVE_TAG_BOOL:  new_val.m_bool_data  = val->m_bool_data;  break;
                    case PRIMITIVE_TAG_CHAR:  new_val.m_char_data  = val->m_char_data;  break;
                    case PRIMITIVE_TAG_INT:   new_val.m_int_data   = val->m_int_data;   break;
                    case PRIMITIVE_TAG_FLOAT: new_val.m_float_data = val->m_float_data; break;
                    case PRIMITIVE_TAG_STR:
                        {
                            Str_base_result temp = str_base_init_str_base(alloc, &val->m_str_data);
                            if (!temp.success)
                                return OOM_ERROR;
                            new_val.m_str_data = temp.result;
                        }
                        break;
                    case PRIMITIVE_TAG_LIST:
                        new_val.m_list_data = val->m_list_data;
                        ++new_val.m_list_data->m_ref_count;
                        break;
                }
                primitive_deinit(self, alloc);
                *self = new_val;
            }
            break;
    }

    return (Primitive_op_result){0};
}

Primitive_op_result primitive_cmp_eq(Primitive *self, Allocator alloc, const Primitive *other){
    assert(self && "<self> is never null");
    assert(other && "<other> is not nullable");

    return primitive_cmp(self, alloc, other, CMP_BIN_OP_EQ);
}
Primitive_op_result primitive_cmp_neq(Primitive *self, Allocator alloc, const Primitive *other){
    assert(self && "<self> is never null");
    assert(other && "<other> is not nullable");

    return primitive_cmp(self, alloc, other, CMP_BIN_OP_NEQ);
}
Primitive_op_result primitive_cmp_le(Primitive *self, Allocator alloc, const Primitive *other){
    assert(self && "<self> is never null");
    assert(other && "<other> is not nullable");

    return primitive_cmp(self, alloc, other, CMP_BIN_OP_LE);
}
Primitive_op_result primitive_cmp_leq(Primitive *self, Allocator alloc, const Primitive *other){
    assert(self && "<self> is never null");
    assert(other && "<other> is not nullable");

    return primitive_cmp(self, alloc, other, CMP_BIN_OP_LEQ);
}
Primitive_op_result primitive_cmp_ge(Primitive *self, Allocator alloc, const Primitive *other){
    assert(self && "<self> is never null");
    assert(other && "<other> is not nullable");

    return primitive_cmp(self, alloc, other, CMP_BIN_OP_GE);
}
Primitive_op_result primitive_cmp_geq(Primitive *self, Allocator alloc, const Primitive *other){
    assert(self && "<self> is never null");
    assert(other && "<other> is not nullable");

    return primitive_cmp(self, alloc, other, CMP_BIN_OP_GEQ);
}

Primitive_op_result primitive_add(Primitive *self, Allocator alloc, const Primitive *other){
    assert(self && "<self> is never null");
    assert(other && "<other> is not nullable");

    if (self->m_tag == PRIMITIVE_TAG_LIST || other->m_tag == PRIMITIVE_TAG_LIST)
        return runtime_error("Trying to use addition on <list>");

    switch (self->m_tag){
        case PRIMITIVE_TAG_BOOL:
            switch (other->m_tag){
                case PRIMITIVE_TAG_BOOL:
                    *self = (Primitive){.m_tag = PRIMITIVE_TAG_BOOL, .m_bool_data = (bool)(((i64)self->m_bool_data + (i64)other->m_bool_data) % 2)};
                    break;
                case PRIMITIVE_TAG_CHAR:
                    *self = (Primitive){.m_tag = PRIMITIVE_TAG_CHAR, .m_char_data = (u8)((u8)self->m_bool_data + other->m_char_data)};
                    break;
                case PRIMITIVE_TAG_INT:
                    *self = (Primitive){.m_tag = PRIMITIVE_TAG_INT, .m_int_data = (i64)self->m_bool_data + other->m_int_data};
                    break;
                case PRIMITIVE_TAG_FLOAT:
                    *self = (Primitive){.m_tag = PRIMITIVE_TAG_FLOAT, .m_float_data = (f64)self->m_bool_data + other->m_float_data};
                    break;
                case PRIMITIVE_TAG_STR:
                    return runtime_error("Trying to use addition between <bool> and <str>");
                case PRIMITIVE_TAG_LIST:
                    break;
            }
            break;
        case PRIMITIVE_TAG_CHAR:
            switch (other->m_tag){
                case PRIMITIVE_TAG_BOOL:
                    *self = (Primitive){.m_tag = PRIMITIVE_TAG_CHAR, .m_char_data = (u8)(self->m_char_data + (u8)other->m_bool_data)};
                    break;
                case PRIMITIVE_TAG_CHAR:
                    *self = (Primitive){.m_tag = PRIMITIVE_TAG_CHAR, .m_char_data = (u8)(self->m_char_data + other->m_char_data)};
                    break;
                case PRIMITIVE_TAG_INT:
                    *self = (Primitive){.m_tag = PRIMITIVE_TAG_INT, .m_int_data = (i64)self->m_char_data + other->m_int_data};
                    break;
                case PRIMITIVE_TAG_FLOAT:
                    *self = (Primitive){.m_tag = PRIMITIVE_TAG_FLOAT, .m_float_data = (f64)self->m_char_data + other->m_float_data};
                    break;
                case PRIMITIVE_TAG_STR:
                    if (self->m_char_data == '\0')
                        *self = (Primitive){.m_tag = PRIMITIVE_TAG_STR, .m_str_data = {0}};
                    else{
                        Str_base_result temp = str_base_init_fmt(alloc, "%c%s", (char)self->m_char_data, str_base_data_const(&other->m_str_data));
                        if (!temp.success)
                            return OOM_ERROR;
                        *self = (Primitive){.m_tag = PRIMITIVE_TAG_STR, .m_str_data = temp.result};
                    }
                    break;
                case PRIMITIVE_TAG_LIST:
                    break;
            }
            break;
        case PRIMITIVE_TAG_INT:
            switch (other->m_tag){
                case PRIMITIVE_TAG_BOOL:
                    *self = (Primitive){.m_tag = PRIMITIVE_TAG_INT, .m_int_data = self->m_int_data + (i64)other->m_bool_data};
                    break;
                case PRIMITIVE_TAG_CHAR:
                    *self = (Primitive){.m_tag = PRIMITIVE_TAG_INT, .m_int_data = self->m_int_data + (i64)other->m_char_data};
                    break;
                case PRIMITIVE_TAG_INT:
                    *self = (Primitive){.m_tag = PRIMITIVE_TAG_INT, .m_int_data = self->m_int_data + other->m_int_data};
                    break;
                case PRIMITIVE_TAG_FLOAT:
                    *self = (Primitive){.m_tag = PRIMITIVE_TAG_FLOAT, .m_float_data = (f64)self->m_int_data + other->m_float_data};
                    break;
                case PRIMITIVE_TAG_STR:
                    return runtime_error("Trying to use addition between <int> and <str>");
                case PRIMITIVE_TAG_LIST:
                    break;
            }
            break;
        case PRIMITIVE_TAG_FLOAT:
            switch (other->m_tag){
                case PRIMITIVE_TAG_BOOL:
                    *self = (Primitive){.m_tag = PRIMITIVE_TAG_FLOAT, .m_float_data = self->m_float_data + (i64)other->m_bool_data};
                    break;
                case PRIMITIVE_TAG_CHAR:
                    *self = (Primitive){.m_tag = PRIMITIVE_TAG_FLOAT, .m_float_data = self->m_float_data + (i64)other->m_char_data};
                    break;
                case PRIMITIVE_TAG_INT:
                    *self = (Primitive){.m_tag = PRIMITIVE_TAG_FLOAT, .m_float_data = self->m_float_data + (f64)other->m_int_data};
                    break;
                case PRIMITIVE_TAG_FLOAT:
                    *self = (Primitive){.m_tag = PRIMITIVE_TAG_FLOAT, .m_float_data = self->m_float_data + other->m_float_data};
                    break;
                case PRIMITIVE_TAG_STR:
                    return runtime_error("Trying to use addition between <float> and <str>");
                case PRIMITIVE_TAG_LIST:
                    break;
            }
            break;
        case PRIMITIVE_TAG_STR:
            switch (other->m_tag){
                case PRIMITIVE_TAG_BOOL:
                    return runtime_error("Trying to use addition between <str> and <bool>");
                case PRIMITIVE_TAG_CHAR:
                    if (!str_base_push_back(&self->m_str_data, alloc, (char)other->m_char_data))
                        return OOM_ERROR;
                    break;
                case PRIMITIVE_TAG_INT:
                    return runtime_error("Trying to use addition between <str> and <int>");
                case PRIMITIVE_TAG_FLOAT:
                    return runtime_error("Trying to use addition between <str> and <float>");
                case PRIMITIVE_TAG_STR:
                    if (!str_base_append_str_base(&self->m_str_data, alloc, &other->m_str_data))
                        return OOM_ERROR;
                    break;
                case PRIMITIVE_TAG_LIST:
                    break;
            }
            break;
        case PRIMITIVE_TAG_LIST:
            break;
    }

    return (Primitive_op_result){0};
}
Primitive_op_result primitive_sub(Primitive *self, const Primitive *other){
    assert(self && "<self> is never null");
    assert(other && "<other> is not nullable");

    return primitive_bin_op(self, other, BIN_OP_SUB);
}
Primitive_op_result primitive_mul(Primitive *self, const Primitive *other){
    assert(self && "<self> is never null");
    assert(other && "<other> is not nullable");

    return primitive_bin_op(self, other, BIN_OP_MUL);
}
Primitive_op_result primitive_div(Primitive *self, const Primitive *other){
    assert(self && "<self> is never null");
    assert(other && "<other> is not nullable");

    return primitive_bin_op(self, other, BIN_OP_DIV);
}
Primitive_op_result primitive_mod(Primitive *self, const Primitive *other){
    assert(self && "<self> is never null");
    assert(other && "<other> is not nullable");

    return primitive_bin_op(self, other, BIN_OP_MOD);
}
Primitive_op_result primitive_pow(Primitive *self, const Primitive *other){
    assert(self && "<self> is never null");
    assert(other && "<other> is not nullable");

    return primitive_bin_op(self, other, BIN_OP_POW);
}

Primitive_op_result primitive_shl(Primitive *self, const Primitive *other){
    assert(self && "<self> is never null");
    assert(other && "<other> is not nullable");

    return primitive_bin_op(self, other, BIN_OP_SHL);
}
Primitive_op_result primitive_shr(Primitive *self, const Primitive *other){
    assert(self && "<self> is never null");
    assert(other && "<other> is not nullable");

    return primitive_bin_op(self, other, BIN_OP_SHR);
}
Primitive_op_result primitive_band(Primitive *self, const Primitive *other){
    assert(self && "<self> is never null");
    assert(other && "<other> is not nullable");

    return primitive_bin_op(self, other, BIN_OP_BAND);
}
Primitive_op_result primitive_bor(Primitive *self, const Primitive *other){
    assert(self && "<self> is never null");
    assert(other && "<other> is not nullable");

    return primitive_bin_op(self, other, BIN_OP_BOR);
}
Primitive_op_result primitive_xor(Primitive *self, const Primitive *other){
    assert(self && "<self> is never null");
    assert(other && "<other> is not nullable");

    return primitive_bin_op(self, other, BIN_OP_XOR);
}
