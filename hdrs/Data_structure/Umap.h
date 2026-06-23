#ifndef DATA_STRUCTURE_UMAP_H
#define DATA_STRUCTURE_UMAP_H

#ifdef __cplusplus
extern "C"{
#endif

#include <stdalign.h>
#include <stdbool.h>

#include "../Allocator/Allocator.h"
#include "../Utils/Cmp.h"
#include "../Utils/Hash.h"
#include "../Utils/Num.h"
#include "Umap_base.h"

typedef struct Umap{
    Umap_base m_base;
    Allocator m_alloc;
} Umap;

Umap umap_init_(
    usize key_alignment,
    usize key_size,
    usize value_alignment,
    usize value_size,
    usize (*hash)(const void*),
    bool (*cmp_eq)(const void*, const void*),
    Allocator alloc
);
#define umap_init(key_type, value_type, alloc) \
    umap_init_(alignof(key_type), sizeof(key_type), alignof(value_type), sizeof(value_type), hash_##key_type, cmp_eq_##key_type, (alloc))
#define umap_init_aligned(key_type, key_alignment, value_type, value_alignment, alloc) \
    ( \
        assert_alignment(key_type, (key_alignment)), \
        assert_alignment(value_type, (value_alignment)), \
        umap_init_((key_alignment), sizeof(key_type), (value_alignment), sizeof(value_type), hash_##key_type, cmp_eq_##key_type, (alloc)) \
    )
#define umap_init_as_set(key_type, alloc) umap_init_(alignof(key_type), sizeof(key_type), 1, 0, hash_##key_type, cmp_eq_##key_type, (alloc))
#define umap_init_as_set_aligned(key_type, key_alignment, alloc) \
    (assert_alignment(key_type, (key_alignment)), umap_init_((key_alignment), sizeof(key_type), 1, 0, hash_##key_type, cmp_eq_##key_type), alloc)
void umap_deinit(const Umap *self);

bool umap_empty(const Umap *self);
usize umap_size(const Umap *self);
usize umap_bucket_count(const Umap *self);

Umap_pair umap_get_pair(Umap *self, const void *key);
Umap_pair_const umap_get_pair_const(const Umap *self, const void *key);
Umap_pair umap_node_get_pair(Umap *self, const struct Snode *node);
Umap_pair_const umap_node_get_pair_const(const Umap *self, const struct Snode *node);

Umap_insert_result umap_insert(Umap *self, const void *key, const void *value);
Umap_insert_result umap_insert_no_rehash(Umap *self, const void *key, const void *value);

bool umap_erase_discard(Umap *self, const void *key);
bool umap_erase_to(Umap *self, const void *key, void *key_dest, void *value_dest);

void umap_clear(Umap *self);

bool umap_rehash(Umap *self, usize new_bucket_capacity);

#ifdef __cplusplus
}
#endif

#endif // DATA_STRUCTURE_UMAP_H
