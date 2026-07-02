#ifndef RANDOM_SPLITMIX64_H
#define RANDOM_SPLITMIX64_H

#ifdef __cplusplus
extern "C"{
#endif

#include "../Utils/Num.h"

typedef struct Splitmix64{
    u64 m_state;
} Splitmix64;

u64 splitmix64_next(Splitmix64 *self);

#ifdef __cplusplus
}
#endif

#endif // RANDOM_SPLITMIX64_H
