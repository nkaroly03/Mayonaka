#include <assert.h>
#include <stdalign.h>
#include <stdbool.h>
#include <stddef.h>

#include "../../hdrs/Allocator/Allocator.h"
#include "../../hdrs/Data_structure/Ordered_umap_base.h"
#include "../../hdrs/Data_structure/Slist_base.h"
#include "../../hdrs/Data_structure/Umap_base.h"
#include "../../hdrs/Utils/Num.h"
#include "../../hdrs/Utils/Utils.h"

// ------------------------------------------------------------------------------------------------

typedef struct Snode Snode;

#ifndef NDEBUG
static bool ordered_umap_base_contains_node(const Ordered_umap_base *self, const struct Snode *node){
    for (usize i = 0; i < self->m_pairs.m_bucket_count; ++i)
        for (const struct Snode *it = self->m_pairs.m_buckets[i].m_next; it; it = it->m_next)
            if (it == node)
                return true;

    return false;
}
#endif // NDEBUG

// ------------------------------------------------------------------------------------------------

Ordered_umap_base ordered_umap_base_init_(
    usize key_alignment,
    usize key_size,
    usize value_alignment,
    usize value_size,
    usize (*hash)(const void*),
    bool (*cmp_eq)(const void*, const void*)
){
    assert(is_power_of_2(key_alignment) && "Alignments are always a power of 2");
    assert(key_size > 0 && "<key_size> must be greater than 0 (<value_size> can be 0 if its used as a set)");
    assert(is_power_of_2(value_alignment) && "Alignments are always a power of 2");
    assert(hash && "<hash> is not nullable");
    assert(cmp_eq && "<cmp_eq> is not nullable");

    usize node_alignment = alignof(Snode);
    node_alignment = max(node_alignment, key_alignment);
    node_alignment = max(node_alignment, value_alignment);

    return (Ordered_umap_base){
        .m_keys  = vec_base_init_(key_alignment, key_size),
        .m_pairs = umap_base_init_(key_alignment, key_size, value_alignment, value_size, hash, cmp_eq)
    };
}
void ordered_umap_base_deinit(const Ordered_umap_base *self, Allocator alloc){
    assert(self && "<self> is never null");

    umap_base_deinit(&self->m_pairs, alloc);
    vec_base_deinit(&self->m_keys, alloc);
}

bool ordered_umap_base_empty(const Ordered_umap_base *self){
    assert(self && "<self> is never null");

    return self->m_keys.m_size == 0;
}
usize ordered_umap_base_size(const Ordered_umap_base *self){
    assert(self && "<self> is never null");

    return self->m_keys.m_size;
}
usize ordered_umap_base_bucket_count(const Ordered_umap_base *self){
    assert(self && "<self> is never null");

    return self->m_pairs.m_bucket_count;
}

Umap_pair ordered_umap_base_at_idx(Ordered_umap_base *self, usize idx){
    assert(self && "<self> is never null");
    assert(idx < self->m_keys.m_size && "<idx> out of range");

    return umap_base_at_key(&self->m_pairs, vec_base_at(&self->m_keys, idx));
}
Umap_pair_const ordered_umap_base_at_idx_const(const Ordered_umap_base *self, usize idx){
    assert(self && "<self> is never null");
    assert(idx < self->m_keys.m_size && "<idx> out of range");

    return umap_base_at_key_const(&self->m_pairs, vec_base_at_const(&self->m_keys, idx));
}
Umap_pair ordered_umap_base_at_key(Ordered_umap_base *self, const void *key){
    assert(self && "<self> is never null");
    assert(key && "<key> is not nullable");

    return umap_base_at_key(&self->m_pairs, key);
}
Umap_pair_const ordered_umap_base_at_key_const(const Ordered_umap_base *self, const void *key){
    assert(self && "<self> is never null");
    assert(key && "<key> is not nullable");

    return umap_base_at_key_const(&self->m_pairs, key);
}
Umap_pair ordered_umap_base_at_node(Ordered_umap_base *self, const struct Snode *node){
    assert(self && "<self> is never null");
    assert(node && "<node> is not nullable");
    assert(ordered_umap_base_contains_node(self, node) && "<self> doesn't contain <node>");

    return umap_base_at_node(&self->m_pairs, node);
}
Umap_pair_const ordered_umap_base_at_node_const(const Ordered_umap_base *self, const struct Snode *node){
    assert(self && "<self> is never null");
    assert(node && "<node> is not nullable");
    assert(ordered_umap_base_contains_node(self, node) && "<self> doesn't contain <node>");

    return umap_base_at_node_const(&self->m_pairs, node);
}

