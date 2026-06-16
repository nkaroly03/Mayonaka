#include <assert.h>
#include <stdalign.h>
#include <stdbool.h>
#include <string.h>

#include "../../hdrs/Allocator/Allocator.h"
#include "../../hdrs/Data_structure/Slist_base.h"
#include "../../hdrs/Data_structure/Umap_base.h"
#include "../../hdrs/Utils/Num.h"
#include "../../hdrs/Utils/Utils.h"

// ------------------------------------------------------------------------------------------------

typedef struct Snode Snode;

typedef struct Snode_offsets{
    usize key_offset, value_offset;
} Snode_offsets;

static Snode* snode_alloc(Allocator alloc, usize node_alignment, usize node_size){
    return (Snode*)allocator_alloc_aligned(alloc, u8, node_alignment, node_size);
}
static void snode_dealloc(Snode *self, Allocator alloc, usize node_size){
    allocator_free(alloc, (u8*)self, node_size);
}

static Umap_pair snode_to_umap_pair(const Snode *self, Snode_offsets offsets){
    return (Umap_pair){.m_key = (u8*)self + offsets.key_offset, .m_value = (u8*)self + offsets.value_offset};
}

#ifndef NDEBUG
static bool umap_base_contains_node(const Umap_base *self, const struct Snode *node){
    for (usize i = 0; i < self->m_bucket_count; ++i)
        for (const struct Snode *it = self->m_buckets[i].m_next; it; it = it->m_next)
            if (it == node)
                return true;

    return false;
}
#endif // NDEBUG

static Snode_offsets umap_base_snode_offsets(const Umap_base *self){
    usize key_offset = align_forward(sizeof(Snode), self->m_node_alignment);
    usize value_offset = key_offset + align_forward(self->m_key_size, self->m_node_alignment);

    return (Snode_offsets){.key_offset = key_offset, .value_offset = value_offset};
}
static usize umap_base_snode_size(const Umap_base *self){
    return umap_base_snode_offsets(self).value_offset + align_forward(self->m_value_size, self->m_node_alignment);
}

typedef struct Umap_find_result{
    usize hash;
    Snode *node_prev;
} Umap_find_result;

static Umap_find_result umap_base_find(const Umap_base *self, const void *key){
    Umap_find_result result = {.hash = self->m_hash(key)};

    if (self->m_bucket_count > 0){
        usize idx = result.hash % self->m_bucket_count;

        Snode_offsets offsets = umap_base_snode_offsets(self);

        for (Snode *prev = &self->m_buckets[idx], *current = prev->m_next; !result.node_prev && current; prev = current, current = current->m_next)
            if (self->m_cmp_eq(key, snode_to_umap_pair(current, offsets).m_key))
                result.node_prev = prev;
    }

    return result;
}

// ------------------------------------------------------------------------------------------------

Umap_base umap_base_init_(
    usize key_alignment,
    usize key_size,
    usize value_alignment,
    usize value_size,
    usize (*hash)(const void*),
    bool (*cmp_eq)(const void*, const void*)
){
    assert(is_power_of_2(key_alignment) && "Alignments are always a power of 2");
    assert(key_size > 0 && "<key_size> must be greater than 0 (<value_size> can be 0 if its used as a set)");
    assert(is_power_of_2(value_alignment) && "Alignments are always a power of 2");
    assert(hash && "<hash> is not nullable");
    assert(cmp_eq && "<cmp_eq> is not nullable");

    usize node_alignment = alignof(Snode);
    node_alignment = max(node_alignment, key_alignment);
    node_alignment = max(node_alignment, value_alignment);

    return (Umap_base){
        .m_node_alignment   = node_alignment,
        .m_key_size         = key_size,
        .m_value_size       = value_size,
        .m_size             = 0,
        .m_bucket_count     = 0,
        .m_buckets          = NULL,
        .m_hash             = hash,
        .m_cmp_eq           = cmp_eq
    };
}
void umap_base_deinit(const Umap_base *self, Allocator alloc){
    assert(self && "<self> is never null");

    Umap_base temp = *self;
    umap_base_clear(&temp, alloc);
    allocator_free(alloc, temp.m_buckets, temp.m_bucket_count);
}

