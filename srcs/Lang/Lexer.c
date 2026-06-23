#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "../../hdrs/Allocator/Allocator.h"
#include "../../hdrs/Allocator/Arena.h"
#include "../../hdrs/Data_structure/Str_base.h"
#include "../../hdrs/Data_structure/Vec_base.h"
#include "../../hdrs/Utils/Num.h"

#include "../../hdrs/Lang/Lexer.h"

// ------------------------------------------------------------------------------------------------

#define BIN_DIGIT_MAX_COUNT 64
#define HEX_DIGIT_MAX_COUNT BIN_DIGIT_MAX_COUNT / 4
#define U64_MAX_STRLEN 20

static const char MULTI_LINE_COMMENT_MARKER[] = "/*/";
static const char SINGLE_LINE_COMMENT_MARKER[] = "//";

typedef struct Lexer_state{
    Allocator alloc;
    FILE *file;
    usize line_number;
    Vec_base tokens;
} Lexer_state;

static void lexer_state_cleanup(Lexer_state *self){
    fclose(self->file);
}
static Lex_result lexer_state_oom_error(Lexer_state *self){
    lexer_state_cleanup(self);

    return (Lex_result){.error = LEX_ERROR_OOM};
}
static Lex_result lexer_state_syntax_error(Lexer_state *self, const char *fmt, ...){
    va_list args;
    va_start(args, fmt);
    Str_base_result error_info = str_base_init_fmt_va_list(self->alloc, fmt, args);
    va_end(args);

    if (!error_info.success)
        return lexer_state_oom_error(self);

    if (self->line_number > 0){
        Str_base_result temp = str_base_init_fmt(self->alloc, "On line <" USIZE_PFMT ">: %s", self->line_number, str_base_data(&error_info.result));
        if (!temp.success)
            return lexer_state_oom_error(self);
        error_info = temp;
    }

    lexer_state_cleanup(self);

    return (Lex_result){.error_info = error_info.result, .error = LEX_ERROR_SYNTAX};
}
static bool lexer_state_token_push_back(Lexer_state *self, enum Token_type type, const char *id){
    Str_base_result temp = str_base_init_raw(self->alloc, id);

    if (temp.success)
        temp.success = vec_base_push_back(&self->tokens, self->alloc, &(Token){.m_type = type, .m_id = temp.result, .m_line_number = self->line_number});

    return temp.success;
}

static int is_not_newline(int c){
    return c != '\n';
}
static int is_space_not_newline(int c){
    return isspace(c) && is_not_newline(c);
}
static int is_bin_digit(int c){
    return c == '0' || c == '1';
}

// ------------------------------------------------------------------------------------------------

void token_slice_print(Token_slice tokens_slice){
    for (usize i = 0; i < tokens_slice.m_size; ++i){
        const Token *t = &tokens_slice.m_data[i];
        printf("{.type = %2d, .id = %s, .line_number = " USIZE_PFMT "}\n", t->m_type, str_base_data_const(&t->m_id), t->m_line_number);
    }
}

