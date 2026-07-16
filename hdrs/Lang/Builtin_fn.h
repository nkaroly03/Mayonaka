#ifndef LANG_BUILTIN_FN_H
#define LANG_BUILTIN_FN_H

#ifdef __cplusplus
extern "C"{
#endif

#include <stdbool.h>

#include "Type_info.h"

enum Builtin_fn_tag{
    BUILTIN_FN_TAG_NONE,
    BUILTIN_FN_TAG_EXIT,
    BUILTIN_FN_TAG_NSLEEP,
    BUILTIN_FN_TAG_PRINT,
    BUILTIN_FN_TAG_SCAN,
    BUILTIN_FN_TAG_POLL_KEYPRESS,
    BUILTIN_FN_TAG_LEN,
    BUILTIN_FN_TAG_RAND,
    BUILTIN_FN_TAG_PUSH_BACK,
    BUILTIN_FN_TAG_POP_BACK
};

enum Builtin_fn_tag builtin_fn_tag_init(const char *str);

const char* builtin_fn_tag_to_str(enum Builtin_fn_tag tag);

typedef struct Builtin_fn_tag_call_result{
    Type_info m_return_type;
    bool m_is_callable;
} Builtin_fn_tag_call_result;

Builtin_fn_tag_call_result builtin_fn_tag_call(enum Builtin_fn_tag tag, Type_info_slice args);

#ifdef __cplusplus
}
#endif

#endif // LANG_BUILTIN_FN_H
