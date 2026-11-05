#include <assert.h>
#include <stdalign.h>
#include <stdbool.h>

#include "../../hdrs/Allocator/Allocator.h"
#include "../../hdrs/Data_structure/Slist_base.h"
#include "../../hdrs/Data_structure/Umap_vec.h"
#include "../../hdrs/Data_structure/Umap_vec_base.h"
#include "../../hdrs/Utils/Num.h"

Umap_vec umap_vec_init_(
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

    return (Umap_vec){.m_base = umap_vec_base_init_(key_alignment, key_size, value_alignment, value_size, hash, cmp_eq), .m_alloc = alloc};
}
void umap_vec_deinit(const Umap_vec *self){
    assert(self && "<self> is never null");

    umap_vec_base_deinit(&self->m_base, self->m_alloc);
}

bool umap_vec_empty(const Umap_vec *self){
    assert(self && "<self> is never null");

    return umap_vec_base_empty(&self->m_base);
}
usize umap_vec_size(const Umap_vec *self){
    assert(self && "<self> is never null");

    return umap_vec_base_size(&self->m_base);
}
usize umap_vec_bucket_count(const Umap_vec *self){
    assert(self && "<self> is never null");

    return umap_vec_base_bucket_count(&self->m_base);
}

Umap_pair umap_vec_at_idx(Umap_vec *self, usize idx){
    assert(self && "<self> is never null");
    assert(idx < self->m_base.m_keys.m_size && "<idx> out of range");

    return umap_vec_base_at_idx(&self->m_base, idx);
}
Umap_pair_const umap_vec_at_idx_const(const Umap_vec *self, usize idx){
    assert(self && "<self> is never null");
    assert(idx < self->m_base.m_keys.m_size && "<idx> out of range");

    return umap_vec_base_at_idx_const(&self->m_base, idx);
}
Umap_pair umap_vec_at_key(Umap_vec *self, const void *key){
    assert(self && "<self> is never null");
    assert(key && "<key> is not nullable");

    return umap_vec_base_at_key(&self->m_base, key);
}
Umap_pair_const umap_vec_at_key_const(const Umap_vec *self, const void *key){
    assert(self && "<self> is never null");
    assert(key && "<key> is not nullable");

    return umap_vec_base_at_key_const(&self->m_base, key);
}
Umap_pair umap_vec_at_node(Umap_vec *self, const struct Snode *node){
    assert(self && "<self> is never null");
    assert(node && "<node> is not nullable");

    return umap_vec_base_at_node(&self->m_base, node);
}
Umap_pair_const umap_vec_at_node_const(const Umap_vec *self, const struct Snode *node){
    assert(self && "<self> is never null");
    assert(node && "<node> is not nullable");

    return umap_vec_base_at_node_const(&self->m_base, node);
}

Umap_insert_result umap_vec_push_back(Umap_vec *self, const void *key, const void *value){
    assert(self && "<self> is never null");
    assert(key && "<key> is not nullable");
    assert(value && "<value> is not nullable");

    return umap_vec_base_push_back(&self->m_base, self->m_alloc, key, value);
}
Umap_insert_result umap_vec_insert(Umap_vec *self, usize idx, const void *key, const void *value){
    assert(self && "<self> is never null");
    assert(idx <= self->m_base.m_keys.m_size && "<idx> out of range");
    assert(key && "<key> is not nullable");
    assert(value && "<value> is not nullable");

    return umap_vec_base_insert(&self->m_base, self->m_alloc, idx, key, value);
}

void umap_vec_pop_back_discard(Umap_vec *self){
    assert(self && "<self> is never null");
    assert(self->m_base.m_keys.m_size > 0 && "<self> is empty");

    umap_vec_base_pop_back_discard(&self->m_base, self->m_alloc);
}
void umap_vec_pop_back_to(Umap_vec *self, void *key_dest, void *value_dest){
    assert(self && "<self> is never null");
    assert(self->m_base.m_keys.m_size > 0 && "<self> is empty");
    assert(key_dest && "<key_dest> is not nullable");
    assert(value_dest && "<value_dest> is not nullable");

    umap_vec_base_pop_back_to(&self->m_base, self->m_alloc, key_dest, value_dest);
}
void umap_vec_erase_idx_discard(Umap_vec *self, usize idx){
    assert(self && "<self> is never null");
    assert(idx < self->m_base.m_keys.m_size && "<idx> out of range");

    umap_vec_base_erase_idx_discard(&self->m_base, self->m_alloc, idx);
}
void umap_vec_erase_idx_to(Umap_vec *self, usize idx, void *key_dest, void *value_dest){
    assert(self && "<self> is never null");
    assert(idx < self->m_base.m_keys.m_size && "<idx> out of range");
    assert(key_dest && "<key_dest> is not nullable");
    assert(value_dest && "<value_dest> is not nullable");

    umap_vec_base_erase_idx_to(&self->m_base, self->m_alloc, idx, key_dest, value_dest);
}
bool umap_vec_erase_key_discard(Umap_vec *self, const void *key){
    assert(self && "<self> is never null");
    assert(key && "<key> is not nullable");

    return umap_vec_base_erase_key_discard(&self->m_base, self->m_alloc, key);
}
bool umap_vec_erase_key_to(Umap_vec *self, const void *key, void *key_dest, void *value_dest){
    assert(self && "<self> is never null");
    assert(key && "<key> is not nullable");
    assert(key_dest && "<key_dest> is not nullable");
    assert(value_dest && "<value_dest> is not nullable");

    return umap_vec_base_erase_key_to(&self->m_base, self->m_alloc, key, key_dest, value_dest);
}

void umap_vec_clear(Umap_vec *self){
    assert(self && "<self> is never null");

    umap_vec_base_clear(&self->m_base, self->m_alloc);
}
void umap_vec_reverse(Umap_vec *self){
    assert(self && "<self> is never null");

    umap_vec_base_reverse(&self->m_base);
}
void umap_vec_sort(Umap_vec *self, int (*cmp_fn)(const void*, const void*)){
    assert(self && "<self> is never null");
    assert(cmp_fn && "<cmp_fn> is not nullable");

    umap_vec_base_sort(&self->m_base, cmp_fn);
}
