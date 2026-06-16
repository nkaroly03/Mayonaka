#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../hdrs/Allocator/Allocator.h"
#include "../../hdrs/Data_structure/Str_base.h"
#include "../../hdrs/Data_structure/Str_view.h"
#include "../../hdrs/Utils/Cmp.h"
#include "../../hdrs/Utils/Num.h"
#include "../../hdrs/Utils/Utils.h"

// ------------------------------------------------------------------------------------------------

typedef struct Str_base_info{
    usize capacity;
    char *str;
} Str_base_info;

static bool str_base_is_alloced(const Str_base *self){
    return self->m_size_info & USIZE_MSBIT;
}

static Str_base_info str_base_info(const Str_base *self){
    return (str_base_is_alloced(self))
        ? (Str_base_info){.capacity = self->m_alloced_capacity, .str = self->m_alloced_str}
        : (Str_base_info){.capacity = STR_BASE_BUFSIZE - 1, .str = (char*)self->m_buffered_str}
    ;
}

static void str_base_set_size(Str_base *self, usize size){
    self->m_size_info = (self->m_size_info & USIZE_MSBIT) | size;
}

static Str_base_result str_base_init(Allocator alloc, const char *raw_str, usize size){
    Str_base result = {.m_size_info = size};
    char *location = result.m_buffered_str;

    if (size >= STR_BASE_BUFSIZE){
        location = allocator_alloc(alloc, char, size + 1);
        if (!location)
            return (Str_base_result){0};

        result.m_size_info |= USIZE_MSBIT;
        result.m_alloced_capacity = size;
        result.m_alloced_str = location;
    }

    memcpy(location, raw_str, sizeof(*location) * size);
    location[size] = '\0';

    return (Str_base_result){.result = result, .success = true};
}

static Str_base_unescape_result str_base_unescape(Allocator alloc, const char *raw_str, usize size){
    Str_base result = {0};
    if (!str_base_reserve(&result, alloc, size))
        return (Str_base_unescape_result){.error = STR_UNESCAPE_ERROR_OOM};

    Str_base_info result_info = str_base_info(&result);
    char *it = result_info.str;

    bool escaped_0 = false;
    for (; size-- > 0; it += !escaped_0, ++raw_str){
        if (*raw_str != '\\')
            *it = *raw_str;
        else{
            if (size-- == 0)
                goto bad_escape_sequence_error;
            char oct_buf[4] = {0};
            char hex_buf[CHAR_BIT / 4 + 1] = {0};
            switch (*++raw_str){
                case '\'':
                case '"':
                case '?':
                case '\\':
                    *it = *raw_str;
                    break;

                case 'a': *it = '\a'; break;
                case 'b': *it = '\b'; break;
                case 'f': *it = '\f'; break;
                case 'n': *it = '\n'; break;
                case 'r': *it = '\r'; break;
                case 't': *it = '\t'; break;
                case 'v': *it = '\v'; break;

                case '0':
                case '1':
                case '2':
                case '3':
                case '4':
                case '5':
                case '6':
                case '7':
                    ++size;
                    for (usize i = 0; size > 0 && i < 3 && *raw_str >= '0' && *raw_str <= '7'; --size, ++raw_str, ++i){
                        if (i == 2 && oct_buf[0] > '3')
                            goto bad_escape_sequence_error;
                        oct_buf[i] = *raw_str;
                    }
                    *it = (char)strtol(oct_buf, NULL, 8);
                    escaped_0 = escaped_0 || !*it;
                    --raw_str;
                    break;
                case 'x':
                    if (size == 0 || !isxdigit(*++raw_str))
                        goto bad_escape_sequence_error;
                    for (usize i = 0; size > 0 && isxdigit(*raw_str); --size, ++raw_str, ++i){
                        if (i == array_size(hex_buf) - 1)
                            goto bad_escape_sequence_error;
                        hex_buf[i] = *raw_str;
                    }
                    *it = (char)strtoumax(hex_buf, NULL, 16);
                    escaped_0 = escaped_0 || !*it;
                    --raw_str;
                    break;
                default:
                    goto bad_escape_sequence_error;
            }
        }
    }

    str_base_set_size(&result, (usize)(it - result_info.str));
    *it = '\0';

    return (Str_base_unescape_result){.result = result, .error = STR_UNESCAPE_ERROR_NONE};

bad_escape_sequence_error:
    str_base_deinit(&result, alloc);

    return (Str_base_unescape_result){.error = STR_UNESCAPE_ERROR_BAD_ESCAPE_SEQUENCE};
}

