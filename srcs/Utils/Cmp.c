#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <wchar.h>

#include "../../hdrs/Data_structure/Str.h"
#include "../../hdrs/Data_structure/Str_base.h"
#include "../../hdrs/Data_structure/Str_view.h"
#include "../../hdrs/Utils/Cmp.h"

#define basic_cmp_generate(type) \
    bool cmp_eq_##type(const void *type##_ptr1, const void *type##_ptr2){ \
        assert(type##_ptr1 && "<ptr1> is not nullable"); \
        assert(type##_ptr2 && "<ptr2> is not nullable"); \
        return *(const type*)type##_ptr1 == *(const type*)type##_ptr2; \
    } \
    bool cmp_le_##type(const void *type##_ptr1, const void *type##_ptr2){ \
        assert(type##_ptr1 && "<ptr1> is not nullable"); \
        assert(type##_ptr2 && "<ptr2> is not nullable"); \
        return *(const type*)type##_ptr1 < *(const type*)type##_ptr2; \
    } \
    int cmp_##type(const void *type##_ptr1, const void *type##_ptr2){ \
        assert(type##_ptr1 && "<ptr1> is not nullable"); \
        assert(type##_ptr2 && "<ptr2> is not nullable"); \
        type type##_1 = *(const type*)type##_ptr1, type##_2 = *(const type*)type##_ptr2; \
        return (type##_1 > type##_2) - (type##_1 < type##_2); \
    } \

basic_cmp_generate(char)
basic_cmp_generate(wchar_t)

basic_cmp_generate(i8)
basic_cmp_generate(i16)
basic_cmp_generate(i32)
basic_cmp_generate(i64)

basic_cmp_generate(u8)
basic_cmp_generate(u16)
basic_cmp_generate(u32)
basic_cmp_generate(u64)

basic_cmp_generate(isize)
basic_cmp_generate(usize)

basic_cmp_generate(f32)
basic_cmp_generate(f64)

bool cmp_eq_Str(const void *str_ptr1, const void *str_ptr2){
    assert(str_ptr1 && "<ptr1> is not nullable");
    assert(str_ptr2 && "<ptr2> is not nullable");

    return cmp_eq_Str_base(&((const Str*)str_ptr1)->m_base, &((const Str*)str_ptr2)->m_base);
}
bool cmp_le_Str(const void *str_ptr1, const void *str_ptr2){
    assert(str_ptr1 && "<ptr1> is not nullable");
    assert(str_ptr2 && "<ptr2> is not nullable");

    return cmp_le_Str_base(&((const Str*)str_ptr1)->m_base, &((const Str*)str_ptr2)->m_base);
}
int cmp_Str(const void *str_ptr1, const void *str_ptr2){
    assert(str_ptr1 && "<ptr1> is not nullable");
    assert(str_ptr2 && "<ptr2> is not nullable");

    return cmp_Str_base(&((const Str*)str_ptr1)->m_base, &((const Str*)str_ptr2)->m_base);
}

bool cmp_eq_Str_base(const void *str_base_ptr1, const void *str_base_ptr2){
    assert(str_base_ptr1 && "<ptr1> is not nullable");
    assert(str_base_ptr2 && "<ptr2> is not nullable");

    return strcmp(str_base_data_const(str_base_ptr1), str_base_data_const(str_base_ptr2)) == 0;
}
bool cmp_le_Str_base(const void *str_base_ptr1, const void *str_base_ptr2){
    assert(str_base_ptr1 && "<ptr1> is not nullable");
    assert(str_base_ptr2 && "<ptr2> is not nullable");

    return strcmp(str_base_data_const(str_base_ptr1), str_base_data_const(str_base_ptr2)) < 0;
}
int cmp_Str_base(const void *str_base_ptr1, const void *str_base_ptr2){
    assert(str_base_ptr1 && "<ptr1> is not nullable");
    assert(str_base_ptr2 && "<ptr2> is not nullable");

    return strcmp(str_base_data_const(str_base_ptr1), str_base_data_const(str_base_ptr2));
}

bool cmp_eq_Str_view(const void *str_view_ptr1, const void *str_view_ptr2){
    assert(str_view_ptr1 && "<str_view_ptr1> is not nullable");
    assert(str_view_ptr2 && "<str_view_ptr2> is not nullable");

    Str_view sv1 = *(const Str_view*)str_view_ptr1, sv2 = *(const Str_view*)str_view_ptr2;

    return sv1.m_size == sv2.m_size && memcmp(sv1.m_str, sv2.m_str, sizeof(*sv1.m_str) * sv1.m_size) == 0;
}
bool cmp_le_Str_view(const void *str_view_ptr1, const void *str_view_ptr2){
    assert(str_view_ptr1 && "<str_view_ptr1> is not nullable");
    assert(str_view_ptr2 && "<str_view_ptr2> is not nullable");

    Str_view sv1 = *(const Str_view*)str_view_ptr1, sv2 = *(const Str_view*)str_view_ptr2;

    int result = memcmp(sv1.m_str, sv2.m_str, sizeof(*sv1.m_str) * min(sv1.m_size, sv2.m_size));

    return result < 0 || (result == 0 && sv1.m_size < sv2.m_size);
}
int cmp_Str_view(const void *str_view_ptr1, const void *str_view_ptr2){
    assert(str_view_ptr1 && "<str_view_ptr1> is not nullable");
    assert(str_view_ptr2 && "<str_view_ptr2> is not nullable");

    Str_view sv1 = *(const Str_view*)str_view_ptr1, sv2 = *(const Str_view*)str_view_ptr2;

    usize min_size = min(sv1.m_size, sv2.m_size);
    int result = memcmp(sv1.m_str, sv2.m_str, sizeof(*sv1.m_str) * min_size);

    return result + ((result == 0) * ((sv1.m_size > sv2.m_size) - (sv1.m_size < sv2.m_size)));
}
