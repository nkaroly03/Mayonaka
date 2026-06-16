#ifndef ALLOCATOR_RAW_MALLOC_H
#define ALLOCATOR_RAW_MALLOC_H

#ifdef __cplusplus
extern "C"{
#endif

#include "Allocator.h"

Allocator raw_malloc_allocator(void);

#ifdef __cplusplus
}
#endif

#endif // ALLOCATOR_RAW_MALLOC_H
