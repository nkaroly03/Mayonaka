#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "../../hdrs/Allocator/Allocator.h"
#include "../../hdrs/Data_structure/Str_base.h"
#include "../../hdrs/Data_structure/Str_view.h"
#include "../../hdrs/Data_structure/Vec_base.h"
#include "../../hdrs/Utils/Cmp.h"
#include "../../hdrs/Utils/Num.h"
#include "../../hdrs/Utils/Utils.h"

#include "../../hdrs/Lang/Primitive.h"

// ------------------------------------------------------------------------------------------------

static const Primitive_op_result  NO_ERROR = {.error = PRIMITIVE_OP_ERROR_NONE};
static const Primitive_op_result OOM_ERROR = {.error = PRIMITIVE_OP_ERROR_OOM, .error_info = "Out of memory"};

#define runtime_error(error_info_val) (Primitive_op_result){.error = PRIMITIVE_OP_ERROR_RUNTIME, .error_info = (error_info_val)}

static bool float_to_char_cast_is_safe(f64 f){
    return isfinite((f32)f) && f > -1.0 && f < 256.0;
}
static bool float_to_int_cast_is_safe(f64 f){
    return isfinite((f32)f) && f >= (f64)I64_MIN && f < (f64)((u64)I64_MAX + 1);
}

typedef struct Primitive_str_conversion_result{
    union{
        bool b;
        i64 i;
        f64 f;
    };
    bool success;
} Primitive_str_conversion_result;

static Primitive_str_conversion_result primitive_str_to_bool(const Primitive *self){
    Str_view sv = str_view_trim_left_while(str_view_trim_right_while(str_base_to_str_view(&self->m_str_data_ptr->m_data), isspace), isspace);

    Str_view match;
    bool val;
    if (
        (val = false, match = str_view_init("false"), sv.m_size != match.m_size) &&
        (val = true,  match = str_view_init("true" ), sv.m_size != match.m_size)
    )
        return (Primitive_str_conversion_result){0};

    for (usize i = 0; i < sv.m_size; ++i)
        if (tolower(sv.m_str[i]) != match.m_str[i])
            return (Primitive_str_conversion_result){0};

    return (Primitive_str_conversion_result){.b = val, .success = true};
}

static Primitive_str_conversion_result primitive_str_to_int(const Primitive *self){
    Str_view sv = str_view_trim_right_while(str_view_trim_left_while(str_base_to_str_view(&self->m_str_data_ptr->m_data), isspace), isspace);

    char *end;
    i64 val = (errno = 0, (i64)strtoll(sv.m_str, &end, 10)); // TODO?: change str conversion to detect base 2 or 16

    if (end == sv.m_str || end != &sv.m_str[sv.m_size] || errno != 0)
        return (Primitive_str_conversion_result){0};

    return (Primitive_str_conversion_result){.i = val, .success = true};
}

static Primitive_str_conversion_result primitive_str_to_float(const Primitive *self){
    Str_view sv = str_view_trim_right_while(str_view_trim_left_while(str_base_to_str_view(&self->m_str_data_ptr->m_data), isspace), isspace);

    char *end;
    f64 val = (errno = 0, (f64)strtod(sv.m_str, &end));

    if (end == sv.m_str || end != &sv.m_str[sv.m_size] || errno != 0)
        return (Primitive_str_conversion_result){0};

    return (Primitive_str_conversion_result){.f = val, .success = true};
}

enum Bin_op{
    BIN_OP_POW,
    BIN_OP_MUL,
    BIN_OP_DIV,
    BIN_OP_REM,
    BIN_OP_SUB,
    BIN_OP_SHL,
    BIN_OP_SHR,
    BIN_OP_CMP_LE,
    BIN_OP_CMP_LEQ,
    BIN_OP_CMP_GE,
    BIN_OP_CMP_GEQ,
    BIN_OP_CMP_EQ,
    BIN_OP_CMP_NEQ,
    BIN_OP_BAND,
    BIN_OP_XOR,
    BIN_OP_BOR
};

static Primitive_op_result primitive_cmp(Primitive *self, const Primitive *other, enum Bin_op cmp_op){
    if (self->m_tag == PRIMITIVE_TAG_LIST || other->m_tag == PRIMITIVE_TAG_LIST)
        return runtime_error("Trying to use comparison on list(s)");

    bool cmp;

    if (self->m_tag == PRIMITIVE_TAG_STR || other->m_tag == PRIMITIVE_TAG_STR){
        if (self->m_tag != PRIMITIVE_TAG_STR || other->m_tag != PRIMITIVE_TAG_STR)
            return runtime_error("Trying to compare <str> to non-str");

        switch (cmp_op){
            case BIN_OP_CMP_LE:  cmp = cmp_le_Str_base (&self->m_str_data_ptr->m_data, &other->m_str_data_ptr->m_data); break;
            case BIN_OP_CMP_LEQ: cmp = cmp_leq_Str_base(&self->m_str_data_ptr->m_data, &other->m_str_data_ptr->m_data); break;
            case BIN_OP_CMP_GE:  cmp = cmp_ge_Str_base (&self->m_str_data_ptr->m_data, &other->m_str_data_ptr->m_data); break;
            case BIN_OP_CMP_GEQ: cmp = cmp_geq_Str_base(&self->m_str_data_ptr->m_data, &other->m_str_data_ptr->m_data); break;
            case BIN_OP_CMP_EQ:  cmp = cmp_eq_Str_base (&self->m_str_data_ptr->m_data, &other->m_str_data_ptr->m_data); break;
            case BIN_OP_CMP_NEQ: cmp = cmp_neq_Str_base(&self->m_str_data_ptr->m_data, &other->m_str_data_ptr->m_data); break;
            default:             unreachable();
        }

        primitive_deinit(self);
    }
    else{
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
                        (void)primitive_to_int(&lhs_temp);
                        (void)primitive_to_int(&rhs_temp);
                        switch (cmp_op){
                            case BIN_OP_CMP_LE:  cmp = cmp_le_i64 (&lhs_temp.m_int_data, &rhs_temp.m_int_data); break;
                            case BIN_OP_CMP_LEQ: cmp = cmp_leq_i64(&lhs_temp.m_int_data, &rhs_temp.m_int_data); break;
                            case BIN_OP_CMP_GE:  cmp = cmp_ge_i64 (&lhs_temp.m_int_data, &rhs_temp.m_int_data); break;
                            case BIN_OP_CMP_GEQ: cmp = cmp_geq_i64(&lhs_temp.m_int_data, &rhs_temp.m_int_data); break;
                            case BIN_OP_CMP_EQ:  cmp = cmp_eq_i64 (&lhs_temp.m_int_data, &rhs_temp.m_int_data); break;
                            case BIN_OP_CMP_NEQ: cmp = cmp_neq_i64(&lhs_temp.m_int_data, &rhs_temp.m_int_data); break;
                            default:             unreachable();
                        }
                        break;
                    case PRIMITIVE_TAG_FLOAT:
                        (void)primitive_to_float(&lhs_temp);
                        goto float_cmp;
                    default:
                        unreachable();
                }
                break;
            case PRIMITIVE_TAG_FLOAT:
                (void)primitive_to_float(&rhs_temp);
            float_cmp:
                switch (cmp_op){
                    case BIN_OP_CMP_LE:  cmp = cmp_le_f64 (&lhs_temp.m_float_data, &rhs_temp.m_float_data); break;
                    case BIN_OP_CMP_LEQ: cmp = cmp_leq_f64(&lhs_temp.m_float_data, &rhs_temp.m_float_data); break;
                    case BIN_OP_CMP_GE:  cmp = cmp_ge_f64 (&lhs_temp.m_float_data, &rhs_temp.m_float_data); break;
                    case BIN_OP_CMP_GEQ: cmp = cmp_geq_f64(&lhs_temp.m_float_data, &rhs_temp.m_float_data); break;
                    case BIN_OP_CMP_EQ:  cmp = cmp_eq_f64 (&lhs_temp.m_float_data, &rhs_temp.m_float_data); break;
                    case BIN_OP_CMP_NEQ: cmp = cmp_neq_f64(&lhs_temp.m_float_data, &rhs_temp.m_float_data); break;
                    default:             unreachable();
                }
                break;
            default:
                unreachable();
        }

    }

    *self = (Primitive){.m_alloc_infos_ptr = self->m_alloc_infos_ptr, .m_tag = PRIMITIVE_TAG_BOOL, .m_bool_data = cmp};

    return NO_ERROR;
}

