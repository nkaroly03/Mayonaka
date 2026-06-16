#ifndef ALLOCATOR_ARENA_H
#define ALLOCATOR_ARENA_H

#ifdef __cplusplus
extern "C"{
#endif

#include "../Data_structure/Slist_base.h"
#include "Allocator.h"

typedef struct Arena{
    Allocator m_child_alloc;
    struct Snode *m_head;
    void *m_current, *m_end;
} Arena;

Arena arena_init(Allocator child_alloc);
void arena_deinit(Arena *self);

Allocator arena_allocator(Arena *self);

#ifdef __cplusplus
}
#endif

#endif // ALLOCATOR_ARENA_H
