#include <assert.h>
#include <stdbool.h>
#include <stddef.h>

#include "../../hdrs/Allocator/Allocator.h"
#include "../../hdrs/Utils/Num.h"
#include "../../hdrs/Utils/Utils.h"

void* allocator_alloc_(Allocator alloc, usize alignment, usize byte_size){
    assert(is_power_of_2(alignment) && "Alignments are always a power of 2");

    return (byte_size > 0) ? alloc.m_vtable->m_alloc(alloc.m_state, alignment, byte_size) : NULL;
}
bool allocator_resize_(Allocator alloc, void *ptr, usize old_byte_size, usize new_byte_size){
    assert(((!ptr && old_byte_size == 0) || (ptr && old_byte_size > 0)) && "<ptr> must have valid size (null's valid size is 0)");

    if (old_byte_size == 0)
        return old_byte_size == new_byte_size;

    return (new_byte_size > 0) ? alloc.m_vtable->m_resize(alloc.m_state, ptr, old_byte_size, new_byte_size) : (allocator_free_(alloc, ptr, old_byte_size), true);
}
void allocator_free_(Allocator alloc, void *ptr, usize byte_size){
    assert(((!ptr && byte_size == 0) || (ptr && byte_size > 0)) && "<ptr> must have valid size (null's valid size is 0)");

    if (ptr)
        alloc.m_vtable->m_free(alloc.m_state, ptr, byte_size);
}
