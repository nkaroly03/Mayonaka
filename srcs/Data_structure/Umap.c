#include <assert.h>
#include <stdalign.h>
#include <stdbool.h>

#include "../../hdrs/Allocator/Allocator.h"
#include "../../hdrs/Data_structure/Umap.h"
#include "../../hdrs/Data_structure/Umap_base.h"
#include "../../hdrs/Utils/Num.h"

Umap umap_init_(
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

    return (Umap){.m_base = umap_base_init_(key_alignment, key_size, value_alignment, value_size, hash, cmp_eq), .m_alloc = alloc};
}
void umap_deinit(const Umap *self){
    assert(self && "<self> is never null");

    umap_base_deinit(&self->m_base, self->m_alloc);
}

bool umap_empty(const Umap *self){
    assert(self && "<self> is never null");

    return umap_base_empty(&self->m_base);
}
usize umap_size(const Umap *self){
    assert(self && "<self> is never null");

    return umap_base_size(&self->m_base);
}
usize umap_bucket_count(const Umap *self){
    assert(self && "<self> is never null");

    return umap_base_bucket_count(&self->m_base);
}

Umap_pair umap_get_pair(Umap *self, const void *key){
    assert(self && "<self> is never null");
    assert(key && "<key> is not nullable");

    return umap_base_get_pair(&self->m_base, key);
}
Umap_pair_const umap_get_pair_const(const Umap *self, const void *key){
    assert(self && "<self> is never null");
    assert(key && "<key> is not nullable");

    return umap_base_get_pair_const(&self->m_base, key);
}
Umap_pair umap_node_get_pair(Umap *self, const struct Snode *node){
    assert(self && "<self> is never null");
    assert(node && "<node> is not nullable");

    return umap_base_node_get_pair(&self->m_base, node);
}
Umap_pair_const umap_node_get_pair_const(const Umap *self, const struct Snode *node){
    assert(self && "<self> is never null");
    assert(node && "<node> is not nullable");

    return umap_base_node_get_pair_const(&self->m_base, node);
}

Umap_insert_result umap_insert(Umap *self, const void *key, const void *value){
    assert(self && "<self> is never null");
    assert(key && "<key> is not nullable");
    assert(value && "<value> is not nullable");

    return umap_base_insert(&self->m_base, self->m_alloc, key, value);
}
Umap_insert_result umap_insert_no_rehash(Umap *self, const void *key, const void *value){
    assert(self && "<self> is never null");
    assert(key && "<key> is not nullable");
    assert(value && "<value> is not nullable");

    return umap_base_insert_no_rehash(&self->m_base, self->m_alloc, key, value);
}

bool umap_erase_discard(Umap *self, const void *key){
    assert(self && "<self> is never null");
    assert(key && "<key> is not nullable");

    return umap_base_erase_discard(&self->m_base, self->m_alloc, key);
}
bool umap_erase_to(Umap *self, const void *key, void *key_dest, void *value_dest){
    assert(self && "<self> is never null");
    assert(key && "<key> is not nullable");
    assert(key_dest && "<key_dest> is not nullable");
    assert(value_dest && "<value_dest> is not nullable");

    return umap_base_erase_to(&self->m_base, self->m_alloc, key, key_dest, value_dest);
}

void umap_clear(Umap *self){
    assert(self && "<self> is never null");

    umap_base_clear(&self->m_base, self->m_alloc);
}

bool umap_rehash(Umap *self, usize new_bucket_capacity){
    assert(self && "<self> is never null");

    return umap_base_rehash(&self->m_base, self->m_alloc, new_bucket_capacity);
}