static bool str_base_assign(Str_base *self, Allocator alloc, const char *raw_str, usize size){
    Str_base_info info = str_base_info(self);
    char *location = info.str;

    if (raw_str != location){
        usize cap = info.capacity;
        if (size > cap){
            usize new_cap = size * 2;
            if (cap < STR_BASE_BUFSIZE || !allocator_resize(alloc, location, cap + 1, new_cap + 1)){
                char *new_location = allocator_alloc(alloc, char, new_cap + 1);
                if (!new_location)
                    return false;

                if (cap >= STR_BASE_BUFSIZE)
                    allocator_free(alloc, location, cap + 1);

                self->m_size_info |= USIZE_MSBIT;
                self->m_alloced_str = location = new_location;
            }
            self->m_alloced_capacity = new_cap;
        }
        memcpy(location, raw_str, sizeof(*location) * size);
    }

    str_base_set_size(self, size);
    location[size] = '\0';

    return true;
}

static bool str_base_append(Str_base *self, Allocator alloc, const char *raw_str, usize size){
    Str_base_info info = str_base_info(self);
    usize cap = info.capacity;
    char *location = info.str;

    bool self_append = (location == raw_str);

    usize old_size = str_base_size(self), new_size = old_size + size;

    if (new_size > cap){
        usize new_cap = new_size * 2;
        if (cap < STR_BASE_BUFSIZE || !allocator_resize(alloc, location, cap + 1, new_cap + 1)){
            char *new_location = allocator_alloc(alloc, char, new_cap + 1);
            if (!new_location)
                return false;

            memcpy(new_location, location, sizeof(*location) * old_size);

            if (cap >= STR_BASE_BUFSIZE)
                allocator_free(alloc, location, cap + 1);

            self->m_size_info |= USIZE_MSBIT;
            self->m_alloced_str = location = new_location;
        }
        self->m_alloced_capacity = new_cap;
    }

    memcpy(&location[old_size], (self_append) ? location : raw_str, sizeof(*location) * size);
    str_base_set_size(self, new_size);
    location[new_size] = '\0';

    return true;
}

// ------------------------------------------------------------------------------------------------

Str_base_result str_base_init_raw(Allocator alloc, const char *raw_str){
    assert(raw_str && "<raw_str> is not nullable");

    return str_base_init(alloc, raw_str, (usize)strlen(raw_str));
}
Str_base_result str_base_init_raw_partial(Allocator alloc, const char *raw_str, usize size){
    assert(raw_str && "<raw_str> is not nullable");

    usize len = (usize)strlen(raw_str);

    return str_base_init(alloc, raw_str, min(len, size));
}
Str_base_result str_base_init_str_base(Allocator alloc, const Str_base *other){
    assert(other && "<other> is not nullable");

    return str_base_init(alloc, str_base_data_const(other), str_base_size(other));
}
Str_base_result str_base_init_str_base_partial(Allocator alloc, const Str_base *other, usize size){
    assert(other && "<other> is not nullable");

    usize other_size = str_base_size(other);

    return str_base_init(alloc, str_base_data_const(other), min(other_size, size));
}
Str_base_result str_base_init_str_view(Allocator alloc, Str_view sv){
    return str_base_init(alloc, sv.m_str, sv.m_size);
}
Str_base_result str_base_init_str_view_partial(Allocator alloc, Str_view sv, usize size){
    return str_base_init(alloc, sv.m_str, min(sv.m_size, size));
}
Str_base_result str_base_init_fmt(Allocator alloc, const char *fmt, ...){
    assert(fmt && "<fmt> is not nullable");

    va_list args;
    va_start(args, fmt);
    Str_base_result result = str_base_init_fmt_va_list(alloc, fmt, args);
    va_end(args);

    return result;
}
Str_base_result str_base_init_fmt_va_list(Allocator alloc, const char *fmt, va_list args){
    assert(fmt && "<fmt> is not nullable");

    va_list args_cpy;
    va_copy(args_cpy, args);
    int size = vsnprintf(NULL, 0, fmt, args_cpy);
    va_end(args_cpy);

    Str_base result = {0};
    if (!str_base_reserve(&result, alloc, (usize)size))
        return (Str_base_result){0};

    vsprintf(str_base_data(&result), fmt, args);
    str_base_set_size(&result, (usize)size);

    return (Str_base_result){.result = result, .success = true};
}
void str_base_deinit(const Str_base *self, Allocator alloc){
    assert(self && "<self> is never null");

    if (str_base_is_alloced(self))
        allocator_free(alloc, self->m_alloced_str, self->m_alloced_capacity + 1);
}

