#include <assert.h>
#include <wchar.h>

#include "../../hdrs/Data_structure/Str.h"
#include "../../hdrs/Data_structure/Str_base.h"
#include "../../hdrs/Data_structure/Str_view.h"
#include "../../hdrs/Utils/Hash.h"
#include "../../hdrs/Utils/Num.h"

#define basic_hash_generate(type) \
    usize hash_##type(const void *type##_ptr){ \
        assert(type##_ptr && "<ptr> is not nullable"); \
        return (usize)*(const type*)type##_ptr; \
    }

basic_hash_generate(char)
basic_hash_generate(wchar_t)

basic_hash_generate(i8)
basic_hash_generate(i16)
basic_hash_generate(i32)
basic_hash_generate(i64)

basic_hash_generate(u8)
basic_hash_generate(u16)
basic_hash_generate(u32)
basic_hash_generate(u64)

basic_hash_generate(isize)
basic_hash_generate(usize)

usize hash_Str(const void *str_ptr){
    assert(str_ptr && "<str_ptr> is not nullable");

    return hash_Str_base(&((const Str*)str_ptr)->m_base);
}
usize hash_Str_base(const void *str_base_ptr){
    assert(str_base_ptr && "<str_base_ptr> is not nullable");

    Str_view sv = str_base_to_str_view(str_base_ptr);

    return hash_Str_view(&sv);
}
usize hash_Str_view(const void *str_view_ptr){
    assert(str_view_ptr && "<str_view_ptr> is not nullable");

    usize h = 5381;
    for (Str_view sv = *(const Str_view*)str_view_ptr; sv.m_size-- > 0; ++sv.m_str)
        h = 33 * h + (usize)*sv.m_str;

    return h;
}
