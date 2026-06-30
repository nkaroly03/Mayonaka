#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../hdrs/Allocator/Allocator.h"
#include "../../hdrs/Data_structure/Str_base.h"
#include "../../hdrs/Data_structure/Vec_base.h"
#include "../../hdrs/Utils/Num.h"
#include "../../hdrs/Utils/Utils.h"

#include "../../hdrs/Lang/Builtin_fn.h"
#include "../../hdrs/Lang/Bytecode_compiler.h"
#include "../../hdrs/Lang/IR_compiler.h"
#include "../../hdrs/Lang/Interpreter.h"
#include "../../hdrs/Lang/Primitive.h"

// ------------------------------------------------------------------------------------------------

static const Interpreter_run_result OOM_ERROR = {.error_info = "Out of memory\n", .error = INTERPRETER_RUN_ERROR_OOM};

typedef struct Interpreter_state{
    Allocator alloc;
    Primitive argv;
    U8_slice bytecode;
    usize pc;
    Vec_base stack;
} Interpreter_state;

static void interpreter_state_deinit(Interpreter_state *self){
    for (usize i = 0; i < self->stack.m_size; ++i)
        primitive_deinit(vec_base_at(&self->stack, i), self->alloc);
    vec_base_deinit(&self->stack, self->alloc);
    primitive_deinit(&self->argv, self->alloc);
}

static Primitive_op_result interpreter_state_call_un_op_fn(Interpreter_state *self, enum Op_code op_code){
    Primitive *last = vec_base_at(&self->stack, self->stack.m_size - 1);
    switch (op_code){
        case OP_CODE_TO_BOOL:  return primitive_to_bool (last, self->alloc);
        case OP_CODE_TO_CHAR:  return primitive_to_char (last, self->alloc);
        case OP_CODE_TO_INT:   return primitive_to_int  (last, self->alloc);
        case OP_CODE_TO_FLOAT: return primitive_to_float(last, self->alloc);
        case OP_CODE_TO_STR:   return primitive_to_str  (last, self->alloc);

        case OP_CODE_NEG:  return primitive_neg (last);
        case OP_CODE_BNEG: return primitive_bneg(last);

        default: unreachable();
    }
}

static Primitive_op_result interpreter_state_call_bin_op_fn(Interpreter_state *self, enum Op_code op_code){
    Primitive *lhs = vec_base_at(&self->stack, self->stack.m_size - 2);
    Primitive *rhs = vec_base_at(&self->stack, self->stack.m_size - 1);
    switch (op_code){
        case OP_CODE_DEREF:   return primitive_deref  (lhs, self->alloc, rhs);

        case OP_CODE_CMP_EQ:  return primitive_cmp_eq (lhs, self->alloc, rhs);
        case OP_CODE_CMP_NEQ: return primitive_cmp_neq(lhs, self->alloc, rhs);
        case OP_CODE_CMP_LE:  return primitive_cmp_le (lhs, self->alloc, rhs);
        case OP_CODE_CMP_LEQ: return primitive_cmp_leq(lhs, self->alloc, rhs);
        case OP_CODE_CMP_GE:  return primitive_cmp_ge (lhs, self->alloc, rhs);
        case OP_CODE_CMP_GEQ: return primitive_cmp_geq(lhs, self->alloc, rhs);

        case OP_CODE_ADD:     return primitive_add    (lhs, self->alloc, rhs);
        
        case OP_CODE_SUB:     return primitive_sub (lhs, rhs);
        case OP_CODE_MUL:     return primitive_mul (lhs, rhs);
        case OP_CODE_DIV:     return primitive_div (lhs, rhs);
        case OP_CODE_MOD:     return primitive_mod (lhs, rhs);
        case OP_CODE_POW:     return primitive_pow (lhs, rhs);

        case OP_CODE_SHL:     return primitive_shl (lhs, rhs);
        case OP_CODE_SHR:     return primitive_shr (lhs, rhs);
        case OP_CODE_BAND:    return primitive_band(lhs, rhs);
        case OP_CODE_BOR:     return primitive_bor (lhs, rhs);
        case OP_CODE_XOR:     return primitive_xor (lhs, rhs);

        default: unreachable();
    }
}

