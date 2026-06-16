#ifndef DATA_STRUCTURE_STR_VIEW_H
#define DATA_STRUCTURE_STR_VIEW_H

#ifdef __cplusplus
extern "C"{
#endif

#include <assert.h>
#include <stdbool.h>

#include "../Utils/Num.h"

typedef struct Str_view{
    usize m_size;
    const char *m_str;
} Str_view;

Str_view str_view_init(const char *raw_str);

Str_view str_view_trim_left(Str_view sv, usize trim_size);
Str_view str_view_trim_left_while(Str_view sv, int (*pred)(int));
Str_view str_view_trim_left_while_not(Str_view sv, int (*pred)(int));
Str_view str_view_trim_left_while_in_set(Str_view sv, const char *set);
Str_view str_view_trim_left_while_not_in_set(Str_view sv, const char *set);

Str_view str_view_trim_right(Str_view sv, usize trim_size);
Str_view str_view_trim_right_while(Str_view sv, int (*pred)(int));
Str_view str_view_trim_right_while_not(Str_view sv, int (*pred)(int));
Str_view str_view_trim_right_while_in_set(Str_view sv, const char *set);
Str_view str_view_trim_right_while_not_in_set(Str_view sv, const char *set);

Str_view str_view_trim_prefix(Str_view sv, const char *prefix);
Str_view str_view_trim_suffix(Str_view sv, const char *suffix);

bool str_view_starts_with(Str_view sv, const char *prefix);
bool str_view_ends_with(Str_view sv, const char *suffix);

bool str_view_all_of(Str_view sv, int (*pred)(int));
bool str_view_any_of(Str_view sv, int (*pred)(int));
bool str_view_none_of(Str_view sv, int (*pred)(int));

#ifdef __cplusplus
}
#endif

#endif // DATA_STRUCTURE_STR_VIEW_H