Str_base_unescape_result str_base_unescape_raw(Allocator alloc, const char *raw_str){
    assert(raw_str && "<raw_str> is not nullable");

    return str_base_unescape(alloc, raw_str, (usize)strlen(raw_str));
}
Str_base_unescape_result str_base_unescape_raw_partial(Allocator alloc, const char *raw_str, usize size){
    assert(raw_str && "<raw_str> is not nullable");

    usize len = (usize)strlen(raw_str);

    return str_base_unescape(alloc, raw_str, min(len, size));
}
Str_base_unescape_result str_base_unescape_str_base(Allocator alloc, const Str_base *other){
    assert(other && "<other> is never null");
    
    return str_base_unescape(alloc, str_base_data_const(other), str_base_size(other));
}
Str_base_unescape_result str_base_unescape_str_base_partial(Allocator alloc, const Str_base *other, usize size){
    assert(other && "<other> is never null");

    usize other_size = str_base_size(other);
    
    return str_base_unescape(alloc, str_base_data_const(other), min(other_size, size));
}
Str_base_unescape_result str_base_unescape_str_view(Allocator alloc, Str_view sv){
    return str_base_unescape(alloc, sv.m_str, sv.m_size);
}
Str_base_unescape_result str_base_unescape_str_view_partial(Allocator alloc, Str_view sv, usize size){
    return str_base_unescape(alloc, sv.m_str, min(sv.m_size, size));
}

usize str_base_empty(const Str_base *self){
    assert(self && "<self> is never null");

    return str_base_size(self) == 0;
}
usize str_base_size(const Str_base *self){
    assert(self && "<self> is never null");

    return self->m_size_info & ~USIZE_MSBIT;
}
usize str_base_capacity(const Str_base *self){
    assert(self && "<self> is never null");

    return str_base_info(self).capacity;
}
char* str_base_data(Str_base *self){
    assert(self && "<self> is never null");

    return str_base_info(self).str;
}
const char* str_base_data_const(const Str_base *self){
    assert(self && "<self> is never null");

    return str_base_info(self).str;
}
Str_view str_base_to_str_view(const Str_base *self){
    assert(self && "<self> is never null");

    return (Str_view){.m_size = str_base_size(self), .m_str = str_base_data_const(self)};
}

