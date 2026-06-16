#ifndef DATA_STRUCTURE_SLIST_BASE_H
#define DATA_STRUCTURE_SLIST_BASE_H

#ifdef __cplusplus
extern "C"{
#endif

#include <stdalign.h>
#include <stdbool.h>
#include <stddef.h>

#include "../Allocator/Allocator.h"
#include "../Utils/Num.h"

struct Snode{
    struct Snode *m_next;
};

typedef struct Slist_base{
    usize m_node_alignment;
    usize m_data_size;
    struct Snode m_head;
} Slist_base;

Slist_base slist_base_init_(usize data_alignment, usize data_size);
#define slist_base_init(type) slist_base_init_(alignof(type), sizeof(type))
#define slist_base_init_aligned(type, alignment) (assert_alignment(type, (alignment)), slist_base_init_((alignment), sizeof(type)))
void slist_base_deinit(const Slist_base *self, Allocator alloc);

bool slist_base_empty(const Slist_base *self);
void* slist_base_snode_data(Slist_base *self, const struct Snode *node);
const void* slist_base_snode_data_const(const Slist_base *self, const struct Snode *node);

void* slist_base_insert_after(Slist_base *self, Allocator alloc, struct Snode *after, const void *data);
void* slist_base_push_front(Slist_base *self, Allocator alloc, const void *data);

void slist_base_erase_after_discard(Slist_base *self, Allocator alloc, struct Snode *after);
void* slist_base_erase_after_to(Slist_base *self, Allocator alloc, struct Snode *after, void *dest);
void slist_base_pop_front_discard(Slist_base *self, Allocator alloc);
void* slist_base_pop_front_to(Slist_base *self, Allocator alloc, void *dest);

void slist_base_clear(Slist_base *self, Allocator alloc);
void slist_base_reverse(Slist_base *self);
void slist_base_sort(Slist_base *self, int (*cmp_fn)(const void*, const void*));

#ifdef __cplusplus
}
#endif

#endif // DATA_STRUCTURE_SLIST_BASE_H