static Primitive_op_result primitive_bin_op(Primitive *self, const Primitive *other, enum Bin_op op){
    if (op >= BIN_OP_CMP_LE && op <= BIN_OP_CMP_NEQ)
        return primitive_cmp(self, other, op);

    if (self->m_tag == PRIMITIVE_TAG_LIST || other->m_tag == PRIMITIVE_TAG_LIST){
        switch (op){
            case BIN_OP_POW:  return runtime_error("Trying to use exponentiation on <list>");
            case BIN_OP_MUL:  return runtime_error("Trying to use multiplication on <list>");
            case BIN_OP_DIV:  return runtime_error("Trying to use division on <list>");
            case BIN_OP_REM:  return runtime_error("Trying to use remainder on <list>");
            case BIN_OP_SUB:  return runtime_error("Trying to use subtration on <list>");
            case BIN_OP_SHL:  return runtime_error("Trying to use left shift on <list>");
            case BIN_OP_SHR:  return runtime_error("Trying to use right shift on <list>");
            case BIN_OP_BAND: return runtime_error("Trying to use bitwise and on <list>");
            case BIN_OP_XOR:  return runtime_error("Trying to use xor on <list>");
            case BIN_OP_BOR:  return runtime_error("Trying to use bitwise or on <list>");
            default:          unreachable();
        }
    }
    if (self->m_tag == PRIMITIVE_TAG_STR || other->m_tag == PRIMITIVE_TAG_STR){
        switch (op){
            case BIN_OP_POW:  return runtime_error("Trying to use exponentiation on <str>");
            case BIN_OP_MUL:  return runtime_error("Trying to use multiplication on <str>");
            case BIN_OP_DIV:  return runtime_error("Trying to use division on <str>");
            case BIN_OP_REM:  return runtime_error("Trying to use remainder on <str>");
            case BIN_OP_SUB:  return runtime_error("Trying to use subtration on <str>");
            case BIN_OP_SHL:  return runtime_error("Trying to use left shift on <str>");
            case BIN_OP_SHR:  return runtime_error("Trying to use right shift on <str>");
            case BIN_OP_BAND: return runtime_error("Trying to use bitwise and on <str>");
            case BIN_OP_XOR:  return runtime_error("Trying to use xor on <str>");
            case BIN_OP_BOR:  return runtime_error("Trying to use bitwise or on <str>");
            default:          unreachable();
        }
    }

    Primitive lhs_temp = *self;
    Primitive rhs_temp = *other;

    switch (lhs_temp.m_tag){
        case PRIMITIVE_TAG_BOOL:
            switch (rhs_temp.m_tag){
                case PRIMITIVE_TAG_BOOL:
                    break;
                case PRIMITIVE_TAG_CHAR:
                    (void)primitive_to_char(&lhs_temp);
                    break;
                case PRIMITIVE_TAG_INT:
                    (void)primitive_to_int(&lhs_temp);
                    break;
                case PRIMITIVE_TAG_FLOAT:
                    (void)primitive_to_float(&lhs_temp);
                    break;
                default:
                    unreachable();
            }
            break;
        case PRIMITIVE_TAG_CHAR:
            switch (rhs_temp.m_tag){
                case PRIMITIVE_TAG_BOOL:
                    (void)primitive_to_char(&rhs_temp);
                    break;
                case PRIMITIVE_TAG_CHAR:
                    break;
                case PRIMITIVE_TAG_INT:
                    (void)primitive_to_int(&lhs_temp);
                    break;
                case PRIMITIVE_TAG_FLOAT:
                    (void)primitive_to_float(&lhs_temp);
                    break;
                default:
                    unreachable();
            }
            break;
        case PRIMITIVE_TAG_INT:
            switch (rhs_temp.m_tag){
                case PRIMITIVE_TAG_BOOL:
                case PRIMITIVE_TAG_CHAR:
                    (void)primitive_to_int(&rhs_temp);
                    break;
                case PRIMITIVE_TAG_INT:
                    break;
                case PRIMITIVE_TAG_FLOAT:
                    (void)primitive_to_float(&lhs_temp);
                    break;
                default:
                    unreachable();
            }
            break;
        case PRIMITIVE_TAG_FLOAT:
            switch (rhs_temp.m_tag){
                case PRIMITIVE_TAG_BOOL:
                case PRIMITIVE_TAG_CHAR:
                case PRIMITIVE_TAG_INT:
                    (void)primitive_to_float(&rhs_temp);
                    break;
                case PRIMITIVE_TAG_FLOAT:
                    break;
                default:
                    unreachable();
            }
            break;
        default:
            unreachable();
    }

    switch (op){
        case BIN_OP_POW:
            switch (lhs_temp.m_tag){
                case PRIMITIVE_TAG_BOOL:
                case PRIMITIVE_TAG_CHAR:
                case PRIMITIVE_TAG_INT:
                    return runtime_error("Trying to use exponentiation on int types");
                case PRIMITIVE_TAG_FLOAT:
                    lhs_temp.m_float_data = (f64)pow(lhs_temp.m_float_data, rhs_temp.m_float_data);
                    break;
                default:
                    unreachable();
            }
            break;
        case BIN_OP_MUL:
            switch (lhs_temp.m_tag){
                case PRIMITIVE_TAG_BOOL:  lhs_temp.m_bool_data  &= rhs_temp.m_bool_data;  break;
                case PRIMITIVE_TAG_CHAR:  lhs_temp.m_char_data  *= rhs_temp.m_char_data;  break;
                case PRIMITIVE_TAG_INT:   lhs_temp.m_int_data   *= rhs_temp.m_int_data;   break;
                case PRIMITIVE_TAG_FLOAT: lhs_temp.m_float_data *= rhs_temp.m_float_data; break;
                default:                  unreachable();
            }
            break;
        case BIN_OP_DIV:
            switch (lhs_temp.m_tag){
                case PRIMITIVE_TAG_BOOL:
                    if (!rhs_temp.m_bool_data)
                        goto int_div_by_0_error;
                    break;
                case PRIMITIVE_TAG_CHAR:
                    if (!rhs_temp.m_char_data)
                        goto int_div_by_0_error;
                    lhs_temp.m_char_data /= rhs_temp.m_char_data;
                    break;
                case PRIMITIVE_TAG_INT:
                    if (!rhs_temp.m_int_data)
                        goto int_div_by_0_error;
                    lhs_temp.m_int_data /= rhs_temp.m_int_data;
                    break;
                case PRIMITIVE_TAG_FLOAT:
                    lhs_temp.m_float_data /= rhs_temp.m_float_data;
                    break;
                default:
                    unreachable();
                int_div_by_0_error:
                    return runtime_error("Integer division by 0");
            }
            break;
        case BIN_OP_REM:
            switch (lhs_temp.m_tag){
                case PRIMITIVE_TAG_BOOL:
                    if (!rhs_temp.m_bool_data)
                        goto int_rem_by_0_error;
                    lhs_temp.m_bool_data = false;
                    break;
                case PRIMITIVE_TAG_CHAR:
                    if (!rhs_temp.m_char_data)
                        goto int_rem_by_0_error;
                    lhs_temp.m_char_data %= rhs_temp.m_char_data;
                    break;
                case PRIMITIVE_TAG_INT:
                    if (!rhs_temp.m_int_data)
                        goto int_rem_by_0_error;
                    lhs_temp.m_int_data %= rhs_temp.m_int_data;
                    break;
                case PRIMITIVE_TAG_FLOAT:
                    lhs_temp.m_float_data = fmod(lhs_temp.m_float_data, rhs_temp.m_float_data);
                    break;
                default:
                    unreachable();
                int_rem_by_0_error:
                    return runtime_error("Integer remainder by 0");
            }
            break;
        case BIN_OP_SUB:
            switch (lhs_temp.m_tag){
                case PRIMITIVE_TAG_BOOL:  lhs_temp.m_bool_data  ^= rhs_temp.m_bool_data;  break;
                case PRIMITIVE_TAG_CHAR:  lhs_temp.m_char_data  -= rhs_temp.m_char_data;  break;
                case PRIMITIVE_TAG_INT:   lhs_temp.m_int_data   -= rhs_temp.m_int_data;   break;
                case PRIMITIVE_TAG_FLOAT: lhs_temp.m_float_data -= rhs_temp.m_float_data; break;
                default:                  unreachable();
            }
            break;
        case BIN_OP_SHL:
            switch (lhs_temp.m_tag){
                case PRIMITIVE_TAG_BOOL:
                    if (rhs_temp.m_bool_data)
                        goto invalid_left_shift_error;
                    break;
                case PRIMITIVE_TAG_CHAR:
                    if (rhs_temp.m_char_data > 7)
                        goto invalid_left_shift_error;
                    lhs_temp.m_char_data <<= rhs_temp.m_char_data;
                    break;
                case PRIMITIVE_TAG_INT:
                    if (rhs_temp.m_int_data > 63 || rhs_temp.m_int_data < 0)
                        goto invalid_left_shift_error;
                    lhs_temp.m_int_data <<= rhs_temp.m_int_data;
                    break;
                case PRIMITIVE_TAG_FLOAT:
                    return runtime_error("Trying to use left shift between non-int types");
                default:
                    unreachable();
                invalid_left_shift_error:
                    return runtime_error("Trying to left shift by more than the type's bit width - 1");
            }
            break;
        case BIN_OP_SHR:
            switch (lhs_temp.m_tag){
                case PRIMITIVE_TAG_BOOL:
                    if (rhs_temp.m_bool_data)
                        goto invalid_right_shift_error;
                    break;
                case PRIMITIVE_TAG_CHAR:
                    if (rhs_temp.m_char_data > 7)
                        goto invalid_right_shift_error;
                    lhs_temp.m_char_data >>= rhs_temp.m_char_data;
                    break;
                case PRIMITIVE_TAG_INT:
                    if (rhs_temp.m_int_data > 63 || rhs_temp.m_int_data < 0)
                        goto invalid_right_shift_error;
                    if (rhs_temp.m_int_data > 0){
                        lhs_temp.m_int_data =
                            (lhs_temp.m_int_data >> rhs_temp.m_int_data) |
                            ((lhs_temp.m_int_data < 0) * (i64)~((U64_MSBIT >> ((u64)rhs_temp.m_int_data - 1)) - 1))
                        ;
                    }
                    break;
                case PRIMITIVE_TAG_FLOAT:
                    return runtime_error("Trying to use right shift between non-int types");
                default:
                    unreachable();
                invalid_right_shift_error:
                    return runtime_error("Trying to right shift by more than the type's bit width - 1");
            }
            break;
        case BIN_OP_BAND:
            switch (lhs_temp.m_tag){
                case PRIMITIVE_TAG_BOOL:  lhs_temp.m_bool_data &= rhs_temp.m_bool_data; break;
                case PRIMITIVE_TAG_CHAR:  lhs_temp.m_char_data &= rhs_temp.m_char_data; break;
                case PRIMITIVE_TAG_INT:   lhs_temp.m_int_data  &= rhs_temp.m_int_data;  break;
                case PRIMITIVE_TAG_FLOAT: return runtime_error("Trying to use bitwise and between non-int types");
                default:                  unreachable();
            }
            break;
        case BIN_OP_XOR:
            switch (lhs_temp.m_tag){
                case PRIMITIVE_TAG_BOOL:  lhs_temp.m_bool_data ^= rhs_temp.m_bool_data; break;
                case PRIMITIVE_TAG_CHAR:  lhs_temp.m_char_data ^= rhs_temp.m_char_data; break;
                case PRIMITIVE_TAG_INT:   lhs_temp.m_int_data  ^= rhs_temp.m_int_data;  break;
                case PRIMITIVE_TAG_FLOAT: return runtime_error("Trying to use xor between non-int types");
                default:                  unreachable();
            }
            break;
        case BIN_OP_BOR:
            switch (lhs_temp.m_tag){
                case PRIMITIVE_TAG_BOOL:  lhs_temp.m_bool_data |= rhs_temp.m_bool_data; break;
                case PRIMITIVE_TAG_CHAR:  lhs_temp.m_char_data |= rhs_temp.m_char_data; break;
                case PRIMITIVE_TAG_INT:   lhs_temp.m_int_data  |= rhs_temp.m_int_data;  break;
                case PRIMITIVE_TAG_FLOAT: return runtime_error("Trying to use bitwise or between non-int types");
                default:                  unreachable();
            }
            break;
        default:
            unreachable();
    }

    *self = lhs_temp;

    return NO_ERROR;
}

