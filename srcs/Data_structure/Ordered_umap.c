#include <assert.h>
#include <stdalign.h>
#include <stdbool.h>

#include "../../hdrs/Allocator/Allocator.h"
#include "../../hdrs/Data_structure/Ordered_umap.h"
#include "../../hdrs/Data_structure/Ordered_umap_base.h"
#include "../../hdrs/Data_structure/Slist_base.h"
#include "../../hdrs/Utils/Num.h"

Ordered_umap ordered_umap_init_(
    usize key_alignment,
    usize key_size,
    usize value_alignment,
    usize value_size,
    usize (*hash)(const void*),
    bool (*cmp_eq)(const void*, const void*),
    Allocator alloc
){
    assert(is_power_of_2(key_alignment) && "Alignments are always a power of 2");
    assert(key_size > 0 && "<key_size> must be greater than 0 (<value_size> can be 0 if its used as a set)");
    assert(is_power_of_2(value_alignment) && "Alignments are always a power of 2");
    assert(hash && "<hash> is not nullable");
    assert(cmp_eq && "<cmp_eq> is not nullable");

    return (Ordered_umap){.m_base = ordered_umap_base_init_(key_alignment, key_size, value_alignment, value_size, hash, cmp_eq), .m_alloc = alloc};
}
void ordered_umap_deinit(const Ordered_umap *self){
    assert(self && "<self> is never null");

    ordered_umap_base_deinit(&self->m_base, self->m_alloc);
}

bool ordered_umap_empty(const Ordered_umap *self){
    assert(self && "<self> is never null");

    return ordered_umap_base_empty(&self->m_base);
}
usize ordered_umap_size(const Ordered_umap *self){
    assert(self && "<self> is never null");

    return ordered_umap_base_size(&self->m_base);
}
usize ordered_umap_bucket_count(const Ordered_umap *self){
    assert(self && "<self> is never null");

    return ordered_umap_base_bucket_count(&self->m_base);
}

Umap_pair ordered_umap_at_idx(Ordered_umap *self, usize idx){
    assert(self && "<self> is never null");
    assert(idx < self->m_base.m_keys.m_size && "<idx> out of range");

    return ordered_umap_base_at_idx(&self->m_base, idx);
}
Umap_pair_const ordered_umap_at_idx_const(const Ordered_umap *self, usize idx){
    assert(self && "<self> is never null");
    assert(idx < self->m_base.m_keys.m_size && "<idx> out of range");

    return ordered_umap_base_at_idx_const(&self->m_base, idx);
}
Umap_pair ordered_umap_at_key(Ordered_umap *self, const void *key){
    assert(self && "<self> is never null");
    assert(key && "<key> is not nullable");

    return ordered_umap_base_at_key(&self->m_base, key);
}
Umap_pair_const ordered_umap_at_key_const(const Ordered_umap *self, const void *key){
    assert(self && "<self> is never null");
    assert(key && "<key> is not nullable");

    return ordered_umap_base_at_key_const(&self->m_base, key);
}
Umap_pair ordered_umap_at_node(Ordered_umap *self, const struct Snode *node){
    assert(self && "<self> is never null");
    assert(node && "<node> is not nullable");

    return ordered_umap_base_at_node(&self->m_base, node);
}
Umap_pair_const ordered_umap_at_node_const(const Ordered_umap *self, const struct Snode *node){
    assert(self && "<self> is never null");
    assert(node && "<node> is not nullable");

    return ordered_umap_base_at_node_const(&self->m_base, node);
}

Umap_insert_result ordered_umap_push_back(Ordered_umap *self, const void *key, const void *value){
    assert(self && "<self> is never null");
    assert(key && "<key> is not nullable");
    assert(value && "<value> is not nullable");

    return ordered_umap_base_push_back(&self->m_base, self->m_alloc, key, value);
}
Umap_insert_result ordered_umap_insert(Ordered_umap *self, usize idx, const void *key, const void *value){
    assert(self && "<self> is never null");
    assert(idx <= self->m_base.m_keys.m_size && "<idx> out of range");
    assert(key && "<key> is not nullable");
    assert(value && "<value> is not nullable");

    return ordered_umap_base_insert(&self->m_base, self->m_alloc, idx, key, value);
}

void ordered_umap_pop_back_discard(Ordered_umap *self){
    assert(self && "<self> is never null");
    assert(self->m_base.m_keys.m_size > 0 && "<self> is empty");

    ordered_umap_base_pop_back_discard(&self->m_base, self->m_alloc);
}
void ordered_umap_pop_back_to(Ordered_umap *self, void *key_dest, void *value_dest){
    assert(self && "<self> is never null");
    assert(self->m_base.m_keys.m_size > 0 && "<self> is empty");
    assert(key_dest && "<key_dest> is not nullable");
    assert(value_dest && "<value_dest> is not nullable");

    ordered_umap_base_pop_back_to(&self->m_base, self->m_alloc, key_dest, value_dest);
}
void ordered_umap_erase_idx_discard(Ordered_umap *self, usize idx){
    assert(self && "<self> is never null");
    assert(idx < self->m_base.m_keys.m_size && "<idx> out of range");

    ordered_umap_base_erase_idx_discard(&self->m_base, self->m_alloc, idx);
}
void ordered_umap_erase_idx_to(Ordered_umap *self, usize idx, void *key_dest, void *value_dest){
    assert(self && "<self> is never null");
    assert(idx < self->m_base.m_keys.m_size && "<idx> out of range");
    assert(key_dest && "<key_dest> is not nullable");
    assert(value_dest && "<value_dest> is not nullable");

    ordered_umap_base_erase_idx_to(&self->m_base, self->m_alloc, idx, key_dest, value_dest);
}
bool ordered_umap_erase_key_discard(Ordered_umap *self, const void *key){
    assert(self && "<self> is never null");
    assert(key && "<key> is not nullable");

    return ordered_umap_base_erase_key_discard(&self->m_base, self->m_alloc, key);
}
bool ordered_umap_erase_key_to(Ordered_umap *self, const void *key, void *key_dest, void *value_dest){
    assert(self && "<self> is never null");
    assert(key && "<key> is not nullable");
    assert(key_dest && "<key_dest> is not nullable");
    assert(value_dest && "<value_dest> is not nullable");

    return ordered_umap_base_erase_key_to(&self->m_base, self->m_alloc, key, key_dest, value_dest);
}

void ordered_umap_clear(Ordered_umap *self){
    assert(self && "<self> is never null");

    ordered_umap_base_clear(&self->m_base, self->m_alloc);
}
void ordered_umap_reverse(Ordered_umap *self){
    assert(self && "<self> is never null");

    ordered_umap_base_reverse(&self->m_base);
}
void ordered_umap_sort(Ordered_umap *self, int (*cmp_fn)(const void*, const void*)){
    assert(self && "<self> is never null");
    assert(cmp_fn && "<cmp_fn> is not nullable");

    ordered_umap_base_sort(&self->m_base, cmp_fn);
}