static Interpreter_run_result interpreter_state_run(Interpreter_state *self){
    Primitive_op_result op_result;

    while (self->pc < self->bytecode.m_size){
        enum Op_code op_code = (enum Op_code)self->bytecode.m_data[self->pc++];
        switch (op_code){
            case OP_CODE_PUSH:
                switch ((enum Op_code_push_tag)self->bytecode.m_data[self->pc++]){
                    case OP_CODE_PUSH_TAG_SP:
                        {
                            usize offset;
                            memcpy(&offset, &self->bytecode.m_data[self->pc], sizeof(offset));
                            self->pc += sizeof(offset);

                            Primitive *pushed = vec_base_at(&self->stack, self->stack.m_size - offset);
                            switch (pushed->m_tag){
                                case PRIMITIVE_TAG_BOOL:
                                case PRIMITIVE_TAG_CHAR:
                                case PRIMITIVE_TAG_INT:
                                case PRIMITIVE_TAG_FLOAT:
                                    if (!vec_base_push_back(&self->stack, self->alloc, pushed))
                                        goto oom_error;
                                    break;
                                case PRIMITIVE_TAG_STR:
                                    if (!vec_base_push_back(&self->stack, self->alloc, pushed))
                                        goto oom_error;
                                    ++pushed->m_str_data_ptr->m_ref_count;
                                    break;
                                case PRIMITIVE_TAG_LIST:
                                    if (!vec_base_push_back(&self->stack, self->alloc, pushed))
                                        goto oom_error;
                                    ++pushed->m_list_data_ptr->m_ref_count;
                                    break;
                            }
                        }
                        break;
                    case OP_CODE_PUSH_TAG_ARGV:
                        if (!vec_base_push_back(&self->stack, self->alloc, &self->argv))
                            goto oom_error;
                        ++self->argv.m_list_data_ptr->m_ref_count;
                        break;
                    case OP_CODE_PUSH_TAG_BOOL:
                        if (!vec_base_push_back(&self->stack, self->alloc, &(Primitive){.m_tag = PRIMITIVE_TAG_BOOL, .m_bool_data = (bool)self->bytecode.m_data[self->pc++]}))
                            goto oom_error;
                        break;
                    case OP_CODE_PUSH_TAG_CHAR:
                        if (!vec_base_push_back(&self->stack, self->alloc, &(Primitive){.m_tag = PRIMITIVE_TAG_CHAR, .m_char_data = (u8)self->bytecode.m_data[self->pc++]}))
                            goto oom_error;
                        break;
                    case OP_CODE_PUSH_TAG_INT:
                        if (!vec_base_push_back(
                            &self->stack,
                            self->alloc,
                            &(Primitive){.m_tag = PRIMITIVE_TAG_INT, .m_int_data = *(i64*)memcpy(&(i64){0}, &self->bytecode.m_data[self->pc], sizeof(i64))}
                        ))
                            goto oom_error;
                        self->pc += sizeof(i64);
                        break;
                    case OP_CODE_PUSH_TAG_FLOAT:
                        if (!vec_base_push_back(
                            &self->stack,
                            self->alloc,
                            &(Primitive){.m_tag = PRIMITIVE_TAG_FLOAT, .m_float_data = *(f64*)memcpy(&(f64){0}, &self->bytecode.m_data[self->pc], sizeof(f64))}
                        ))
                            goto oom_error;
                        self->pc += sizeof(f64);
                        break;
                    case OP_CODE_PUSH_TAG_STR:
                        {
                            Primitive temp = {.m_tag = PRIMITIVE_TAG_STR, .m_str_data_ptr = allocator_alloc(self->alloc, Primitive_str_data, 1)};
                            if (!temp.m_str_data_ptr)
                                goto oom_error;
                            *temp.m_str_data_ptr = (Primitive_str_data){.m_ref_count = 1, .m_data = {0}};
                            if (
                                !str_base_assign_raw(&temp.m_str_data_ptr->m_data, self->alloc, (const char*)&self->bytecode.m_data[self->pc]) ||
                                !vec_base_push_back(&self->stack, self->alloc, &temp)
                            ){
                                primitive_deinit(&temp, self->alloc);
                                goto oom_error;
                            }
                            self->pc += (str_base_size(&temp.m_str_data_ptr->m_data) + 1);
                        }
                        break;
                    case OP_CODE_PUSH_TAG_LIST:
                        {
                            Primitive temp = {.m_tag = PRIMITIVE_TAG_LIST, .m_list_data_ptr = allocator_alloc(self->alloc, Primitive_list_data, 1)};
                            if (!temp.m_list_data_ptr)
                                goto oom_error;
                            *temp.m_list_data_ptr = (Primitive_list_data){.m_ref_count = 1, .m_data = vec_base_init(Primitive)};
                            if (!vec_base_push_back(&self->stack, self->alloc, &temp)){
                                primitive_deinit(&temp, self->alloc);
                                goto oom_error;
                            }
                        }
                        break;
                }
                break;
            case OP_CODE_POP:
                {
                    Primitive popped;
                    vec_base_pop_back_to(&self->stack, &popped);
                    primitive_deinit(&popped, self->alloc);
                }
                break;

            case OP_CODE_CALL:
                switch ((enum Builtin_fn_tag)self->bytecode.m_data[self->pc++]){
                    case BUILTIN_FN_TAG_NONE:
                        fprintf(stderr, "User defined functions are not implemented\n");
                        abort();
                    case BUILTIN_FN_TAG_EXIT:
                        {
                            Primitive last;
                            vec_base_pop_back_to(&self->stack, &last);
                            interpreter_state_deinit(self);
                            return (Interpreter_run_result){.result = last, .error = INTERPRETER_RUN_ERROR_NONE};
                        }
                    case BUILTIN_FN_TAG_PRINT:
                        {
                            Primitive last;
                            vec_base_pop_back_to(&self->stack, &last);
                            primitive_print(&last);
                            primitive_deinit(&last, self->alloc);
                        }
                        break;
                    case BUILTIN_FN_TAG_SCAN:
                        {
                            Primitive last;
                            vec_base_pop_back_to(&self->stack, &last);

                            primitive_print(&last);
                            primitive_deinit(&last, self->alloc);

                            Str_base str_result = {0};
                            enum Str_getline_error getline_result = str_base_getline(&str_result, self->alloc, stdin);
                            switch (getline_result){
                                case STR_GETLINE_ERROR_NONE:
                                    break;
                                case STR_GETLINE_ERROR_OOM:
                                    str_base_deinit(&str_result, self->alloc);
                                    goto oom_error;
                                case STR_GETLINE_ERROR_FEOF:
                                    str_base_deinit(&str_result, self->alloc);
                                    interpreter_state_deinit(self);
                                    return (Interpreter_run_result){.error_info = "<feof> on stdin", .error = INTERPRETER_RUN_ERROR_RUNTIME};
                                case STR_GETLINE_ERROR_FERROR:
                                    str_base_deinit(&str_result, self->alloc);
                                    interpreter_state_deinit(self);
                                    return (Interpreter_run_result){.error_info = "<ferror> on stdin", .error = INTERPRETER_RUN_ERROR_RUNTIME};
                            }
                            Primitive scanned = {.m_tag = PRIMITIVE_TAG_STR, .m_str_data_ptr = allocator_alloc(self->alloc, Primitive_str_data, 1)};
                            if (!scanned.m_str_data_ptr){
                                str_base_deinit(&str_result, self->alloc);
                                goto oom_error;
                            }
                            *scanned.m_str_data_ptr = (Primitive_str_data){.m_ref_count = 1, .m_data = str_result};
                            op_result = primitive_mov(vec_base_at(&self->stack, self->stack.m_size - 1), self->alloc, &scanned);
                            primitive_deinit(&scanned, self->alloc);
                            if (op_result.error != PRIMITIVE_OP_ERROR_NONE)
                                goto primitive_op_error;
                        }
                        break;
                    case BUILTIN_FN_TAG_LEN:
                        {
                            Primitive *last = vec_base_at(&self->stack, self->stack.m_size - 1);
                            Primitive len = {.m_tag = PRIMITIVE_TAG_INT};
                            switch (last->m_tag){
                                case PRIMITIVE_TAG_BOOL:
                                case PRIMITIVE_TAG_CHAR:
                                case PRIMITIVE_TAG_INT:
                                case PRIMITIVE_TAG_FLOAT:
                                    interpreter_state_deinit(self);
                                    return (Interpreter_run_result){.error_info = "Trying to get the length of a numeric type", .error = INTERPRETER_RUN_ERROR_RUNTIME};
                                case PRIMITIVE_TAG_STR:
                                    len.m_int_data = (i64)str_base_size(&last->m_str_data_ptr->m_data);
                                    break;
                                case PRIMITIVE_TAG_LIST:
                                    len.m_int_data = (i64)last->m_list_data_ptr->m_data.m_size;
                                    break;
                            }
                            op_result = primitive_mov(vec_base_at(&self->stack, self->stack.m_size - 2), self->alloc, &len);
                            if (op_result.error != PRIMITIVE_OP_ERROR_NONE)
                                goto primitive_op_error;
                            primitive_deinit(last, self->alloc);
                            vec_base_pop_back_discard(&self->stack);
                        }
                        break;
                    case BUILTIN_FN_TAG_RAND:
                        fprintf(stderr, "Not implemented");
                        abort();
                    case BUILTIN_FN_TAG_PUSH_BACK:
                        {
                            Primitive *list = vec_base_at(&self->stack, self->stack.m_size - 2);
                            Primitive *last = vec_base_at(&self->stack, self->stack.m_size - 1);

                            if (list->m_tag != PRIMITIVE_TAG_LIST){
                                interpreter_state_deinit(self);
                                return (Interpreter_run_result){.error_info = "<push_back> called on non-list type", .error = INTERPRETER_RUN_ERROR_RUNTIME};
                            }
                            if (!vec_base_push_back(&list->m_list_data_ptr->m_data, self->alloc, last))
                                goto oom_error;

                            primitive_deinit(last, self->alloc);
                            primitive_deinit(list, self->alloc);
                            vec_base_pop_back_discard(&self->stack);
                            vec_base_pop_back_discard(&self->stack);
                        }
                        break;
                    case BUILTIN_FN_TAG_POP_BACK:
                        {
                            Primitive *list = vec_base_at(&self->stack, self->stack.m_size - 1);
                            if (list->m_tag != PRIMITIVE_TAG_LIST){
                                interpreter_state_deinit(self);
                                return (Interpreter_run_result){.error_info = "<pop_back> called on non-list type", .error = INTERPRETER_RUN_ERROR_RUNTIME};
                            }

                            Primitive popped;
                            vec_base_pop_back_to(&list->m_list_data_ptr->m_data, &popped);
                            primitive_deinit(&popped, self->alloc);

                            primitive_deinit(list, self->alloc);
                            vec_base_pop_back_discard(&self->stack);
                        }
                        break;
                }
                break;
            case OP_CODE_RET:
                fprintf(stderr, "Not implemented\n");
                abort();

            case OP_CODE_JMP:
                self->pc = *(usize*)memcpy(&(usize){0}, &self->bytecode.m_data[self->pc], sizeof(usize));
                break;
            case OP_CODE_JMPZ:
                {
                    Primitive *last = vec_base_at(&self->stack, self->stack.m_size - 1);

                    op_result = primitive_to_bool(last, self->alloc);
                    if (op_result.error != PRIMITIVE_OP_ERROR_NONE)
                        goto primitive_op_error;

                    bool val = last->m_bool_data;
                    primitive_deinit(last, self->alloc);
                    vec_base_pop_back_discard(&self->stack);

                    usize offset;
                    memcpy(&offset, &self->bytecode.m_data[self->pc], sizeof(offset));
                    self->pc += sizeof(offset);
                    if (!val)
                        self->pc = offset;
                }
                break;

            case OP_CODE_TO_BOOL:
            case OP_CODE_TO_CHAR:
            case OP_CODE_TO_INT:
            case OP_CODE_TO_FLOAT:
            case OP_CODE_TO_STR:
            case OP_CODE_NEG:
            case OP_CODE_BNEG:
                op_result = interpreter_state_call_un_op_fn(self, op_code);
                if (op_result.error != PRIMITIVE_OP_ERROR_NONE)
                    goto primitive_op_error;
                break;

            case OP_CODE_MOV:
                {
                    usize offset;
                    memcpy(&offset, &self->bytecode.m_data[self->pc], sizeof(offset));
                    self->pc += sizeof(offset);

                    Primitive *lhs = vec_base_at(&self->stack, self->stack.m_size - offset);
                    Primitive *rhs = vec_base_at(&self->stack, self->stack.m_size - 1);

                    op_result = primitive_mov(lhs, self->alloc, rhs);
                    if (op_result.error != PRIMITIVE_OP_ERROR_NONE)
                        goto primitive_op_error;

                    primitive_deinit(rhs, self->alloc);
                    vec_base_pop_back_discard(&self->stack);
                }
                break;
            case OP_CODE_MOV_DEREF:
                {
                    Primitive *lhs = vec_base_at(&self->stack, self->stack.m_size - 3);
                    Primitive *idx = vec_base_at(&self->stack, self->stack.m_size - 2);
                    Primitive *rhs = vec_base_at(&self->stack, self->stack.m_size - 1);
                    op_result = primitive_mov_deref(lhs, self->alloc, idx, rhs);
                    if (op_result.error != PRIMITIVE_OP_ERROR_NONE)
                        goto primitive_op_error;
                    primitive_deinit(rhs, self->alloc);
                    primitive_deinit(idx, self->alloc);
                    primitive_deinit(lhs, self->alloc);
                    vec_base_pop_back_discard(&self->stack);
                    vec_base_pop_back_discard(&self->stack);
                    vec_base_pop_back_discard(&self->stack);
                }
                break;

            case OP_CODE_DEREF:
            case OP_CODE_CMP_EQ:
            case OP_CODE_CMP_NEQ:
            case OP_CODE_CMP_LE:
            case OP_CODE_CMP_LEQ:
            case OP_CODE_CMP_GE:
            case OP_CODE_CMP_GEQ:
            case OP_CODE_ADD:
            case OP_CODE_SUB:
            case OP_CODE_MUL:
            case OP_CODE_DIV:
            case OP_CODE_MOD:
            case OP_CODE_POW:
            case OP_CODE_SHL:
            case OP_CODE_SHR:
            case OP_CODE_BAND:
            case OP_CODE_BOR:
            case OP_CODE_XOR:
                op_result = interpreter_state_call_bin_op_fn(self, op_code);
                if (op_result.error != PRIMITIVE_OP_ERROR_NONE)
                    goto primitive_op_error;
                primitive_deinit(vec_base_at(&self->stack, self->stack.m_size - 1), self->alloc);
                vec_base_pop_back_discard(&self->stack);
                break;
        }
    }

primitive_op_error:
    interpreter_state_deinit(self);
    return (Interpreter_run_result){.error_info = op_result.error_msg, .error = (enum Interpreter_run_error)op_result.error};
oom_error:
    interpreter_state_deinit(self);
    return OOM_ERROR;
}

