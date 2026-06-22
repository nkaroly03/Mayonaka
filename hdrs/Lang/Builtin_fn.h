#ifndef LANG_BUILTIN_FN_H
#define LANG_BUILTIN_FN_H

#ifdef __cplusplus
extern "C"{
#endif

#include <stdbool.h>

#include "Type_info.h"

enum Builtin_fn_tag{
    BUILTIN_FN_TAG_NONE,
    BUILTIN_FN_TAG_PRINT,
    BUILTIN_FN_TAG_SCAN,
    BUILTIN_FN_TAG_LEN,
    BUILTIN_FN_TAG_RAND
};

enum Builtin_fn_tag builtin_fn_tag_init(const char *str);

Type_info builtin_fn_return_type_info(enum Builtin_fn_tag tag);
bool builtin_fn_is_callable(enum Builtin_fn_tag tag, Type_info_slice args);

#ifdef __cplusplus
}
#endif

#endif // LANG_BUILTIN_FN_H
