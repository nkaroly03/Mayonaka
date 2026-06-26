#include <assert.h>

#include "../../hdrs/Allocator/Allocator.h"
#include "../../hdrs/Allocator/Arena.h"
#include "../../hdrs/Data_structure/Str_base.h"
#include "../../hdrs/Data_structure/Str_view.h"
#include "../../hdrs/Data_structure/Vec_base.h"
#include "../../hdrs/Utils/Num.h"

#include "../../hdrs/Lang/Bytecode_compiler.h"
#include "../../hdrs/Lang/IR_compiler.h"

// ------------------------------------------------------------------------------------------------

typedef struct Bytecode_compiler_state{
    Allocator alloc;
    Vec_base instruction_views;
    Vec_base bytecode;
} Bytecode_compiler_state;

// ------------------------------------------------------------------------------------------------

Bytecode_compile_result bytecode_compile(Arena *arena, const Str_base *IR){
    assert(arena && "<arena> is not nullable");
    assert(IR && "<IR> is not nullable");

    Bytecode_compiler_state state = {
        .alloc             = arena_allocator(arena),
        .instruction_views = vec_base_init(Str_view),
        .bytecode          = vec_base_init(u8)
    };

    return (Bytecode_compile_result){.bytecode = state.bytecode, .error = COMPILE_ERROR_NONE};
}
