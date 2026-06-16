#include <stdalign.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

#include "../../hdrs/Allocator/Allocator.h"
#include "../../hdrs/Allocator/Raw_malloc.h"
#include "../../hdrs/Utils/Num.h"

// ------------------------------------------------------------------------------------------------

static void* raw_malloc_alloc(void *state, usize alignment, usize byte_size){
    (void)state;
    (void)alignment;

    assert(alignment <= alignof(max_align_t) && "Raw malloc only supports alignment upto alignof(max_align_t)");

    return (byte_size > 0) ? malloc(byte_size) : NULL;
}
static bool raw_malloc_resize(void *state, void *ptr, usize old_byte_size, usize new_byte_size){
    (void)state;
    (void)ptr;

    return new_byte_size <= old_byte_size;
}
static void raw_malloc_free(void *state, void *ptr, usize byte_size){
    (void)state;
    (void)byte_size;

    free(ptr);
}

static const struct Allocator_vtable RAW_MALLOC_VTABLE = {.m_alloc = raw_malloc_alloc, .m_resize = raw_malloc_resize, .m_free = raw_malloc_free};

// ------------------------------------------------------------------------------------------------

Allocator raw_malloc_allocator(void){
    return (Allocator){.m_state = NULL, .m_vtable = &RAW_MALLOC_VTABLE};
}
