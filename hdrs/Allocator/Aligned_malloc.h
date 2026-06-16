#ifndef ALLOCATOR_ALIGNED_MALLOC_H
#define ALLOCATOR_ALIGNED_MALLOC_H

#ifdef __cplusplus
extern "C"{
#endif

#include "Allocator.h"

Allocator aligned_malloc_allocator(void);

#ifdef __cplusplus
}
#endif

#endif // ALLOCATOR_ALIGNED_MALLOC_H
