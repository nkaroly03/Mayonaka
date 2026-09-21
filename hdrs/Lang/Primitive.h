#ifndef LANG_PRIMITIVE_H
#define LANG_PRIMITIVE_H

#ifdef __cplusplus
extern "C"{
#endif

#include <stdbool.h>
#include <stdio.h>

#include "../Allocator/Allocator.h"
#include "../Data_structure/Ordered_umap.h"
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

typedef struct Primitive_str_data{
    usize m_ref_count;
    Str_base m_data;
} Primitive_str_data;

typedef struct Primitive_list_data{
    usize m_ref_count;
    Vec_base m_data;
} Primitive_list_data;

typedef struct Primitive{
    Ordered_umap *m_alloc_infos_ptr;
    enum Primitive_tag m_tag;
    union{
        bool m_bool_data;
        u8 m_char_data;
        i64 m_int_data;
        f64 m_float_data;
        Primitive_str_data *m_str_data_ptr;
        Primitive_list_data *m_list_data_ptr;
    };
} Primitive;

enum Primitive_op_error{
    PRIMITIVE_OP_ERROR_NONE,
    PRIMITIVE_OP_ERROR_OOM,
    PRIMITIVE_OP_ERROR_RUNTIME
};
typedef struct Primitive_op_result{
    enum Primitive_op_error error;
    const char *error_info;
} Primitive_op_result;

typedef struct Primitive_result{
    Primitive result;
    bool success;
} Primitive_result;

Primitive_result primitive_init_str(Ordered_umap *alloc_infos, Str_base *data);
Primitive_result primitive_init_list(Ordered_umap *alloc_infos, Vec_base *data);
void primitive_deinit(const Primitive *self);

enum Primitive_print_error{
    PRIMITIVE_PRINT_ERROR_NONE,
    PRIMITIVE_PRINT_ERROR_PRINT,
    PRIMITIVE_PRINT_ERROR_FLUSH
};

typedef struct Primitive_print_result{
    i64 result;
    enum Primitive_print_error error;
} Primitive_print_result;

Primitive_print_result primitive_print(const Primitive *self, FILE *file, bool to_flush);

Primitive_op_result primitive_to_bool (Primitive *self);
Primitive_op_result primitive_to_char (Primitive *self);
Primitive_op_result primitive_to_int  (Primitive *self);
Primitive_op_result primitive_to_float(Primitive *self);
Primitive_op_result primitive_to_str  (Primitive *self);

Primitive_op_result primitive_neg (Primitive *self);
Primitive_op_result primitive_bneg(Primitive *self);

Primitive_op_result primitive_deref  (Primitive *self, const Primitive *other);
Primitive_op_result primitive_pow    (Primitive *self, const Primitive *other);
Primitive_op_result primitive_mul    (Primitive *self, const Primitive *other);
Primitive_op_result primitive_div    (Primitive *self, const Primitive *other);
Primitive_op_result primitive_rem    (Primitive *self, const Primitive *other);
Primitive_op_result primitive_add    (Primitive *self, const Primitive *other);
Primitive_op_result primitive_sub    (Primitive *self, const Primitive *other);
Primitive_op_result primitive_shl    (Primitive *self, const Primitive *other);
Primitive_op_result primitive_shr    (Primitive *self, const Primitive *other);
Primitive_op_result primitive_cmp_le (Primitive *self, const Primitive *other);
Primitive_op_result primitive_cmp_leq(Primitive *self, const Primitive *other);
Primitive_op_result primitive_cmp_ge (Primitive *self, const Primitive *other);
Primitive_op_result primitive_cmp_geq(Primitive *self, const Primitive *other);
Primitive_op_result primitive_cmp_eq (Primitive *self, const Primitive *other);
Primitive_op_result primitive_cmp_neq(Primitive *self, const Primitive *other);
Primitive_op_result primitive_band   (Primitive *self, const Primitive *other);
Primitive_op_result primitive_xor    (Primitive *self, const Primitive *other);
Primitive_op_result primitive_bor    (Primitive *self, const Primitive *other);

Primitive_op_result primitive_mov(Primitive *self, const Primitive *other);
Primitive_op_result primitive_mov_deref(Primitive *self, const Primitive *idx, const Primitive *other);

#ifdef __cplusplus
}
#endif

#endif // LANG_PRIMITIVE_H
