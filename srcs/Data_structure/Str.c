#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>

#include "../../hdrs/Allocator/Allocator.h"
#include "../../hdrs/Data_structure/Str.h"
#include "../../hdrs/Data_structure/Str_base.h"
#include "../../hdrs/Data_structure/Str_view.h"
#include "../../hdrs/Utils/Num.h"

Str_result str_init_raw(Allocator alloc, const char *raw_str){
    assert(raw_str && "<raw_str> is not nullable");

    Str_result result = {0};

    Str_base_result str_base_result = str_base_init_raw(alloc, raw_str);
    if (str_base_result.success)
        result = (Str_result){.result = {.m_base = str_base_result.result, .m_alloc = alloc}, .success = true};

    return result;
}
Str_result str_init_raw_partial(Allocator alloc, const char *raw_str, usize size){
    assert(raw_str && "<raw_str> is not nullable");

    Str_result result = {0};

    Str_base_result str_base_result = str_base_init_raw_partial(alloc, raw_str, size);
    if (str_base_result.success)
        result = (Str_result){.result = {.m_base = str_base_result.result, .m_alloc = alloc}, .success = true};

    return result;
}
Str_result str_init_str(Allocator alloc, const Str *other){
    assert(other && "<other> is not nullable");

    Str_result result = {0};

    Str_base_result str_base_result = str_base_init_str_base(alloc, &other->m_base);
    if (str_base_result.success)
        result = (Str_result){.result = {.m_base = str_base_result.result, .m_alloc = alloc}, .success = true};

    return result;
}
Str_result str_init_str_partial(Allocator alloc, const Str *other, usize size){
    assert(other && "<other> is not nullable");

    Str_result result = {0};

    Str_base_result str_base_result = str_base_init_str_base_partial(alloc, &other->m_base, size);
    if (str_base_result.success)
        result = (Str_result){.result = {.m_base = str_base_result.result, .m_alloc = alloc}, .success = true};

    return result;
}
Str_result str_init_str_view(Allocator alloc, Str_view sv){
    Str_result result = {0};

    Str_base_result str_base_result = str_base_init_str_view(alloc, sv);
    if (str_base_result.success)
        result = (Str_result){.result = {.m_base = str_base_result.result, .m_alloc = alloc}, .success = true};

    return result;
}
Str_result str_init_str_view_partial(Allocator alloc, Str_view sv, usize size){
    Str_result result = {0};

    Str_base_result str_base_result = str_base_init_str_view_partial(alloc, sv, size);
    if (str_base_result.success)
        result = (Str_result){.result = {.m_base = str_base_result.result, .m_alloc = alloc}, .success = true};

    return result;
}
Str_result str_init_fmt(Allocator alloc, const char *fmt, ...){
    assert(fmt && "<fmt> is not nullable");

    va_list args;
    va_start(args, fmt);
    Str_result result = str_init_fmt_va_list(alloc, fmt, args);
    va_end(args);

    return result;
}
Str_result str_init_fmt_va_list(Allocator alloc, const char *fmt, va_list args){
    assert(fmt && "<fmt> is not nullable");

    Str_result result = {0};

    Str_base_result str_base_result = str_base_init_fmt_va_list(alloc, fmt, args);
    if (str_base_result.success)
        result = (Str_result){.result = {.m_base = str_base_result.result, .m_alloc = alloc}, .success = true};

    return result;
}
void str_deinit(const Str *self){
    assert(self && "<self> is never null");

    str_base_deinit(&self->m_base, self->m_alloc);
}

