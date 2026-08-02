#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <conio.h>
#else
#include <termios.h>
#include <unistd.h>
#endif // _WIN32

#include "../../hdrs/Allocator/Allocator.h"
#include "../../hdrs/Data_structure/Str_base.h"
#include "../../hdrs/Random/Xoshiro256.h"
#include "../../hdrs/Data_structure/Vec_base.h"
#include "../../hdrs/Utils/Num.h"
#include "../../hdrs/Utils/Utils.h"

#include "../../hdrs/Lang/Builtin_fn.h"
#include "../../hdrs/Lang/Bytecode_compiler.h"
#include "../../hdrs/Lang/IR_compiler.h"
#include "../../hdrs/Lang/Interpreter.h"
#include "../../hdrs/Lang/Primitive.h"

// ------------------------------------------------------------------------------------------------

typedef struct Interpreter_state{
    Allocator alloc;
    Xoshiro256 rand;
    Primitive argv;
    U8_slice bytecode;
    usize pc;
    Vec_base data_stack;
    Vec_base return_address_stack;
} Interpreter_state;

static void interpreter_state_deinit(Interpreter_state *self){
    vec_base_deinit(&self->return_address_stack, self->alloc);
    while (self->data_stack.m_size > 0){
        Primitive popped;
        vec_base_pop_back_to(&self->data_stack, &popped);
        primitive_deinit(&popped, self->alloc);
    }
    vec_base_deinit(&self->data_stack, self->alloc);
    primitive_deinit(&self->argv, self->alloc);
}

static Interpreter_run_result interpreter_state_oom_error(Interpreter_state *self){
    interpreter_state_deinit(self);
    return (Interpreter_run_result){.error = INTERPRETER_RUN_ERROR_OOM};
}
#define oom_error() interpreter_state_oom_error(self)

static Interpreter_run_result interpreter_state_runtime_error(Interpreter_state *self, const char *fmt, ...){
    va_list args;
    va_start(args, fmt);
    Str_base_result error_info = str_base_init_fmt_va_list(self->alloc, fmt, args);
    va_end(args);
    if (!error_info.success)
        return oom_error();
    interpreter_state_deinit(self);
    return (Interpreter_run_result){.error_info = error_info.result, .error = INTERPRETER_RUN_ERROR_RUNTIME};
}
#define runtime_error(...) interpreter_state_runtime_error(self, __VA_ARGS__)

static Interpreter_run_result interpreter_state_bad_instruction_error(Interpreter_state *self, usize instruction_idx){
    return runtime_error("Bad <%s> instruction at idx <" USIZE_PFMT ">", op_code_to_str((enum Op_code)self->bytecode.m_data[instruction_idx]), instruction_idx);
}
#define bad_instruction_error(instruction_idx) interpreter_state_bad_instruction_error(self, (instruction_idx))

static Interpreter_run_result interpreter_state_builtin_fn_arg_count_error(Interpreter_state *self, enum Builtin_fn_tag bfn_tag){
    return runtime_error("Not enough arguments for builtin function <%s>", builtin_fn_tag_to_str(bfn_tag));
}
#define builtin_fn_arg_count_error(bfn_tag) interpreter_state_builtin_fn_arg_count_error(self, (bfn_tag))

static Interpreter_run_result interpreter_state_primitive_op_error(Interpreter_state *self, Primitive_op_result op_result){
    return (op_result.error == PRIMITIVE_OP_ERROR_OOM) ? oom_error() : runtime_error("%s", op_result.error_info);
}
#define primitive_op_error(op_result) interpreter_state_primitive_op_error(self, (op_result))

