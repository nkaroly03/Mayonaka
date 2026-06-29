#ifndef LANG_PRIMITIVE_H
#define LANG_PRIMITIVE_H

#ifdef __cplusplus
extern "C"{
#endif

#include <stdbool.h>

#include "../Allocator/Allocator.h"
#include "../Data_structure/Str_base.h"
#include "../Data_structure/Vec_base.h"
#include "../Utils/Num.h"

enum Primitive_tag{
    PRIMITIVE_TAG_BOOL,
    PRIMITIVE_TAG_CHAR,
    PRIMITIVE_TAG_INT,
    PRIMITIVE_TAG_FLOAT,
    PRIMITIVE_TAG_STR,
    PRIMITIVE_TAG_LIST
};

typedef struct Primitive_list_data{
    usize m_ref_count;
    Vec_base m_data;
} Primitive_list_data;

typedef struct Primitive{
    enum Primitive_tag m_tag;
    union{
        bool m_bool_data;
        char m_char_data;
        i64 m_int_data;
        f64 m_float_data;
        Str_base m_str_data;
        Primitive_list_data *m_list_data;
    };
} Primitive;

enum Primitive_op_error{
    PRIMITIVE_OP_ERROR_NONE,
    PRIMITIVE_OP_ERROR_OOM,
    PRIMITIVE_OP_ERROR_RUNTIME
};

typedef struct Primitive_op_result{
    enum Primitive_op_error error;
    const char *error_msg;
} Primitive_op_result;

void primitive_deinit(const Primitive *self, Allocator alloc);

Primitive_op_result primitive_to_bool (Primitive *self, Allocator alloc);
Primitive_op_result primitive_to_char (Primitive *self, Allocator alloc);
Primitive_op_result primitive_to_int  (Primitive *self, Allocator alloc);
Primitive_op_result primitive_to_float(Primitive *self, Allocator alloc);
Primitive_op_result primitive_to_str  (Primitive *self, Allocator alloc);

Primitive_op_result primitive_neg (Primitive *self);
Primitive_op_result primitive_bneg(Primitive *self);

Primitive_op_result primitive_eq  (Primitive *self, Allocator alloc, const Primitive *other);
Primitive_op_result primitive_neq (Primitive *self, Allocator alloc, const Primitive *other);
Primitive_op_result primitive_le  (Primitive *self, Allocator alloc, const Primitive *other);
Primitive_op_result primitive_leq (Primitive *self, Allocator alloc, const Primitive *other);
Primitive_op_result primitive_ge  (Primitive *self, Allocator alloc, const Primitive *other);
Primitive_op_result primitive_geq (Primitive *self, Allocator alloc, const Primitive *other);

Primitive_op_result primitive_add (Primitive *self, Allocator alloc, const Primitive *other);
Primitive_op_result primitive_sub (Primitive *self, Allocator alloc, const Primitive *other);
Primitive_op_result primitive_mul (Primitive *self, Allocator alloc, const Primitive *other);
Primitive_op_result primitive_div (Primitive *self, Allocator alloc, const Primitive *other);
Primitive_op_result primitive_mod (Primitive *self, Allocator alloc, const Primitive *other);
Primitive_op_result primitive_pow (Primitive *self, Allocator alloc, const Primitive *other);

Primitive_op_result primitive_shl (Primitive *self, Allocator alloc, const Primitive *other);
Primitive_op_result primitive_shr (Primitive *self, Allocator alloc, const Primitive *other);
Primitive_op_result primitive_band(Primitive *self, Allocator alloc, const Primitive *other);
Primitive_op_result primitive_bor (Primitive *self, Allocator alloc, const Primitive *other);
Primitive_op_result primitive_xor (Primitive *self, Allocator alloc, const Primitive *other);

#ifdef __cplusplus
}
#endif

#endif // LANG_PRIMITIVE_H
