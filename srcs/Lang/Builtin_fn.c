#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "../../hdrs/Lang/Builtin_fn.h"
#include "../../hdrs/Lang/Type_info.h"

// ------------------------------------------------------------------------------------------------

static const char *BUILTIN_FN_TAG_SYMBOLS[] = {
    [BUILTIN_FN_TAG_NONE]          = NULL,
    [BUILTIN_FN_TAG_EXIT]          = "exit",
    [BUILTIN_FN_TAG_NSLEEP]        = "nsleep",
    [BUILTIN_FN_TAG_PRINT]         = "print",
    [BUILTIN_FN_TAG_SCAN]          = "scan",
    [BUILTIN_FN_TAG_POLL_KEYPRESS] = "poll_keypress",
    [BUILTIN_FN_TAG_LEN]           = "len",
    [BUILTIN_FN_TAG_RAND]          = "rand",
    [BUILTIN_FN_TAG_PUSH_BACK]     = "push_back",
    [BUILTIN_FN_TAG_POP_BACK]      = "pop_back",
};

// ------------------------------------------------------------------------------------------------

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
    return BUILTIN_FN_TAG_SYMBOLS[tag];
}

Type_info builtin_fn_tag_return_type_info(enum Builtin_fn_tag tag){
    switch (tag){
        case BUILTIN_FN_TAG_EXIT:
        case BUILTIN_FN_TAG_NSLEEP:
        case BUILTIN_FN_TAG_PRINT:
        case BUILTIN_FN_TAG_PUSH_BACK:
        case BUILTIN_FN_TAG_POP_BACK:
            return (Type_info){.m_tag = TYPE_INFO_TAG_VOID, .m_dimensions = 0};
        case BUILTIN_FN_TAG_SCAN:
            return (Type_info){.m_tag = TYPE_INFO_TAG_STR, .m_dimensions = 0};
        case BUILTIN_FN_TAG_POLL_KEYPRESS:
            return (Type_info){.m_tag = TYPE_INFO_TAG_CHAR, .m_dimensions = 0};
        case BUILTIN_FN_TAG_LEN:
        case BUILTIN_FN_TAG_RAND:
            return (Type_info){.m_tag = TYPE_INFO_TAG_INT, .m_dimensions = 0};
        default:
            return (Type_info){0};
    }
}
bool builtin_fn_tag_is_callable(enum Builtin_fn_tag tag, Type_info_slice args){
    switch (tag){
        case BUILTIN_FN_TAG_EXIT:
        case BUILTIN_FN_TAG_PRINT:
        case BUILTIN_FN_TAG_SCAN:
            return args.m_size == 1;
        case BUILTIN_FN_TAG_NSLEEP:
            return args.m_size == 1 && args.m_data[0].m_tag != TYPE_INFO_TAG_STR && args.m_data[0].m_dimensions == 0;
        case BUILTIN_FN_TAG_LEN:
            return args.m_size == 1 && (args.m_data[0].m_tag == TYPE_INFO_TAG_STR || args.m_data[0].m_dimensions > 0);
        case BUILTIN_FN_TAG_POLL_KEYPRESS:
        case BUILTIN_FN_TAG_RAND:
            return args.m_size == 0;
        case BUILTIN_FN_TAG_PUSH_BACK:
            return args.m_size == 2 && args.m_data[0].m_tag == args.m_data[1].m_tag && args.m_data[0].m_dimensions - 1 == args.m_data[1].m_dimensions;
        case BUILTIN_FN_TAG_POP_BACK:
            return args.m_size == 1 && args.m_data[0].m_dimensions > 0;
        default:
            return false;
    }
}