static Interpreter_run_result interpreter_state_run(Interpreter_state *self){
    while (self->pc < self->bytecode.m_size){
        Primitive_op_result op_result;

        usize instruction_idx = self->pc;

        enum Op_code op_code = (enum Op_code)self->bytecode.m_data[self->pc++];
        const char *op_code_str = op_code_to_str(op_code);

        switch (op_code){
            case OP_CODE_PUSH:{
                if (self->pc >= self->bytecode.m_size)
                    return bad_instruction_error(instruction_idx);
                Primitive temp;
                switch ((enum Op_code_push_tag)self->bytecode.m_data[self->pc++]){
                    case OP_CODE_PUSH_TAG_SP:{
                        usize sp_offset;
                        if (self->pc + sizeof(sp_offset) > self->bytecode.m_size)
                            return bad_instruction_error(instruction_idx);
                        memcpy(&sp_offset, &self->bytecode.m_data[self->pc], sizeof(sp_offset));
                        self->pc += sizeof(sp_offset);

                        usize offset = self->data_stack.m_size - sp_offset;
                        if (offset >= self->data_stack.m_size)
                            return runtime_error("<%s> instruction references an out of range element on the stack", op_code_str);

                        temp = *(Primitive*)vec_base_at(&self->data_stack, offset);
                        if (!vec_base_push_back(&self->data_stack, self->alloc, &temp))
                            return oom_error();

                        switch (temp.m_tag){
                            case PRIMITIVE_TAG_BOOL:
                            case PRIMITIVE_TAG_CHAR:
                            case PRIMITIVE_TAG_INT:
                            case PRIMITIVE_TAG_FLOAT:
                                break;
                            case PRIMITIVE_TAG_STR:
                                ++temp.m_str_data_ptr->m_ref_count;
                                break;
                            case PRIMITIVE_TAG_LIST:
                                ++temp.m_list_data_ptr->m_ref_count;
                                break;
                        }
                        break;
                    }
                    case OP_CODE_PUSH_TAG_ARGV:
                        if (!vec_base_push_back(&self->data_stack, self->alloc, &self->argv))
                            return oom_error();
                        ++self->argv.m_list_data_ptr->m_ref_count;
                        break;
                    case OP_CODE_PUSH_TAG_BOOL:
                        if (!vec_base_push_back(&self->data_stack, self->alloc, &(Primitive){.m_tag = PRIMITIVE_TAG_BOOL, .m_bool_data = (bool)self->bytecode.m_data[self->pc++]}))
                            return oom_error();
                        break;
                    case OP_CODE_PUSH_TAG_CHAR:
                        if (!vec_base_push_back(&self->data_stack, self->alloc, &(Primitive){.m_tag = PRIMITIVE_TAG_CHAR, .m_char_data = self->bytecode.m_data[self->pc++]}))
                            return oom_error();
                        break;
                    case OP_CODE_PUSH_TAG_INT:{
                        i64 i64_data;
                        if (self->pc + sizeof(i64_data) > self->bytecode.m_size)
                            return bad_instruction_error(instruction_idx);
                        memcpy(&i64_data, &self->bytecode.m_data[self->pc], sizeof(i64_data));
                        if (!vec_base_push_back(&self->data_stack, self->alloc, &(Primitive){.m_tag = PRIMITIVE_TAG_INT, .m_int_data = i64_data}))
                            return oom_error();
                        self->pc += sizeof(i64_data);
                        break;
                    }
                    case OP_CODE_PUSH_TAG_FLOAT:{
                        f64 f64_data;
                        if (self->pc + sizeof(f64_data) > self->bytecode.m_size)
                            return bad_instruction_error(instruction_idx);
                        memcpy(&f64_data, &self->bytecode.m_data[self->pc], sizeof(f64_data));
                        if (!vec_base_push_back(&self->data_stack, self->alloc, &(Primitive){.m_tag = PRIMITIVE_TAG_FLOAT, .m_float_data = f64_data}))
                            return oom_error();
                        self->pc += sizeof(f64_data);
                        break;
                    }
                    case OP_CODE_PUSH_TAG_STR:{
                        usize i = self->pc;
                        while (i < self->bytecode.m_size && self->bytecode.m_data[i] != '\0')
                            ++i;
                        if (i >= self->bytecode.m_size)
                            return bad_instruction_error(instruction_idx);
                        temp = (Primitive){.m_tag = PRIMITIVE_TAG_STR, .m_str_data_ptr = allocator_alloc(self->alloc, Primitive_str_data, 1)};
                        if (!temp.m_str_data_ptr)
                            return oom_error();
                        *temp.m_str_data_ptr = (Primitive_str_data){.m_ref_count = 1, .m_data = {0}};
                        if (
                            !str_base_assign_raw(&temp.m_str_data_ptr->m_data, self->alloc, (const char*)&self->bytecode.m_data[self->pc]) ||
                            !vec_base_push_back(&self->data_stack, self->alloc, &temp)
                        ){
                            primitive_deinit(&temp, self->alloc);
                            return oom_error();
                        }
                        self->pc = i + 1;
                        break;
                    }
                    case OP_CODE_PUSH_TAG_LIST:
                        temp = (Primitive){.m_tag = PRIMITIVE_TAG_LIST, .m_list_data_ptr = allocator_alloc(self->alloc, Primitive_list_data, 1)};
                        if (!temp.m_list_data_ptr)
                            return oom_error();
                        *temp.m_list_data_ptr = (Primitive_list_data){.m_ref_count = 1, .m_data = vec_base_init(Primitive)};
                        if (!vec_base_push_back(&self->data_stack, self->alloc, &temp)){
                            primitive_deinit(&temp, self->alloc);
                            return oom_error();
                        }
                        break;
                    default:
                        return bad_instruction_error(instruction_idx);
                }
                break;
            }
            case OP_CODE_POP:
                if (self->data_stack.m_size == 0)
                    return runtime_error("<%s> instruction used on empty stack", op_code_str);
                primitive_deinit(vec_base_at(&self->data_stack, self->data_stack.m_size - 1), self->alloc);
                vec_base_pop_back_discard(&self->data_stack);
                break;

            case OP_CODE_CALL:{
                if (self->pc >= self->bytecode.m_size)
                    return bad_instruction_error(instruction_idx);
                enum Builtin_fn_tag bfn_tag = (enum Builtin_fn_tag)self->bytecode.m_data[self->pc++];
                switch (bfn_tag){
                    case BUILTIN_FN_TAG_NONE:{
                        usize call_offset;
                        if (self->pc + sizeof(call_offset) > self->bytecode.m_size)
                            return bad_instruction_error(instruction_idx);
                        memcpy(&call_offset, &self->bytecode.m_data[self->pc], sizeof(call_offset));
                        if (call_offset > self->bytecode.m_size)
                            return runtime_error("<%s> instruction jumps past the end of the program", op_code_str);
                        self->pc += sizeof(call_offset);
                        if (!vec_base_push_back(&self->return_address_stack, self->alloc, &self->pc))
                            return oom_error();
                        self->pc = call_offset;
                        break;
                    }
                    case BUILTIN_FN_TAG_EXIT:{
                        if (self->data_stack.m_size < 1)
                            return builtin_fn_arg_count_error(bfn_tag);
                        Primitive exit_val;
                        vec_base_pop_back_to(&self->data_stack, &exit_val);
                        interpreter_state_deinit(self);
                        return (Interpreter_run_result){.result = exit_val, .error = INTERPRETER_RUN_ERROR_NONE};
                    }
                    case BUILTIN_FN_TAG_NSLEEP:{
                        if (self->data_stack.m_size < 1)
                            return builtin_fn_arg_count_error(bfn_tag);
                        Primitive *sleep_for = vec_base_at(&self->data_stack, self->data_stack.m_size - 1);
                        i64 sleep_for_val;
                        switch (sleep_for->m_tag){
                            case PRIMITIVE_TAG_BOOL:  sleep_for_val = sleep_for->m_bool_data;       break;
                            case PRIMITIVE_TAG_CHAR:  sleep_for_val = sleep_for->m_char_data;       break;
                            case PRIMITIVE_TAG_INT:   sleep_for_val = sleep_for->m_int_data;        break;
                            case PRIMITIVE_TAG_FLOAT: sleep_for_val = (i64)sleep_for->m_float_data; break;
                            default:                  return runtime_error("Builtin function <%s> called on non-numeric type", builtin_fn_tag_to_str(bfn_tag));
                        }
                        primitive_deinit(sleep_for, self->alloc);
                        vec_base_pop_back_discard(&self->data_stack);
                        if (errno = 0, nanosleep(&(struct timespec){.tv_sec = (time_t)(sleep_for_val / 1000000000), .tv_nsec = (i32)(sleep_for_val % 1000000000)}, NULL) != 0)
                            return runtime_error(strerror(errno));
                        break;
                    }
                    case BUILTIN_FN_TAG_PRINT:{
                        if (self->data_stack.m_size < 1)
                            return builtin_fn_arg_count_error(bfn_tag);
                        Primitive to_print;
                        vec_base_pop_back_to(&self->data_stack, &to_print);
                        primitive_print(&to_print);
                        primitive_deinit(&to_print, self->alloc);
                        break;
                    }
                    case BUILTIN_FN_TAG_SCAN:{
                        if (self->data_stack.m_size < 1)
                            return builtin_fn_arg_count_error(bfn_tag);

                        Primitive *to_print = vec_base_at(&self->data_stack, self->data_stack.m_size - 1);
                        primitive_print(to_print);

                        Str_base str_result = {0};
#ifdef _WIN32
                        enum Str_getline_error getline_result = str_base_getline(&str_result, self->alloc, stdin);
#else
                        struct termios old_settings, new_settings = (tcgetattr(0, &old_settings), old_settings);
                        new_settings.c_lflag |= (tcflag_t)ECHO;
                        tcsetattr(0, TCSANOW, &new_settings);
                        enum Str_getline_error getline_result = str_base_getline(&str_result, self->alloc, stdin);
                        tcsetattr(0, TCSANOW, &old_settings);
#endif // _WIN32
                        switch (getline_result){
                            case STR_GETLINE_ERROR_NONE:
                                break;
                            case STR_GETLINE_ERROR_FEOF:
                                if (str_base_push_back(&str_result, self->alloc, '\n'))
                                    break;
                                FALLTHROUGH;
                            case STR_GETLINE_ERROR_OOM:
                                str_base_deinit(&str_result, self->alloc);
                                return oom_error();
                            case STR_GETLINE_ERROR_FERROR:
                                str_base_deinit(&str_result, self->alloc);
                                return runtime_error("<ferror> on stdin");
                        }

                        Primitive scanned = {.m_tag = PRIMITIVE_TAG_STR, .m_str_data_ptr = allocator_alloc(self->alloc, Primitive_str_data, 1)};
                        if (!scanned.m_str_data_ptr){
                            str_base_deinit(&str_result, self->alloc);
                            return oom_error();
                        }
                        *scanned.m_str_data_ptr = (Primitive_str_data){.m_ref_count = 1, .m_data = str_result};

                        primitive_deinit(to_print, self->alloc);
                        *to_print = scanned;
                        break;
                    }
                    case BUILTIN_FN_TAG_POLL_KEYPRESS:{
                        char keypress = '\0';
#ifdef _WIN32
                        if (kbhit()){
                            keypress = (char)getch();
                            while (kbhit())
                                (void)getch();
                        }
#else
                        struct termios old_settings, new_settings = (tcgetattr(0, &old_settings), old_settings);
                        new_settings.c_lflag &= (tcflag_t)~ICANON;
                        new_settings.c_cc[VTIME] = 0;
                        new_settings.c_cc[VMIN]  = 0;
                        tcsetattr(0, TCSANOW, &new_settings);
                        if (read(0, &keypress, 1) > 0)
                            tcflush(0, TCIFLUSH);
                        tcsetattr(0, TCSANOW, &old_settings);
#endif // _WIN32
                        if (!vec_base_push_back(&self->data_stack, self->alloc, &(Primitive){.m_tag = PRIMITIVE_TAG_CHAR, .m_char_data = (u8)keypress}))
                            return oom_error();
                        break;
                    }
                    case BUILTIN_FN_TAG_LEN:{
                        if (self->data_stack.m_size < 1)
                            return builtin_fn_arg_count_error(bfn_tag);

                        Primitive len = {.m_tag = PRIMITIVE_TAG_INT};

                        Primitive *list_like = vec_base_at(&self->data_stack, self->data_stack.m_size - 1);
                        switch (list_like->m_tag){
                            case PRIMITIVE_TAG_STR:  len.m_int_data = (i64)str_base_size(&list_like->m_str_data_ptr->m_data); break;
                            case PRIMITIVE_TAG_LIST: len.m_int_data = (i64)list_like->m_list_data_ptr->m_data.m_size;         break;
                            default:                 return runtime_error("Builtin function <%s> called on a numeric type", builtin_fn_tag_to_str(bfn_tag));
                        }
                        primitive_deinit(list_like, self->alloc);

                        *list_like = len;
                        break;
                    }
                    case BUILTIN_FN_TAG_RAND:
                        if (!vec_base_push_back(&self->data_stack, self->alloc, &(Primitive){.m_tag = PRIMITIVE_TAG_INT, .m_int_data = (i64)xoshiro256_next(&self->rand)}))
                            return oom_error();
                        break;
                    case BUILTIN_FN_TAG_PUSH_BACK:{
                        if (self->data_stack.m_size < 2)
                            return builtin_fn_arg_count_error(bfn_tag);

                        Primitive to_push;
                        vec_base_pop_back_to(&self->data_stack, &to_push);

                        Primitive *list = vec_base_at(&self->data_stack, self->data_stack.m_size - 1);
                        if (list->m_tag != PRIMITIVE_TAG_LIST){
                            primitive_deinit(&to_push, self->alloc);
                            return runtime_error("<%s> called on non-list type", builtin_fn_tag_to_str(bfn_tag));
                        }

                        if (!vec_base_push_back(&list->m_list_data_ptr->m_data, self->alloc, &to_push)){
                            primitive_deinit(&to_push, self->alloc);
                            return oom_error();
                        }

                        primitive_deinit(list, self->alloc);
                        vec_base_pop_back_discard(&self->data_stack);
                        break;
                    }
                    case BUILTIN_FN_TAG_POP_BACK:{
                        if (self->data_stack.m_size < 1)
                            return builtin_fn_arg_count_error(bfn_tag);

                        Primitive *list = vec_base_at(&self->data_stack, self->data_stack.m_size - 1);
                        if (list->m_tag != PRIMITIVE_TAG_LIST)
                            return runtime_error("<%s> called on non-list type", builtin_fn_tag_to_str(bfn_tag));

                        if (list->m_list_data_ptr->m_data.m_size == 0)
                            return runtime_error("<%s> called on empty list", builtin_fn_tag_to_str(bfn_tag));

                        Primitive popped;
                        vec_base_pop_back_to(&list->m_list_data_ptr->m_data, &popped);
                        primitive_deinit(&popped, self->alloc);

                        primitive_deinit(list, self->alloc);
                        vec_base_pop_back_discard(&self->data_stack);
                        break;
                    }
                    default:
                        return bad_instruction_error(instruction_idx);
                }
                break;
            }
            case OP_CODE_RET:
            case OP_CODE_RETV:{
                usize pop_count;
                if (self->pc + sizeof(pop_count) > self->bytecode.m_size)
                    return bad_instruction_error(instruction_idx);
                memcpy(&pop_count, &self->bytecode.m_data[self->pc], sizeof(pop_count));

                if (pop_count + (op_code == OP_CODE_RET) > self->data_stack.m_size)
                    return runtime_error("<%s> instruction pops the stack too many times", op_code_str);

                Primitive ret_val;
                if (op_code == OP_CODE_RET)
                    vec_base_pop_back_to(&self->data_stack, &ret_val);

                while (pop_count-- > 0){
                    primitive_deinit(vec_base_at(&self->data_stack, self->data_stack.m_size - 1), self->alloc);
                    vec_base_pop_back_discard(&self->data_stack);
                }

                if (op_code == OP_CODE_RET)
                    (void)vec_base_push_back(&self->data_stack, self->alloc, &ret_val);

                if (self->return_address_stack.m_size == 0)
                    goto end;

                usize return_address;
                vec_base_pop_back_to(&self->return_address_stack, &return_address);
                self->pc = return_address;
                break;
            }

            case OP_CODE_JMP:{
                usize jmp_offset;
                if (self->pc + sizeof(jmp_offset) > self->bytecode.m_size)
                    return bad_instruction_error(instruction_idx);
                memcpy(&jmp_offset, &self->bytecode.m_data[self->pc], sizeof(jmp_offset));
                self->pc = jmp_offset;
                break;
            }
            case OP_CODE_JMPZ:{
                usize jmpz_offset;
                if (self->pc + sizeof(jmpz_offset) > self->bytecode.m_size)
                    return bad_instruction_error(instruction_idx);
                memcpy(&jmpz_offset, &self->bytecode.m_data[self->pc], sizeof(jmpz_offset));
                self->pc += sizeof(jmpz_offset);

                if (self->data_stack.m_size == 0)
                    return runtime_error("<%s> instruction used on empty stack", op_code_str);

                Primitive *condition = vec_base_at(&self->data_stack, self->data_stack.m_size - 1);

                op_result = primitive_to_bool(condition, self->alloc);
                if (op_result.error != PRIMITIVE_OP_ERROR_NONE)
                    return primitive_op_error(op_result);

                bool condition_val = condition->m_bool_data;
                primitive_deinit(condition, self->alloc);
                vec_base_pop_back_discard(&self->data_stack);

                if (!condition_val)
                    self->pc = jmpz_offset;
                break;
            }

            case OP_CODE_TO_BOOL:
            case OP_CODE_TO_CHAR:
            case OP_CODE_TO_INT:
            case OP_CODE_TO_FLOAT:
            case OP_CODE_TO_STR:
            case OP_CODE_NEG:
            case OP_CODE_BNEG:{
                if (self->data_stack.m_size == 0)
                    return runtime_error("<%s> instruction used on empty stack", op_code_str);
                Primitive *last = vec_base_at(&self->data_stack, self->data_stack.m_size - 1);
                switch (op_code){
                    case OP_CODE_TO_BOOL:  op_result = primitive_to_bool (last, self->alloc); break;
                    case OP_CODE_TO_CHAR:  op_result = primitive_to_char (last, self->alloc); break;
                    case OP_CODE_TO_INT:   op_result = primitive_to_int  (last, self->alloc); break;
                    case OP_CODE_TO_FLOAT: op_result = primitive_to_float(last, self->alloc); break;
                    case OP_CODE_TO_STR:   op_result = primitive_to_str  (last, self->alloc); break;

                    case OP_CODE_NEG:      op_result = primitive_neg (last); break;
                    case OP_CODE_BNEG:     op_result = primitive_bneg(last); break;

                    default:               unreachable();
                }
                if (op_result.error != PRIMITIVE_OP_ERROR_NONE)
                    return primitive_op_error(op_result);
                break;
            }

            case OP_CODE_MOV:{
                usize sp_offset;
                if (self->pc + sizeof(sp_offset) > self->bytecode.m_size)
                    return bad_instruction_error(instruction_idx);
                memcpy(&sp_offset, &self->bytecode.m_data[self->pc], sizeof(sp_offset));
                self->pc += sizeof(sp_offset);

                usize offset = self->data_stack.m_size - sp_offset;
                if (offset >= self->data_stack.m_size)
                    return runtime_error("<%s> instruction references an out of range element on the stack", op_code_str);
                if (offset == self->data_stack.m_size - 1)
                    return runtime_error("<%s> instruction references the last element on the stack", op_code_str);
                if (self->data_stack.m_size < 2)
                    return runtime_error("<%s> instruction used with less than 2 elements on the stack", op_code_str);

                Primitive *lhs = vec_base_at(&self->data_stack, self->data_stack.m_size - sp_offset);
                Primitive *rhs = vec_base_at(&self->data_stack, self->data_stack.m_size - 1);

                op_result = primitive_mov(lhs, self->alloc, rhs);
                if (op_result.error != PRIMITIVE_OP_ERROR_NONE)
                    return primitive_op_error(op_result);

                primitive_deinit(rhs, self->alloc);
                vec_base_pop_back_discard(&self->data_stack);
                break;
            }
            case OP_CODE_MOV_DEREF:{
                if (self->data_stack.m_size < 3)
                    return runtime_error("<%s> instruction used with less than 3 elements on the stack", op_code_str);
                Primitive *lhs = vec_base_at(&self->data_stack, self->data_stack.m_size - 3);
                Primitive *idx = vec_base_at(&self->data_stack, self->data_stack.m_size - 2);
                Primitive *rhs = vec_base_at(&self->data_stack, self->data_stack.m_size - 1);
                op_result = primitive_mov_deref(lhs, self->alloc, idx, rhs);
                if (op_result.error != PRIMITIVE_OP_ERROR_NONE)
                    return primitive_op_error(op_result);
                primitive_deinit(rhs, self->alloc);
                primitive_deinit(idx, self->alloc);
                primitive_deinit(lhs, self->alloc);
                vec_base_pop_back_discard(&self->data_stack);
                vec_base_pop_back_discard(&self->data_stack);
                vec_base_pop_back_discard(&self->data_stack);
                break;
            }

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
            case OP_CODE_XOR:{
                if (self->data_stack.m_size < 2)
                    return runtime_error("<%s> instruction used with less than 2 elements on the stack", op_code_str);
                Primitive *lhs = vec_base_at(&self->data_stack, self->data_stack.m_size - 2);
                Primitive *rhs = vec_base_at(&self->data_stack, self->data_stack.m_size - 1);
                switch (op_code){
                    case OP_CODE_DEREF:   op_result = primitive_deref  (lhs, self->alloc, rhs); break;
                    case OP_CODE_CMP_EQ:  op_result = primitive_cmp_eq (lhs, self->alloc, rhs); break;
                    case OP_CODE_CMP_NEQ: op_result = primitive_cmp_neq(lhs, self->alloc, rhs); break;
                    case OP_CODE_CMP_LE:  op_result = primitive_cmp_le (lhs, self->alloc, rhs); break;
                    case OP_CODE_CMP_LEQ: op_result = primitive_cmp_leq(lhs, self->alloc, rhs); break;
                    case OP_CODE_CMP_GE:  op_result = primitive_cmp_ge (lhs, self->alloc, rhs); break;
                    case OP_CODE_CMP_GEQ: op_result = primitive_cmp_geq(lhs, self->alloc, rhs); break;
                    case OP_CODE_ADD:     op_result = primitive_add    (lhs, self->alloc, rhs); break;

                    case OP_CODE_SUB:     op_result = primitive_sub (lhs, rhs); break;
                    case OP_CODE_MUL:     op_result = primitive_mul (lhs, rhs); break;
                    case OP_CODE_DIV:     op_result = primitive_div (lhs, rhs); break;
                    case OP_CODE_MOD:     op_result = primitive_mod (lhs, rhs); break;
                    case OP_CODE_POW:     op_result = primitive_pow (lhs, rhs); break;
                    case OP_CODE_SHL:     op_result = primitive_shl (lhs, rhs); break;
                    case OP_CODE_SHR:     op_result = primitive_shr (lhs, rhs); break;
                    case OP_CODE_BAND:    op_result = primitive_band(lhs, rhs); break;
                    case OP_CODE_BOR:     op_result = primitive_bor (lhs, rhs); break;
                    case OP_CODE_XOR:     op_result = primitive_xor (lhs, rhs); break;

                    default:              unreachable();
                }
                if (op_result.error != PRIMITIVE_OP_ERROR_NONE)
                    return primitive_op_error(op_result);
                primitive_deinit(rhs, self->alloc);
                vec_base_pop_back_discard(&self->data_stack);
                break;
            }

            default:
                return runtime_error("Unknown instruction with value <%d> at idx <" USIZE_PFMT ">", (int)op_code, instruction_idx);
        }
    }

end:
    return runtime_error("Program must exit with the builtin <%s> function", builtin_fn_tag_to_str(BUILTIN_FN_TAG_EXIT));
}