bool str_base_assign_raw(Str_base *self, Allocator alloc, const char *raw_str){
    assert(self && "<self> is never null");
    assert(raw_str && "<raw_str> is not nullable");

    return str_base_assign(self, alloc, raw_str, (usize)strlen(raw_str));
}
bool str_base_assign_raw_partial(Str_base *self, Allocator alloc, const char *raw_str, usize size){
    assert(self && "<self> is never null");
    assert(raw_str && "<raw_str> is not nullable");

    usize len = (usize)strlen(raw_str);

    return str_base_assign(self, alloc, raw_str, min(len, size));
}
bool str_base_assign_str_base(Str_base *self, Allocator alloc, const Str_base *other){
    assert(self && "<self> is never null");
    assert(other && "<other> is not nullable");

    return str_base_assign(self, alloc, str_base_data_const(other), str_base_size(other));
}
bool str_base_assign_str_base_partial(Str_base *self, Allocator alloc, const Str_base *other, usize size){
    assert(self && "<self> is never null");
    assert(other && "<other> is not nullable");

    usize other_size = str_base_size(other);

    return str_base_assign(self, alloc, str_base_data_const(other), min(other_size, size));
}
bool str_base_assign_str_view(Str_base *self, Allocator alloc, Str_view sv){
    assert(self && "<self> is never null");
    
    return str_base_assign(self, alloc, sv.m_str, sv.m_size);
}
bool str_base_assign_str_view_partial(Str_base *self, Allocator alloc, Str_view sv, usize size){
    assert(self && "<self> is never null");
    
    return str_base_assign(self, alloc, sv.m_str, min(sv.m_size, size));
}
bool str_base_assign_fmt(Str_base *self, Allocator alloc, const char *fmt, ...){
    assert(self && "<self> is never null");
    assert(fmt && "<fmt> is not nullable");
    
    va_list args;
    va_start(args, fmt);
    bool result = str_base_assign_fmt_va_list(self, alloc, fmt, args);
    va_end(args);

    return result;
}
bool str_base_assign_fmt_va_list(Str_base *self, Allocator alloc, const char *fmt, va_list args){
    assert(self && "<self> is never null");
    assert(fmt && "<fmt> is not nullable");

    va_list args_cpy;
    va_copy(args_cpy, args);
    int size = vsnprintf(NULL, 0, fmt, args_cpy);
    va_end(args_cpy);

    if (!str_base_reserve(self, alloc, (usize)size))
        return false;

    vsprintf(str_base_data(self), fmt, args);
    str_base_set_size(self, (usize)size);

    return true;
}

enum Str_getline_error str_base_getline(Str_base *self, Allocator alloc, FILE *file){
    assert(self && "<self> is never null");
    assert(file && "<file> is not nullable");

    char buf[BUFSIZ];

    str_base_clear(self);

    while (fgets(buf, array_size(buf), file)){
        usize newline_pos = (usize)strcspn(buf, "\n");
        bool newline_found = (buf[newline_pos] == '\n');
        buf[newline_pos] = '\0';

        if (!str_base_append_raw(self, alloc, buf))
            return STR_GETLINE_ERROR_OOM;

        if (newline_found)
            return STR_GETLINE_ERROR_NONE;
    }

    return (ferror(file)) ? STR_GETLINE_ERROR_FERROR : STR_GETLINE_ERROR_FEOF;
}

bool str_base_append_raw(Str_base *self, Allocator alloc, const char *raw_str){
    assert(self && "<self> is never null");
    assert(raw_str && "<raw_str> is not nullable");

    return str_base_append(self, alloc, raw_str, (usize)strlen(raw_str));
}
bool str_base_append_raw_partial(Str_base *self, Allocator alloc, const char *raw_str, usize size){
    assert(self && "<self> is never null");
    assert(raw_str && "<raw_str> is not nullable");

    usize len = (usize)strlen(raw_str);

    return str_base_append(self, alloc, raw_str, min(len, size));
}
bool str_base_append_str_base(Str_base *self, Allocator alloc, const Str_base *other){
    assert(self && "<self> is never null");
    assert(other && "<other> is not nullable");

    return str_base_append(self, alloc, str_base_data_const(other), str_base_size(other));
}
bool str_base_append_str_base_partial(Str_base *self, Allocator alloc, const Str_base *other, usize size){
    assert(self && "<self> is never null");
    assert(other && "<other> is not nullable");

    usize other_size = str_base_size(other);

    return str_base_append(self, alloc, str_base_data_const(other), min(other_size, size));
}
bool str_base_append_str_view(Str_base *self, Allocator alloc, Str_view sv){
    assert(self && "<self> is never null");

    return str_base_append(self, alloc, sv.m_str, sv.m_size);
}
bool str_base_append_str_view_partial(Str_base *self, Allocator alloc, Str_view sv, usize size){
    assert(self && "<self> is never null");

    return str_base_append(self, alloc, sv.m_str, min(sv.m_size, size));
}
bool str_base_append_fmt(Str_base *self, Allocator alloc, const char *fmt, ...){
    assert(self && "<self> is never null");
    assert(fmt && "<fmt> is not nullable");
    
    va_list args;
    va_start(args, fmt);
    bool result = str_base_append_fmt_va_list(self, alloc, fmt, args);
    va_end(args);

    return result;
}
bool str_base_append_fmt_va_list(Str_base *self, Allocator alloc, const char *fmt, va_list args){
    assert(self && "<self> is never null");
    assert(fmt && "<fmt> is not nullable");

    va_list args_cpy;
    va_copy(args_cpy, args);
    int size = vsnprintf(NULL, 0, fmt, args_cpy);
    va_end(args_cpy);

    usize str_size = str_base_size(self), new_size = str_size + (usize)size;
    if (!str_base_reserve(self, alloc, new_size))
        return false;

    vsprintf(&str_base_data(self)[str_size], fmt, args);
    str_base_set_size(self, new_size);

    return true;
}

