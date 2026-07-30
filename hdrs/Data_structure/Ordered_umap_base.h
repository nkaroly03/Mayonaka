#ifndef DATA_STRUCTURE_ORDERED_UMAP_BASE_H
#define DATA_STRUCTURE_ORDERED_UMAP_BASE_H

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

typedef struct Ordered_umap_base{
    Vec_base m_keys;
    Umap_base m_pairs;
} Ordered_umap_base;

Ordered_umap_base ordered_umap_base_init_(
    usize key_alignment,
    usize key_size,
    usize value_alignment,
    usize value_size,
    usize (*hash)(const void*),
    bool (*cmp_eq)(const void*, const void*)
);
#define ordered_umap_base_init(key_type, value_type) \
    ordered_umap_base_init_(alignof(key_type), sizeof(key_type), alignof(value_type), sizeof(value_type), hash_##key_type, cmp_eq_##key_type)
#define ordered_umap_base_init_aligned(key_type, key_alignment, value_type, value_alignment) \
    ( \
        assert_alignment(key_type, (key_alignment)), \
        assert_alignment(value_type, (value_alignment)), \
        ordered_umap_base_init_((key_alignment), sizeof(key_type), (value_alignment), sizeof(value_type), hash_##key_type, cmp_eq_##key_type) \
    )
#define ordered_umap_base_init_as_set(key_type) ordered_umap_base_init_(alignof(key_type), sizeof(key_type), 1, 0, hash_##key_type, cmp_eq_##key_type)
#define ordered_umap_base_init_as_set_aligned(key_type, key_alignment) \
    (assert_alignment(key_type, (key_alignment)), ordered_umap_base_init_((key_alignment), sizeof(key_type), 1, 0, hash_##key_type, cmp_eq_##key_type))
void ordered_umap_base_deinit(const Ordered_umap_base *self, Allocator alloc);

bool ordered_umap_base_empty(const Ordered_umap_base *self);
usize ordered_umap_base_size(const Ordered_umap_base *self);
usize ordered_umap_base_bucket_count(const Ordered_umap_base *self);

Umap_pair ordered_umap_base_at_idx(Ordered_umap_base *self, usize idx);
Umap_pair_const ordered_umap_base_at_idx_const(const Ordered_umap_base *self, usize idx);
Umap_pair ordered_umap_base_at_key(Ordered_umap_base *self, const void *key);
Umap_pair_const ordered_umap_base_at_key_const(const Ordered_umap_base *self, const void *key);
Umap_pair ordered_umap_base_at_node(Ordered_umap_base *self, const struct Snode *node);
Umap_pair_const ordered_umap_base_at_node_const(const Ordered_umap_base *self, const struct Snode *node);

Umap_insert_result ordered_umap_base_push_back(Ordered_umap_base *self, Allocator alloc, const void *key, const void *value);
Umap_insert_result ordered_umap_base_insert(Ordered_umap_base *self, Allocator alloc, usize idx, const void *key, const void *value);

void ordered_umap_base_pop_back_discard(Ordered_umap_base *self, Allocator alloc);
void ordered_umap_base_pop_back_to(Ordered_umap_base *self, Allocator alloc, void *key_dest, void *value_dest);
void ordered_umap_base_erase_idx_discard(Ordered_umap_base *self, Allocator alloc, usize idx);
void ordered_umap_base_erase_idx_to(Ordered_umap_base *self, Allocator alloc, usize idx, void *key_dest, void *value_dest);
bool ordered_umap_base_erase_key_discard(Ordered_umap_base *self, Allocator alloc, const void *key);
bool ordered_umap_base_erase_key_to(Ordered_umap_base *self, Allocator alloc, const void *key, void *key_dest, void *value_dest);

void ordered_umap_base_clear(Ordered_umap_base *self, Allocator alloc);
void ordered_umap_base_reverse(Ordered_umap_base *self);
void ordered_umap_base_sort(Ordered_umap_base *self, int (*cmp_fn)(const void*, const void*));

#ifdef __cplusplus
}
#endif

#endif // DATA_STRUCTURE_ORDERED_UMAP_BASE_H