// ------------------------------------------------------------------------------------------------

Primitive_result primitive_init_str(Ordered_umap *alloc_infos, Str_base *data){
    assert(alloc_infos && "<alloc_infos> is not nullable");
    assert(data && "<data> is not nullable");

    Primitive temp = {.m_alloc_infos_ptr = alloc_infos, .m_tag = PRIMITIVE_TAG_STR, .m_str_data_ptr = allocator_alloc(alloc_infos->m_alloc, Primitive_str_data, 1)};
    if (!temp.m_str_data_ptr)
        goto oom_error;
    if (ordered_umap_push_back(alloc_infos, &(usize){(usize)temp.m_str_data_ptr}, &temp.m_tag).error != UMAP_INSERT_ERROR_NONE){
        allocator_free(alloc_infos->m_alloc, temp.m_str_data_ptr, 1);
        goto oom_error;
    }
    *temp.m_str_data_ptr = (Primitive_str_data){.m_ref_count = 1, .m_data = *data};

    return (Primitive_result){.result = temp, .success = true};

oom_error:
    return (Primitive_result){0};
}
Primitive_result primitive_init_list(Ordered_umap *alloc_infos, Vec_base *data){
    assert(alloc_infos && "<alloc_infos> is not nullable");
    assert(data && "<data> is not nullable");

    Primitive temp = {.m_alloc_infos_ptr = alloc_infos, .m_tag = PRIMITIVE_TAG_LIST, .m_list_data_ptr = allocator_alloc(alloc_infos->m_alloc, Primitive_list_data, 1)};
    if (!temp.m_list_data_ptr)
        goto oom_error;
    if (ordered_umap_push_back(alloc_infos, &(usize){(usize)temp.m_list_data_ptr}, &temp.m_tag).error != UMAP_INSERT_ERROR_NONE){
        allocator_free(alloc_infos->m_alloc, temp.m_list_data_ptr, 1);
        goto oom_error;
    }
    *temp.m_list_data_ptr = (Primitive_list_data){.m_ref_count = 1, .m_data = *data};

    return (Primitive_result){.result = temp, .success = true};

oom_error:
    return (Primitive_result){0};
}
void primitive_deinit(const Primitive *self){
    assert(self && "<self> is never null");

    Ordered_umap *alloc_infos_ptr = self->m_alloc_infos_ptr;
    Allocator alloc = alloc_infos_ptr->m_alloc;

    usize data_ptr_as_usize;

    switch (self->m_tag){
        case PRIMITIVE_TAG_BOOL:
        case PRIMITIVE_TAG_CHAR:
        case PRIMITIVE_TAG_INT:
        case PRIMITIVE_TAG_FLOAT:
            break;
        case PRIMITIVE_TAG_STR:
            if (--self->m_str_data_ptr->m_ref_count == 0){
                str_base_deinit(&self->m_str_data_ptr->m_data, alloc);
                data_ptr_as_usize = (usize)self->m_str_data_ptr;
                allocator_free(alloc, self->m_str_data_ptr, 1);
                bool erase_result = ordered_umap_erase_key_discard(alloc_infos_ptr, &data_ptr_as_usize);
                (void)erase_result;
                assert(erase_result);
            }
            break;
        case PRIMITIVE_TAG_LIST:
            if (--self->m_list_data_ptr->m_ref_count == 0){
                vec_base_for_each(self->m_list_data_ptr->m_data, it){
                    primitive_deinit(it);
                }
                vec_base_deinit(&self->m_list_data_ptr->m_data, alloc);
                data_ptr_as_usize = (usize)self->m_list_data_ptr;
                allocator_free(alloc, self->m_list_data_ptr, 1);
                bool erase_result = ordered_umap_erase_key_discard(alloc_infos_ptr, &data_ptr_as_usize);
                (void)erase_result;
                assert(erase_result);
            }
            break;
    }
}

