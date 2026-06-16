#ifndef ALLOCATOR_ALLOCATOR_H
#define ALLOCATOR_ALLOCATOR_H

#ifdef __cplusplus
extern "C"{
#endif

#include <assert.h>
#include <stdalign.h>
#include <stdbool.h>

#include "../Utils/Num.h"
#include "../Utils/Utils.h"

struct Allocator_vtable{
    void* (*m_alloc)(void *state, usize alignment, usize byte_size);
    bool (*m_resize)(void *state, void *ptr, usize old_byte_size, usize new_byte_size);
    void (*m_free)(void *state, void *ptr, usize byte_size);
};

typedef struct Allocator{
    void *m_state;
    const struct Allocator_vtable *m_vtable;
} Allocator;

#define assert_alignment(type, alignment) (assert(is_power_of_2((alignment)) && "Alignments are always a power of 2"), assert((alignment) >= alignof(type) && "Alignment is not strict enough for given type"))

void* allocator_alloc_(Allocator alloc, usize alignment, usize byte_size);
bool allocator_resize_(Allocator alloc, void *ptr, usize old_byte_size, usize new_byte_size);
void allocator_free_(Allocator alloc, void *ptr, usize byte_size);

#define allocator_alloc(alloc, type, count) (type*)allocator_alloc_((alloc), alignof(type), sizeof(type) * (count))
#define allocator_alloc_aligned(alloc, type, alignment, count) (assert_alignment(type, (alignment)), (type*)allocator_alloc_((alloc), (alignment), sizeof(type) * (count)))
#define allocator_resize(alloc, ptr, old_count, new_count) allocator_resize_((alloc), (ptr), sizeof(*(ptr)) * (old_count), sizeof(*(ptr)) * (new_count))
#define allocator_free(alloc, ptr, count) allocator_free_((alloc), (ptr), sizeof(*(ptr)) * (count))

#ifdef __cplusplus
}
#endif

#endif // ALLOCATOR_ALLOCATOR_H
