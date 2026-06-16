#ifndef DATA_STRUCTURE_STR_H
#define DATA_STRUCTURE_STR_H

#ifdef __cplusplus
extern "C"{
#endif

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>

#include "../Allocator/Allocator.h"
#include "../Utils/Num.h"
#include "Str_base.h"
#include "Str_view.h"

typedef struct Str{
    Str_base m_base;
    Allocator m_alloc;
} Str;

typedef struct Str_result{
    Str result;
    bool success;
} Str_result;

Str_result str_init_raw(Allocator alloc, const char *raw_str);
Str_result str_init_raw_partial(Allocator alloc, const char *raw_str, usize size);
Str_result str_init_str(Allocator alloc, const Str *other);
Str_result str_init_str_partial(Allocator alloc, const Str *other, usize size);
Str_result str_init_str_view(Allocator alloc, Str_view sv);
Str_result str_init_str_view_partial(Allocator alloc, Str_view sv, usize size);
Str_result str_init_fmt(Allocator alloc, const char *fmt, ...);
Str_result str_init_fmt_va_list(Allocator alloc, const char *fmt, va_list args);
void str_deinit(const Str *self);

typedef struct Str_unescape_result{
    Str result;
    enum Str_unescape_error error;
} Str_unescape_result;

Str_unescape_result str_unescape_raw(Allocator alloc, const char *raw_str);
Str_unescape_result str_unescape_raw_partial(Allocator alloc, const char *raw_str, usize size);
Str_unescape_result str_unescape_str(Allocator alloc, const Str *other);
Str_unescape_result str_unescape_str_partial(Allocator alloc, const Str *other, usize size);
Str_unescape_result str_unescape_str_view(Allocator alloc, Str_view sv);
Str_unescape_result str_unescape_str_view_partial(Allocator alloc, Str_view sv, usize size);

usize str_empty(const Str *self);
usize str_size(const Str *self);
usize str_capacity(const Str *self);
char* str_data(Str *self);
const char* str_data_const(const Str *self);
Str_view str_to_str_view(const Str *self);

bool str_assign_raw(Str *self, const char *raw_str);
bool str_assign_raw_partial(Str *self, const char *raw_str, usize size);
bool str_assign_str(Str *self, const Str *other);
bool str_assign_str_partial(Str *self, const Str *other, usize size);
bool str_assign_str_view(Str *self, Str_view sv);
bool str_assign_str_view_partial(Str *self, Str_view sv, usize size);
bool str_assign_fmt(Str *self, const char *fmt, ...);
bool str_assign_fmt_va_list(Str *self, const char *fmt, va_list args);

enum Str_getline_error str_getline(Str *self, FILE *file);

bool str_append_raw(Str *self, const char *raw_str);
bool str_append_raw_partial(Str *self, const char *raw_str, usize size);
bool str_append_str(Str *self, const Str *other);
bool str_append_str_partial(Str *self, const Str *other, usize size);
bool str_append_str_view(Str *self, Str_view sv);
bool str_append_str_view_partial(Str *self, Str_view sv, usize size);
bool str_append_fmt(Str *self, const char *fmt, ...);
bool str_append_fmt_va_list(Str *self, const char *fmt, va_list args);

bool str_push_back(Str *self, char c);
char str_pop_back(Str *self);

void str_tolower(Str *self);
void str_toupper(Str *self);

void str_clear(Str *self);
void str_reverse(Str *self);
void str_sort(Str *self);

bool str_reserve(Str *self, usize reserve_size);
bool str_shrink_to_fit(Str *self);

#ifdef __cplusplus
}
#endif

#endif // DATA_STRUCTURE_STR_H