bool umap_base_empty(const Umap_base *self){
    assert(self && "<self> is never null");

    return self->m_size == 0;
}
usize umap_base_size(const Umap_base *self){
    assert(self && "<self> is never null");

    return self->m_size;
}
usize umap_base_bucket_count(const Umap_base *self){
    assert(self && "<self> is never null");

    return self->m_bucket_count;
}

Umap_pair umap_base_get_pair(Umap_base *self, const void *key){
    assert(self && "<self> is never null");
    assert(key && "<key> is not nullable");

    Umap_pair result = {0};

    Umap_find_result find_result = umap_base_find(self, key);
    if (find_result.node_prev)
        result = snode_to_umap_pair(find_result.node_prev->m_next, umap_base_snode_offsets(self));

    return result;
}
Umap_pair_const umap_base_get_pair_const(const Umap_base *self, const void *key){
    assert(self && "<self> is never null");
    assert(key && "<key> is not nullable");

    Umap_pair_const result = {0};

    Umap_find_result find_result = umap_base_find(self, key);
    if (find_result.node_prev){
        Umap_pair p = snode_to_umap_pair(find_result.node_prev->m_next, umap_base_snode_offsets(self));
        result = (Umap_pair_const){.m_key = p.m_key, .m_value = p.m_value};
    }

    return result;
}
Umap_pair umap_base_node_get_pair(Umap_base *self, const struct Snode *node){
    assert(self && "<self> is never null");
    assert(node && "<node> is not nullable");
    assert(umap_base_contains_node(self, node) && "<self> doesn't contain <node>");

    return snode_to_umap_pair(node, umap_base_snode_offsets(self));
}
Umap_pair_const umap_base_node_get_pair_const(const Umap_base *self, const struct Snode *node){
    assert(self && "<self> is never null");
    assert(node && "<node> is not nullable");
    assert(umap_base_contains_node(self, node) && "<self> doesn't contain <node>");

    Umap_pair p = snode_to_umap_pair(node, umap_base_snode_offsets(self));

    return (Umap_pair_const){.m_key = p.m_key, .m_value = p.m_value};
}

Umap_insert_result umap_base_insert(Umap_base *self, Allocator alloc, const void *key, const void *value){
    assert(self && "<self> is never null");
    assert(key && "<key> is not nullable");
    assert(value && "<value> is not nullable");

    Umap_insert_result result = {.error = UMAP_INSERT_ERROR_OOM};

    Umap_find_result find_result = umap_base_find(self, key);
    if (find_result.node_prev){
        result = (Umap_insert_result){
            .result = snode_to_umap_pair(find_result.node_prev->m_next, umap_base_snode_offsets(self)),
            .error  = UMAP_INSERT_ERROR_ALREADY_INSERTED
        };
    }
    else{
        usize node_size = umap_base_snode_size(self);

        Snode *new_node = snode_alloc(alloc, self->m_node_alignment, node_size);
        if (new_node){
            if (self->m_bucket_count > 0){
                if ((f32)self->m_size / (f32)self->m_bucket_count >= UMAP_BASE_LOAD_FACTOR)
                    umap_base_rehash(self, alloc, self->m_bucket_count * 2);
            }
            else if (!umap_base_rehash(self, alloc, 32)){
                snode_dealloc(new_node, alloc, node_size);

                return result;
            }

            usize idx = find_result.hash % self->m_bucket_count;

            new_node->m_next = self->m_buckets[idx].m_next;
            self->m_buckets[idx].m_next = new_node;

            Umap_pair p = snode_to_umap_pair(new_node, umap_base_snode_offsets(self));

            memcpy((void*)p.m_key, key, self->m_key_size);
            memcpy(p.m_value, value, self->m_value_size);

            ++self->m_size;

            result = (Umap_insert_result){.result = p, .error = UMAP_INSERT_ERROR_NONE};
        }
    }

    return result;
}
Umap_insert_result umap_base_insert_no_rehash(Umap_base *self, Allocator alloc, const void *key, const void *value){
    assert(self && "<self> is never null");
    assert(key && "<key> is not nullable");
    assert(value && "<value> is not nullable");

    Umap_insert_result result = {.error = UMAP_INSERT_ERROR_OOM};

    if (self->m_bucket_count > 0){
        Umap_find_result find_result = umap_base_find(self, key);
        if (find_result.node_prev){
            result = (Umap_insert_result){
                .result = snode_to_umap_pair(find_result.node_prev->m_next, umap_base_snode_offsets(self)),
                .error  = UMAP_INSERT_ERROR_ALREADY_INSERTED
            };
        }
        else{
            Snode *new_node = snode_alloc(alloc, self->m_node_alignment, umap_base_snode_size(self));
            if (new_node){
                usize idx = find_result.hash % self->m_bucket_count;

                new_node->m_next = self->m_buckets[idx].m_next;
                self->m_buckets[idx].m_next = new_node;

                Umap_pair p = snode_to_umap_pair(new_node, umap_base_snode_offsets(self));

                memcpy((void*)p.m_key, key, self->m_key_size);
                memcpy(p.m_value, value, self->m_value_size);

                ++self->m_size;

                result = (Umap_insert_result){.result = p, .error = UMAP_INSERT_ERROR_NONE};
            }
        }
    }

    return result;
}

