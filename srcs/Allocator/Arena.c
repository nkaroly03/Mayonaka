#include <assert.h>
#include <stdbool.h>
#include <stddef.h>

#include "../../hdrs/Allocator/Allocator.h"
#include "../../hdrs/Allocator/Arena.h"
#include "../../hdrs/Data_structure/Slist_base.h"
#include "../../hdrs/Utils/Num.h"
#include "../../hdrs/Utils/Utils.h"

// ------------------------------------------------------------------------------------------------

typedef struct Snode Snode;

typedef struct Arena_node{
    Snode base;
    void *end;
} Arena_node;

static void* arena_alloc(void *state, usize alignment, usize byte_size){
    Arena *a = state;

    usize aligned_ptr;

    if (!a->m_head || (aligned_ptr = align_forward((usize)a->m_current, alignment)) + byte_size > (usize)a->m_end){
        usize old_byte_size = (usize)a->m_end - (usize)a->m_head;
        usize node_alloc_byte_size = sizeof(Arena_node) + 16 + byte_size + alignment;

        if (a->m_head){
            if (allocator_resize(a->m_child_alloc, (u8*)a->m_head, old_byte_size, (aligned_ptr - (usize)a->m_head) + byte_size)){
                a->m_current = a->m_end = ((Arena_node*)a->m_head)->end = (u8*)aligned_ptr + byte_size;

                return (void*)aligned_ptr;
            }
            node_alloc_byte_size = (old_byte_size + byte_size + alignment) * 2;
        }

        Arena_node *new_node = (Arena_node*)allocator_alloc_aligned(a->m_child_alloc, u8, alignof(Arena_node), node_alloc_byte_size);
        if (!new_node)
            return NULL;

        new_node->base.m_next = a->m_head;
        a->m_head = &new_node->base;

        a->m_current = new_node + 1;
        a->m_end = new_node->end = (u8*)new_node + node_alloc_byte_size;

        aligned_ptr = align_forward((usize)a->m_current, alignment);
    }

    a->m_current = (u8*)aligned_ptr + byte_size;

    return (void*)aligned_ptr;
}
static bool arena_resize(void *state, void *ptr, usize old_byte_size, usize new_byte_size){
    Arena *a = state;

    bool success = (new_byte_size <= old_byte_size);

    if ((u8*)a->m_current == (u8*)ptr + old_byte_size){
        usize new_current = (usize)ptr + new_byte_size;

        success = (new_current <= (usize)a->m_end);
        if (success)
            a->m_current = (void*)new_current;
        else if (allocator_resize(a->m_child_alloc, (u8*)a->m_head, (usize)((u8*)a->m_end - (u8*)a->m_head), new_current - (usize)a->m_head)){
            success = true;
            a->m_current = a->m_end = ((Arena_node*)a->m_head)->end = (void*)new_current;
        }
    }

    return success;
}
static void arena_free(void *state, void *ptr, usize byte_size){
    Arena *a = state;

    if ((u8*)a->m_current == (u8*)ptr + byte_size)
        a->m_current = ptr;
}

static const struct Allocator_vtable ARENA_VTABLE = {.m_alloc = arena_alloc, .m_resize = arena_resize, .m_free = arena_free};

// ------------------------------------------------------------------------------------------------

Arena arena_init(Allocator child_alloc){
    return (Arena){
        .m_child_alloc = child_alloc,
        .m_head        = NULL,
        .m_current     = NULL,
        .m_end         = NULL,
    };
}
void arena_deinit(Arena *self){
    assert(self && "<self> is never null");

    Snode *current = self->m_head, *next;
    while (current){
        next = current->m_next;
        allocator_free(self->m_child_alloc, (u8*)current, (usize)((u8*)((Arena_node*)current)->end - (u8*)current));
        current = next;
    }
}

Allocator arena_allocator(Arena *self){
    assert(self && "<self> is never null");

    return (Allocator){.m_state = self, .m_vtable = &ARENA_VTABLE};
}
