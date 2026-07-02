#include <assert.h>

#include "../../hdrs/Random/Splitmix64.h"
#include "../../hdrs/Utils/Num.h"

u64 splitmix64_next(Splitmix64 *self){
    assert(self && "<self> is never null");

    self->m_state += 0x9e3779b97f4a7c15;

    u64 z = self->m_state;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9;
    z = (z ^ (z >> 27)) * 0x94d049bb133111eb;

    return z ^ (z >> 31);
}
