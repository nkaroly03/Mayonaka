#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "../../hdrs/Allocator/Allocator.h"
#include "../../hdrs/Data_structure/Vec_base.h"
#include "../../hdrs/Utils/Num.h"
#include "../../hdrs/Utils/Utils.h"

// ------------------------------------------------------------------------------------------------

static bool vec_base_realloc(Vec_base *self, Allocator alloc, usize new_capacity){
    usize old_byte_cap = self->m_data_size * self->m_capacity, new_byte_cap = self->m_data_size * new_capacity;

    if (!allocator_resize(alloc, (u8*)self->m_data, old_byte_cap, new_byte_cap)){
        void *new_location = allocator_alloc(alloc, u8, new_byte_cap);
        if (!new_location)
            return false;

        // this branch is needed because undefined behaviour on memcpy(non-null, null, 0) which shouldn't be a thing
        if (self->m_capacity > 0){
            memcpy(new_location, self->m_data, self->m_data_size * self->m_size);
            allocator_free(alloc, (u8*)self->m_data, old_byte_cap);
        }

        self->m_data = new_location;
    }

    self->m_capacity = new_capacity;

    return true;
}

static void* vec_base_at_unchecked(const Vec_base *self, usize idx){
    return (u8*)self->m_data + self->m_data_size * idx;
}

// ------------------------------------------------------------------------------------------------

Vec_base vec_base_init_(usize data_alignment, usize data_size){
    assert(is_power_of_2(data_alignment) && "Alignments are always a power of 2");
    assert(data_size > 0 && "Data must have size greater than 0");

    return (Vec_base){
        .m_data_alignment = data_alignment,
        .m_data_size      = data_size,
        .m_size           = 0,
        .m_capacity       = 0,
        .m_data           = NULL
    };
}
void vec_base_deinit(const Vec_base *self, Allocator alloc){
    assert(self && "<self> is never null");

    allocator_free(alloc, (u8*)self->m_data, self->m_data_size * self->m_capacity);
}

bool vec_base_empty(const Vec_base *self){
    assert(self && "<self> is never null");

    return self->m_size == 0;
}
usize vec_base_size(const Vec_base *self){
    assert(self && "<self> is never null");

    return self->m_size;
}
usize vec_base_capacity(const Vec_base *self){
    assert(self && "<self> is never null");

    return self->m_capacity;
}
void* vec_base_at(Vec_base *self, usize idx){
    assert(self && "<self> is never null");
    assert(idx < self->m_size && "<idx> out of range");

    return vec_base_at_unchecked(self, idx);
}
const void* vec_base_at_const(const Vec_base *self, usize idx){
    assert(self && "<self> is never null");
    assert(idx < self->m_size && "<idx> out of range");

    return vec_base_at_unchecked(self, idx);
}

void* vec_base_push_back(Vec_base *self, Allocator alloc, const void *data){
    assert(self && "<self> is never null");
    assert(data && "<data> is not nullable");

    void *ptr = NULL;

    if (self->m_size < self->m_capacity || vec_base_realloc(self, alloc, self->m_capacity * 2 + (self->m_capacity == 0))){
        ptr = vec_base_at_unchecked(self, self->m_size++);
        memcpy(ptr, data, self->m_data_size);
    }

    return ptr;
}
void* vec_base_insert(Vec_base *self, Allocator alloc, usize idx, const void *data){
    assert(self && "<self> is never null");
    assert(idx <= self->m_size && "<idx> out of range");
    assert(data && "<data> is not nullable");

    void *ptr = NULL;

    if (self->m_size < self->m_capacity || vec_base_realloc(self, alloc, self->m_capacity * 2 + (self->m_capacity == 0))){
        ptr = vec_base_at_unchecked(self, idx);
        memmove((u8*)ptr + self->m_data_size, ptr, self->m_data_size * (self->m_size++ - idx));
        memcpy(ptr, data, self->m_data_size);
    }

    return ptr;
}

void vec_base_pop_back_discard(Vec_base *self){
    assert(self && "<self> is never null");
    assert(!vec_base_empty(self) && "<self> is empty");

    --self->m_size;
}
void* vec_base_pop_back_to(Vec_base *self, void *dest){
    assert(self && "<self> is never null");
    assert(!vec_base_empty(self) && "<self> is empty");
    assert(dest && "<dest> is not nullable");

    memcpy(dest, vec_base_at_unchecked(self, --self->m_size), self->m_data_size);

    return dest;
}
void vec_base_erase_discard(Vec_base *self, usize idx){
    assert(self && "<self> is never null");
    assert(idx < self->m_size && "<idx> out of range");

    void *ptr = vec_base_at_unchecked(self, idx);
    memmove(ptr, (u8*)ptr + self->m_data_size, self->m_data_size * (--self->m_size - idx));
}
void* vec_base_erase_to(Vec_base *self, usize idx, void *dest){
    assert(self && "<self> is never null");
    assert(idx < self->m_size && "<idx> out of range");
    assert(dest && "<dest> is not nullable");

    memcpy(dest, vec_base_at_unchecked(self, idx), self->m_data_size);
    vec_base_erase_discard(self, idx);

    return dest;
}

void vec_base_clear(Vec_base *self){
    assert(self && "<self> is never null");

    self->m_size = 0;
}
void vec_base_reverse(Vec_base *self){
    assert(self && "<self> is never null");

    reverse_elements(self->m_data, self->m_data_size, self->m_size);
}
void vec_base_sort(Vec_base *self, int (*cmp_fn)(const void*, const void*)){
    assert(self && "<self> is never null");
    assert(cmp_fn && "<cmp_fn> is not nullable");

    sort_elements(self->m_data, self->m_data_size, self->m_size, cmp_fn);
}

bool vec_base_reserve(Vec_base *self, Allocator alloc, usize reserve_size){
    assert(self && "<self> is never null");

    return (reserve_size > self->m_capacity) ? vec_base_realloc(self, alloc, reserve_size) : true;
}
bool vec_base_shrink_to_fit(Vec_base *self, Allocator alloc){
    assert(self && "<self> is never null");

    if (self->m_size == 0){
        vec_base_deinit(self, alloc);
        self->m_capacity = 0;
        self->m_data = NULL;

        return true;
    }

    return vec_base_realloc(self, alloc, self->m_size);
}
