#include <assert.h>
#include <stdbool.h>
#include <string.h>

#include "../../hdrs/Data_structure/Str_view.h"
#include "../../hdrs/Utils/Num.h"
#include "../../hdrs/Utils/Utils.h"

// ------------------------------------------------------------------------------------------------

static bool logical_value(bool b){
    return b;
}
static bool not_logical_value(bool b){
    return !b;
}

static Str_view str_view_trim_left_while_(Str_view sv, int (*pred)(int), bool (*logical_value_fn)(bool)){
    while (sv.m_size > 0 && logical_value_fn(pred(*sv.m_str))){
        --sv.m_size;
        ++sv.m_str;
    }

    return sv;
}

static Str_view str_view_trim_right_while_(Str_view sv, int (*pred)(int), bool (*logical_value_fn)(bool)){
    const char *itr = &sv.m_str[sv.m_size];
    while (sv.m_size > 0 && logical_value_fn(pred(*--itr)))
        --sv.m_size;

    return sv;
}

static bool char_in_set(char c, const char *set){
    bool result = false;
    while (!result && *set)
        result = (c == *set++);

    return result;
}

static Str_view str_view_trim_left_while_set(Str_view sv, const char *set, bool (*logical_value_fn)(bool)){
    while (sv.m_size > 0 && logical_value_fn(char_in_set(*sv.m_str, set))){
        --sv.m_size;
        ++sv.m_str;
    }

    return sv;
}
static Str_view str_view_trim_right_while_set(Str_view sv, const char *set, bool (*logical_value_fn)(bool)){
    const char *itr = &sv.m_str[sv.m_size];
    while (sv.m_size > 0 && logical_value_fn(char_in_set(*--itr, set)))
        --sv.m_size;

    return sv;
}

static bool str_view_of_(Str_view sv, int (*pred)(int), bool (*logical_value_fn)(bool)){
    while (sv.m_size-- > 0)
        if (logical_value_fn(pred(*sv.m_str++)))
            return logical_value_fn(true);

    return logical_value_fn(false);
}

typedef struct Str_view_match_result{
    usize len;
    bool matched;
} Str_view_match_result;

static Str_view_match_result str_view_starts_with_(Str_view sv, const char *prefix){
    usize len = (usize)strlen(prefix);

    return (Str_view_match_result){.len = len, .matched = (sv.m_size >= len && memcmp(sv.m_str, prefix, len) == 0)};
}
static Str_view_match_result str_view_ends_with_(Str_view sv, const char *suffix){
    usize len = (usize)strlen(suffix);

    return (Str_view_match_result){.len = len, .matched = (sv.m_size >= len && memcmp(&sv.m_str[sv.m_size - len], suffix, len) == 0)};
}

// ------------------------------------------------------------------------------------------------

Str_view str_view_init(const char *raw_str){
    assert(raw_str && "<raw_str> is not nullable");

    return (Str_view){.m_size = (usize)strlen(raw_str), .m_str = raw_str};
}

Str_view str_view_trim_left(Str_view sv, usize trim_size){
    usize trim = min(sv.m_size, trim_size);
    sv.m_size -= trim;
    sv.m_str  += trim;

    return sv;
}
Str_view str_view_trim_left_while(Str_view sv, int (*pred)(int)){
    assert(pred && "<pred> is not nullable");

    return str_view_trim_left_while_(sv, pred, logical_value);
}
Str_view str_view_trim_left_while_not(Str_view sv, int (*pred)(int)){
    assert(pred && "<pred> is not nullable");

    return str_view_trim_left_while_(sv, pred, not_logical_value);
}
Str_view str_view_trim_left_while_in_set(Str_view sv, const char *set){
    assert(set && "<set> is not nullable");

    return str_view_trim_left_while_set(sv, set, logical_value);
}
Str_view str_view_trim_left_while_not_in_set(Str_view sv, const char *set){
    assert(set && "<set> is not nullable");

    return str_view_trim_left_while_set(sv, set, not_logical_value);
}

Str_view str_view_trim_right(Str_view sv, usize trim_size){
    usize trim = min(sv.m_size, trim_size);
    sv.m_size -= trim;

    return sv;
}
Str_view str_view_trim_right_while(Str_view sv, int (*pred)(int)){
    assert(pred && "<pred> is not nullable");

    return str_view_trim_right_while_(sv, pred, logical_value);
}
Str_view str_view_trim_right_while_not(Str_view sv, int (*pred)(int)){
    assert(pred && "<pred> is not nullable");

    return str_view_trim_right_while_(sv, pred, not_logical_value);
}
Str_view str_view_trim_right_while_in_set(Str_view sv, const char *set){
    assert(set && "<set> is not nullable");

    return str_view_trim_right_while_set(sv, set, logical_value);
}
Str_view str_view_trim_right_while_not_in_set(Str_view sv, const char *set){
    assert(set && "<set> is not nullable");

    return str_view_trim_right_while_set(sv, set, not_logical_value);
}

Str_view str_view_trim_prefix(Str_view sv, const char *prefix){
    assert(prefix && "<prefix> is not nullable");

    str_view_trim_prefix_in_place(&sv, prefix);

    return sv;
}
Str_view str_view_trim_suffix(Str_view sv, const char *suffix){
    assert(suffix && "<suffix> is not nullable");

    str_view_trim_suffix_in_place(&sv, suffix);

    return sv;
}
bool str_view_trim_prefix_in_place(Str_view *self, const char *prefix){
    assert(self && "<self> is never null");
    assert(prefix && "<prefix> is not nullable");

    Str_view_match_result match_result = str_view_starts_with_(*self, prefix);
    usize len = match_result.matched * match_result.len;
    self->m_size -= len;
    self->m_str  += len;

    return match_result.matched;
}
bool str_view_trim_suffix_in_place(Str_view *self, const char *suffix){
    assert(self && "<self> is never null");
    assert(suffix && "<suffix> is not nullable");
    
    Str_view_match_result match_result = str_view_ends_with_(*self, suffix);
    self->m_size -= (match_result.matched * match_result.len);

    return match_result.matched;
}

bool str_view_starts_with(Str_view sv, const char *prefix){
    assert(prefix && "<prefix> is not nullable");

    return str_view_starts_with_(sv, prefix).matched;
}
bool str_view_ends_with(Str_view sv, const char *suffix){
    assert(suffix && "<suffix> is not nullable");

    return str_view_ends_with_(sv, suffix).matched;
}

bool str_view_all_of(Str_view sv, int (*pred)(int)){
    assert(pred && "<pred> is not nullable");

    return str_view_of_(sv, pred, not_logical_value);
}
bool str_view_any_of(Str_view sv, int (*pred)(int)){
    assert(pred && "<pred> is not nullable");

    return str_view_of_(sv, pred, logical_value);
}
bool str_view_none_of(Str_view sv, int (*pred)(int)){
    assert(pred && "<pred> is not nullable");

    return !str_view_any_of(sv, pred);
}