Primitive_print_result primitive_print(const Primitive *self, FILE *file, bool to_flush){
    assert(self && "<self> is never null");
    assert(file && "<file> is not nullable");

    i64 chars_written = 0;

    switch (self->m_tag){
        case PRIMITIVE_TAG_BOOL:  chars_written = fprintf(file, "%s", (self->m_bool_data) ? "true" : "false");             break;
        case PRIMITIVE_TAG_CHAR:  chars_written = fprintf(file, "%c", (char)self->m_char_data);                            break;
        case PRIMITIVE_TAG_INT:   chars_written = fprintf(file, I64_PFMT, self->m_int_data);                               break;
        case PRIMITIVE_TAG_FLOAT: chars_written = fprintf(file, "%lf", self->m_float_data);                                break;
        case PRIMITIVE_TAG_STR:   chars_written = fprintf(file, "%s", str_base_data_const(&self->m_str_data_ptr->m_data)); break;
        case PRIMITIVE_TAG_LIST:
            if (fputc('[', file) == EOF)
                goto print_error;
            ++chars_written;
            vec_base_for_each(self->m_list_data_ptr->m_data, it){
                Primitive_print_result print_result = primitive_print(it, file, to_flush);
                if (print_result.error != PRIMITIVE_PRINT_ERROR_NONE)
                    return print_result;
                chars_written += print_result.result;
                if (fputc(',', file) == EOF)
                    goto print_error;
                ++chars_written;
            }
            if (fputc(']', file) == EOF)
                goto print_error;
            ++chars_written;
            break;
    }

    if (chars_written < 0){
    print_error:
        return (Primitive_print_result){.error = PRIMITIVE_PRINT_ERROR_PRINT};
    }

    if (to_flush && fflush(file) == EOF)
        return (Primitive_print_result){.error = PRIMITIVE_PRINT_ERROR_FLUSH};

    return (Primitive_print_result){.result = chars_written, .error = PRIMITIVE_PRINT_ERROR_NONE};
}