// ------------------------------------------------------------------------------------------------

Interpreter_run_result interpreter_run(Allocator alloc, U8_slice bytecode, int argc, const char *const *argv){
    Interpreter_state state = {
        .alloc    = alloc,
        .argv     = {.m_tag = PRIMITIVE_TAG_LIST, .m_list_data_ptr = allocator_alloc(alloc, Primitive_list_data, 1)},
        .bytecode = bytecode,
        .pc       = 0,
        .stack    = vec_base_init(Primitive),
    };

    if (!state.argv.m_list_data_ptr)
        return OOM_ERROR;

    *state.argv.m_list_data_ptr = (Primitive_list_data){.m_ref_count = 1, .m_data = vec_base_init(Primitive)};

    for (int i = 0; i < argc; ++i){
        Primitive temp = {.m_tag = PRIMITIVE_TAG_STR, .m_str_data_ptr = allocator_alloc(state.alloc, Primitive_str_data, 1)};
        if (!temp.m_str_data_ptr){
            interpreter_state_deinit(&state);
            return OOM_ERROR;
        }
        *temp.m_str_data_ptr = (Primitive_str_data){.m_ref_count = 1, .m_data = {0}};
        if (!str_base_assign_raw(&temp.m_str_data_ptr->m_data, state.alloc, argv[i]) || !vec_base_push_back(&state.argv.m_list_data_ptr->m_data, state.alloc, &temp)){
            str_base_deinit(&temp.m_str_data_ptr->m_data, state.alloc);
            allocator_free(state.alloc, temp.m_str_data_ptr, 1);
            interpreter_state_deinit(&state);
            return OOM_ERROR;
        }
    }

    return interpreter_state_run(&state);
}
