#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "../../hdrs/Lang/Builtin_fn.h"

enum Builtin_fn_tag builtin_fn_tag_init(const char *str){
    assert(str && "<str> is not nullable");

    enum Builtin_fn_tag bfn_tag;
    #define match_bfn_tag(bfn_tag_val) (bfn_tag = (bfn_tag_val), strcmp(str, builtin_fn_tag_to_str(bfn_tag)) == 0) \

    if (
        match_bfn_tag(BUILTIN_FN_TAG_GET_ARGV     ) ||
        match_bfn_tag(BUILTIN_FN_TAG_EXIT         ) ||
        match_bfn_tag(BUILTIN_FN_TAG_NSLEEP       ) ||
        match_bfn_tag(BUILTIN_FN_TAG_GET_ERRNO    ) ||
        match_bfn_tag(BUILTIN_FN_TAG_SET_ERRNO    ) ||
        match_bfn_tag(BUILTIN_FN_TAG_STDIN        ) ||
        match_bfn_tag(BUILTIN_FN_TAG_STDOUT       ) ||
        match_bfn_tag(BUILTIN_FN_TAG_STDERR       ) ||
        match_bfn_tag(BUILTIN_FN_TAG_PRINT        ) ||
        match_bfn_tag(BUILTIN_FN_TAG_SCAN         ) ||
        match_bfn_tag(BUILTIN_FN_TAG_POLL_KEYPRESS) ||
        match_bfn_tag(BUILTIN_FN_TAG_RAND         ) ||
        match_bfn_tag(BUILTIN_FN_TAG_LEN          ) ||
        match_bfn_tag(BUILTIN_FN_TAG_PUSH_BACK    ) ||
        match_bfn_tag(BUILTIN_FN_TAG_POP_BACK     )
    )
        return bfn_tag;

    return BUILTIN_FN_TAG_NONE;
}

const char* builtin_fn_tag_to_str(enum Builtin_fn_tag tag){
    switch (tag){
        case BUILTIN_FN_TAG_GET_ARGV:      return "get_argv";
        case BUILTIN_FN_TAG_EXIT:          return "exit";
        case BUILTIN_FN_TAG_NSLEEP:        return "nsleep";
        case BUILTIN_FN_TAG_GET_ERRNO:     return "get_errno";
        case BUILTIN_FN_TAG_SET_ERRNO:     return "set_errno";
        case BUILTIN_FN_TAG_STDIN:         return "stdin";
        case BUILTIN_FN_TAG_STDOUT:        return "stdout";
        case BUILTIN_FN_TAG_STDERR:        return "stderr";
        case BUILTIN_FN_TAG_PRINT:         return "print";
        case BUILTIN_FN_TAG_SCAN:          return "scan";
        case BUILTIN_FN_TAG_POLL_KEYPRESS: return "poll_keypress";
        case BUILTIN_FN_TAG_RAND:          return "rand";
        case BUILTIN_FN_TAG_LEN:           return "len";
        case BUILTIN_FN_TAG_PUSH_BACK:     return "push_back";
        case BUILTIN_FN_TAG_POP_BACK:      return "pop_back";
        default:                           return NULL;
    };
}
