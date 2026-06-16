#ifndef UTILS_UTILS_H
#define UTILS_UTILS_H

#ifdef __cplusplus
extern "C"{
#endif

#include <stdbool.h>

#include "Num.h"

#if __STDC_VERSION__ >= 202311l
#define FALLTHROUGH [[fallthrough]]
#elif defined(__GNUC__)
#define FALLTHROUGH __attribute__((fallthrough))
#else
#define FALLTHROUGH
#endif // __STDC_VERSION__ >= 202311l

#define tok_to_str_(tok) #tok
#define tok_to_str(tok) tok_to_str_(tok)
#define tok_concat_(tok1, tok2) tok1##tok2
#define tok_concat(tok1, tok2) tok_concat_(tok1, tok2)

#define array_size(arr) (sizeof((arr)) / sizeof(*(arr)))

#define min(x, y) (((x) < (y)) ? (x) : (y))
#define max(x, y) (((x) > (y)) ? (x) : (y))

bool is_little_endian(void);

bool is_power_of_2(usize val);
usize align_backward(usize val, usize alignment);
usize align_forward(usize val, usize alignment);

void swap_bytes(void *data1, void *data2, usize data_size);

void reverse_elements(void *data, usize data_size, usize count);
void sort_elements(void *data, usize data_size, usize count, int (*cmp_fn)(const void*, const void*));

#ifdef __cplusplus
}
#endif

#endif // UTILS_UTILS_H
