#ifndef RANDOM_XOSHIRO256_H
#define RANDOM_XOSHIRO256_H

#ifdef __cplusplus
extern "C"{
#endif

#include "../Utils/Num.h"

typedef struct Xoshiro256{
    u64 m_state[4];
} Xoshiro256;

Xoshiro256 xoshiro256_init(u64 seed);

u64 xoshiro256_next(Xoshiro256 *self);

#ifdef __cplusplus
}
#endif

#endif // RANDOM_XOSHIRO256_H