bool umap_base_erase_discard(Umap_base *self, Allocator alloc, const void *key){
    assert(self && "<self> is never null");
    assert(key && "<key> is not nullable");

    Umap_find_result find_result = umap_base_find(self, key);
    if (find_result.node_prev){
        Snode *node = find_result.node_prev->m_next;
        find_result.node_prev->m_next = node->m_next;

        snode_dealloc(node, alloc, umap_base_snode_size(self));

        --self->m_size;
    }

    return find_result.node_prev;
}
bool umap_base_erase_to(Umap_base *self, Allocator alloc, const void *key, void *key_dest, void *value_dest){
    assert(self && "<self> is never null");
    assert(key && "<key> is not nullable");
    assert(key_dest && "<key_dest> is not nullable");
    assert(value_dest && "<value_dest> is not nullable");

    Umap_find_result find_result = umap_base_find(self, key);
    if (find_result.node_prev){
        Snode *node = find_result.node_prev->m_next;
        find_result.node_prev->m_next = node->m_next;

        Umap_pair result = snode_to_umap_pair(node, umap_base_snode_offsets(self));
        memcpy(key_dest, result.m_key, self->m_key_size);
        memcpy(value_dest, result.m_value, self->m_value_size);

        snode_dealloc(node, alloc, umap_base_snode_size(self));

        --self->m_size;
    }

    return find_result.node_prev;
}

void umap_base_clear(Umap_base *self, Allocator alloc){
    assert(self && "<self> is never null");

    if (self->m_bucket_count > 0){
        usize node_size = umap_base_snode_size(self);
        for (usize i = 0; i < self->m_bucket_count; ++i){
            Snode *current = self->m_buckets[i].m_next, *next;
            while (current){
                next = current->m_next;
                snode_dealloc(current, alloc, node_size);
                current = next;
            }
        }
        self->m_size = 0;
        memset(self->m_buckets, 0, sizeof(*self->m_buckets) * self->m_bucket_count);
    }
}

bool umap_base_rehash(Umap_base *self, Allocator alloc, usize new_bucket_capacity){
    assert(self && "<self> is never null");

    bool success = (new_bucket_capacity > 0);
    if (success){
        Snode *new_buckets = allocator_alloc(alloc, Snode, new_bucket_capacity);
        success = new_buckets;
        if (success){
            Snode_offsets offsets = umap_base_snode_offsets(self);

            memset(new_buckets, 0, sizeof(*new_buckets) * new_bucket_capacity);
            for (usize i = 0; i < self->m_bucket_count; ++i){
                Snode *current = self->m_buckets[i].m_next, *next;
                while (current){
                    next = current->m_next;
                    usize idx = self->m_hash(snode_to_umap_pair(current, offsets).m_key) % new_bucket_capacity;
                    current->m_next = new_buckets[idx].m_next;
                    new_buckets[idx].m_next = current;
                    current = next;
                }
            }

            allocator_free(alloc, self->m_buckets, self->m_bucket_count);

            self->m_bucket_count = new_bucket_capacity;
            self->m_buckets = new_buckets;
        }
    }

    return success;
}
