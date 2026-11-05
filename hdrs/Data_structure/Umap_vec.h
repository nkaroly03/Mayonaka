#ifndef DATA_STRUCTURE_UMAP_VEC_H
#define DATA_STRUCTURE_UMAP_VEC_H

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
#include "Umap_vec_base.h"

typedef struct Umap_vec{
    Umap_vec_base m_base;
    Allocator m_alloc;
} Umap_vec;

Umap_vec umap_vec_init_(
    usize key_alignment,
    usize key_size,
    usize value_alignment,
    usize value_size,
    usize (*hash)(const void*),
    bool (*cmp_eq)(const void*, const void*),
    Allocator alloc
);
#define umap_vec_init(key_type, value_type, alloc) \
    umap_vec_init_(alignof(key_type), sizeof(key_type), alignof(value_type), sizeof(value_type), hash_##key_type, cmp_eq_##key_type, (alloc))
#define umap_vec_init_aligned(key_type, key_alignment, value_type, value_alignment, alloc) \
    ( \
        assert_alignment(key_type, (key_alignment)), \
        assert_alignment(value_type, (value_alignment)), \
        umap_vec_init_((key_alignment), sizeof(key_type), (value_alignment), sizeof(value_type), hash_##key_type, cmp_eq_##key_type, (alloc)) \
    )
#define umap_vec_init_as_set(key_type, alloc) umap_vec_init_(alignof(key_type), sizeof(key_type), 1, 0, hash_##key_type, cmp_eq_##key_type, (alloc))
#define umap_vec_init_as_set_aligned(key_type, key_alignment, alloc) \
    (assert_alignment(key_type, (key_alignment)), umap_vec_init_((key_alignment), sizeof(key_type), 1, 0, hash_##key_type, cmp_eq_##key_type, (alloc)))
void umap_vec_deinit(const Umap_vec *self);

bool umap_vec_empty(const Umap_vec *self);
usize umap_vec_size(const Umap_vec *self);
usize umap_vec_bucket_count(const Umap_vec *self);

Umap_pair umap_vec_at_idx(Umap_vec *self, usize idx);
Umap_pair_const umap_vec_at_idx_const(const Umap_vec *self, usize idx);
Umap_pair umap_vec_at_key(Umap_vec *self, const void *key);
Umap_pair_const umap_vec_at_key_const(const Umap_vec *self, const void *key);
Umap_pair umap_vec_at_node(Umap_vec *self, const struct Snode *node);
Umap_pair_const umap_vec_at_node_const(const Umap_vec *self, const struct Snode *node);

Umap_insert_result umap_vec_push_back(Umap_vec *self, const void *key, const void *value);
Umap_insert_result umap_vec_insert(Umap_vec *self, usize idx, const void *key, const void *value);

void umap_vec_pop_back_discard(Umap_vec *self);
void umap_vec_pop_back_to(Umap_vec *self, void *key_dest, void *value_dest);
void umap_vec_erase_idx_discard(Umap_vec *self, usize idx);
void umap_vec_erase_idx_to(Umap_vec *self, usize idx, void *key_dest, void *value_dest);
bool umap_vec_erase_key_discard(Umap_vec *self, const void *key);
bool umap_vec_erase_key_to(Umap_vec *self, const void *key, void *key_dest, void *value_dest);

void umap_vec_clear(Umap_vec *self);
void umap_vec_reverse(Umap_vec *self);
void umap_vec_sort(Umap_vec *self, int (*cmp_fn)(const void*, const void*));

#ifdef __cplusplus
}
#endif

#endif // DATA_STRUCTURE_UMAP_VEC_H