Str_unescape_result str_unescape_raw(Allocator alloc, const char *raw_str){
    assert(raw_str && "<raw_str> is not nullable");

    Str_base_unescape_result str_base_unescape_result = str_base_unescape_raw(alloc, raw_str);
    Str_unescape_result result = {.error = str_base_unescape_result.error};

    if (str_base_unescape_result.error == STR_UNESCAPE_ERROR_NONE)
        result.result = (Str){.m_base = str_base_unescape_result.result, .m_alloc = alloc};

    return result;
}
Str_unescape_result str_unescape_raw_partial(Allocator alloc, const char *raw_str, usize size){
    assert(raw_str && "<raw_str> is not nullable");

    Str_base_unescape_result str_base_unescape_result = str_base_unescape_raw_partial(alloc, raw_str, size);
    Str_unescape_result result = {.error = str_base_unescape_result.error};

    if (str_base_unescape_result.error == STR_UNESCAPE_ERROR_NONE)
        result.result = (Str){.m_base = str_base_unescape_result.result, .m_alloc = alloc};

    return result;
}
Str_unescape_result str_unescape_str(Allocator alloc, const Str *other){
    assert(other && "<other> is not nullable");

    Str_base_unescape_result str_base_unescape_result = str_base_unescape_str_base(alloc, &other->m_base);
    Str_unescape_result result = {.error = str_base_unescape_result.error};

    if (str_base_unescape_result.error == STR_UNESCAPE_ERROR_NONE)
        result.result = (Str){.m_base = str_base_unescape_result.result, .m_alloc = alloc};

    return result;
}
Str_unescape_result str_unescape_str_partial(Allocator alloc, const Str *other, usize size){
    assert(other && "<other> is not nullable");

    Str_base_unescape_result str_base_unescape_result = str_base_unescape_str_base_partial(alloc, &other->m_base, size);
    Str_unescape_result result = {.error = str_base_unescape_result.error};

    if (str_base_unescape_result.error == STR_UNESCAPE_ERROR_NONE)
        result.result = (Str){.m_base = str_base_unescape_result.result, .m_alloc = alloc};

    return result;
}
Str_unescape_result str_unescape_str_view(Allocator alloc, Str_view sv){
    Str_base_unescape_result str_base_unescape_result = str_base_unescape_str_view(alloc, sv);
    Str_unescape_result result = {.error = str_base_unescape_result.error};

    if (str_base_unescape_result.error == STR_UNESCAPE_ERROR_NONE)
        result.result = (Str){.m_base = str_base_unescape_result.result, .m_alloc = alloc};

    return result;
}
Str_unescape_result str_unescape_str_view_partial(Allocator alloc, Str_view sv, usize size){
    Str_base_unescape_result str_base_unescape_result = str_base_unescape_str_view_partial(alloc, sv, size);
    Str_unescape_result result = {.error = str_base_unescape_result.error};

    if (str_base_unescape_result.error == STR_UNESCAPE_ERROR_NONE)
        result.result = (Str){.m_base = str_base_unescape_result.result, .m_alloc = alloc};

    return result;
}

usize str_empty(const Str *self){
    assert(self && "<self> is never null");

    return str_base_empty(&self->m_base);
}
usize str_size(const Str *self){
    assert(self && "<self> is never null");

    return str_base_size(&self->m_base);
}
usize str_capacity(const Str *self){
    assert(self && "<self> is never null");

    return str_base_capacity(&self->m_base);
}
char* str_data(Str *self){
    assert(self && "<self> is never null");

    return str_base_data(&self->m_base);
}
const char* str_cdata(const Str *self){
    assert(self && "<self> is never null");

    return str_base_data_const(&self->m_base);
}
Str_view str_to_str_view(const Str *self){
    assert(self && "<self> is never null");

    return str_base_to_str_view(&self->m_base);
}

bool str_assign_raw(Str *self, const char *raw_str){
    assert(self && "<self> is never null");
    assert(raw_str && "<raw_str> is not nullable");

    return str_base_assign_raw(&self->m_base, self->m_alloc, raw_str);
}
bool str_assign_raw_partial(Str *self, const char *raw_str, usize size){
    assert(self && "<self> is never null");
    assert(raw_str && "<raw_str> is not nullable");

    return str_base_assign_raw_partial(&self->m_base, self->m_alloc, raw_str, size);
}
bool str_assign_str(Str *self, const Str *other){
    assert(self && "<self> is never null");
    assert(other && "<other> is not nullable");

    return str_base_assign_str_base(&self->m_base, self->m_alloc, &other->m_base);
}
bool str_assign_str_partial(Str *self, const Str *other, usize size){
    assert(self && "<self> is never null");
    assert(other && "<other> is not nullable");

    return str_base_assign_str_base_partial(&self->m_base, self->m_alloc, &other->m_base, size);
}
bool str_assign_str_view(Str *self, Str_view sv){
    assert(self && "<self> is never null");

    return str_base_assign_str_view(&self->m_base, self->m_alloc, sv);
}
bool str_assign_str_view_partial(Str *self, Str_view sv, usize size){
    assert(self && "<self> is never null");

    return str_base_assign_str_view_partial(&self->m_base, self->m_alloc, sv, size);
}
bool str_assign_fmt(Str *self, const char *fmt, ...){
    assert(fmt && "<fmt> is not nullable");

    va_list args;
    va_start(args, fmt);
    bool result = str_base_assign_fmt_va_list(&self->m_base, self->m_alloc, fmt, args);
    va_end(args);

    return result;
}
bool str_assign_fmt_va_list(Str *self, const char *fmt, va_list args){
    assert(fmt && "<fmt> is not nullable");

    return str_base_assign_fmt_va_list(&self->m_base, self->m_alloc, fmt, args);
}

