#include <assert.h>

#include "../../hdrs/Random/Splitmix64.h"
#include "../../hdrs/Random/Xoshiro256.h"
#include "../../hdrs/Utils/Num.h"

// ------------------------------------------------------------------------------------------------

static u64 rotl(u64 val, u64 rot){
    return (val << rot) | (val >> (64 - rot));
}

// ------------------------------------------------------------------------------------------------

Xoshiro256 xoshiro256_init(u64 seed){
    Splitmix64 sp64 = {.m_state = seed};

    return (Xoshiro256){.m_state = {splitmix64_next(&sp64), splitmix64_next(&sp64), splitmix64_next(&sp64), splitmix64_next(&sp64)}};
}

u64 xoshiro256_next(Xoshiro256 *self){
    assert(self && "<self> is never null");

    u64 result = rotl(self->m_state[0] + self->m_state[3], 23) + self->m_state[0];

    u64 t = self->m_state[1] << 17;

    self->m_state[2] ^= self->m_state[0];
    self->m_state[3] ^= self->m_state[1];
    self->m_state[1] ^= self->m_state[2];
    self->m_state[0] ^= self->m_state[3];

    self->m_state[2] ^= t;

    self->m_state[3] = rotl(self->m_state[3], 45);

    return result;
}