Primitive_op_result primitive_to_bool(Primitive *self){
    assert(self && "<self> is never null");

    bool bool_data;

    switch (self->m_tag){
        case PRIMITIVE_TAG_BOOL:  return NO_ERROR;
        case PRIMITIVE_TAG_CHAR:  bool_data = (bool)self->m_char_data;  break;
        case PRIMITIVE_TAG_INT:   bool_data = (bool)self->m_int_data;   break;
        case PRIMITIVE_TAG_FLOAT: bool_data = (bool)self->m_float_data; break;
        case PRIMITIVE_TAG_STR:{
            Primitive_str_conversion_result bool_result = primitive_str_to_bool(self);
            if (!bool_result.success)
                return runtime_error("Trying to convert invalid <str> to <bool>");
            primitive_deinit(self);
            bool_data = bool_result.b;
            break;
        }
        case PRIMITIVE_TAG_LIST:
            return runtime_error("Trying to convert <list> to <bool>");
        default:
            unreachable();
    }

    *self = (Primitive){.m_alloc_infos_ptr = self->m_alloc_infos_ptr, .m_tag = PRIMITIVE_TAG_BOOL, .m_bool_data = bool_data};

    return NO_ERROR;
}
Primitive_op_result primitive_to_char(Primitive *self){
    assert(self && "<self> is never null");

    u8 char_data;

    switch (self->m_tag){
        case PRIMITIVE_TAG_BOOL:
            char_data = self->m_bool_data;
            break;
        case PRIMITIVE_TAG_CHAR:
            return NO_ERROR;
        case PRIMITIVE_TAG_INT:
            char_data = (u8)self->m_int_data;
            break;
        case PRIMITIVE_TAG_FLOAT:
            if (!float_to_char_cast_is_safe(self->m_float_data))
                return runtime_error("Trying to convert invalid <float> to <char>");
            char_data = (u8)self->m_float_data;
            break;
        case PRIMITIVE_TAG_STR:{
            if (str_base_size(&self->m_str_data_ptr->m_data) != 1)
                return runtime_error("Trying to convert <str> with size != 1 to <char>");
            char_data = (u8)str_base_data(&self->m_str_data_ptr->m_data)[0];
            primitive_deinit(self);
            break;
        }
        case PRIMITIVE_TAG_LIST:
            return runtime_error("Trying to convert <list> to <char>");
        default:
            unreachable();
    }

    *self = (Primitive){.m_alloc_infos_ptr = self->m_alloc_infos_ptr, .m_tag = PRIMITIVE_TAG_CHAR, .m_char_data = char_data};

    return NO_ERROR;
}
Primitive_op_result primitive_to_int(Primitive *self){
    assert(self && "<self> is never null");

    i64 int_data;

    switch (self->m_tag){
        case PRIMITIVE_TAG_BOOL:  int_data = self->m_bool_data; break;
        case PRIMITIVE_TAG_CHAR:  int_data = self->m_char_data; break;
        case PRIMITIVE_TAG_INT:   return NO_ERROR;
        case PRIMITIVE_TAG_FLOAT:
            if (!float_to_int_cast_is_safe(self->m_float_data))
                return runtime_error("Trying to convert invalid <float> to <int>");
            int_data = (i64)self->m_float_data;
            break;
        case PRIMITIVE_TAG_STR:{
            Primitive_str_conversion_result int_result = primitive_str_to_int(self);
            if (!int_result.success)
                return runtime_error("Trying to convert invalid <str> to <int>");
            primitive_deinit(self);
            int_data = int_result.i;
            break;
        }
        case PRIMITIVE_TAG_LIST:
            return runtime_error("Trying to convert <list> to <int>");
        default:
            unreachable();
    }

    *self = (Primitive){.m_alloc_infos_ptr = self->m_alloc_infos_ptr, .m_tag = PRIMITIVE_TAG_INT, .m_int_data = int_data};

    return NO_ERROR;
}
Primitive_op_result primitive_to_float(Primitive *self){
    assert(self && "<self> is never null");

    f64 float_data;

    switch (self->m_tag){
        case PRIMITIVE_TAG_BOOL:  float_data = (f64)self->m_bool_data; break;
        case PRIMITIVE_TAG_CHAR:  float_data = (f64)self->m_char_data; break;
        case PRIMITIVE_TAG_INT:   float_data = (f64)self->m_int_data;  break;
        case PRIMITIVE_TAG_FLOAT: return NO_ERROR;
        case PRIMITIVE_TAG_STR:{
            Primitive_str_conversion_result float_result = primitive_str_to_float(self);
            if (!float_result.success)
                return runtime_error("Trying to convert invalid <str> to <float>");
            primitive_deinit(self);
            float_data = float_result.f;
            break;
        }
        case PRIMITIVE_TAG_LIST:
            return runtime_error("Trying to convert <list> to <float>");
        default:
            unreachable();
    }

    *self = (Primitive){.m_alloc_infos_ptr = self->m_alloc_infos_ptr, .m_tag = PRIMITIVE_TAG_FLOAT, .m_float_data = float_data};

    return NO_ERROR;
}
Primitive_op_result primitive_to_str(Primitive *self){
    assert(self && "<self> is never null");

    enum Primitive_tag tag = self->m_tag;

    if (tag == PRIMITIVE_TAG_LIST)
        return runtime_error("Trying to convert <list> to <str>");

    Ordered_umap *alloc_infos_ptr = self->m_alloc_infos_ptr;
    Allocator alloc = alloc_infos_ptr->m_alloc;

    Primitive temp;

    bool cpy_str;
    if (tag != PRIMITIVE_TAG_STR || (cpy_str = (self->m_str_data_ptr->m_ref_count > 1))){
        Primitive_result primitive_init_result = primitive_init_str(self->m_alloc_infos_ptr, &(Str_base){0});
        if (!primitive_init_result.success)
            return OOM_ERROR;
        temp = primitive_init_result.result;
    }

    switch (tag){
        case PRIMITIVE_TAG_BOOL:
        case PRIMITIVE_TAG_CHAR:
        case PRIMITIVE_TAG_INT:
        case PRIMITIVE_TAG_FLOAT:
            switch (tag){
                case PRIMITIVE_TAG_BOOL:
                    if (!str_base_assign_raw(&temp.m_str_data_ptr->m_data, alloc, (self->m_bool_data) ? "true" : "false"))
                        goto oom_error;
                    break;
                case PRIMITIVE_TAG_CHAR:
                    if (!str_base_push_back(&temp.m_str_data_ptr->m_data, alloc, (char)self->m_char_data))
                        goto oom_error;
                    break;
                case PRIMITIVE_TAG_INT:
                    if (!str_base_assign_fmt(&temp.m_str_data_ptr->m_data, alloc, I64_PFMT, self->m_int_data))
                        goto oom_error;
                    break;
                case PRIMITIVE_TAG_FLOAT:
                    if (!str_base_assign_fmt(&temp.m_str_data_ptr->m_data, alloc, "%lf", self->m_float_data))
                        goto oom_error;
                    break;
                default:
                    unreachable();
            }
            *self = temp;
            break;
        case PRIMITIVE_TAG_STR:
            if (cpy_str){
                if (!str_base_assign_str_base(&temp.m_str_data_ptr->m_data, alloc, &self->m_str_data_ptr->m_data))
                    goto oom_error;
                primitive_deinit(self);
                *self = temp;
            }
            break;
        default:
            unreachable();
    }

    return NO_ERROR;

oom_error:
    primitive_deinit(&temp);
    return OOM_ERROR;
}

