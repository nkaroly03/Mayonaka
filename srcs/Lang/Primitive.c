#include <assert.h>
#include <ctype.h>
#include <errno.h>
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

enum Cmp_tag{
    CMP_TAG_EQ,
    CMP_TAG_NEQ,
    CMP_TAG_LE,
    CMP_TAG_LEQ,
    CMP_TAG_GE,
    CMP_TAG_GEQ
};

static Primitive_op_result primitive_cmp(Primitive *self, Allocator alloc, const Primitive *other, enum Cmp_tag cmp_tag){
    if (self->m_tag == PRIMITIVE_TAG_LIST || other->m_tag == PRIMITIVE_TAG_LIST)
        return runtime_error("Trying to use comparison on list(s)");

    if (self->m_tag == PRIMITIVE_TAG_STR || other->m_tag == PRIMITIVE_TAG_STR){
        if (self->m_tag != PRIMITIVE_TAG_STR || other->m_tag != PRIMITIVE_TAG_STR)
            return runtime_error("Trying to compare a <str> to a non-str");
        bool cmp;
        switch (cmp_tag){
            case CMP_TAG_EQ:  cmp = cmp_eq_Str_base (&self->m_str_data, &other->m_str_data); break;
            case CMP_TAG_NEQ: cmp = cmp_neq_Str_base(&self->m_str_data, &other->m_str_data); break;
            case CMP_TAG_LE:  cmp = cmp_le_Str_base (&self->m_str_data, &other->m_str_data); break;
            case CMP_TAG_LEQ: cmp = cmp_leq_Str_base(&self->m_str_data, &other->m_str_data); break;
            case CMP_TAG_GE:  cmp = cmp_ge_Str_base (&self->m_str_data, &other->m_str_data); break;
            case CMP_TAG_GEQ: cmp = cmp_geq_Str_base(&self->m_str_data, &other->m_str_data); break;
        }
        str_base_deinit(&self->m_str_data, alloc);
        *self = (Primitive){.m_tag = PRIMITIVE_TAG_BOOL, .m_bool_data = cmp};
    }
    else{
        Primitive lhs_temp = *self;
        Primitive rhs_temp = *other;
        bool cmp;

        switch (lhs_temp.m_tag){
            case PRIMITIVE_TAG_BOOL:
            case PRIMITIVE_TAG_CHAR:
            case PRIMITIVE_TAG_INT:
                switch (rhs_temp.m_tag){
                    case PRIMITIVE_TAG_BOOL:
                    case PRIMITIVE_TAG_CHAR:
                    case PRIMITIVE_TAG_INT:
                        primitive_to_int(&lhs_temp, alloc);
                        primitive_to_int(&rhs_temp, alloc);
                        switch (cmp_tag){
                            case CMP_TAG_EQ:  cmp = cmp_eq_i64 (&lhs_temp.m_int_data, &rhs_temp.m_int_data); break;
                            case CMP_TAG_NEQ: cmp = cmp_neq_i64(&lhs_temp.m_int_data, &rhs_temp.m_int_data); break;
                            case CMP_TAG_LE:  cmp = cmp_le_i64 (&lhs_temp.m_int_data, &rhs_temp.m_int_data); break;
                            case CMP_TAG_LEQ: cmp = cmp_leq_i64(&lhs_temp.m_int_data, &rhs_temp.m_int_data); break;
                            case CMP_TAG_GE:  cmp = cmp_ge_i64 (&lhs_temp.m_int_data, &rhs_temp.m_int_data); break;
                            case CMP_TAG_GEQ: cmp = cmp_geq_i64(&lhs_temp.m_int_data, &rhs_temp.m_int_data); break;
                        }
                        break;
                    case PRIMITIVE_TAG_FLOAT:
                        primitive_to_float(&lhs_temp, alloc);
                        goto float_cmp;
                    case PRIMITIVE_TAG_STR:
                        break;
                    case PRIMITIVE_TAG_LIST:
                        break;
                }
            case PRIMITIVE_TAG_FLOAT:
                primitive_to_float(&rhs_temp, alloc);
            float_cmp:
                switch (cmp_tag){
                    case CMP_TAG_EQ:  cmp = cmp_eq_f64 (&lhs_temp.m_float_data, &rhs_temp.m_float_data); break;
                    case CMP_TAG_NEQ: cmp = cmp_neq_f64(&lhs_temp.m_float_data, &rhs_temp.m_float_data); break;
                    case CMP_TAG_LE:  cmp = cmp_le_f64 (&lhs_temp.m_float_data, &rhs_temp.m_float_data); break;
                    case CMP_TAG_LEQ: cmp = cmp_leq_f64(&lhs_temp.m_float_data, &rhs_temp.m_float_data); break;
                    case CMP_TAG_GE:  cmp = cmp_ge_f64 (&lhs_temp.m_float_data, &rhs_temp.m_float_data); break;
                    case CMP_TAG_GEQ: cmp = cmp_geq_f64(&lhs_temp.m_float_data, &rhs_temp.m_float_data); break;
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
            *self = (Primitive){.m_tag = PRIMITIVE_TAG_CHAR, .m_char_data = (char)self->m_int_data};
            break;
        case PRIMITIVE_TAG_FLOAT:
            {
                f64 val = self->m_float_data;
                if (val > (f64)CHAR_MAX || val < (f64)CHAR_MIN)
                    return runtime_error("Value of <float> does not fit into type <char>");
                *self = (Primitive){.m_tag = PRIMITIVE_TAG_CHAR, .m_char_data = (char)val};
            }
            break;
        case PRIMITIVE_TAG_STR:
            if (str_base_size(&self->m_str_data) == 1){
                char c = str_base_data(&self->m_str_data)[0];
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
            {
                f64 val = self->m_float_data;
                if (!(val >= -9223372036854775808.0 && val < 9223372036854775808.0))
                    return runtime_error("Value of <float> does not fit into type <int>");
                *self = (Primitive){.m_tag = PRIMITIVE_TAG_INT, .m_int_data = (i64)val};
            }
            break;
        case PRIMITIVE_TAG_STR:
            {
                char *data = str_base_data(&self->m_str_data);

                char *end;
                i64 val = (errno = 0, (i64)strtoll(data, &end, 0));

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
                if (errno != 0)
                    return runtime_error("<str> converted to <int> is out of range");

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
                f64 val = (errno = 0, (i64)strtod(data, &end));

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
                if (errno != 0)
                    return runtime_error("<str> converted to <float> is out of range");

                str_base_deinit(&self->m_str_data, alloc);
                *self = (Primitive){.m_tag = PRIMITIVE_TAG_FLOAT, .m_int_data = val};
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
            str_result = str_base_init_raw(alloc, (char[]){self->m_char_data, '\0'});
            break;
        case PRIMITIVE_TAG_INT:
            str_result = str_base_init_fmt(alloc, I64_PFMT, self->m_int_data);
            break;
        case PRIMITIVE_TAG_FLOAT:
            str_result = str_base_init_fmt(alloc, "%lf", self->m_float_data);
            break;
        case PRIMITIVE_TAG_STR:
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
        case PRIMITIVE_TAG_BOOL:  self->m_bool_data  = !self->m_bool_data;       break;
        case PRIMITIVE_TAG_CHAR:  self->m_char_data  = (char)-self->m_char_data; break;
        case PRIMITIVE_TAG_INT:   self->m_int_data   = (i64)-self->m_int_data;   break;
        case PRIMITIVE_TAG_FLOAT: self->m_float_data = -self->m_float_data;      break;
        case PRIMITIVE_TAG_STR:   return runtime_error("Trying to use negation on <str>");
        case PRIMITIVE_TAG_LIST:  return runtime_error("Trying to use negation on <list>");
    }

    return (Primitive_op_result){0};
}
Primitive_op_result primitive_bneg(Primitive *self){
    assert(self && "<self> is never null");

    switch (self->m_tag){
        case PRIMITIVE_TAG_BOOL:  self->m_bool_data  = !self->m_bool_data;       break;
        case PRIMITIVE_TAG_CHAR:  self->m_char_data  = (char)~self->m_char_data; break;
        case PRIMITIVE_TAG_INT:   self->m_int_data   = (i64)~self->m_int_data;   break;
        case PRIMITIVE_TAG_FLOAT: return runtime_error("Trying to use bitwise negation on <float>");
        case PRIMITIVE_TAG_STR:   return runtime_error("Trying to use bitwise negation on <str>");
        case PRIMITIVE_TAG_LIST:  return runtime_error("Trying to use bitwise negation on <list>");
    }

    return (Primitive_op_result){0};
}

Primitive_op_result primitive_eq(Primitive *self, Allocator alloc, const Primitive *other){
    assert(self && "<self> is never null");
    assert(other && "<other> is not nullable");

    return primitive_cmp(self, alloc, other, CMP_TAG_EQ);
}
Primitive_op_result primitive_neq(Primitive *self, Allocator alloc, const Primitive *other){
    assert(self && "<self> is never null");
    assert(other && "<other> is not nullable");

    return primitive_cmp(self, alloc, other, CMP_TAG_NEQ);
}
Primitive_op_result primitive_le(Primitive *self, Allocator alloc, const Primitive *other){
    assert(self && "<self> is never null");
    assert(other && "<other> is not nullable");

    return primitive_cmp(self, alloc, other, CMP_TAG_LE);
}
Primitive_op_result primitive_leq(Primitive *self, Allocator alloc, const Primitive *other){
    assert(self && "<self> is never null");
    assert(other && "<other> is not nullable");

    return primitive_cmp(self, alloc, other, CMP_TAG_LEQ);
}
Primitive_op_result primitive_ge(Primitive *self, Allocator alloc, const Primitive *other){
    assert(self && "<self> is never null");
    assert(other && "<other> is not nullable");

    return primitive_cmp(self, alloc, other, CMP_TAG_GE);
}
Primitive_op_result primitive_geq(Primitive *self, Allocator alloc, const Primitive *other){
    assert(self && "<self> is never null");
    assert(other && "<other> is not nullable");

    return primitive_cmp(self, alloc, other, CMP_TAG_GEQ);
}

Primitive_op_result primitive_add(Primitive *self, Allocator alloc, const Primitive *other);
Primitive_op_result primitive_sub(Primitive *self, Allocator alloc, const Primitive *other);
Primitive_op_result primitive_mul(Primitive *self, Allocator alloc, const Primitive *other);
Primitive_op_result primitive_div(Primitive *self, Allocator alloc, const Primitive *other);
Primitive_op_result primitive_mod(Primitive *self, Allocator alloc, const Primitive *other);
Primitive_op_result primitive_pow(Primitive *self, Allocator alloc, const Primitive *other);

Primitive_op_result primitive_shl(Primitive *self, Allocator alloc, const Primitive *other);
Primitive_op_result primitive_shr(Primitive *self, Allocator alloc, const Primitive *other);
Primitive_op_result primitive_band(Primitive *self, Allocator alloc, const Primitive *other);
Primitive_op_result primitive_bor(Primitive *self, Allocator alloc, const Primitive *other);
Primitive_op_result primitive_xor(Primitive *self, Allocator alloc, const Primitive *other);
