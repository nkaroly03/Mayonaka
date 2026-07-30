#ifndef DATA_STRUCTURE_ORDERED_UMAP_H
#define DATA_STRUCTURE_ORDERED_UMAP_H

#ifdef __cplusplus
extern "C"{
#endif

#include <stdalign.h>
#include <stdbool.h>

#include "../Allocator/Allocator.h"
#include "../Utils/Cmp.h"
#include "../Utils/Hash.h"
#include "../Utils/Num.h"
#include "Ordered_umap_base.h"
#include "Slist_base.h"

typedef struct Ordered_umap{
    Ordered_umap_base m_base;
    Allocator m_alloc;
} Ordered_umap;

Ordered_umap ordered_umap_init_(
    usize key_alignment,
    usize key_size,
    usize value_alignment,
    usize value_size,
    usize (*hash)(const void*),
    bool (*cmp_eq)(const void*, const void*),
    Allocator alloc
);
#define ordered_umap_init(key_type, value_type, alloc) \
    ordered_umap_init_(alignof(key_type), sizeof(key_type), alignof(value_type), sizeof(value_type), hash_##key_type, cmp_eq_##key_type, (alloc))
#define ordered_umap_init_aligned(key_type, key_alignment, value_type, value_alignment, alloc) \
    ( \
        assert_alignment(key_type, (key_alignment)), \
        assert_alignment(value_type, (value_alignment)), \
        ordered_umap_init_((key_alignment), sizeof(key_type), (value_alignment), sizeof(value_type), hash_##key_type, cmp_eq_##key_type, (alloc)) \
    )
#define ordered_umap_init_as_set(key_type, alloc) ordered_umap_init_(alignof(key_type), sizeof(key_type), 1, 0, hash_##key_type, cmp_eq_##key_type, (alloc))
#define ordered_umap_init_as_set_aligned(key_type, key_alignment, alloc) \
    (assert_alignment(key_type, (key_alignment)), ordered_umap_init_((key_alignment), sizeof(key_type), 1, 0, hash_##key_type, cmp_eq_##key_type, (alloc)))
void ordered_umap_deinit(const Ordered_umap *self);

bool ordered_umap_empty(const Ordered_umap *self);
usize ordered_umap_size(const Ordered_umap *self);
usize ordered_umap_bucket_count(const Ordered_umap *self);

Umap_pair ordered_umap_at_idx(Ordered_umap *self, usize idx);
Umap_pair_const ordered_umap_at_idx_const(const Ordered_umap *self, usize idx);
Umap_pair ordered_umap_at_key(Ordered_umap *self, const void *key);
Umap_pair_const ordered_umap_at_key_const(const Ordered_umap *self, const void *key);
Umap_pair ordered_umap_at_node(Ordered_umap *self, const struct Snode *node);
Umap_pair_const ordered_umap_at_node_const(const Ordered_umap *self, const struct Snode *node);

Umap_insert_result ordered_umap_push_back(Ordered_umap *self, const void *key, const void *value);
Umap_insert_result ordered_umap_insert(Ordered_umap *self, usize idx, const void *key, const void *value);

void ordered_umap_pop_back_discard(Ordered_umap *self);
void ordered_umap_pop_back_to(Ordered_umap *self, void *key_dest, void *value_dest);
void ordered_umap_erase_idx_discard(Ordered_umap *self, usize idx);
void ordered_umap_erase_idx_to(Ordered_umap *self, usize idx, void *key_dest, void *value_dest);
bool ordered_umap_erase_key_discard(Ordered_umap *self, const void *key);
bool ordered_umap_erase_key_to(Ordered_umap *self, const void *key, void *key_dest, void *value_dest);

void ordered_umap_clear(Ordered_umap *self);
void ordered_umap_reverse(Ordered_umap *self);
void ordered_umap_sort(Ordered_umap *self, int (*cmp_fn)(const void*, const void*));

#ifdef __cplusplus
}
#endif

#endif // DATA_STRUCTURE_ORDERED_UMAP_H