Primitive_op_result primitive_neg(Primitive *self){
    assert(self && "<self> is never null");

    switch (self->m_tag){
        case PRIMITIVE_TAG_BOOL:  self->m_bool_data  = !self->m_bool_data;     break;
        case PRIMITIVE_TAG_CHAR:  self->m_char_data  = (u8)-self->m_char_data; break;
        case PRIMITIVE_TAG_INT:   self->m_int_data   = -self->m_int_data;      break;
        case PRIMITIVE_TAG_FLOAT: self->m_float_data = -self->m_float_data;    break;
        case PRIMITIVE_TAG_STR:   return runtime_error("Trying to use negation on <str>");
        case PRIMITIVE_TAG_LIST:  return runtime_error("Trying to use negation on <list>");
    }

    return NO_ERROR;
}
Primitive_op_result primitive_bneg(Primitive *self){
    assert(self && "<self> is never null");

    switch (self->m_tag){
        case PRIMITIVE_TAG_BOOL:  self->m_bool_data = !self->m_bool_data;     break;
        case PRIMITIVE_TAG_CHAR:  self->m_char_data = (u8)~self->m_char_data; break;
        case PRIMITIVE_TAG_INT:   self->m_int_data  = ~self->m_int_data;      break;
        case PRIMITIVE_TAG_FLOAT: return runtime_error("Trying to use bitwise negation on <float>");
        case PRIMITIVE_TAG_STR:   return runtime_error("Trying to use bitwise negation on <str>");
        case PRIMITIVE_TAG_LIST:  return runtime_error("Trying to use bitwise negation on <list>");
    }

    return NO_ERROR;
}

Primitive_op_result primitive_deref(Primitive *self, const Primitive *other){
    assert(self && "<self> is never null");
    assert(other && "<other> is not nullable");

    u64 i;
    switch (other->m_tag){
        case PRIMITIVE_TAG_BOOL: i = other->m_bool_data;     break;
        case PRIMITIVE_TAG_CHAR: i = other->m_char_data;     break;
        case PRIMITIVE_TAG_INT:  i = (u64)other->m_int_data; break;
        default:                 return runtime_error("Trying to index with non-int type");
    }

    switch (self->m_tag){
        case PRIMITIVE_TAG_BOOL:  return runtime_error("Trying to index a <bool>");
        case PRIMITIVE_TAG_CHAR:  return runtime_error("Trying to index a <char>");
        case PRIMITIVE_TAG_INT:   return runtime_error("Trying to index an <int>");
        case PRIMITIVE_TAG_FLOAT: return runtime_error("Trying to index a <float>");
        case PRIMITIVE_TAG_STR:{
            if (i >= str_base_size(&self->m_str_data_ptr->m_data))
                return runtime_error("Idx out of range");
            u8 c = (u8)str_base_data(&self->m_str_data_ptr->m_data)[i];
            primitive_deinit(self);
            *self = (Primitive){.m_alloc_infos_ptr = self->m_alloc_infos_ptr, .m_tag = PRIMITIVE_TAG_CHAR, .m_char_data = c};
            break;
        }
        case PRIMITIVE_TAG_LIST:{
            if (i >= self->m_list_data_ptr->m_data.m_size)
                return runtime_error("Idx out of range");
            Primitive new_val = *(Primitive*)vec_base_at(&self->m_list_data_ptr->m_data, (usize)i);
            switch (new_val.m_tag){
                case PRIMITIVE_TAG_BOOL:
                case PRIMITIVE_TAG_CHAR:
                case PRIMITIVE_TAG_INT:
                case PRIMITIVE_TAG_FLOAT:
                    break;
                case PRIMITIVE_TAG_STR:
                    ++new_val.m_str_data_ptr->m_ref_count;
                    break;
                case PRIMITIVE_TAG_LIST:
                    ++new_val.m_list_data_ptr->m_ref_count;
                    break;
            }
            primitive_deinit(self);
            *self = new_val;
            break;
        }
    }

    return NO_ERROR;
}

Primitive_op_result primitive_add(Primitive *self, const Primitive *other){
    assert(self && "<self> is never null");
    assert(other && "<other> is not nullable");

    if (self->m_tag == PRIMITIVE_TAG_LIST || other->m_tag == PRIMITIVE_TAG_LIST)
        return runtime_error("Trying to use addition on <list>");

    Ordered_umap *alloc_infos_ptr = self->m_alloc_infos_ptr;
    Allocator alloc = alloc_infos_ptr->m_alloc;

    Primitive_result primitive_init_result;
    Primitive temp = {.m_alloc_infos_ptr = self->m_alloc_infos_ptr, .m_tag = max(self->m_tag, other->m_tag)};

    switch (self->m_tag){
        case PRIMITIVE_TAG_BOOL:
            switch (other->m_tag){
                case PRIMITIVE_TAG_BOOL:  temp.m_bool_data  = (self->m_bool_data != other->m_bool_data);    break;
                case PRIMITIVE_TAG_CHAR:  temp.m_char_data  = (u8)(self->m_bool_data + other->m_char_data); break;
                case PRIMITIVE_TAG_INT:   temp.m_int_data   = self->m_bool_data + other->m_int_data;        break;
                case PRIMITIVE_TAG_FLOAT: temp.m_float_data = (f64)self->m_bool_data + other->m_float_data; break;
                case PRIMITIVE_TAG_STR:   return runtime_error("Trying to use addition between <bool> and <str>");
                default:                  unreachable();
            }
            break;
        case PRIMITIVE_TAG_CHAR:
            switch (other->m_tag){
                case PRIMITIVE_TAG_BOOL:  temp.m_char_data  = (u8)(self->m_char_data + other->m_bool_data); break;
                case PRIMITIVE_TAG_CHAR:  temp.m_char_data  = (u8)(self->m_char_data + other->m_char_data); break;
                case PRIMITIVE_TAG_INT:   temp.m_int_data   = self->m_char_data + other->m_int_data;        break;
                case PRIMITIVE_TAG_FLOAT: temp.m_float_data = (f64)self->m_char_data + other->m_float_data; break;
                case PRIMITIVE_TAG_STR:
                    primitive_init_result = primitive_init_str(self->m_alloc_infos_ptr, &(Str_base){0});
                    if (!primitive_init_result.success)
                        return OOM_ERROR;
                    temp = primitive_init_result.result;
                    if (
                        self->m_char_data != 0 && !(
                            str_base_push_back(&temp.m_str_data_ptr->m_data, alloc, (char)self->m_char_data) &&
                            str_base_append_str_base(&temp.m_str_data_ptr->m_data, alloc, &other->m_str_data_ptr->m_data)
                        )
                    )
                        goto oom_error;
                    break;
                default:
                    unreachable();
            }
            break;
        case PRIMITIVE_TAG_INT:
            switch (other->m_tag){
                case PRIMITIVE_TAG_BOOL:  temp.m_int_data   = self->m_int_data + other->m_bool_data;       break;
                case PRIMITIVE_TAG_CHAR:  temp.m_int_data   = self->m_int_data + other->m_char_data;       break;
                case PRIMITIVE_TAG_INT:   temp.m_int_data   = self->m_int_data + other->m_int_data;        break;
                case PRIMITIVE_TAG_FLOAT: temp.m_float_data = (f64)self->m_int_data + other->m_float_data; break;
                case PRIMITIVE_TAG_STR:   return runtime_error("Trying to use addition between <int> and <str>");
                default:                  unreachable();
            }
            break;
        case PRIMITIVE_TAG_FLOAT:
            switch (other->m_tag){
                case PRIMITIVE_TAG_BOOL:  temp.m_float_data = self->m_float_data + (f64)other->m_bool_data; break;
                case PRIMITIVE_TAG_CHAR:  temp.m_float_data = self->m_float_data + (f64)other->m_char_data; break;
                case PRIMITIVE_TAG_INT:   temp.m_float_data = self->m_float_data + (f64)other->m_int_data;  break;
                case PRIMITIVE_TAG_FLOAT: temp.m_float_data = self->m_float_data + other->m_float_data;     break;
                case PRIMITIVE_TAG_STR:   return runtime_error("Trying to use addition between <float> and <str>");
                default:                  unreachable();
            }
            break;
        case PRIMITIVE_TAG_STR:
            switch (other->m_tag){
                case PRIMITIVE_TAG_BOOL:  return runtime_error("Trying to use addition between <str> and <bool>");
                case PRIMITIVE_TAG_INT:   return runtime_error("Trying to use addition between <str> and <int>");
                case PRIMITIVE_TAG_FLOAT: return runtime_error("Trying to use addition between <str> and <float>");
                case PRIMITIVE_TAG_CHAR:
                case PRIMITIVE_TAG_STR:{
                    Str_view sv = (other->m_tag == PRIMITIVE_TAG_CHAR)
                        ? (Str_view){.m_size = (other->m_char_data != 0), .m_str = (char*)&other->m_char_data}
                        : str_base_to_str_view(&other->m_str_data_ptr->m_data)
                    ;
                    if (self->m_str_data_ptr->m_ref_count > 1){
                        primitive_init_result = primitive_init_str(self->m_alloc_infos_ptr, &(Str_base){0});
                        if (!primitive_init_result.success)
                            return OOM_ERROR;
                        temp = primitive_init_result.result;
                        if (!(
                            str_base_assign_str_base(&temp.m_str_data_ptr->m_data, alloc, &self->m_str_data_ptr->m_data) &&
                            str_base_append_str_view(&temp.m_str_data_ptr->m_data, alloc, sv)
                        ))
                            goto oom_error;
                        primitive_deinit(self);
                    }
                    else if (
                        temp = *self, !(
                            str_base_assign_str_base(&temp.m_str_data_ptr->m_data, alloc, &self->m_str_data_ptr->m_data) &&
                            str_base_append_str_view(&temp.m_str_data_ptr->m_data, alloc, sv)
                        )
                    )
                        return OOM_ERROR;
                    break;
                }
                default:
                    unreachable();
            }
            break;
        default:
            unreachable();
    }

    *self = temp;

    return NO_ERROR;

oom_error:
    primitive_deinit(&temp);
    return OOM_ERROR;
}

