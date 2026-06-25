#include <stdbool.h>
#include <string.h>

#include "../../hdrs/Lang/Builtin_fn.h"
#include "../../hdrs/Lang/Type_info.h"

enum Builtin_fn_tag builtin_fn_tag_init(const char *str){
    if (strcmp(str, "print") == 0)
        return BUILTIN_FN_TAG_PRINT;
    if (strcmp(str, "scan") == 0)
        return BUILTIN_FN_TAG_SCAN;
    if (strcmp(str, "len") == 0)
        return BUILTIN_FN_TAG_LEN;
    if (strcmp(str, "rand") == 0)
        return BUILTIN_FN_TAG_RAND;
    if (strcmp(str, "push_back") == 0)
        return BUILTIN_FN_PUSH_BACK;
    if (strcmp(str, "pop_back") == 0)
        return BUILTIN_FN_POP_BACK;
    return BUILTIN_FN_TAG_NONE;
}

Type_info builtin_fn_return_type_info(enum Builtin_fn_tag tag){
    switch (tag){
        case BUILTIN_FN_TAG_PRINT:
        case BUILTIN_FN_PUSH_BACK:
        case BUILTIN_FN_POP_BACK:
            return (Type_info){.m_tag = TYPE_INFO_TAG_VOID, .m_dimensions = 0};
        case BUILTIN_FN_TAG_SCAN:
            return (Type_info){.m_tag = TYPE_INFO_TAG_STR, .m_dimensions = 0};
        case BUILTIN_FN_TAG_LEN:
        case BUILTIN_FN_TAG_RAND:
            return (Type_info){.m_tag = TYPE_INFO_TAG_INT, .m_dimensions = 0};
        case BUILTIN_FN_TAG_NONE:
        default:
            return (Type_info){0};
    }
}
bool builtin_fn_is_callable(enum Builtin_fn_tag tag, Type_info_slice args){
    switch (tag){
        case BUILTIN_FN_TAG_PRINT:
        case BUILTIN_FN_TAG_SCAN:
            return args.m_size == 1;
        case BUILTIN_FN_TAG_LEN:
            return args.m_size == 1 && (args.m_data[0].m_tag == TYPE_INFO_TAG_STR || (args.m_data[0].m_tag > TYPE_INFO_TAG_VOID && args.m_data[0].m_dimensions > 0));
        case BUILTIN_FN_TAG_RAND:
            return args.m_size == 0;
        case BUILTIN_FN_PUSH_BACK:
            return args.m_size == 2 && args.m_data[0].m_tag > TYPE_INFO_TAG_VOID && (
                (args.m_data[0].m_dimensions == 1 && args.m_data[1].m_dimensions == 0) ||
                (args.m_data[0].m_dimensions >  1 && args.m_data[1].m_tag == TYPE_INFO_TAG_VOID) ||
                (args.m_data[0].m_dimensions >  1 && args.m_data[0].m_dimensions - 1 == args.m_data[1].m_dimensions && args.m_data[0].m_tag == args.m_data[1].m_tag)
            );
        case BUILTIN_FN_POP_BACK:
            return args.m_size == 1 && args.m_data[0].m_tag > TYPE_INFO_TAG_VOID && args.m_data[0].m_dimensions > 0;
        case BUILTIN_FN_TAG_NONE:
        default:
            return false;
    }
}
