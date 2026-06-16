#ifndef DATA_STRUCTURE_SLIST_H
#define DATA_STRUCTURE_SLIST_H

#ifdef __cplusplus
extern "C"{
#endif

#include <stdalign.h>
#include <stdbool.h>

#include "../Allocator/Allocator.h"
#include "../Utils/Num.h"
#include "Slist_base.h"

typedef struct Slist{
    Slist_base m_base;
    Allocator m_alloc;
} Slist;

Slist slist_init_(usize data_alignment, usize data_size, Allocator alloc);
#define slist_init(type, alloc) slist_init_(alignof(type), sizeof(type), (alloc))
#define slist_init_aligned(type, alignment, alloc) (assert_alignment(type, (alignment)), slist_init_((alignment), sizeof(type)), (alloc))
void slist_deinit(const Slist *self);

bool slist_empty(const Slist *self);
void* slist_snode_data(Slist *self, const struct Snode *node);
const void* slist_snode_data_const(const Slist *self, const struct Snode *node);

void* slist_insert_after(Slist *self, struct Snode *after, const void *data);
void* slist_push_front(Slist *self, const void *data);

void slist_erase_after_discard(Slist *self, struct Snode *after);
void* slist_erase_after_to(Slist *self, struct Snode *after, void *dest);
void slist_pop_front_discard(Slist *self);
void* slist_pop_front_to(Slist *self, void *dest);

void slist_clear(Slist *self);
void slist_reverse(Slist *self);
void slist_sort(Slist *self, int (*cmp_fn)(const void*, const void*));

#ifdef __cplusplus
}
#endif

#endif // DATA_STRUCTURE_SLIST_H
