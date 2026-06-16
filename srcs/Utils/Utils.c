#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>

#include "../../hdrs/Utils/Num.h"
#include "../../hdrs/Utils/Utils.h"

bool is_little_endian(void){
    return *(u8*)&(i32){1} == 1;
}

bool is_power_of_2(usize val){
    return val > 0 && (val & (val - 1)) == 0;
}
usize align_backward(usize val, usize alignment){
    assert(alignment && "Alignments are always a power of 2");

    return val & ~(alignment - 1);
}
usize align_forward(usize val, usize alignment){
    assert(alignment && "Alignments are always a power of 2");

    usize temp = alignment - 1;

    return (val + temp) & ~temp;
}

void swap_bytes(void *data1, void *data2, usize data_size){
    assert(data1 && "<data1> is not nullable");
    assert(data2 && "<data2> is not nullable");

    for (u8 *it1 = data1, *it2 = data2; data_size-- > 0; ++it1, ++it2){
        u8 temp = *it1;
        *it1 = *it2;
        *it2 = temp;
    }
}

void reverse_elements(void *data, usize data_size, usize count){
    assert((data || (!data && count == 0)) && "<data> must have valid <count> (null's valid count is 0)");
    assert(data_size > 0 && "Data must have size greater than 0");

    if (count > 0){
        usize count_half = count / 2;
        for (u8 *it = data, *itr = it + data_size * count; count_half-- > 0; it += data_size){
            itr -= data_size;
            swap_bytes(it, itr, data_size);
        }
    }
}
void sort_elements(void *data, usize data_size, usize count, int (*cmp_fn)(const void*, const void*)){
    assert((data || (!data && count == 0)) && "<data> must have valid <count> (null's valid count is 0)");
    assert(data_size > 0 && "Data must have size greater than 0");

    if (count > 0)
        qsort(data, count, data_size, cmp_fn);
}
