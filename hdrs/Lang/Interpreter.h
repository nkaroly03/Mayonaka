#ifndef LANG_INTERPRETER_H
#define LANG_INTERPRETER_H

#ifdef __cplusplus
extern "C"{
#endif

#include "../Allocator/Allocator.h"

#include "Bytecode_compiler.h"
#include "Primitive.h"

enum Interpreter_run_error{
    INTERPRETER_RUN_ERROR_NONE,
    INTERPRETER_RUN_ERROR_OOM,
    INTERPRETER_RUN_ERROR_RUNTIME
};

typedef struct Interpreter_run_result{
    union{
        Primitive result;
        const char *error_info;
    };
    enum Interpreter_run_error error;
} Interpreter_run_result;

Interpreter_run_result interpreter_run(Allocator alloc, U8_slice bytecode, int argc, const char *const *argv);

#ifdef __cplusplus
}
#endif

#endif // LANG_INTERPRETER_H
