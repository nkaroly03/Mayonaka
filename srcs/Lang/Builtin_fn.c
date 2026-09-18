#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "../../hdrs/Lang/Builtin_fn.h"
#include "../../hdrs/Lang/Type_info.h"

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

Builtin_fn_tag_call_result builtin_fn_tag_call(enum Builtin_fn_tag tag, Type_info_slice args){
    switch (tag){
        case BUILTIN_FN_TAG_GET_ARGV:
            return (Builtin_fn_tag_call_result){.m_return_type_info = {.m_tag = TYPE_INFO_TAG_STR, .m_dimensions = 1}, .m_is_callable = (args.m_size == 0)};
        case BUILTIN_FN_TAG_EXIT:
        case BUILTIN_FN_TAG_NSLEEP:
        case BUILTIN_FN_TAG_SET_ERRNO:
            return (Builtin_fn_tag_call_result){
                .m_return_type_info = {.m_tag = TYPE_INFO_TAG_VOID, .m_dimensions = 0},
                .m_is_callable      = (args.m_size == 1 && args.m_data[0].m_tag == TYPE_INFO_TAG_INT && args.m_data[0].m_dimensions == 0)
            };
        case BUILTIN_FN_TAG_GET_ERRNO:
        case BUILTIN_FN_TAG_STDIN:
        case BUILTIN_FN_TAG_STDOUT:
        case BUILTIN_FN_TAG_STDERR:
        case BUILTIN_FN_TAG_RAND:
            return (Builtin_fn_tag_call_result){.m_return_type_info = {.m_tag = TYPE_INFO_TAG_INT, .m_dimensions = 0}, .m_is_callable = (args.m_size == 0)};
        case BUILTIN_FN_TAG_PRINT:
            return (Builtin_fn_tag_call_result){
                .m_return_type_info = {.m_tag = TYPE_INFO_TAG_INT, .m_dimensions = 0},
                .m_is_callable      = (
                    args.m_size == 3 &&
                    args.m_data[0].m_tag == TYPE_INFO_TAG_INT  && args.m_data[0].m_dimensions == 0 &&
                    args.m_data[1].m_tag >= TYPE_INFO_TAG_BOOL && args.m_data[1].m_tag <= TYPE_INFO_TAG_STR &&
                    args.m_data[2].m_tag == TYPE_INFO_TAG_BOOL && args.m_data[2].m_dimensions == 0
                )
            };
        case BUILTIN_FN_TAG_SCAN:
            return (Builtin_fn_tag_call_result){
                .m_return_type_info = {.m_tag = TYPE_INFO_TAG_STR, .m_dimensions = 0},
                .m_is_callable      = (args.m_size == 1 && args.m_data[0].m_tag == TYPE_INFO_TAG_INT && args.m_data[0].m_dimensions == 0)
            };
        case BUILTIN_FN_TAG_POLL_KEYPRESS:
            return (Builtin_fn_tag_call_result){.m_return_type_info = {.m_tag = TYPE_INFO_TAG_CHAR, .m_dimensions = 0}, .m_is_callable = (args.m_size == 0)};
        case BUILTIN_FN_TAG_LEN:
            return (Builtin_fn_tag_call_result){
                .m_return_type_info = {.m_tag = TYPE_INFO_TAG_INT, .m_dimensions = 0},
                .m_is_callable      = (args.m_size == 1 && (args.m_data[0].m_tag == TYPE_INFO_TAG_STR || args.m_data[0].m_dimensions > 0))
            };
        case BUILTIN_FN_TAG_PUSH_BACK:
            return (Builtin_fn_tag_call_result){
                .m_return_type_info = {.m_tag = TYPE_INFO_TAG_VOID, .m_dimensions = 0},
                .m_is_callable      = (args.m_size == 2 && args.m_data[0].m_tag == args.m_data[1].m_tag && args.m_data[0].m_dimensions - 1 == args.m_data[1].m_dimensions)
            };
        case BUILTIN_FN_TAG_POP_BACK:
            return (Builtin_fn_tag_call_result){
                .m_return_type_info = {.m_tag = TYPE_INFO_TAG_VOID, .m_dimensions = 0},
                .m_is_callable      = (args.m_size == 1 && args.m_data[0].m_dimensions > 0)
            };
        default:
            return (Builtin_fn_tag_call_result){.m_is_callable = false};
    }
}
