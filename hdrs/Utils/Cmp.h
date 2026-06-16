#ifndef UTILS_CMP_H
#define UTILS_CMP_H

#ifdef __cplusplus
extern "C"{
#endif

#include <stdbool.h>

#define cmp_le_to_leq(cmp_le, ptr1, ptr2) (!(cmp_le)((ptr2), (ptr1)))
#define cmp_le_to_ge(cmp_le, ptr1, ptr2)  ( (cmp_le)((ptr2), (ptr1)))
#define cmp_le_to_geq(cmp_le, ptr1, ptr2) (!(cmp_le)((ptr1), (ptr2)))

bool cmp_eq_char(const void *char_ptr1, const void *char_ptr2);
#define cmp_neq_char(char_ptr1, char_ptr2) (!cmp_eq_char((char_ptr1), (char_ptr2)))
bool cmp_le_char(const void *char_ptr1, const void *char_ptr2);
#define cmp_leq_char(char_ptr1, char_ptr2) cmp_le_to_leq((cmp_le_char), (char_ptr1), (char_ptr2))
#define cmp_ge_char(char_ptr1, char_ptr2)  cmp_le_to_ge( (cmp_le_char), (char_ptr1), (char_ptr2))
#define cmp_geq_char(char_ptr1, char_ptr2) cmp_le_to_geq((cmp_le_char), (char_ptr1), (char_ptr2))
int cmp_char(const void *char_ptr1, const void *char_ptr2);

bool cmp_eq_wchar_t(const void *wchar_t_ptr1, const void *wchar_t_ptr2);
#define cmp_neq_wchar_t(wchar_t_ptr1, wchar_t_ptr2) (!cmp_eq_wchar_t((wchar_t_ptr1), (wchar_t_ptr2)))
bool cmp_le_wchar_t(const void *wchar_t_ptr1, const void *wchar_t_ptr2);
#define cmp_leq_wchar_t(wchar_t_ptr1, wchar_t_ptr2) cmp_le_to_leq((cmp_le_wchar_t), (wchar_t_ptr1), (wchar_t_ptr2))
#define cmp_ge_wchar_t(wchar_t_ptr1, wchar_t_ptr2)  cmp_le_to_ge( (cmp_le_wchar_t), (wchar_t_ptr1), (wchar_t_ptr2))
#define cmp_geq_wchar_t(wchar_t_ptr1, wchar_t_ptr2) cmp_le_to_geq((cmp_le_wchar_t), (wchar_t_ptr1), (wchar_t_ptr2))
int cmp_wchar_t(const void *wchar_t_ptr1, const void *wchar_t_ptr2);

bool cmp_eq_i8(const void *i8_ptr1, const void *i8_ptr2);
#define cmp_neq_i8(i8_ptr1, i8_ptr2) (!cmp_eq_i8((i8_ptr1), (i8_ptr2)))
bool cmp_le_i8(const void *i8_ptr1, const void *i8_ptr2);
#define cmp_leq_i8(i8_ptr1, i8_ptr2) cmp_le_to_leq((cmp_le_i8), (i8_ptr1), (i8_ptr2))
#define cmp_ge_i8(i8_ptr1, i8_ptr2)  cmp_le_to_ge( (cmp_le_i8), (i8_ptr1), (i8_ptr2))
#define cmp_geq_i8(i8_ptr1, i8_ptr2) cmp_le_to_geq((cmp_le_i8), (i8_ptr1), (i8_ptr2))
int cmp_i8(const void *i8_ptr1, const void *i8_ptr2);

bool cmp_eq_i16(const void *i16_ptr1, const void *i16_ptr2);
#define cmp_neq_i16(i16_ptr1, i16_ptr2) (!cmp_eq_i16((i16_ptr1), (i16_ptr2)))
bool cmp_le_i16(const void *i16_ptr1, const void *i16_ptr2);
#define cmp_leq_i16(i16_ptr1, i16_ptr2) cmp_le_to_leq((cmp_le_i16), (i16_ptr1), (i16_ptr2))
#define cmp_ge_i16(i16_ptr1, i16_ptr2)  cmp_le_to_ge( (cmp_le_i16), (i16_ptr1), (i16_ptr2))
#define cmp_geq_i16(i16_ptr1, i16_ptr2) cmp_le_to_geq((cmp_le_i16), (i16_ptr1), (i16_ptr2))
int cmp_i16(const void *i16_ptr1, const void *i16_ptr2);

bool cmp_eq_i32(const void *i32_ptr1, const void *i32_ptr2);
#define cmp_neq_i32(i32_ptr1, i32_ptr2) (!cmp_eq_i32((i32_ptr1), (i32_ptr2)))
bool cmp_le_i32(const void *i32_ptr1, const void *i32_ptr2);
#define cmp_leq_i32(i32_ptr1, i32_ptr2) cmp_le_to_leq((cmp_le_i32), (i32_ptr1), (i32_ptr2))
#define cmp_ge_i32(i32_ptr1, i32_ptr2)  cmp_le_to_ge( (cmp_le_i32), (i32_ptr1), (i32_ptr2))
#define cmp_geq_i32(i32_ptr1, i32_ptr2) cmp_le_to_geq((cmp_le_i32), (i32_ptr1), (i32_ptr2))
int cmp_i32(const void *i32_ptr1, const void *i32_ptr2);

bool cmp_eq_i64(const void *i64_ptr1, const void *i64_ptr2);
#define cmp_neq_i64(i64_ptr1, i64_ptr2) (!cmp_eq_i64((i64_ptr1), (i64_ptr2)))
bool cmp_le_i64(const void *i64_ptr1, const void *i64_ptr2);
#define cmp_leq_i64(i64_ptr1, i64_ptr2) cmp_le_to_leq((cmp_le_i64), (i64_ptr1), (i64_ptr2))
#define cmp_ge_i64(i64_ptr1, i64_ptr2)  cmp_le_to_ge( (cmp_le_i64), (i64_ptr1), (i64_ptr2))
#define cmp_geq_i64(i64_ptr1, i64_ptr2) cmp_le_to_geq((cmp_le_i64), (i64_ptr1), (i64_ptr2))
int cmp_i64(const void *i64_ptr1, const void *i64_ptr2);

bool cmp_eq_u8(const void *u8_ptr1, const void *u8_ptr2);
#define cmp_neq_u8(u8_ptr1, u8_ptr2) (!cmp_eq_u8((u8_ptr1), (u8_ptr2)))
bool cmp_le_u8(const void *u8_ptr1, const void *u8_ptr2);
#define cmp_leq_u8(u8_ptr1, u8_ptr2) cmp_le_to_leq((cmp_le_u8), (u8_ptr1), (u8_ptr2))
#define cmp_ge_u8(u8_ptr1, u8_ptr2)  cmp_le_to_ge( (cmp_le_u8), (u8_ptr1), (u8_ptr2))
#define cmp_geq_u8(u8_ptr1, u8_ptr2) cmp_le_to_geq((cmp_le_u8), (u8_ptr1), (u8_ptr2))
int cmp_u8(const void *u8_ptr1, const void *u8_ptr2);

bool cmp_eq_u16(const void *u16_ptr1, const void *u16_ptr2);
#define cmp_neq_u16(u16_ptr1, u16_ptr2) (!cmp_eq_u16((u16_ptr1), (u16_ptr2)))
bool cmp_le_u16(const void *u16_ptr1, const void *u16_ptr2);
#define cmp_leq_u16(u16_ptr1, u16_ptr2) cmp_le_to_leq((cmp_le_u16), (u16_ptr1), (u16_ptr2))
#define cmp_ge_u16(u16_ptr1, u16_ptr2)  cmp_le_to_ge( (cmp_le_u16), (u16_ptr1), (u16_ptr2))
#define cmp_geq_u16(u16_ptr1, u16_ptr2) cmp_le_to_geq((cmp_le_u16), (u16_ptr1), (u16_ptr2))
int cmp_u16(const void *u16_ptr1, const void *u16_ptr2);

bool cmp_eq_u32(const void *u32_ptr1, const void *u32_ptr2);
#define cmp_neq_u32(u32_ptr1, u32_ptr2) (!cmp_eq_u32((u32_ptr1), (u32_ptr2)))
bool cmp_le_u32(const void *u32_ptr1, const void *u32_ptr2);
#define cmp_leq_u32(u32_ptr1, u32_ptr2) cmp_le_to_leq((cmp_le_u32), (u32_ptr1), (u32_ptr2))
#define cmp_ge_u32(u32_ptr1, u32_ptr2)  cmp_le_to_ge( (cmp_le_u32), (u32_ptr1), (u32_ptr2))
#define cmp_geq_u32(u32_ptr1, u32_ptr2) cmp_le_to_geq((cmp_le_u32), (u32_ptr1), (u32_ptr2))
int cmp_u32(const void *u32_ptr1, const void *u32_ptr2);

bool cmp_eq_u64(const void *u64_ptr1, const void *u64_ptr2);
#define cmp_neq_u64(u64_ptr1, u64_ptr2) (!cmp_eq_u64((u64_ptr1), (u64_ptr2)))
bool cmp_le_u64(const void *u64_ptr1, const void *u64_ptr2);
#define cmp_leq_u64(u64_ptr1, u64_ptr2) cmp_le_to_leq((cmp_le_u64), (u64_ptr1), (u64_ptr2))
#define cmp_ge_u64(u64_ptr1, u64_ptr2)  cmp_le_to_ge( (cmp_le_u64), (u64_ptr1), (u64_ptr2))
#define cmp_geq_u64(u64_ptr1, u64_ptr2) cmp_le_to_geq((cmp_le_u64), (u64_ptr1), (u64_ptr2))
int cmp_u64(const void *u64_ptr1, const void *u64_ptr2);

bool cmp_eq_isize(const void *isize_ptr1, const void *isize_ptr2);
#define cmp_neq_isize(isize_ptr1, isize_ptr2) (!cmp_eq_isize((isize_ptr1), (isize_ptr2)))
bool cmp_le_isize(const void *isize_ptr1, const void *isize_ptr2);
#define cmp_leq_isize(isize_ptr1, isize_ptr2) cmp_le_to_leq((cmp_le_isize), (isize_ptr1), (isize_ptr2))
#define cmp_ge_isize(isize_ptr1, isize_ptr2)  cmp_le_to_ge( (cmp_le_isize), (isize_ptr1), (isize_ptr2))
#define cmp_geq_isize(isize_ptr1, isize_ptr2) cmp_le_to_geq((cmp_le_isize), (isize_ptr1), (isize_ptr2))
int cmp_isize(const void *isize_ptr1, const void *isize_ptr2);

bool cmp_eq_usize(const void *usize_ptr1, const void *usize_ptr2);
#define cmp_neq_usize(usize_ptr1, usize_ptr2) (!cmp_eq_usize((usize_ptr1), (usize_ptr2)))
bool cmp_le_usize(const void *usize_ptr1, const void *usize_ptr2);
#define cmp_leq_usize(usize_ptr1, usize_ptr2) cmp_le_to_leq((cmp_le_usize), (usize_ptr1), (usize_ptr2))
#define cmp_ge_usize(usize_ptr1, usize_ptr2)  cmp_le_to_ge( (cmp_le_usize), (usize_ptr1), (usize_ptr2))
#define cmp_geq_usize(usize_ptr1, usize_ptr2) cmp_le_to_geq((cmp_le_usize), (usize_ptr1), (usize_ptr2))
int cmp_usize(const void *usize_ptr1, const void *usize_ptr2);

bool cmp_eq_f32(const void *f32_ptr1, const void *f32_ptr2);
#define cmp_neq_f32(f32_ptr1, f32_ptr2) (!cmp_eq_f32((f32_ptr1), (f32_ptr2)))
bool cmp_le_f32(const void *f32_ptr1, const void *f32_ptr2);
#define cmp_leq_f32(f32_ptr1, f32_ptr2) cmp_le_to_leq((cmp_le_f32), (f32_ptr1), (f32_ptr2))
#define cmp_ge_f32(f32_ptr1, f32_ptr2)  cmp_le_to_ge( (cmp_le_f32), (f32_ptr1), (f32_ptr2))
#define cmp_geq_f32(f32_ptr1, f32_ptr2) cmp_le_to_geq((cmp_le_f32), (f32_ptr1), (f32_ptr2))
int cmp_f32(const void *f32_ptr1, const void *f32_ptr2);

bool cmp_eq_f64(const void *f64_ptr1, const void *f64_ptr2);
#define cmp_neq_f64(f64_ptr1, f64_ptr2) (!cmp_eq_f64((f64_ptr1), (f64_ptr2)))
bool cmp_le_f64(const void *f64_ptr1, const void *f64_ptr2);
#define cmp_leq_f64(f64_ptr1, f64_ptr2) cmp_le_to_leq((cmp_le_f64), (f64_ptr1), (f64_ptr2))
#define cmp_ge_f64(f64_ptr1, f64_ptr2)  cmp_le_to_ge( (cmp_le_f64), (f64_ptr1), (f64_ptr2))
#define cmp_geq_f64(f64_ptr1, f64_ptr2) cmp_le_to_geq((cmp_le_f64), (f64_ptr1), (f64_ptr2))
int cmp_f64(const void *f64_ptr1, const void *f64_ptr2);

bool cmp_eq_Str(const void *str_ptr1, const void *str_ptr2);
#define cmp_neq_Str(str_ptr1, str_ptr2) (!cmp_eq_Str((str_ptr1), (str_ptr2)))
bool cmp_le_Str(const void *str_ptr1, const void *str_ptr2);
#define cmp_leq_Str(str_ptr1, str_ptr2) cmp_le_to_leq((cmp_le_Str), (str_ptr1), (str_ptr2))
#define cmp_ge_Str(str_ptr1, str_ptr2)  cmp_le_to_ge( (cmp_le_Str), (str_ptr1), (str_ptr2))
#define cmp_geq_Str(str_ptr1, str_ptr2) cmp_le_to_geq((cmp_le_Str), (str_ptr1), (str_ptr2))
int cmp_Str(const void *str_ptr1, const void *str_ptr2);

bool cmp_eq_Str_base(const void *str_base_ptr1, const void *str_base_ptr2);
#define cmp_neq_Str_base(str_base_ptr1, str_base_ptr2) (!cmp_eq_Str_base((str_base_ptr1), (str_base_ptr2)))
bool cmp_le_Str_base(const void *str_base_ptr1, const void *str_base_ptr2);
#define cmp_leq_Str_base(str_base_ptr1, str_base_ptr2) cmp_le_to_leq((cmp_le_Str_base), (str_base_ptr1), (str_base_ptr2))
#define cmp_ge_Str_base(str_base_ptr1, str_base_ptr2)  cmp_le_to_ge( (cmp_le_Str_base), (str_base_ptr1), (str_base_ptr2))
#define cmp_geq_Str_base(str_base_ptr1, str_base_ptr2) cmp_le_to_geq((cmp_le_Str_base), (str_base_ptr1), (str_base_ptr2))
int cmp_Str_base(const void *str_base_ptr1, const void *str_base_ptr2);

bool cmp_eq_Str_view(const void *str_view_ptr1, const void *str_view_ptr2);
#define cmp_neq_Str_view(str_view_ptr1, str_view_ptr2) (!cmp_eq_Str_view((str_view_ptr1), (str_view_ptr2)))
bool cmp_le_Str_view(const void *str_view_ptr1, const void *str_view_ptr2);
#define cmp_leq_Str_view(str_view_ptr1, str_view_ptr2) cmp_le_to_leq((cmp_le_Str_view), (str_view_ptr1), (str_view_ptr2))
#define cmp_ge_Str_view(str_view_ptr1, str_view_ptr2)  cmp_le_to_ge( (cmp_le_Str_view), (str_view_ptr1), (str_view_ptr2))
#define cmp_geq_Str_view(str_view_ptr1, str_view_ptr2) cmp_le_to_geq((cmp_le_Str_view), (str_view_ptr1), (str_view_ptr2))
int cmp_Str_view(const void *str_view_ptr1, const void *str_view_ptr2);

#ifdef __cplusplus
}
#endif

#endif // UTILS_CMP_H