Umap_insert_result ordered_umap_base_push_back(Ordered_umap_base *self, Allocator alloc, const void *key, const void *value){
    assert(self && "<self> is never null");
    assert(key && "<key> is not nullable");
    assert(value && "<value> is not nullable");

    Umap_insert_result ires = umap_base_insert(&self->m_pairs, alloc, key, value);
    if (ires.error == UMAP_INSERT_ERROR_NONE && !vec_base_push_back(&self->m_keys, alloc, key)){
        umap_base_erase_key_discard(&self->m_pairs, alloc, key);
        ires.error = UMAP_INSERT_ERROR_OOM;
    }

    return ires;
}
Umap_insert_result ordered_umap_base_insert(Ordered_umap_base *self, Allocator alloc, usize idx, const void *key, const void *value){
    assert(self && "<self> is never null");
    assert(idx <= self->m_keys.m_size && "<idx> out of range");
    assert(key && "<key> is not nullable");
    assert(value && "<value> is not nullable");

    Umap_insert_result ires = umap_base_insert(&self->m_pairs, alloc, key, value);
    if (ires.error == UMAP_INSERT_ERROR_NONE && !vec_base_insert(&self->m_keys, alloc, idx, key)){
        umap_base_erase_key_discard(&self->m_pairs, alloc, key);
        ires.error = UMAP_INSERT_ERROR_OOM;
    }

    return ires;
}

void ordered_umap_base_pop_back_discard(Ordered_umap_base *self, Allocator alloc){
    assert(self && "<self> is never null");
    assert(self->m_keys.m_size > 0 && "<self> is empty");

    umap_base_erase_key_discard(&self->m_pairs, alloc, vec_base_at(&self->m_keys, self->m_keys.m_size - 1));
    vec_base_pop_back_discard(&self->m_keys);
}
void ordered_umap_base_pop_back_to(Ordered_umap_base *self, Allocator alloc, void *key_dest, void *value_dest){
    assert(self && "<self> is never null");
    assert(self->m_keys.m_size > 0 && "<self> is empty");
    assert(key_dest && "<key_dest> is not nullable");
    assert(value_dest && "<value_dest> is not nullable");

    umap_base_erase_key_to(&self->m_pairs, alloc, vec_base_at(&self->m_keys, self->m_keys.m_size - 1), key_dest, value_dest);
    vec_base_pop_back_discard(&self->m_keys);
}
void ordered_umap_base_erase_idx_discard(Ordered_umap_base *self, Allocator alloc, usize idx){
    assert(self && "<self> is never null");
    assert(idx < self->m_keys.m_size && "<idx> out of range");

    umap_base_erase_key_discard(&self->m_pairs, alloc, vec_base_at(&self->m_keys, idx));
    vec_base_erase_discard(&self->m_keys, idx);
}
void ordered_umap_base_erase_idx_to(Ordered_umap_base *self, Allocator alloc, usize idx, void *key_dest, void *value_dest){
    assert(self && "<self> is never null");
    assert(idx < self->m_keys.m_size && "<idx> out of range");
    assert(key_dest && "<key_dest> is not nullable");
    assert(value_dest && "<value_dest> is not nullable");

    umap_base_erase_key_to(&self->m_pairs, alloc, vec_base_at(&self->m_keys, idx), key_dest, value_dest);
    vec_base_erase_discard(&self->m_keys, idx);
}
bool ordered_umap_base_erase_key_discard(Ordered_umap_base *self, Allocator alloc, const void *key){
    assert(self && "<self> is never null");
    assert(key && "<key> is not nullable");

    usize i = 0;

    vec_base_for_each(self->m_keys, it){
        if (self->m_pairs.m_cmp_eq(key, it)){
            vec_base_erase_discard(&self->m_keys, i);
            return umap_base_erase_key_discard(&self->m_pairs, alloc, key);
        }
        ++i;
    }

    return false;
}
bool ordered_umap_base_erase_key_to(Ordered_umap_base *self, Allocator alloc, const void *key, void *key_dest, void *value_dest){
    assert(self && "<self> is never null");
    assert(key && "<key> is not nullable");
    assert(key_dest && "<key_dest> is not nullable");
    assert(value_dest && "<value_dest> is not nullable");

    usize i = 0;

    vec_base_for_each(self->m_keys, it){
        if (self->m_pairs.m_cmp_eq(key, it)){
            vec_base_erase_discard(&self->m_keys, i);
            return umap_base_erase_key_to(&self->m_pairs, alloc, key, key_dest, value_dest);
        }
        ++i;
    }

    return false;
}

void ordered_umap_base_clear(Ordered_umap_base *self, Allocator alloc){
    assert(self && "<self> is never null");

    umap_base_clear(&self->m_pairs, alloc);
    vec_base_clear(&self->m_keys);
}
void ordered_umap_base_reverse(Ordered_umap_base *self){
    assert(self && "<self> is never null");
    
    vec_base_reverse(&self->m_keys);
}
void ordered_umap_base_sort(Ordered_umap_base *self, int (*cmp_fn)(const void*, const void*)){
    assert(self && "<self> is never null");
    assert(cmp_fn && "<cmp_fn> is not nullable");

    vec_base_sort(&self->m_keys, cmp_fn);
}
