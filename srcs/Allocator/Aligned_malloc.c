#include <stdalign.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

#include "../../hdrs/Allocator/Aligned_malloc.h"
#include "../../hdrs/Allocator/Allocator.h"
#include "../../hdrs/Utils/Num.h"
#include "../../hdrs/Utils/Utils.h"

// ------------------------------------------------------------------------------------------------

static void** aligned_malloc_get_header(void *ptr){
    return (void**)((u8*)ptr - sizeof(void*));
}

static void* aligned_malloc_alloc(void *state, usize alignment, usize byte_size){
    (void)state;

    void *aligned_ptr = NULL;

    void *base_ptr = malloc(byte_size + alignment - 1 + sizeof(void*));
    if (base_ptr){
        aligned_ptr = (void*)align_forward((usize)((u8*)base_ptr + sizeof(void*)), alignment);
        *aligned_malloc_get_header(aligned_ptr) = base_ptr;
    }

    return aligned_ptr;
}
static bool aligned_malloc_resize(void *state, void *ptr, usize old_byte_size, usize new_byte_size){
    (void)state;
    (void)ptr;

    return new_byte_size <= old_byte_size;
}
static void aligned_malloc_free(void *state, void *ptr, usize byte_size){
    (void)state;
    (void)byte_size;

    free(*aligned_malloc_get_header(ptr));
}

static const struct Allocator_vtable ALIGNED_MALLOC_VTABLE = {.m_alloc = aligned_malloc_alloc, .m_resize = aligned_malloc_resize, .m_free = aligned_malloc_free};

// ------------------------------------------------------------------------------------------------

Allocator aligned_malloc_allocator(void){
    return (Allocator){.m_state = NULL, .m_vtable = &ALIGNED_MALLOC_VTABLE};
}
