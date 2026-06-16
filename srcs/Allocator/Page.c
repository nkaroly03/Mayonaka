#ifndef _WIN32
#define _GNU_SOURCE
#endif // _WIN32

#include <stdbool.h>
#include <stddef.h>

#include "../../hdrs/Allocator/Allocator.h"
#include "../../hdrs/Allocator/Page.h"
#include "../../hdrs/Utils/Num.h"
#include "../../hdrs/Utils/Utils.h"

// ------------------------------------------------------------------------------------------------

#ifdef _WIN32
#include <memoryapi.h>
#include <sysinfoapi.h>

static SYSTEM_INFO get_info(void){
    static struct{
        bool m_is_init;
        SYSTEM_INFO m_info;
    } info = {0};

    if (!info.m_is_init){
        info.m_is_init = true;
        GetSystemInfo(&info.m_info);
    }

    return info.m_info;
}
static usize get_page_size(void){
    static usize page_size = 0;

    if (page_size == 0)
        page_size = (usize)get_info().dwPageSize;

    return page_size;
}
static usize get_granularity(void){
    static usize granularity = 0;

    if (granularity == 0)
        granularity = (usize)get_info().dwAllocationGranularity;

    return granularity;
}

static void* page_alloc(void *state, usize alignment, usize byte_size){
    (void)state;

    usize granularity = get_granularity();
    usize granularity_aligned_reserve_byte_size = align_forward(byte_size, granularity);

    void *ptr;

    if (alignment <= granularity){
        ptr = VirtualAlloc(NULL, granularity_aligned_reserve_byte_size, MEM_RESERVE, PAGE_READWRITE);
        if (ptr && !VirtualAlloc(ptr, byte_size, MEM_COMMIT, PAGE_READWRITE)){
            VirtualFree(ptr, 0, MEM_RELEASE);
            ptr = NULL;
        }
    }
    else{
        usize overalloced_byte_size = byte_size + alignment - granularity;

        bool not_aligned = true;
        while (not_aligned && (ptr = VirtualAlloc(NULL, overalloced_byte_size, MEM_RESERVE, PAGE_NOACCESS))){
            void *aligned_ptr = (void*)align_forward((usize)ptr, alignment);

            VirtualFree(ptr, 0, MEM_RELEASE);

            ptr = VirtualAlloc(aligned_ptr, granularity_aligned_reserve_byte_size, MEM_RESERVE, PAGE_READWRITE);
            if (ptr && !VirtualAlloc(ptr, byte_size, MEM_COMMIT, PAGE_READWRITE)){
                VirtualFree(ptr, 0, MEM_RELEASE);
                ptr = NULL;
            }

            not_aligned = (ptr == NULL);
        }
    }

    return ptr;
}
static bool page_resize(void *state, void *ptr, usize old_byte_size, usize new_byte_size){
    (void)state;

    usize page_size = get_page_size(), granularity = get_granularity();

    usize page_aligned_old_byte_size = align_forward(old_byte_size, page_size), page_aligned_new_byte_size = align_forward(new_byte_size, page_size);

    bool success = (new_byte_size <= old_byte_size);
    if (success){
        if (page_aligned_new_byte_size < page_aligned_old_byte_size)
            VirtualFree((u8*)ptr + page_aligned_new_byte_size, page_aligned_old_byte_size - page_aligned_new_byte_size, MEM_DECOMMIT);
    }
    else if (align_forward(new_byte_size, granularity) <= align_forward(old_byte_size, granularity)){
        success = (
            page_aligned_old_byte_size == page_aligned_new_byte_size ||
            VirtualAlloc((u8*)ptr + page_aligned_old_byte_size, page_aligned_new_byte_size - page_aligned_old_byte_size, MEM_COMMIT, PAGE_READWRITE) != NULL
        );
    }

    return success;
}
static void page_free(void *state, void *ptr, usize byte_size){
    (void)state;
    (void)byte_size;

    VirtualFree(ptr, 0, MEM_RELEASE);
}

#else
#include <sys/mman.h>
#include <unistd.h>

static usize get_page_size(void){
    static usize page_size = 0;

    if (page_size == 0)
        page_size = (usize)sysconf(_SC_PAGESIZE);

    return page_size;
}

static void* page_alloc(void *state, usize alignment, usize byte_size){
    (void)state;

    usize page_size = get_page_size();

    void *ptr;

    if (alignment <= page_size)
        ptr = mmap(NULL, byte_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    else{
        usize overalloced_byte_size = byte_size + alignment - page_size;

        ptr = mmap(NULL, overalloced_byte_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (ptr != MAP_FAILED){
            void *aligned_ptr = (void*)align_forward((usize)ptr, alignment);
            if (aligned_ptr > ptr){
                munmap(ptr, (usize)((u8*)aligned_ptr - (u8*)ptr));
                ptr = aligned_ptr;
            }
            else{
                usize aligned_byte_size = align_forward(byte_size, page_size);
                munmap((u8*)ptr + aligned_byte_size, overalloced_byte_size - aligned_byte_size);
            }
        }
    }
    
    return (ptr != MAP_FAILED) ? ptr : NULL;
}
static bool page_resize(void *state, void *ptr, usize old_byte_size, usize new_byte_size){
    (void)state;

    usize page_size = get_page_size();

    usize page_aligned_old_byte_size = align_forward(old_byte_size, page_size), page_aligned_new_byte_size = align_forward(new_byte_size, page_size);

    bool success = (page_aligned_new_byte_size <= page_aligned_old_byte_size);
    if (success){
        if (page_aligned_new_byte_size < page_aligned_old_byte_size)
            munmap((u8*)ptr + page_aligned_new_byte_size, page_aligned_old_byte_size - page_aligned_new_byte_size);
    }
    else{
        usize new_alloc_byte_size = page_aligned_new_byte_size - page_aligned_old_byte_size;

        void *new_page = mmap((u8*)ptr + page_aligned_old_byte_size, new_alloc_byte_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

        success = (new_page != MAP_FAILED);
        if (success && (u8*)ptr + page_aligned_old_byte_size != (u8*)new_page){
            munmap(new_page, new_alloc_byte_size);
            success = false;
        }
    }

    return success;
}
static void page_free(void *state, void *ptr, usize byte_size){
    (void)state;

    munmap(ptr, byte_size);
}

#endif // _WIN32

static const struct Allocator_vtable PAGE_VTABLE = {.m_alloc = page_alloc, .m_resize = page_resize, .m_free = page_free};

// ------------------------------------------------------------------------------------------------

Allocator page_allocator(void){
    return (Allocator){.m_state = NULL, .m_vtable = &PAGE_VTABLE};
}