// ------------------------------------------------------------------------------------------------

Interpreter_run_result interpreter_run(Allocator alloc, U8_slice bytecode, int argc, const char *const *argv){
    assert((argv || argc == 0) && "<argv> is only nullable if <argc> == 0");

    Interpreter_state state = {
        .alloc                = alloc,
        .rand                 = xoshiro256_init((u64)time(NULL)),
        .argv                 = {.m_tag = PRIMITIVE_TAG_LIST, .m_list_data_ptr = allocator_alloc(alloc, Primitive_list_data, 1)},
        .bytecode             = bytecode,
        .pc                   = 0,
        .data_stack           = vec_base_init(Primitive),
        .return_address_stack = vec_base_init(usize)
    };

    if (!state.argv.m_list_data_ptr)
        return (Interpreter_run_result){.error = INTERPRETER_RUN_ERROR_OOM};

    *state.argv.m_list_data_ptr = (Primitive_list_data){.m_ref_count = 1, .m_data = vec_base_init(Primitive)};

    for (int i = 0; i < argc; ++i){
        Primitive temp = {.m_tag = PRIMITIVE_TAG_STR, .m_str_data_ptr = allocator_alloc(state.alloc, Primitive_str_data, 1)};
        if (!temp.m_str_data_ptr)
            return interpreter_state_oom_error(&state);
        *temp.m_str_data_ptr = (Primitive_str_data){.m_ref_count = 1, .m_data = {0}};
        if (!str_base_assign_raw(&temp.m_str_data_ptr->m_data, state.alloc, argv[i]) || !vec_base_push_back(&state.argv.m_list_data_ptr->m_data, state.alloc, &temp)){
            primitive_deinit(&temp, state.alloc);
            return interpreter_state_oom_error(&state);
        }
    }

#ifdef _WIN32
    return interpreter_state_run(&state);
#else
    struct termios old_settings, new_settings = (tcgetattr(0, &old_settings), old_settings);
    new_settings.c_lflag &= (tcflag_t)~ECHO;
    tcsetattr(0, TCSANOW, &new_settings);
    Interpreter_run_result result = interpreter_state_run(&state);
    tcsetattr(0, TCSANOW, &old_settings);
    return result;
#endif // _WIN32
}
