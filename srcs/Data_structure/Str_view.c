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

    bool found = true;
    while (sv.m_size > 0 && found){
        found = false;
        for (const char *it = set; !found && *it; ++it)
            found = (*sv.m_str == *it);
        sv.m_size -= found;
        sv.m_str  += found;
    }

    return sv;
}
Str_view str_view_trim_left_while_not_in_set(Str_view sv, const char *set){
    assert(set && "<set> is not nullable");

    bool not_found = true;
    while (sv.m_size > 0 && not_found){
        for (const char *it = set; not_found && *it; ++it)
            not_found = (*sv.m_str != *it);
        sv.m_size -= not_found;
        sv.m_str  += not_found;
    }

    return sv;
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

    const char *itr = &sv.m_str[sv.m_size];
    bool found = true;
    while (sv.m_size > 0 && found){
        found = false;
        --itr;
        for (const char *it = set; !found && *it; ++it)
            found = (*itr == *it);
        sv.m_size -= found;
    }

    return sv;
}
Str_view str_view_trim_right_while_not_in_set(Str_view sv, const char *set){
    assert(set && "<set> is not nullable");

    const char *itr = &sv.m_str[sv.m_size];
    bool not_found = true;
    while (sv.m_size > 0 && not_found){
        --itr;
        for (const char *it = set; not_found && *it; ++it)
            not_found = (*itr != *it);
        sv.m_size -= not_found;
    }

    return sv;
}

Str_view str_view_trim_prefix(Str_view sv, const char *prefix){
    assert(prefix && "<prefix> is not nullable");

    Str_view_match_result match_result = str_view_starts_with_(sv, prefix);
    usize len = match_result.matched * match_result.len;
    sv.m_size -= len;
    sv.m_str  += len;

    return sv;
}
Str_view str_view_trim_suffix(Str_view sv, const char *suffix){
    assert(suffix && "<suffix> is not nullable");

    Str_view_match_result match_result = str_view_ends_with_(sv, suffix);
    sv.m_size -= (match_result.matched * match_result.len);

    return sv;
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

    while (sv.m_size-- > 0)
        if (!pred(*sv.m_str++))
            return false;

    return true;
}
bool str_view_any_of(Str_view sv, int (*pred)(int)){
    assert(pred && "<pred> is not nullable");

    while (sv.m_size-- > 0)
        if (pred(*sv.m_str++))
            return true;

    return false;
}
bool str_view_none_of(Str_view sv, int (*pred)(int)){
    assert(pred && "<pred> is not nullable");

    return !str_view_any_of(sv, pred);
}
