#ifndef DATA_STRUCTURE_VEC_BASE_H
#define DATA_STRUCTURE_VEC_BASE_H

#ifdef __cplusplus
extern "C"{
#endif

#include <stdalign.h>
#include <stdbool.h>

#include "../Allocator/Allocator.h"
#include "../Utils/Num.h"

typedef struct Vec_base{
    usize m_data_alignment, m_data_size;
    usize m_size, m_capacity;
    void *m_data;
} Vec_base;

#define vec_base_for_each(vec_base, it_name) \
    for ( \
        void *it_name = (vec_base).m_data; \
        it_name && (u8*)it_name < (u8*)(vec_base).m_data + (vec_base).m_data_size * (vec_base).m_size; \
        it_name = (u8*)it_name + (vec_base).m_data_size \
    )

Vec_base vec_base_init_(usize data_alignment, usize data_size);
#define vec_base_init(type) vec_base_init_(alignof(type), sizeof(type))
#define vec_base_init_aligned(type, alignment) (assert_alignment(type, (alignment)), vec_base_init_(alignof(type), sizeof(type)))
void vec_base_deinit(const Vec_base *self, Allocator alloc);

bool vec_base_empty(const Vec_base *self);
usize vec_base_size(const Vec_base *self);
usize vec_base_capacity(const Vec_base *self);
void* vec_base_at(Vec_base *self, usize idx);
const void* vec_base_at_const(const Vec_base *self, usize idx);

void* vec_base_push_back(Vec_base *self, Allocator alloc, const void *data);
void* vec_base_insert(Vec_base *self, Allocator alloc, usize idx, const void *data);

void vec_base_pop_back_discard(Vec_base *self);
void* vec_base_pop_back_to(Vec_base *self, void *dest);
void vec_base_erase_discard(Vec_base *self, usize idx);
void* vec_base_erase_to(Vec_base *self, usize idx, void *dest);

void vec_base_clear(Vec_base *self);
void vec_base_reverse(Vec_base *self);
void vec_base_sort(Vec_base *self, int (*cmp_fn)(const void*, const void*));

bool vec_base_reserve(Vec_base *self, Allocator alloc, usize reserve_size);
bool vec_base_shrink_to_fit(Vec_base *self, Allocator alloc);

#ifdef __cplusplus
}
#endif

#endif // DATA_STRUCTURE_VEC_BASE_H
