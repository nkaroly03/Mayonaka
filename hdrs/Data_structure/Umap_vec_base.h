#ifndef DATA_STRUCTURE_UMAP_VEC_BASE_H
#define DATA_STRUCTURE_UMAP_VEC_BASE_H

#ifdef __cplusplus
extern "C"{
#endif

#include <stdalign.h>
#include <stdbool.h>

#include "../Allocator/Allocator.h"
#include "../Utils/Cmp.h"
#include "../Utils/Hash.h"
#include "../Utils/Num.h"
#include "Slist_base.h"
#include "Umap_base.h"
#include "Vec_base.h"

typedef struct Umap_vec_base{
    Vec_base m_keys;
    Umap_base m_pairs;
} Umap_vec_base;

Umap_vec_base umap_vec_base_init_(
    usize key_alignment,
    usize key_size,
    usize value_alignment,
    usize value_size,
    usize (*hash)(const void*),
    bool (*cmp_eq)(const void*, const void*)
);
#define umap_vec_base_init(key_type, value_type) \
    umap_vec_base_init_(alignof(key_type), sizeof(key_type), alignof(value_type), sizeof(value_type), hash_##key_type, cmp_eq_##key_type)
#define umap_vec_base_init_aligned(key_type, key_alignment, value_type, value_alignment) \
    ( \
        assert_alignment(key_type, (key_alignment)), \
        assert_alignment(value_type, (value_alignment)), \
        umap_vec_base_init_((key_alignment), sizeof(key_type), (value_alignment), sizeof(value_type), hash_##key_type, cmp_eq_##key_type) \
    )
#define umap_vec_base_init_as_set(key_type) umap_vec_base_init_(alignof(key_type), sizeof(key_type), 1, 0, hash_##key_type, cmp_eq_##key_type)
#define umap_vec_base_init_as_set_aligned(key_type, key_alignment) \
    (assert_alignment(key_type, (key_alignment)), umap_vec_base_init_((key_alignment), sizeof(key_type), 1, 0, hash_##key_type, cmp_eq_##key_type))
void umap_vec_base_deinit(const Umap_vec_base *self, Allocator alloc);

bool umap_vec_base_empty(const Umap_vec_base *self);
usize umap_vec_base_size(const Umap_vec_base *self);
usize umap_vec_base_bucket_count(const Umap_vec_base *self);

Umap_pair umap_vec_base_at_idx(Umap_vec_base *self, usize idx);
Umap_pair_const umap_vec_base_at_idx_const(const Umap_vec_base *self, usize idx);
Umap_pair umap_vec_base_at_key(Umap_vec_base *self, const void *key);
Umap_pair_const umap_vec_base_at_key_const(const Umap_vec_base *self, const void *key);
Umap_pair umap_vec_base_at_node(Umap_vec_base *self, const struct Snode *node);
Umap_pair_const umap_vec_base_at_node_const(const Umap_vec_base *self, const struct Snode *node);

Umap_insert_result umap_vec_base_push_back(Umap_vec_base *self, Allocator alloc, const void *key, const void *value);
Umap_insert_result umap_vec_base_insert(Umap_vec_base *self, Allocator alloc, usize idx, const void *key, const void *value);

void umap_vec_base_pop_back_discard(Umap_vec_base *self, Allocator alloc);
void umap_vec_base_pop_back_to(Umap_vec_base *self, Allocator alloc, void *key_dest, void *value_dest);
void umap_vec_base_erase_idx_discard(Umap_vec_base *self, Allocator alloc, usize idx);
void umap_vec_base_erase_idx_to(Umap_vec_base *self, Allocator alloc, usize idx, void *key_dest, void *value_dest);
bool umap_vec_base_erase_key_discard(Umap_vec_base *self, Allocator alloc, const void *key);
bool umap_vec_base_erase_key_to(Umap_vec_base *self, Allocator alloc, const void *key, void *key_dest, void *value_dest);

void umap_vec_base_clear(Umap_vec_base *self, Allocator alloc);
void umap_vec_base_reverse(Umap_vec_base *self);
void umap_vec_base_sort(Umap_vec_base *self, int (*cmp_fn)(const void*, const void*));

#ifdef __cplusplus
}
#endif

#endif // DATA_STRUCTURE_UMAP_VEC_BASE_H
