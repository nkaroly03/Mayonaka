#ifndef DATA_STRUCTURE_VEC_H
#define DATA_STRUCTURE_VEC_H

#ifdef __cplusplus
extern "C"{
#endif

#include <stdalign.h>
#include <stdbool.h>

#include "../Allocator/Allocator.h"
#include "../Utils/Num.h"
#include "Vec_base.h"

typedef struct Vec{
    Vec_base m_base;
    Allocator m_alloc;
} Vec;

#define vec_for_each(vec, it_name) vec_base_for_each((vec).m_base, it_name)

Vec vec_init_(usize data_alignment, usize data_size, Allocator alloc);
#define vec_init(type, alloc) vec_init_(alignof(type), sizeof(type), (alloc))
#define vec_init_aligned(type, alignment, alloc) (assert_alignment(type, (alignment)), vec_init_(alignof(type), sizeof(type), (alloc)))
void vec_deinit(const Vec *self);

bool vec_empty(const Vec *self);
usize vec_size(const Vec *self);
usize vec_capacity(const Vec *self);
void* vec_at(Vec *self, usize idx);
const void* vec_at_const(const Vec *self, usize idx);

void* vec_push_back(Vec *self, const void *data);
void* vec_insert(Vec *self, usize idx, const void *data);

void vec_pop_back_discard(Vec *self);
void* vec_pop_back_to(Vec *self, void *dest);
void vec_erase_discard(Vec *self, usize idx);
void* vec_erase_to(Vec *self, usize idx, void *dest);

void vec_clear(Vec *self);
void vec_reverse(Vec *self);
void vec_sort(Vec *self, int (*cmp_fn)(const void*, const void*));

bool vec_reserve(Vec *self, usize reserve_size);
bool vec_shrink_to_fit(Vec *self);

#ifdef __cplusplus
}
#endif

#endif // DATA_STRUCTURE_VEC_H
