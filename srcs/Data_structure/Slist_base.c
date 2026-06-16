#include <assert.h>
#include <stdalign.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "../../hdrs/Allocator/Allocator.h"
#include "../../hdrs/Data_structure/Slist_base.h"
#include "../../hdrs/Utils/Num.h"
#include "../../hdrs/Utils/Utils.h"

// ------------------------------------------------------------------------------------------------

typedef struct Snode Snode;

static Snode* snode_alloc(Allocator alloc, usize node_alignment, usize node_size){
    return (Snode*)allocator_alloc_aligned(alloc, u8, node_alignment, node_size);
}
static void snode_dealloc(Snode *self, Allocator alloc, usize node_size){
    allocator_free(alloc, (u8*)self, node_size);
}

static void* snode_data(const Snode *self, usize data_offset){
    return (u8*)self + data_offset;
}

static void snode_add_after(Snode *self, Snode *node){
    node->m_next = self->m_next;
    self->m_next = node;
}
static Snode* snode_remove_after(Snode *self){
    Snode *node = self->m_next;
    self->m_next = node->m_next;

    return node;
}

static void snode_clear_after(Snode *self, Allocator alloc, usize node_size){
    Snode *current = self->m_next, *next;
    while (current){
        next = current->m_next;
        snode_dealloc(current, alloc, node_size);
        current = next;
    }
    self->m_next = NULL;
}
static void snode_reverse_after(Snode *self){
    Snode *prev = NULL, *current = self->m_next, *next;
    while (current){
        next = current->m_next;
        current->m_next = prev;
        prev = current;
        current = next;
    }
    self->m_next = prev;
}
static void snode_sort_after(Snode *self, usize data_offset, int (*cmp_fn)(const void*, const void*)){
    Snode head = {0};

    Snode *current = self->m_next, *next;
    while (current){
        next = current->m_next;
        Snode *head_it = &head;
        while (head_it->m_next && cmp_fn(snode_data(current, data_offset), snode_data(head_it->m_next, data_offset)) > 0)
            head_it = head_it->m_next;
        snode_add_after(head_it, current);
        current = next;
    }

    *self = head;
}

#ifndef NDEBUG
static bool slist_base_contains_node(const Slist_base *self, const Snode *node){
    for (const Snode *it = &self->m_head; it; it = it->m_next)
        if (it == node)
            return true;

    return false;
}
#endif // NDEBUG

static usize slist_base_data_offset(const Slist_base *self){
    return align_forward(sizeof(Snode), self->m_node_alignment);
}
static usize slist_base_snode_size(const Slist_base *self){
    return slist_base_data_offset(self) + align_forward(self->m_data_size, self->m_node_alignment);
}

static void* slist_base_snode_data_unchecked(const Slist_base *self, const Snode *node){
    return snode_data(node, slist_base_data_offset(self));
}

static void* slist_base_insert_after_unchecked(Slist_base *self, Allocator alloc, Snode *after, const void *data){
    void *ptr = NULL;

    Snode *new_node = snode_alloc(alloc, self->m_node_alignment, slist_base_snode_size(self));
    if (new_node){
        snode_add_after(after, new_node);
        ptr = slist_base_snode_data_unchecked(self, new_node);
        memcpy(ptr, data, self->m_data_size);
    }

    return ptr;
}

static void slist_base_erase_after_discard_unchecked(Slist_base *self, Allocator alloc, Snode *after){
    snode_dealloc(snode_remove_after(after), alloc, slist_base_snode_size(self));
}
static void* slist_base_erase_after_to_unchecked(Slist_base *self, Allocator alloc, Snode *after, void *dest){
    memcpy(dest, slist_base_snode_data_unchecked(self, after->m_next), self->m_data_size);
    slist_base_erase_after_discard_unchecked(self, alloc, after);

    return dest;
}

// ------------------------------------------------------------------------------------------------