#define primitive_bin_op_generate(bin_op_type, bin_op) \
    Primitive_op_result primitive_##bin_op_type(Primitive *self, const Primitive *other){ \
        assert(self && "<self> is never null"); \
        assert(other && "<other> is not nullable"); \
        return primitive_bin_op(self, other, (bin_op)); \
    }

primitive_bin_op_generate(pow,     BIN_OP_POW)
primitive_bin_op_generate(mul,     BIN_OP_MUL)
primitive_bin_op_generate(div,     BIN_OP_DIV)
primitive_bin_op_generate(rem,     BIN_OP_REM)
primitive_bin_op_generate(sub,     BIN_OP_SUB)
primitive_bin_op_generate(shl,     BIN_OP_SHL)
primitive_bin_op_generate(shr,     BIN_OP_SHR)
primitive_bin_op_generate(cmp_le,  BIN_OP_CMP_LE)
primitive_bin_op_generate(cmp_leq, BIN_OP_CMP_LEQ)
primitive_bin_op_generate(cmp_ge,  BIN_OP_CMP_GE)
primitive_bin_op_generate(cmp_geq, BIN_OP_CMP_GEQ)
primitive_bin_op_generate(cmp_eq,  BIN_OP_CMP_EQ)
primitive_bin_op_generate(cmp_neq, BIN_OP_CMP_NEQ)
primitive_bin_op_generate(band,    BIN_OP_BAND)
primitive_bin_op_generate(xor,     BIN_OP_XOR)
primitive_bin_op_generate(bor,     BIN_OP_BOR)

