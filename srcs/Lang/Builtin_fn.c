#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "../../hdrs/Lang/Builtin_fn.h"
#include "../../hdrs/Lang/Type_info.h"

enum Builtin_fn_tag builtin_fn_tag_init(const char *str){
    assert(str && "<str> is not nullable");

    #define cmp_ret(bfn_tag) \
        do{ \
            if (strcmp(str, builtin_fn_tag_to_str((bfn_tag))) == 0) \
                return (bfn_tag); \
        } while (0)

    cmp_ret(BUILTIN_FN_TAG_EXIT);
    cmp_ret(BUILTIN_FN_TAG_NSLEEP);
    cmp_ret(BUILTIN_FN_TAG_PRINT);
    cmp_ret(BUILTIN_FN_TAG_SCAN);
    cmp_ret(BUILTIN_FN_TAG_POLL_KEYPRESS);
    cmp_ret(BUILTIN_FN_TAG_LEN);
    cmp_ret(BUILTIN_FN_TAG_RAND);
    cmp_ret(BUILTIN_FN_TAG_PUSH_BACK);
    cmp_ret(BUILTIN_FN_TAG_POP_BACK);

    return BUILTIN_FN_TAG_NONE;
}

const char* builtin_fn_tag_to_str(enum Builtin_fn_tag tag){
    switch (tag){
        case BUILTIN_FN_TAG_EXIT:          return "exit";
        case BUILTIN_FN_TAG_NSLEEP:        return "nsleep";
        case BUILTIN_FN_TAG_PRINT:         return "print";
        case BUILTIN_FN_TAG_SCAN:          return "scan";
        case BUILTIN_FN_TAG_POLL_KEYPRESS: return "poll_keypress";
        case BUILTIN_FN_TAG_LEN:           return "len";
        case BUILTIN_FN_TAG_RAND:          return "rand";
        case BUILTIN_FN_TAG_PUSH_BACK:     return "push_back";
        case BUILTIN_FN_TAG_POP_BACK:      return "pop_back";
        default:                           return NULL;
    };
}

Builtin_fn_tag_call_result builtin_fn_tag_call(enum Builtin_fn_tag tag, Type_info_slice args){
    switch (tag){
        case BUILTIN_FN_TAG_EXIT:
        case BUILTIN_FN_TAG_PRINT:
            return (Builtin_fn_tag_call_result){.m_return_type_info = {.m_tag = TYPE_INFO_TAG_VOID, .m_dimensions = 0}, .m_is_callable = (args.m_size == 1)};
        case BUILTIN_FN_TAG_NSLEEP:
            return (Builtin_fn_tag_call_result){
                .m_return_type_info = {.m_tag = TYPE_INFO_TAG_VOID, .m_dimensions = 0},
                .m_is_callable = (args.m_size == 1 && args.m_data[0].m_tag != TYPE_INFO_TAG_STR && args.m_data[0].m_dimensions == 0)
            };
        case BUILTIN_FN_TAG_SCAN:
            return (Builtin_fn_tag_call_result){.m_return_type_info = {.m_tag = TYPE_INFO_TAG_STR, .m_dimensions = 0}, .m_is_callable = (args.m_size == 1)};
        case BUILTIN_FN_TAG_POLL_KEYPRESS:
            return (Builtin_fn_tag_call_result){.m_return_type_info = {.m_tag = TYPE_INFO_TAG_CHAR, .m_dimensions = 0}, .m_is_callable = (args.m_size == 0)};
        case BUILTIN_FN_TAG_LEN:
            return (Builtin_fn_tag_call_result){
                .m_return_type_info = {.m_tag = TYPE_INFO_TAG_INT, .m_dimensions = 0},
                .m_is_callable = (args.m_size == 1 && (args.m_data[0].m_tag == TYPE_INFO_TAG_STR || args.m_data[0].m_dimensions > 0))
            };
        case BUILTIN_FN_TAG_RAND:
            return (Builtin_fn_tag_call_result){.m_return_type_info = {.m_tag = TYPE_INFO_TAG_INT, .m_dimensions = 0}, .m_is_callable = (args.m_size == 0)};
        case BUILTIN_FN_TAG_PUSH_BACK:
            return (Builtin_fn_tag_call_result){
                .m_return_type_info = {.m_tag = TYPE_INFO_TAG_VOID, .m_dimensions = 0},
                .m_is_callable = (args.m_size == 2 && args.m_data[0].m_tag == args.m_data[1].m_tag && args.m_data[0].m_dimensions - 1 == args.m_data[1].m_dimensions)
            };
        case BUILTIN_FN_TAG_POP_BACK:
            return (Builtin_fn_tag_call_result){
                .m_return_type_info = {.m_tag = TYPE_INFO_TAG_VOID, .m_dimensions = 0},
                .m_is_callable = (args.m_size == 1 && args.m_data[0].m_dimensions > 0)
            };
        default:
            return (Builtin_fn_tag_call_result){0};
    }
}