Lex_result lex(Arena *arena, const char *path){
    assert(arena && "<arena> is not nullable");
    assert(path && "<path> is not nullable");

    Lexer_state state = {
        .alloc       = arena_allocator(arena),
        .file        = fopen(path, "r"),
        .line_number = 1,
        .tokens      = vec_base_init(Token)
    };
    #define oom_error() lexer_state_oom_error(&state)
    #define syntax_error(...) lexer_state_syntax_error(&state, __VA_ARGS__)
    #define token_push_back(token_type, id) \
        do{ \
            if (!lexer_state_token_push_back(&state, (token_type), (id))) \
                return oom_error(); \
        } while (0)
    #define match_starts_with(id_var, starts_with) ((id_var) = (starts_with), str_view_starts_with((sv), (id_var)))
    #define match_token_push_back(token_type, id) \
        do{ \
            token_push_back((token_type), (id)); \
            sv = str_view_trim_prefix(sv, (id)); \
        } while (0)

    if (!state.file){
        Str_base_result error_info = str_base_init_raw(state.alloc, "File not found");
        return (error_info.success)
            ? (Lex_result){.error_info = error_info.result, .error = LEX_ERROR_FILE}
            : (Lex_result){.error = LEX_ERROR_OOM}
        ;
    }

    Str_base lines = {0};

    {
        Str_base line = {0};

        enum Str_getline_error getline_error;
        while ((getline_error = str_base_getline(&line, state.alloc, state.file)) == STR_GETLINE_ERROR_NONE)
            if (!str_base_append_str_base(&lines, state.alloc, &line) || !str_base_push_back(&lines, state.alloc, '\n'))
                return oom_error();

        switch (getline_error){
            case STR_GETLINE_ERROR_NONE:
            case STR_GETLINE_ERROR_FEOF:
                break;
            case STR_GETLINE_ERROR_OOM:
                return oom_error();
            case STR_GETLINE_ERROR_FERROR:
                {
                    Str_base_result error_info = str_base_init_raw(state.alloc, "<ferror> during tokenization");
                    if (!error_info.success)
                        return oom_error();

                    lexer_state_cleanup(&state);

                    return (Lex_result){.error_info = error_info.result, .error = LEX_ERROR_FILE};
                }
        }
    }

    usize lparen_count   = 0, rparen_count   = 0;
    usize lbracket_count = 0, rbracket_count = 0;
    usize lbrace_count   = 0, rbrace_count   = 0;

    for (Str_view sv = str_base_to_str_view(&lines); (sv = str_view_trim_left_while(sv, is_space_not_newline)).m_size > 0;){
        const char *punct_match;
        #define punct_match_starts_with(starts_with) match_starts_with(punct_match, starts_with)
        #define punct_match_token_push_back(token_type) match_token_push_back(token_type, punct_match)

        if (str_view_starts_with(sv, "\n")){
            ++state.line_number;
            sv = str_view_trim_prefix(sv, "\n");
        }
        else if (str_view_starts_with(sv, MULTI_LINE_COMMENT_MARKER)){
            usize multiline_newline_count = 0;

            sv = str_view_trim_prefix(sv, MULTI_LINE_COMMENT_MARKER);
            while (sv.m_size > 0 && !str_view_starts_with(sv, MULTI_LINE_COMMENT_MARKER)){
                multiline_newline_count += str_view_starts_with(sv, "\n");
                sv = str_view_trim_left(sv, 1);
            }
            if (sv.m_size == 0)
                return syntax_error("Unclosed multi line comment");
            
            state.line_number += multiline_newline_count;
            sv = str_view_trim_prefix(sv, MULTI_LINE_COMMENT_MARKER);
        }
        else if (str_view_starts_with(sv, SINGLE_LINE_COMMENT_MARKER))
            sv = str_view_trim_left_while(sv, is_not_newline);
        else if (str_view_starts_with(sv, "\"") || str_view_starts_with(sv, "'")){
            char quote = (sv.m_str[0] == '"') ? '"' : '\'';

            usize quote_end_pos = 0;
            while (++quote_end_pos < sv.m_size && sv.m_str[quote_end_pos] != quote){
                if (sv.m_str[quote_end_pos] == '\n')
                    return syntax_error("<%s> literal contains newline character(s)", (quote == '\'') ? "char" : "str");
                quote_end_pos += (sv.m_str[quote_end_pos] == '\\');
            }
            if (quote_end_pos >= sv.m_size)
                return syntax_error("Unclosed <%s> literal", (quote == '\'') ? "char" : "str");

            Str_view quoted_sv = str_view_trim_right(sv, sv.m_size - quote_end_pos - 1);

            Str_base_unescape_result temp = str_base_unescape_str_view(state.alloc, quoted_sv);
            switch (temp.error){
                case STR_UNESCAPE_ERROR_NONE:                break;
                case STR_UNESCAPE_ERROR_OOM:                 return oom_error();
                case STR_UNESCAPE_ERROR_BAD_ESCAPE_SEQUENCE: return syntax_error("<%s> literal is escaped incorrectly", (quote == '\'') ? "char" : "str");
            }

            // TODO: <char> literals than have '\0'(s) might be consumed even when they contain multiple characters
            if (quote == '\'' && (quoted_sv.m_size == 2 || str_base_size(&temp.result) > 1 + 2))
                return syntax_error("<char> literal must represent 1 character");

            if (quote == '"' && !vec_base_empty(&state.tokens) && ((Token*)vec_base_at(&state.tokens, state.tokens.m_size - 1))->m_type == TOKEN_TYPE_STR_LIT){
                Token *last = vec_base_at(&state.tokens, state.tokens.m_size - 1);
                str_base_pop_back(&last->m_id);
                if (!str_base_append_str_view(&last->m_id, state.alloc, str_view_trim_left(quoted_sv, 1)))
                    return oom_error();
            }
            else if (
                !str_base_assign_str_view(&temp.result, state.alloc, quoted_sv) ||
                !vec_base_push_back(
                    &state.tokens,
                    state.alloc,
                    &(Token){.m_type = (quote == '\'') ? TOKEN_TYPE_CHAR_LIT : TOKEN_TYPE_STR_LIT, .m_id = temp.result, .m_line_number = state.line_number}
                )
            )
                return oom_error();

            sv = str_view_trim_left(sv, quoted_sv.m_size);
        }
        
        else if (punct_match_starts_with("(")){ punct_match_token_push_back(TOKEN_TYPE_LPAREN);   ++lparen_count;   }
        else if (punct_match_starts_with(")")){ punct_match_token_push_back(TOKEN_TYPE_RPAREN);   ++rparen_count;   }
        else if (punct_match_starts_with("[")){ punct_match_token_push_back(TOKEN_TYPE_LBRACKET); ++lbracket_count; }
        else if (punct_match_starts_with("]")){ punct_match_token_push_back(TOKEN_TYPE_RBRACKET); ++rbracket_count; }
        else if (punct_match_starts_with("{")){ punct_match_token_push_back(TOKEN_TYPE_LBRACE);   ++lbrace_count;   }
        else if (punct_match_starts_with("}")){ punct_match_token_push_back(TOKEN_TYPE_RBRACE);   ++rbrace_count;   }

        else if (punct_match_starts_with("," )) punct_match_token_push_back(TOKEN_TYPE_COMMA);
        else if (punct_match_starts_with(":" )) punct_match_token_push_back(TOKEN_TYPE_COLON);
        else if (punct_match_starts_with(";" )) punct_match_token_push_back(TOKEN_TYPE_SEMICOLON);
        else if (punct_match_starts_with("..")) punct_match_token_push_back(TOKEN_TYPE_DOT2);
        else if (punct_match_starts_with("+" )) punct_match_token_push_back(TOKEN_TYPE_PLUS);
        else if (punct_match_starts_with("-" )) punct_match_token_push_back(TOKEN_TYPE_MINUS);
        else if (punct_match_starts_with("**")) punct_match_token_push_back(TOKEN_TYPE_ASTERISK2);
        else if (punct_match_starts_with("*" )) punct_match_token_push_back(TOKEN_TYPE_ASTERISK1);
        else if (punct_match_starts_with("/" )) punct_match_token_push_back(TOKEN_TYPE_SLASH);
        else if (punct_match_starts_with("%" )) punct_match_token_push_back(TOKEN_TYPE_PERCENT);
        else if (punct_match_starts_with("<<")) punct_match_token_push_back(TOKEN_TYPE_LESS_THAN2);
        else if (punct_match_starts_with(">>")) punct_match_token_push_back(TOKEN_TYPE_GREATER_THAN2);
        else if (punct_match_starts_with("&" )) punct_match_token_push_back(TOKEN_TYPE_AMPERSAND);
        else if (punct_match_starts_with("|" )) punct_match_token_push_back(TOKEN_TYPE_PIPE);
        else if (punct_match_starts_with("^" )) punct_match_token_push_back(TOKEN_TYPE_CARET);
        else if (punct_match_starts_with("~" )) punct_match_token_push_back(TOKEN_TYPE_TILDE);
        else if (punct_match_starts_with("==")) punct_match_token_push_back(TOKEN_TYPE_EQUALS2);
        else if (punct_match_starts_with("!=")) punct_match_token_push_back(TOKEN_TYPE_NOT_EQUALS);
        else if (punct_match_starts_with("<=")) punct_match_token_push_back(TOKEN_TYPE_LESS_THAN1_EQUALS);
        else if (punct_match_starts_with("<" )) punct_match_token_push_back(TOKEN_TYPE_LESS_THAN1);
        else if (punct_match_starts_with(">=")) punct_match_token_push_back(TOKEN_TYPE_GREATER_THAN1_EQUALS);
        else if (punct_match_starts_with(">" )) punct_match_token_push_back(TOKEN_TYPE_GREATER_THAN1);
        else if (punct_match_starts_with("=" )) punct_match_token_push_back(TOKEN_TYPE_EQUALS1);

        else if (isdigit(sv.m_str[0])){
            if (str_view_starts_with(sv, "0x") || str_view_starts_with(sv, "0X") || str_view_starts_with(sv, "0b") || str_view_starts_with(sv, "0B")){
                bool is_hex = (tolower(sv.m_str[1]) == 'x');

                int (*is_fn)(int)     = isxdigit;
                usize digit_max_count = HEX_DIGIT_MAX_COUNT;
                int base              = 16;

                if (!is_hex){
                    is_fn           = is_bin_digit;
                    digit_max_count = BIN_DIGIT_MAX_COUNT;
                    base            = 2;
                }

                sv = str_view_trim_left(sv, 2);

                if (sv.m_size == 0 || !is_fn(sv.m_str[0]))
                    return syntax_error((is_hex) ? "Hexadecimal prefix followed by non-hex digit(s)" : "Binary prefix followed by non-binary digit(s)");

                char digit_buf[BIN_DIGIT_MAX_COUNT + 1] = {0};

                usize digit_count = 0;
                usize i = 0;
                for (; is_fn(sv.m_str[i]) || sv.m_str[i] == '_'; ++i){
                    if (digit_count >= digit_max_count)
                        return syntax_error("%s <int> literal out of range", (is_hex) ? "Hexadecimal" : "Binary");
                    if (sv.m_str[i] != '_')
                        digit_buf[digit_count++] = sv.m_str[i];
                }

                if (sv.m_str[i - 1] == '_' || isalnum(sv.m_str[i]))
                    return syntax_error("%s <int> literal followed by digit separator(s) <_> or alphanumeric character(s)", (is_hex) ? "Hexadecimal" : "Binary");

                char int_buf[U64_MAX_STRLEN + 1];
                sprintf(int_buf, I64_PFMT, (i64)strtoull(digit_buf, NULL, base));

                token_push_back(TOKEN_TYPE_INT_LIT, int_buf);

                sv = str_view_trim_left(sv, i);
            }
            else{
                Str_base decimal_buf = {0};

                usize dot_count = 0;
                usize i = 0;
                for (; i < sv.m_size && (isdigit(sv.m_str[i]) || sv.m_str[i] == '_' || sv.m_str[i] == '.'); ++i){
                    if (sv.m_str[i] == '.'){
                        char next = sv.m_str[i + 1];
                        if (next == '.')
                            break;
                        if (++dot_count > 1 || sv.m_str[i - 1] == '_' || !next || next == '_')
                            return syntax_error("Digit separator <_> must not come before or after <.>");
                        if (!str_base_push_back(&decimal_buf, state.alloc, '.'))
                            return oom_error();
                    }
                    else if (isdigit(sv.m_str[i]) && !str_base_push_back(&decimal_buf, state.alloc, sv.m_str[i]))
                        return oom_error();
                }

                if (sv.m_str[i - 1] == '_' || (!isspace(sv.m_str[i]) && !ispunct(sv.m_str[i])))
                    return syntax_error("<%s> literal followed by digit separator(s) <_>", (dot_count > 0) ? "float" : "int");

                char *data = str_base_data(&decimal_buf);

                if (dot_count > 0){
                    (void)(errno = 0, strtod(data, NULL));
                    if (errno == ERANGE)
                        return syntax_error("<float> literal out of range");
                }
                else{
                    u64 temp = (errno = 0, (u64)strtoull(data, NULL, 10));
                    if (errno == ERANGE || temp > I64_MAX)
                        return syntax_error("<int> literal out of range");
                }

                token_push_back((dot_count > 0) ? TOKEN_TYPE_FLOAT_LIT : TOKEN_TYPE_INT_LIT, data);

                sv = str_view_trim_left(sv, i);
            }
        }
        else if (isalpha(sv.m_str[0]) || sv.m_str[0] == '_'){
            const char *keyword_match;
            #define keyword_match_starts_with(starts_with) match_starts_with(keyword_match, starts_with)
            #define keyword_match_token_push_back(token_type) match_token_push_back(token_type, keyword_match)

            usize id_end_pos = 0;
            while (++id_end_pos < sv.m_size && (isalnum(sv.m_str[id_end_pos]) || sv.m_str[id_end_pos] == '_'));

            Str_view id_sv = str_view_trim_right(sv, sv.m_size - id_end_pos);

            if      (keyword_match_starts_with("argv"  )) keyword_match_token_push_back(TOKEN_TYPE_ARGV);
            else if (keyword_match_starts_with("false" )) keyword_match_token_push_back(TOKEN_TYPE_FALSE);
            else if (keyword_match_starts_with("true"  )) keyword_match_token_push_back(TOKEN_TYPE_TRUE);
            else if (keyword_match_starts_with("and"   )) keyword_match_token_push_back(TOKEN_TYPE_AND);
            else if (keyword_match_starts_with("or"    )) keyword_match_token_push_back(TOKEN_TYPE_OR);
            else if (keyword_match_starts_with("not"   )) keyword_match_token_push_back(TOKEN_TYPE_NOT);
            else if (keyword_match_starts_with("fn"    )) keyword_match_token_push_back(TOKEN_TYPE_FN);
            else if (keyword_match_starts_with("let"   )) keyword_match_token_push_back(TOKEN_TYPE_LET);
            else if (keyword_match_starts_with("void"  )) keyword_match_token_push_back(TOKEN_TYPE_VOID);
            else if (keyword_match_starts_with("bool"  )) keyword_match_token_push_back(TOKEN_TYPE_BOOL);
            else if (keyword_match_starts_with("char"  )) keyword_match_token_push_back(TOKEN_TYPE_CHAR);
            else if (keyword_match_starts_with("int"   )) keyword_match_token_push_back(TOKEN_TYPE_INT);
            else if (keyword_match_starts_with("float" )) keyword_match_token_push_back(TOKEN_TYPE_FLOAT);
            else if (keyword_match_starts_with("str"   )) keyword_match_token_push_back(TOKEN_TYPE_STR);
            else if (keyword_match_starts_with("if"    )) keyword_match_token_push_back(TOKEN_TYPE_IF);
            else if (keyword_match_starts_with("else"  )) keyword_match_token_push_back(TOKEN_TYPE_ELSE);
            else if (keyword_match_starts_with("while" )) keyword_match_token_push_back(TOKEN_TYPE_WHILE);
            else if (keyword_match_starts_with("for"   )) keyword_match_token_push_back(TOKEN_TYPE_FOR);
            else if (keyword_match_starts_with("return")) keyword_match_token_push_back(TOKEN_TYPE_RETURN);
            else{
                Str_base_result id = str_base_init_str_view(state.alloc, id_sv);
                if (!id.success || !vec_base_push_back(&state.tokens, state.alloc, &(Token){.m_type = TOKEN_TYPE_ID, .m_id = id.result, .m_line_number = state.line_number}))
                    return oom_error();
                sv = str_view_trim_prefix(sv, str_base_data(&id.result));
            }
        }
        else{
            Str_base_result temp = str_base_init_fmt(state.alloc, "Found unknown token <%c>", sv.m_str[0]);
            if (!temp.success)
                return oom_error();
            return syntax_error(str_base_data(&temp.result));
        }
    }

    state.line_number = 0;
    if (lparen_count != rparen_count)
        return syntax_error("Number of opening and closing parentheses must match");
    if (lbracket_count != rbracket_count)
        return syntax_error("Number of opening and closing brackets must match");
    if (lbrace_count != rbrace_count)
        return syntax_error("Number of opening and closing braces must match");

    lexer_state_cleanup(&state);

    return (Lex_result){.tokens = {.m_size = state.tokens.m_size, .m_data = state.tokens.m_data}, .error = LEX_ERROR_NONE};
}