Primitive_op_result primitive_mov(Primitive *self, const Primitive *other){
    assert(self && "<self> is never null");
    assert(other && "<other> is not nullable");

    switch (self->m_tag){
        case PRIMITIVE_TAG_BOOL:
            switch (other->m_tag){
                case PRIMITIVE_TAG_BOOL:  self->m_bool_data = other->m_bool_data;        break;
                case PRIMITIVE_TAG_CHAR:  self->m_bool_data = (bool)other->m_char_data;  break;
                case PRIMITIVE_TAG_INT:   self->m_bool_data = (bool)other->m_int_data;   break;
                case PRIMITIVE_TAG_FLOAT: self->m_bool_data = (bool)other->m_float_data; break;
                case PRIMITIVE_TAG_STR:{
                    Primitive_str_conversion_result bool_result = primitive_str_to_bool(other);
                    if (!bool_result.success)
                        return runtime_error("Trying to move invalid <str> to <bool>");
                    self->m_bool_data = bool_result.b;
                    break;
                }
                case PRIMITIVE_TAG_LIST:
                    return runtime_error("Trying to move <list> into <bool>");
            }
            break;
        case PRIMITIVE_TAG_CHAR:
            switch (other->m_tag){
                case PRIMITIVE_TAG_BOOL:  self->m_char_data = other->m_bool_data;    break;
                case PRIMITIVE_TAG_CHAR:  self->m_char_data = other->m_char_data;    break;
                case PRIMITIVE_TAG_INT:   self->m_char_data = (u8)other->m_int_data; break;
                case PRIMITIVE_TAG_FLOAT:
                    if (!float_to_char_cast_is_safe(other->m_float_data))
                        return runtime_error("Trying to move invalid <float> into <char>");
                    self->m_char_data = (u8)other->m_float_data;
                    break;
                case PRIMITIVE_TAG_STR:
                    if (str_base_size(&other->m_str_data_ptr->m_data) != 1)
                        return runtime_error("Trying to move <str> with size != 1 into <char>");
                    self->m_char_data = (u8)str_base_data_const(&other->m_str_data_ptr->m_data)[0];
                    break;
                case PRIMITIVE_TAG_LIST:
                    return runtime_error("Trying to move <list> into <char>");
            }
            break;
        case PRIMITIVE_TAG_INT:
            switch (other->m_tag){
                case PRIMITIVE_TAG_BOOL:  self->m_int_data = other->m_bool_data; break;
                case PRIMITIVE_TAG_CHAR:  self->m_int_data = other->m_char_data; break;
                case PRIMITIVE_TAG_INT:   self->m_int_data = other->m_int_data;  break;
                case PRIMITIVE_TAG_FLOAT:
                    if (!float_to_int_cast_is_safe(other->m_float_data))
                        return runtime_error("Trying to move invalid <float> into <int>");
                    self->m_int_data = (i64)other->m_float_data;
                    break;
                case PRIMITIVE_TAG_STR:{
                    Primitive_str_conversion_result int_result = primitive_str_to_int(other);
                    if (!int_result.success)
                        return runtime_error("Trying to move invalid <str> into <int>");
                    self->m_int_data = int_result.i;
                    break;
                }
                case PRIMITIVE_TAG_LIST:
                    return runtime_error("Trying to move <list> into <int>");
            }
            break;
        case PRIMITIVE_TAG_FLOAT:
            switch (other->m_tag){
                case PRIMITIVE_TAG_BOOL:  self->m_float_data = (f64)other->m_bool_data; break;
                case PRIMITIVE_TAG_CHAR:  self->m_float_data = (f64)other->m_char_data; break;
                case PRIMITIVE_TAG_INT:   self->m_float_data = (f64)other->m_int_data;  break;
                case PRIMITIVE_TAG_FLOAT: self->m_float_data = other->m_float_data;     break;
                case PRIMITIVE_TAG_STR:{
                    Primitive_str_conversion_result float_result = primitive_str_to_float(other);
                    if (!float_result.success)
                        return runtime_error("Trying to move invalid <str> into <float>");
                    self->m_float_data = float_result.f;
                    break;
                }
                case PRIMITIVE_TAG_LIST:
                    return runtime_error("Trying to move <list> into <float>");
            }
            break;
        case PRIMITIVE_TAG_STR:{
            Allocator alloc = self->m_alloc_infos_ptr->m_alloc;
            Str_base *data_ptr = &self->m_str_data_ptr->m_data;
            switch (other->m_tag){
                case PRIMITIVE_TAG_BOOL:
                    if (!str_base_assign_raw(data_ptr, alloc, (other->m_bool_data) ? "true" : "false"))
                        return OOM_ERROR;
                    break;
                case PRIMITIVE_TAG_CHAR:
                    if (!str_base_assign_raw(data_ptr, alloc, (char[]){(char)other->m_char_data, '\0'}))
                        return OOM_ERROR;
                    break;
                case PRIMITIVE_TAG_INT:
                    if (!str_base_assign_fmt(data_ptr, alloc, I64_PFMT, other->m_int_data))
                        return OOM_ERROR;
                    break;
                case PRIMITIVE_TAG_FLOAT:
                    if (!str_base_assign_fmt(data_ptr, alloc, "%lf", other->m_float_data))
                        return OOM_ERROR;
                    break;
                case PRIMITIVE_TAG_STR:
                    if (!str_base_assign_str_base(data_ptr, alloc, &other->m_str_data_ptr->m_data))
                        return OOM_ERROR;
                    break;
                case PRIMITIVE_TAG_LIST:
                    return runtime_error("Trying to move <list> into <str>");
            }
            break;
        }
        case PRIMITIVE_TAG_LIST:
            switch (other->m_tag){
                case PRIMITIVE_TAG_BOOL:  return runtime_error("Trying to move <bool> into <list>");
                case PRIMITIVE_TAG_CHAR:  return runtime_error("Trying to move <char> into <list>");
                case PRIMITIVE_TAG_INT:   return runtime_error("Trying to move <int> into <list>");
                case PRIMITIVE_TAG_FLOAT: return runtime_error("Trying to move <float> into <list>");
                case PRIMITIVE_TAG_STR:   return runtime_error("Trying to move <str> into <list>");
                case PRIMITIVE_TAG_LIST:
                    primitive_deinit(self);
                    self->m_list_data_ptr = other->m_list_data_ptr;
                    ++self->m_list_data_ptr->m_ref_count;
                    break;
            }
            break;
    }

    return NO_ERROR;
}
Primitive_op_result primitive_mov_deref(Primitive *self, const Primitive *idx, const Primitive *other){
    assert(self && "<self> is never null");
    assert(idx && "<idx> is not nullable");
    assert(other && "<other> is not nullable");

    u64 i;

    switch (idx->m_tag){
        case PRIMITIVE_TAG_BOOL: i = idx->m_bool_data;     break;
        case PRIMITIVE_TAG_CHAR: i = idx->m_char_data;     break;
        case PRIMITIVE_TAG_INT:  i = (u64)idx->m_int_data; break;
        default:                 return runtime_error("Trying to index with non-int type");
    }

    Allocator alloc = self->m_alloc_infos_ptr->m_alloc;

    switch (self->m_tag){
        case PRIMITIVE_TAG_BOOL:  return runtime_error("Trying to index a <bool>");
        case PRIMITIVE_TAG_CHAR:  return runtime_error("Trying to index a <char>");
        case PRIMITIVE_TAG_INT:   return runtime_error("Trying to index an <int>");
        case PRIMITIVE_TAG_FLOAT: return runtime_error("Trying to index a <float>");
        case PRIMITIVE_TAG_STR:{
            Str_base *data_ptr = &self->m_str_data_ptr->m_data;
            if (i >= str_base_size(data_ptr))
                return runtime_error("Idx out of range");
            switch (other->m_tag){
                case PRIMITIVE_TAG_BOOL:
                    if (!other->m_bool_data)
                        goto truncate_str;
                    str_base_data(data_ptr)[i] = other->m_bool_data;
                    break;
                case PRIMITIVE_TAG_CHAR:
                    if (!(char)other->m_char_data)
                        goto truncate_str;
                    str_base_data(data_ptr)[i] = (char)other->m_char_data;
                    break;
                case PRIMITIVE_TAG_INT:
                    if (!(char)other->m_int_data)
                        goto truncate_str;
                    str_base_data(data_ptr)[i] = (char)other->m_int_data;
                    break;
                case PRIMITIVE_TAG_FLOAT:
                    if (!float_to_char_cast_is_safe(other->m_float_data))
                        return runtime_error("Trying to set <str>'s <char> to invalid <float>");
                    if (!(char)other->m_float_data)
                        goto truncate_str;
                    str_base_data(data_ptr)[i] = (char)other->m_float_data;
                    break;
                case PRIMITIVE_TAG_STR:
                    if (str_base_size(&other->m_str_data_ptr->m_data) != 1)
                        return runtime_error("Trying to set <str>'s <char> to <str> with size != 1");
                    str_base_data(data_ptr)[i] = str_base_data_const(&other->m_str_data_ptr->m_data)[0];
                    break;
                case PRIMITIVE_TAG_LIST:
                    return runtime_error("Trying to to set <str>'s <char> to <list>");
                truncate_str:
                    if (!str_base_assign_str_base_partial(data_ptr, alloc, data_ptr, (usize)i))
                        return OOM_ERROR;
            }
            break;
        }
        case PRIMITIVE_TAG_LIST:{
            Vec_base *list_ptr = &self->m_list_data_ptr->m_data;
            return (i < list_ptr->m_size)
                ? primitive_mov(vec_base_at(list_ptr, (usize)i), other)
                : runtime_error("Idx out of range")
            ;
        }
    }

    return NO_ERROR;
}

