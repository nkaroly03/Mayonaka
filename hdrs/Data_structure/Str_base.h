#ifndef DATA_STRUCTURE_STR_BASE_H
#define DATA_STRUCTURE_STR_BASE_H

#ifdef __cplusplus
extern "C"{
#endif

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>

#include "../Allocator/Allocator.h"
#include "../Utils/Num.h"
#include "Str_view.h"

#define STR_BASE_BUFSIZE (24 / sizeof(char))

typedef struct Str_base{
    usize m_size_info;
    union{
        char m_buffered_str[STR_BASE_BUFSIZE];
        struct{
            usize m_alloced_capacity;
            char *m_alloced_str;
        };
    };
} Str_base;

typedef struct Str_base_result{
    Str_base result;
    bool success;
} Str_base_result;

Str_base_result str_base_init_raw(Allocator alloc, const char *raw_str);
Str_base_result str_base_init_raw_partial(Allocator alloc, const char *raw_str, usize size);
Str_base_result str_base_init_str_base(Allocator alloc, const Str_base *other);
Str_base_result str_base_init_str_base_partial(Allocator alloc, const Str_base *other, usize size);
Str_base_result str_base_init_str_view(Allocator alloc, Str_view sv);
Str_base_result str_base_init_str_view_partial(Allocator alloc, Str_view sv, usize size);
Str_base_result str_base_init_fmt(Allocator alloc, const char *fmt, ...);
Str_base_result str_base_init_fmt_va_list(Allocator alloc, const char *fmt, va_list args);
void str_base_deinit(const Str_base *self, Allocator alloc);

enum Str_unescape_error{
    STR_UNESCAPE_ERROR_NONE,
    STR_UNESCAPE_ERROR_OOM,
    STR_UNESCAPE_ERROR_BAD_ESCAPE_SEQUENCE
};
typedef struct Str_base_unescape_result{
    Str_base result;
    enum Str_unescape_error error;
} Str_base_unescape_result;

Str_base_unescape_result str_base_unescape_raw(Allocator alloc, const char *raw_str);
Str_base_unescape_result str_base_unescape_raw_partial(Allocator alloc, const char *raw_str, usize size);
Str_base_unescape_result str_base_unescape_str_base(Allocator alloc, const Str_base *other);
Str_base_unescape_result str_base_unescape_str_base_partial(Allocator alloc, const Str_base *other, usize size);
Str_base_unescape_result str_base_unescape_str_view(Allocator alloc, Str_view sv);
Str_base_unescape_result str_base_unescape_str_view_partial(Allocator alloc, Str_view sv, usize size);

usize str_base_empty(const Str_base *self);
usize str_base_size(const Str_base *self);
usize str_base_capacity(const Str_base *self);
char* str_base_data(Str_base *self);
const char* str_base_data_const(const Str_base *self);
Str_view str_base_to_str_view(const Str_base *self);

bool str_base_assign_raw(Str_base *self, Allocator alloc, const char *raw_str);
bool str_base_assign_raw_partial(Str_base *self, Allocator alloc, const char *raw_str, usize size);
bool str_base_assign_str_base(Str_base *self, Allocator alloc, const Str_base *other);
bool str_base_assign_str_base_partial(Str_base *self, Allocator alloc, const Str_base *other, usize size);
bool str_base_assign_str_view(Str_base *self, Allocator alloc, Str_view sv);
bool str_base_assign_str_view_partial(Str_base *self, Allocator alloc, Str_view sv, usize size);
bool str_base_assign_fmt(Str_base *self, Allocator alloc, const char *fmt, ...);
bool str_base_assign_fmt_va_list(Str_base *self, Allocator alloc, const char *fmt, va_list args);

enum Str_getline_error{
    STR_GETLINE_ERROR_NONE,
    STR_GETLINE_ERROR_OOM,
    STR_GETLINE_ERROR_FEOF,
    STR_GETLINE_ERROR_FERROR
};

enum Str_getline_error str_base_getline(Str_base *self, Allocator alloc, FILE *file);

bool str_base_append_raw(Str_base *self, Allocator alloc, const char *raw_str);
bool str_base_append_raw_partial(Str_base *self, Allocator alloc, const char *raw_str, usize size);
bool str_base_append_str_base(Str_base *self, Allocator alloc, const Str_base *other);
bool str_base_append_str_base_partial(Str_base *self, Allocator alloc, const Str_base *other, usize size);
bool str_base_append_str_view(Str_base *self, Allocator alloc, Str_view sv);
bool str_base_append_str_view_partial(Str_base *self, Allocator alloc, Str_view sv, usize size);
bool str_base_append_fmt(Str_base *self, Allocator alloc, const char *fmt, ...);
bool str_base_append_fmt_va_list(Str_base *self, Allocator alloc, const char *fmt, va_list args);

bool str_base_push_back(Str_base *self, Allocator alloc, char c);
char str_base_pop_back(Str_base *self);

void str_base_tolower(Str_base *self);
void str_base_toupper(Str_base *self);

void str_base_clear(Str_base *self);
void str_base_reverse(Str_base *self);
void str_base_sort(Str_base *self);

bool str_base_reserve(Str_base *self, Allocator alloc, usize reserve_size);
bool str_base_shrink_to_fit(Str_base *self, Allocator alloc);

#ifdef __cplusplus
}
#endif

#endif // DATA_STRUCTURE_STR_BASE_H
