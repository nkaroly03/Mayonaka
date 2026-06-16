#include <assert.h>
#include <stdbool.h>

#include "../../hdrs/Allocator/Allocator.h"
#include "../../hdrs/Data_structure/Vec.h"
#include "../../hdrs/Data_structure/Vec_base.h"
#include "../../hdrs/Utils/Num.h"

Vec vec_init_(usize data_alignment, usize data_size, Allocator alloc){
    assert(is_power_of_2(data_alignment) && "Alignments are always a power of 2");
    assert(data_size > 0 && "Data must have size greater than 0");
    
    return (Vec){.m_base = vec_base_init_(data_alignment, data_size), .m_alloc = alloc};
}
void vec_deinit(const Vec *self){
    assert(self && "<self> is never null");

    vec_base_deinit(&self->m_base, self->m_alloc);
}

bool vec_empty(const Vec *self){
    assert(self && "<self> is never null");

    return vec_base_empty(&self->m_base);
}
usize vec_size(const Vec *self){
    assert(self && "<self> is never null");

    return vec_base_size(&self->m_base);
}
usize vec_capacity(const Vec *self){
    assert(self && "<self> is never null");

    return vec_base_capacity(&self->m_base);
}
void* vec_at(Vec *self, usize idx){
    assert(self && "<self> is never null");
    assert(idx < vec_size(self) && "<idx> out of range");

    return vec_base_at(&self->m_base, idx);
}
const void* vec_at_const(const Vec *self, usize idx){
    assert(self && "<self> is never null");
    assert(idx < vec_size(self) && "<idx> out of range");

    return vec_base_at_const(&self->m_base, idx);
}

void* vec_push_back(Vec *self, const void *data){
    assert(self && "<self> is never null");
    assert(data && "<data> is not nullable");

    return vec_base_push_back(&self->m_base, self->m_alloc, data);
}
void* vec_insert(Vec *self, usize idx, const void *data){
    assert(self && "<self> is never null");
    assert(idx <= vec_size(self) && "<idx> out of range");
    assert(data && "<data> is not nullable");

    return vec_base_insert(&self->m_base, self->m_alloc, idx, data);
}

void vec_pop_back_discard(Vec *self){
    assert(self && "<self> is never null");
    assert(!vec_empty(self) && "<self> is empty");

    vec_base_pop_back_discard(&self->m_base);
}
void* vec_pop_back_to(Vec *self, void *dest){
    assert(self && "<self> is never null");
    assert(!vec_empty(self) && "<self> is empty");
    assert(dest && "<dest> is not nullable");

    return vec_base_pop_back_to(&self->m_base, dest);
}
void vec_erase_discard(Vec *self, usize idx){
    assert(self && "<self> is never null");
    assert(idx < vec_size(self) && "<idx> out of range");

    vec_base_erase_discard(&self->m_base, idx);
}
void* vec_erase_to(Vec *self, usize idx, void *dest){
    assert(self && "<self> is never null");
    assert(idx < vec_size(self) && "<idx> out of range");
    assert(dest && "<dest> is not nullable");

    return vec_base_erase_to(&self->m_base, idx, dest);
}

void vec_clear(Vec *self){
    assert(self && "<self> is never null");
    
    vec_base_clear(&self->m_base);
}
void vec_reverse(Vec *self){
    assert(self && "<self> is never null");

    vec_base_reverse(&self->m_base);
}
void vec_sort(Vec *self, int (*cmp_fn)(const void*, const void*)){
    assert(self && "<self> is never null");
    assert(cmp_fn && "<cmp_fn> is not nullable");

    vec_base_sort(&self->m_base, cmp_fn);
}

bool vec_reserve(Vec *self, usize reserve_size){
    assert(self && "<self> is never null");

    return vec_base_reserve(&self->m_base, self->m_alloc, reserve_size);
}
bool vec_shrink_to_fit(Vec *self){
    assert(self && "<self> is never null");

    return vec_base_shrink_to_fit(&self->m_base, self->m_alloc);
}
