#ifndef DATA_STRUCTURE_UMAP_BASE_H
#define DATA_STRUCTURE_UMAP_BASE_H

#ifdef __cplusplus
extern "C"{
#endif

#include <stdalign.h>
#include <stdbool.h>

#include "../Allocator/Allocator.h"
#include "../Utils/Num.h"
#include "Slist_base.h"

#define UMAP_BASE_LOAD_FACTOR 0.7f

typedef struct Umap_base{
    usize m_node_alignment;
    usize m_key_size, m_value_size;
    usize m_size;
    usize m_bucket_count;
    struct Snode *m_buckets;
    usize (*m_hash)(const void*);
    bool (*m_cmp_eq)(const void*, const void*);
} Umap_base;

Umap_base umap_base_init_(
    usize key_alignment,
    usize key_size,
    usize value_alignment,
    usize value_size,
    usize (*hash)(const void*),
    bool (*cmp_eq)(const void*, const void*)
);
#define umap_base_init(key_type, value_type) \
    umap_base_init_(alignof(key_type), sizeof(key_type), alignof(value_type), sizeof(value_type), hash_##key_type, cmp_eq_##key_type)
#define umap_base_init_aligned(key_type, key_alignment, value_type, value_alignment) \
    ( \
        assert_alignment(key_type, (key_alignment)), \
        assert_alignment(value_type, (value_alignment)), \
        umap_base_init_((key_alignment), sizeof(key_type), (value_alignment), sizeof(value_type), hash_##key_type, cmp_eq_##key_type) \
    )
#define umap_base_init_as_set(key_type) umap_base_init_(alignof(key_type), sizeof(key_type), 1, 0, hash_##key_type, cmp_eq_##key_type)
#define umap_base_init_as_set_aligned(key_type, key_alignment) \
    (assert_alignment(key_type, (key_alignment)), umap_base_init_((key_alignment), sizeof(key_type), 1, 0, hash_##key_type, cmp_eq_##key_type))
void umap_base_deinit(const Umap_base *self, Allocator alloc);

bool umap_base_empty(const Umap_base *self);
usize umap_base_size(const Umap_base *self);
usize umap_base_bucket_count(const Umap_base *self);

typedef struct Umap_pair{
    const void *m_key;
    void *m_value;
} Umap_pair;
typedef struct Umap_pair_const{
    const void *m_key, *m_value;
} Umap_pair_const;

Umap_pair umap_base_get_pair(Umap_base *self, const void *key);
Umap_pair_const umap_base_get_pair_const(const Umap_base *self, const void *key);
Umap_pair umap_base_node_get_pair(Umap_base *self, const struct Snode *node);
Umap_pair_const umap_base_node_get_pair_const(const Umap_base *self, const struct Snode *node);

enum Umap_insert_error{
    UMAP_INSERT_ERROR_NONE,
    UMAP_INSERT_ERROR_OOM,
    UMAP_INSERT_ERROR_ALREADY_INSERTED
};
typedef struct Umap_insert_result{
    Umap_pair result;
    enum Umap_insert_error error;
} Umap_insert_result;

Umap_insert_result umap_base_insert(Umap_base *self, Allocator alloc, const void *key, const void *value);
Umap_insert_result umap_base_insert_no_rehash(Umap_base *self, Allocator alloc, const void *key, const void *value);

bool umap_base_erase_discard(Umap_base *self, Allocator alloc, const void *key);
bool umap_base_erase_to(Umap_base *self, Allocator alloc, const void *key, void *key_dest, void *value_dest);

void umap_base_clear(Umap_base *self, Allocator alloc);

bool umap_base_rehash(Umap_base *self, Allocator alloc, usize new_bucket_capacity);

#ifdef __cplusplus
}
#endif

#endif // DATA_STRUCTURE_UMAP_BASE_H
