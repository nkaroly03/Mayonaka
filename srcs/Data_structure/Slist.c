#include <assert.h>
#include <stdbool.h>

#include "../../hdrs/Allocator/Allocator.h"
#include "../../hdrs/Data_structure/Slist.h"
#include "../../hdrs/Data_structure/Slist_base.h"
#include "../../hdrs/Utils/Num.h"
#include "../../hdrs/Utils/Utils.h"

Slist slist_init_(usize data_alignment, usize data_size, Allocator alloc){
    assert(is_power_of_2(data_alignment) && "Alignments are always a power of 2");
    assert(data_size > 0 && "Data must have size greater than 0");

    return (Slist){.m_base = slist_base_init_(data_alignment, data_size), .m_alloc = alloc};
}
void slist_deinit(const Slist *self){
    assert(self && "<self> is never null");

    slist_base_deinit(&self->m_base, self->m_alloc);
}

bool slist_empty(const Slist *self){
    assert(self && "<self> is never null");

    return slist_base_empty(&self->m_base);
}
void* slist_snode_data(Slist *self, const struct Snode *node){
    assert(self && "<self> is never null");
    assert(node && "<node> is not nullable");

    return slist_base_snode_data(&self->m_base, node);
}
const void* slist_snode_data_const(const Slist *self, const struct Snode *node){
    assert(self && "<self> is never null");
    assert(node && "<node> is not nullable");

    return slist_base_snode_data_const(&self->m_base, node);
}

void* slist_insert_after(Slist *self, struct Snode *after, const void *data){
    assert(self && "<self> is never null");
    assert(after && "<after> is not nullable");
    assert(data && "<data> is not nullable");

    return slist_base_insert_after(&self->m_base, self->m_alloc, after, data);
}
void* slist_push_front(Slist *self, const void *data){
    assert(self && "<self> is never null");
    assert(data && "<data> is not nullable");

    return slist_base_push_front(&self->m_base, self->m_alloc, data);
}

void slist_erase_after_discard(Slist *self, struct Snode *after){
    assert(self && "<self> is never null");
    assert(after && "<after> is not nullable");

    slist_base_erase_after_discard(&self->m_base, self->m_alloc, after);
}
void* slist_erase_after_to(Slist *self, struct Snode *after, void *dest){
    assert(self && "<self> is never null");
    assert(after && "<after> is not nullable");
    assert(dest && "<dest> is not nullable");

    return slist_base_erase_after_to(&self->m_base, self->m_alloc, after, dest);
}
void slist_pop_front_discard(Slist *self){
    assert(self && "<self> is never null");
    assert(!slist_empty(self) && "<self> is empty");

    slist_base_pop_front_discard(&self->m_base, self->m_alloc);
}
void* slist_pop_front_to(Slist *self, void *dest){
    assert(self && "<self> is never null");
    assert(!slist_empty(self) && "<self> is empty");
    assert(dest && "<dest> is not nullable");

    return slist_base_pop_front_to(&self->m_base, self->m_alloc, dest);
}

void slist_clear(Slist *self){
    assert(self && "<self> is never null");

    slist_base_clear(&self->m_base, self->m_alloc);
}
void slist_reverse(Slist *self){
    assert(self && "<self> is never null");

    slist_base_reverse(&self->m_base);
}
void slist_sort(Slist *self, int (*cmp_fn)(const void*, const void*)){
    assert(self && "<self> is never null");
    assert(cmp_fn && "<cmp_fn> is not nullable");

    slist_base_sort(&self->m_base, cmp_fn);
}
