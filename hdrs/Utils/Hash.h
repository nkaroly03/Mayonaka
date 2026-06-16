#ifndef UTILS_HASH_H
#define UTILS_HASH_H

#ifdef __cplusplus
extern "C"{
#endif

#include "Num.h"

usize hash_char(const void *char_ptr);
usize hash_wchar_t(const void *wchar_t_ptr);

usize hash_i8(const void *i8_ptr);
usize hash_i16(const void *i16_ptr);
usize hash_i32(const void *i32_ptr);
usize hash_i64(const void *i64_ptr);

usize hash_u8(const void *u8_ptr);
usize hash_u16(const void *u16_ptr);
usize hash_u32(const void *u32_ptr);
usize hash_u64(const void *u64_ptr);

usize hash_isize(const void *isize_ptr);
usize hash_usize(const void *usize_ptr);

usize hash_Str(const void *str_ptr);
usize hash_Str_base(const void *str_base_ptr);
usize hash_Str_view(const void *str_view_ptr);

#ifdef __cplusplus
}
#endif

#endif // UTILS_HASH_H