bool str_base_push_back(Str_base *self, Allocator alloc, char c){
    assert(self && "<self> is never null");
    assert(c && "<c> is not allowed to be the 0 terminator");

    usize size = str_base_size(self), cap = str_base_capacity(self);

    if (size >= cap && !str_base_reserve(self, alloc, cap * 2))
        return false;

    char *data = str_base_data(self);
    data[size++] = c;
    data[size] = '\0';
    str_base_set_size(self, size);

    return true;
}
char str_base_pop_back(Str_base *self){
    assert(self && "<self> is never null");
    assert(!str_base_empty(self) && "<self> is empty");

    usize size = str_base_size(self);
    char *data = str_base_data(self);

    char c = data[--size];
    data[size] = '\0';
    str_base_set_size(self, size);

    return c;
}

void str_base_tolower(Str_base *self){
    assert(self && "<self> is never null");

    for (char *it = str_base_data(self); *it; ++it)
        *it = (char)tolower(*it);
}
void str_base_toupper(Str_base *self){
    assert(self && "<self> is never null");

    for (char *it = str_base_data(self); *it; ++it)
        *it = (char)toupper(*it);
}

void str_base_clear(Str_base *self){
    assert(self && "<self> is never null");

    str_base_set_size(self, 0);
    str_base_data(self)[0] = '\0';
}
void str_base_reverse(Str_base *self){
    assert(self && "<self> is never null");

    for (char *it = str_base_data(self), *itr = &it[str_base_size(self)]; it < --itr; ++it){
        char temp = *it;
        *it = *itr;
        *itr = temp;
    }
}
void str_base_sort(Str_base *self){
    assert(self && "<self> is never null");

    sort_elements(str_base_data(self), sizeof(*self->m_alloced_str), str_base_size(self), cmp_char);
}

bool str_base_reserve(Str_base *self, Allocator alloc, usize reserve_size){
    assert(self && "<self> is never null");

    Str_base_info info = str_base_info(self);
    usize cap = info.capacity;
    
    if (reserve_size > cap){
        char *location = info.str;
        if (cap < STR_BASE_BUFSIZE || !allocator_resize(alloc, location, cap + 1, reserve_size + 1)){
            char *new_location = allocator_alloc(alloc, char, reserve_size + 1);
            if (!new_location)
                return false;

            strcpy(new_location, location);

            if (cap >= STR_BASE_BUFSIZE)
                allocator_free(alloc, location, cap + 1);

            self->m_size_info |= USIZE_MSBIT;
            self->m_alloced_str = new_location;
        }
        self->m_alloced_capacity = reserve_size;
    }

    return true;
}
bool str_base_shrink_to_fit(Str_base *self, Allocator alloc){
    assert(self && "<self> is never null");

    Str_base_info info = str_base_info(self);
    usize cap = info.capacity;

    if (cap >= STR_BASE_BUFSIZE){
        usize size = str_base_size(self);
        if (size < cap){
            char *location = info.str;
            if (size < STR_BASE_BUFSIZE){
                strcpy(self->m_buffered_str, location);
                allocator_free(alloc, location, cap + 1);
                self->m_size_info &= ~USIZE_MSBIT;
            }
            else{
                if (!allocator_resize(alloc, location, cap + 1, size + 1)){
                    char *new_location = allocator_alloc(alloc, char, size + 1);
                    if (!new_location)
                        return false;

                    strcpy(new_location, location);
                    allocator_free(alloc, location, cap + 1);
                    self->m_alloced_str = new_location;
                }
                self->m_alloced_capacity = size;
            }
        }
    }

    return true;
}