enum Str_getline_error str_getline(Str *self, FILE *file){
    assert(self && "<self> is never null");
    assert(file && "<file> is not nullable");

    return str_base_getline(&self->m_base, self->m_alloc, file);
}

bool str_append_raw(Str *self, const char *raw_str){
    assert(self && "<self> is never null");
    assert(raw_str && "<raw_str> is not nullable");

    return str_base_append_raw(&self->m_base, self->m_alloc, raw_str);
}
bool str_append_raw_partial(Str *self, const char *raw_str, usize size){
    assert(self && "<self> is never null");
    assert(raw_str && "<raw_str> is not nullable");

    return str_base_append_raw_partial(&self->m_base, self->m_alloc, raw_str, size);
}
bool str_append_str(Str *self, const Str *other){
    assert(self && "<self> is never null");
    assert(other && "<other> is not nullable");

    return str_base_append_str_base(&self->m_base, self->m_alloc, &other->m_base);
}
bool str_append_str_partial(Str *self, const Str *other, usize size){
    assert(self && "<self> is never null");
    assert(other && "<other> is not nullable");

    return str_base_append_str_base_partial(&self->m_base, self->m_alloc, &other->m_base, size);
}
bool str_append_str_view(Str *self, Str_view sv){
    assert(self && "<self> is never null");

    return str_base_append_str_view(&self->m_base, self->m_alloc, sv);
}
bool str_append_str_view_partial(Str *self, Str_view sv, usize size){
    assert(self && "<self> is never null");

    return str_base_append_str_view_partial(&self->m_base, self->m_alloc, sv, size);
}
bool str_append_fmt(Str *self, const char *fmt, ...){
    assert(fmt && "<fmt> is not nullable");

    va_list args;
    va_start(args, fmt);
    bool result = str_base_append_fmt_va_list(&self->m_base, self->m_alloc, fmt, args);
    va_end(args);

    return result;
}
bool str_append_fmt_va_list(Str *self, const char *fmt, va_list args){
    assert(fmt && "<fmt> is not nullable");

    return str_base_append_fmt_va_list(&self->m_base, self->m_alloc, fmt, args);
}

bool str_push_back(Str *self, char c){
    assert(self && "<self> is never null");
    assert(c && "<c> is not allowed to be the 0 terminator");

    return str_base_push_back(&self->m_base, self->m_alloc, c);
}
char str_pop_back(Str *self){
    assert(self && "<self> is never null");

    return str_base_pop_back(&self->m_base);
}

void str_tolower(Str *self){
    assert(self && "<self> is never null");

    str_base_tolower(&self->m_base);
}
void str_toupper(Str *self){
    assert(self && "<self> is never null");

    str_base_toupper(&self->m_base);
}

void str_clear(Str *self){
    assert(self && "<self> is never null");

    str_base_clear(&self->m_base);
}
void str_reverse(Str *self){
    assert(self && "<self> is never null");

    str_base_reverse(&self->m_base);
}
void str_sort(Str *self){
    assert(self && "<self> is never null");

    str_base_sort(&self->m_base);
}

bool str_reserve(Str *self, usize reserve_size){
    assert(self && "<self> is never null");

    return str_base_reserve(&self->m_base, self->m_alloc, reserve_size);
}
bool str_shrink_to_fit(Str *self){
    assert(self && "<self> is never null");

    return str_base_shrink_to_fit(&self->m_base, self->m_alloc);
}