Slist_base slist_base_init_(usize data_alignment, usize data_size){
    assert(is_power_of_2(data_alignment) && "Alignments are always a power of 2");
    assert(data_size > 0 && "Data must have size greater than 0");

    return (Slist_base){
        .m_node_alignment = max(alignof(Snode), data_alignment),
        .m_data_size      = data_size,
        .m_head           = {0}
    };
}
void slist_base_deinit(const Slist_base *self, Allocator alloc){
    assert(self && "<self> is never null");

    Slist_base temp = *self;
    slist_base_clear(&temp, alloc);
}

bool slist_base_empty(const Slist_base *self){
    assert(self && "<self> is never null");

    return !self->m_head.m_next;
}
void* slist_base_snode_data(Slist_base *self, const struct Snode *node){
    assert(self && "<self> is never null");
    assert(node && "<node> is not nullable");
    assert(slist_base_contains_node(self, node) && "<self> doesn't contain <node>");

    return slist_base_snode_data_unchecked(self, node);
}
const void* slist_base_snode_data_const(const Slist_base *self, const struct Snode *node){
    assert(self && "<self> is never null");
    assert(node && "<node> is not nullable");
    assert(slist_base_contains_node(self, node) && "<self> doesn't contain <node>");

    return slist_base_snode_data_unchecked(self, node);
}

void* slist_base_insert_after(Slist_base *self, Allocator alloc, struct Snode *after, const void *data){
    assert(self && "<self> is never null");
    assert(after && "<after> is not nullable");
    assert(slist_base_contains_node(self, after) && "<self> doesn't contain <after>");
    assert(data && "<data> is not nullable");

    return slist_base_insert_after_unchecked(self, alloc, after, data);
}
void* slist_base_push_front(Slist_base *self, Allocator alloc, const void *data){
    assert(self && "<self> is never null");
    assert(data && "<data> is not nullable");

    return slist_base_insert_after_unchecked(self, alloc, &self->m_head, data);
}

void slist_base_erase_after_discard(Slist_base *self, Allocator alloc, struct Snode *after){
    assert(self && "<self> is never null");
    assert(after && "<after> is not nullable");
    assert(slist_base_contains_node(self, after) && "<self> doesn't contain <after>");
    assert(after->m_next && "Erasing after the last elements");

    slist_base_erase_after_discard_unchecked(self, alloc, after);
}
void* slist_base_erase_after_to(Slist_base *self, Allocator alloc, struct Snode *after, void *dest){
    assert(self && "<self> is never null");
    assert(after && "<after> is not nullable");
    assert(slist_base_contains_node(self, after) && "<self> doesn't contain <after>");
    assert(after->m_next && "Erasing after the last elements");
    assert(dest && "<dest> is not nullable");

    return slist_base_erase_after_to_unchecked(self, alloc, after, dest);
}
void slist_base_pop_front_discard(Slist_base *self, Allocator alloc){
    assert(self && "<self> is never null");
    assert(self->m_head.m_next && "<self> is empty");

    slist_base_erase_after_discard_unchecked(self, alloc, &self->m_head);
}
void* slist_base_pop_front_to(Slist_base *self, Allocator alloc, void *dest){
    assert(self && "<self> is never null");
    assert(self->m_head.m_next && "<self> is empty");
    assert(dest && "<dest> is not nullable");

    return slist_base_erase_after_to_unchecked(self, alloc, &self->m_head, dest);
}

void slist_base_clear(Slist_base *self, Allocator alloc){
    assert(self && "<self> is never null");

    snode_clear_after(&self->m_head, alloc, slist_base_snode_size(self));
}
void slist_base_reverse(Slist_base *self){
    assert(self && "<self> is never null");

    snode_reverse_after(&self->m_head);
}
void slist_base_sort(Slist_base *self, int (*cmp_fn)(const void*, const void*)){
    assert(self && "<self> is never null");
    assert(cmp_fn && "<cmp_fn> is not nullable");

    snode_sort_after(&self->m_head, slist_base_data_offset(self), cmp_fn);
}
