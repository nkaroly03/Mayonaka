#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../hdrs/Allocator/Allocator.h"
#include "../../hdrs/Allocator/Arena.h"
#include "../../hdrs/Data_structure/Str_base.h"
#include "../../hdrs/Data_structure/Str_view.h"
#include "../../hdrs/Data_structure/Vec_base.h"
#include "../../hdrs/Utils/Cmp.h"
#include "../../hdrs/Utils/Num.h"

#include "../../hdrs/Lang/Lexer.h"

// ------------------------------------------------------------------------------------------------

static int is_newline(int c){
    return c == '\n';
}
static int is_space_not_newline(int c){
    return !is_newline(c) && isspace(c);
}
static int is_bin_digit(int c){
    return c == '0' || c == '1';
}

typedef struct Lexer_state{
    Allocator alloc;
    FILE *file;
    Token_positiion_info pos;
    Vec_base tokens;
} Lexer_state;

#define BIN_DIGIT_MAX_COUNT 64
#define HEX_DIGIT_MAX_COUNT (BIN_DIGIT_MAX_COUNT / 4)
#define U64_MAX_STRLEN 20

static const char  MULTI_LINE_COMMENT_SYMBOL[] = "/*/";
static const char SINGLE_LINE_COMMENT_SYMBOL[] = "//";

#define  MULTI_LINE_COMMENT_SYMBOL_SIZE (array_size( MULTI_LINE_COMMENT_SYMBOL) - 1)
#define SINGLE_LINE_COMMENT_SYMBOL_SIZE (array_size(SINGLE_LINE_COMMENT_SYMBOL) - 1)

static const Lex_result OOM_ERROR = {.error = LEX_ERROR_OOM};

static void lexer_state_cleanup(Lexer_state *self){
    fclose(self->file);
}
static Lex_result lexer_state_oom_error(Lexer_state *self){
    lexer_state_cleanup(self);
    return OOM_ERROR;
}
static Lex_result lexer_state_syntax_error(Lexer_state *self, const char *fmt, ...){
    va_list args;
    va_start(args, fmt);
    Str_base_result error_info = str_base_init_fmt_va_list(self->alloc, fmt, args);
    va_end(args);

    if (!error_info.success)
        return lexer_state_oom_error(self);

    Token_positiion_info pos = self->pos;
    if (pos.m_line > 0){
        Str_base_result temp = str_base_init_fmt(self->alloc, "<" USIZE_PFMT ":" USIZE_PFMT ">: %s", pos.m_line, pos.m_column, str_base_data(&error_info.result));
        if (!temp.success)
            return lexer_state_oom_error(self);
        error_info = temp;
    }

    lexer_state_cleanup(self);

    return (Lex_result){.error_info = error_info.result, .error = LEX_ERROR_SYNTAX};
}
static bool lexer_state_token_push_back(Lexer_state *self, enum Token_type type, const char *id){
    Str_base_result temp = str_base_init_raw(self->alloc, id);
    return temp.success && vec_base_push_back(&self->tokens, self->alloc, &(Token){.m_type = type, .m_id = temp.result, .m_pos = self->pos});
}

static const char* token_type_enum_to_str(enum Token_type token_type){
    #define generate_case(token_type_id) case token_type_id: return #token_type_id
    switch (token_type){
        generate_case(TOKEN_TYPE_ID);
        generate_case(TOKEN_TYPE_FALSE);
        generate_case(TOKEN_TYPE_TRUE);
        generate_case(TOKEN_TYPE_CHAR_LIT);
        generate_case(TOKEN_TYPE_INT_LIT);
        generate_case(TOKEN_TYPE_FLOAT_LIT);
        generate_case(TOKEN_TYPE_STR_LIT);
        generate_case(TOKEN_TYPE_COMMA);
        generate_case(TOKEN_TYPE_COLON);
        generate_case(TOKEN_TYPE_SEMICOLON);
        generate_case(TOKEN_TYPE_LPAREN);
        generate_case(TOKEN_TYPE_RPAREN);
        generate_case(TOKEN_TYPE_LBRACKET);
        generate_case(TOKEN_TYPE_RBRACKET);
        generate_case(TOKEN_TYPE_LBRACE);
        generate_case(TOKEN_TYPE_RBRACE);
        generate_case(TOKEN_TYPE_TILDE);
        generate_case(TOKEN_TYPE_NOT);
        generate_case(TOKEN_TYPE_DOT1);
        generate_case(TOKEN_TYPE_DOT2);
        generate_case(TOKEN_TYPE_AS);
        generate_case(TOKEN_TYPE_EQUALS1);
        generate_case(TOKEN_TYPE_EQUALS2);
        generate_case(TOKEN_TYPE_EXCL_EQUALS1);
        generate_case(TOKEN_TYPE_LESS_THAN1);
        generate_case(TOKEN_TYPE_LESS_THAN1_EQUALS1);
        generate_case(TOKEN_TYPE_GREATER_THAN1);
        generate_case(TOKEN_TYPE_GREATER_THAN1_EQUALS1);
        generate_case(TOKEN_TYPE_PLUS);
        generate_case(TOKEN_TYPE_MINUS);
        generate_case(TOKEN_TYPE_ASTERISK1);
        generate_case(TOKEN_TYPE_SLASH);
        generate_case(TOKEN_TYPE_PERCENT);
        generate_case(TOKEN_TYPE_ASTERISK2);
        generate_case(TOKEN_TYPE_LESS_THAN2);
        generate_case(TOKEN_TYPE_GREATER_THAN2);
        generate_case(TOKEN_TYPE_AMPERSAND);
        generate_case(TOKEN_TYPE_PIPE);
        generate_case(TOKEN_TYPE_CARET);
        generate_case(TOKEN_TYPE_AND);
        generate_case(TOKEN_TYPE_OR);
        generate_case(TOKEN_TYPE_FN);
        generate_case(TOKEN_TYPE_LET);
        generate_case(TOKEN_TYPE_DEFTYPE);
        generate_case(TOKEN_TYPE_VOID);
        generate_case(TOKEN_TYPE_BOOL);
        generate_case(TOKEN_TYPE_CHAR);
        generate_case(TOKEN_TYPE_INT);
        generate_case(TOKEN_TYPE_FLOAT);
        generate_case(TOKEN_TYPE_STR);
        generate_case(TOKEN_TYPE_IF);
        generate_case(TOKEN_TYPE_ELSE);
        generate_case(TOKEN_TYPE_WHILE);
        generate_case(TOKEN_TYPE_FOR);
        generate_case(TOKEN_TYPE_BREAK);
        generate_case(TOKEN_TYPE_CONTINUE);
        generate_case(TOKEN_TYPE_RETURN);
    }
    unreachable();
}

// ------------------------------------------------------------------------------------------------

const char* token_type_to_str(enum Token_type token_type){
    switch (token_type){
        case TOKEN_TYPE_FALSE:                 return "false";
        case TOKEN_TYPE_TRUE:                  return "true";
        case TOKEN_TYPE_COMMA:                 return ",";
        case TOKEN_TYPE_COLON:                 return ":";
        case TOKEN_TYPE_SEMICOLON:             return ";";
        case TOKEN_TYPE_LPAREN:                return "(";
        case TOKEN_TYPE_RPAREN:                return ")";
        case TOKEN_TYPE_LBRACKET:              return "[";
        case TOKEN_TYPE_RBRACKET:              return "]";
        case TOKEN_TYPE_LBRACE:                return "{";
        case TOKEN_TYPE_RBRACE:                return "}";
        case TOKEN_TYPE_TILDE:                 return "~";
        case TOKEN_TYPE_NOT:                   return "not";
        case TOKEN_TYPE_DOT1:                  return ".";
        case TOKEN_TYPE_DOT2:                  return "..";
        case TOKEN_TYPE_AS:                    return "as";
        case TOKEN_TYPE_EQUALS1:               return "=";
        case TOKEN_TYPE_EQUALS2:               return "==";
        case TOKEN_TYPE_EXCL_EQUALS1:           return "!=";
        case TOKEN_TYPE_LESS_THAN1:            return "<";
        case TOKEN_TYPE_LESS_THAN1_EQUALS1:    return "<=";
        case TOKEN_TYPE_GREATER_THAN1:         return ">";
        case TOKEN_TYPE_GREATER_THAN1_EQUALS1: return ">=";
        case TOKEN_TYPE_PLUS:                  return "+";
        case TOKEN_TYPE_MINUS:                 return "-";
        case TOKEN_TYPE_ASTERISK1:             return "*";
        case TOKEN_TYPE_SLASH:                 return "/";
        case TOKEN_TYPE_PERCENT:               return "%";
        case TOKEN_TYPE_ASTERISK2:             return "**";
        case TOKEN_TYPE_LESS_THAN2:            return "<<";
        case TOKEN_TYPE_GREATER_THAN2:         return ">>";
        case TOKEN_TYPE_AMPERSAND:             return "&";
        case TOKEN_TYPE_PIPE:                  return "|";
        case TOKEN_TYPE_CARET:                 return "^";
        case TOKEN_TYPE_AND:                   return "and";
        case TOKEN_TYPE_OR:                    return "or";
        case TOKEN_TYPE_FN:                    return "fn";
        case TOKEN_TYPE_LET:                   return "let";
        case TOKEN_TYPE_DEFTYPE:               return "deftype";
        case TOKEN_TYPE_VOID:                  return "void";
        case TOKEN_TYPE_BOOL:                  return "bool";
        case TOKEN_TYPE_CHAR:                  return "char";
        case TOKEN_TYPE_INT:                   return "int";
        case TOKEN_TYPE_FLOAT:                 return "float";
        case TOKEN_TYPE_STR:                   return "str";
        case TOKEN_TYPE_IF:                    return "if";
        case TOKEN_TYPE_ELSE:                  return "else";
        case TOKEN_TYPE_WHILE:                 return "while";
        case TOKEN_TYPE_FOR:                   return "for";
        case TOKEN_TYPE_BREAK:                 return "break";
        case TOKEN_TYPE_CONTINUE:              return "continue";
        case TOKEN_TYPE_RETURN:                return "return";
        default:                               return NULL;
    }
}

i64 token_slice_print(Token_slice tokens_slice, FILE *file){
    assert(file && "<file> is not nullable");

    i64 chars_written = 0;

    for (usize i = 0; i < tokens_slice.m_size; ++i){
        const Token *t = &tokens_slice.m_data[i];
        int temp = fprintf(
            file,
            "{.type = %s, .id = %s, .pos = <" USIZE_PFMT ":" USIZE_PFMT ">}\n",
            token_type_enum_to_str(t->m_type),
            str_base_data_const(&t->m_id),
            t->m_pos.m_line,
            t->m_pos.m_column
        );
        if (temp < 0)
            return temp;
        chars_written += temp;
    }

    return chars_written;
}

Lex_result lex(Arena *arena, const char *path){
    assert(arena && "<arena> is not nullable");
    assert(path && "<path> is not nullable");

    Lexer_state state = {
        .alloc       = arena_allocator(arena),
        .file        = fopen(path, "r"),
        .pos         = {.m_line = 1, .m_column = 1},
        .tokens      = vec_base_init(Token)
    };
    #define oom_error() lexer_state_oom_error(&state)
    #define syntax_error(...) lexer_state_syntax_error(&state, __VA_ARGS__)
    #define token_push_back(token_type_val, token_type_id) lexer_state_token_push_back(&state, (token_type_val), (token_type_id))

    if (!state.file){
        Str_base_result error_info = str_base_init_fmt(state.alloc, "<%s>: %s", path, strerror(errno));
        return (error_info.success) ? (Lex_result){.error_info = error_info.result, .error = LEX_ERROR_FILE} : OOM_ERROR;
    }

    Str_base lines = {0};

    {
        Str_base line = {0};

        enum Str_getline_error getline_error;
        while ((getline_error = str_base_getline(&line, state.alloc, state.file)) == STR_GETLINE_ERROR_NONE)
            if (!(str_base_append_str_base(&lines, state.alloc, &line) && str_base_push_back(&lines, state.alloc, '\n')))
                return oom_error();

        switch (getline_error){
            case STR_GETLINE_ERROR_NONE:
            case STR_GETLINE_ERROR_FEOF:
                break;
            case STR_GETLINE_ERROR_OOM:
                return oom_error();
            case STR_GETLINE_ERROR_FERROR:{
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

    for (
        Str_view sv = str_base_to_str_view(&lines), sv_temp;
        (sv_temp = str_view_trim_left_while(sv, is_space_not_newline), state.pos.m_column += (sv.m_size - sv_temp.m_size), sv = sv_temp).m_size > 0;
    ){
        enum Token_type punct_token_type;
        const char *punct_token_id;
        #define match_punct(punct_token_type_val) \
            ( \
                punct_token_type = (punct_token_type_val), \
                punct_token_id = token_type_to_str(punct_token_type), \
                str_view_trim_prefix_in_place(&sv, punct_token_id) \
            )

        if (str_view_trim_prefix_in_place(&sv, "\n"))
            state.pos = (Token_positiion_info){.m_line = state.pos.m_line + 1, .m_column = 1};
        else if (str_view_trim_prefix_in_place(&sv, MULTI_LINE_COMMENT_SYMBOL)){
            Token_positiion_info new_pos = state.pos;
            new_pos.m_column += MULTI_LINE_COMMENT_SYMBOL_SIZE;

            usize i = 0;
            for (; i < sv.m_size && !str_view_starts_with(str_view_trim_left(sv, i), MULTI_LINE_COMMENT_SYMBOL); ++i){
                if (sv.m_str[i] == '\n')
                    new_pos = (Token_positiion_info){.m_line = new_pos.m_line + 1, .m_column = 0};
                ++new_pos.m_column;
            }
            if (i >= sv.m_size)
                return syntax_error("Unclosed multi line comment");

            state.pos = new_pos;
            state.pos.m_column += MULTI_LINE_COMMENT_SYMBOL_SIZE;

            sv = str_view_trim_left(sv, i + MULTI_LINE_COMMENT_SYMBOL_SIZE);
        }
        else if (str_view_starts_with(sv, SINGLE_LINE_COMMENT_SYMBOL))
            sv = str_view_trim_left_while_not(sv, is_newline);
        else if (sv.m_str[0] == '\'' || sv.m_str[0] == '"'){
            char quote = sv.m_str[0];
            bool is_single_quote = (quote == '\'');
            const char *type_str = (is_single_quote) ? "char" : "str";

            usize quote_end_pos = 0;
            while (++quote_end_pos < sv.m_size && sv.m_str[quote_end_pos] != quote){
                if (sv.m_str[quote_end_pos] == '\n')
                    return syntax_error("<%s> literal contains newline character(s)", type_str);
                quote_end_pos += (sv.m_str[quote_end_pos] == '\\');
            }
            if (quote_end_pos >= sv.m_size)
                return syntax_error("Unclosed <%s> literal", type_str);

            Str_view quoted_sv = str_view_trim_right(sv, sv.m_size - quote_end_pos - 1);

            Str_base_unescape_result temp = str_base_unescape_str_view(state.alloc, quoted_sv);
            switch (temp.error){
                case STR_UNESCAPE_ERROR_NONE:                break;
                case STR_UNESCAPE_ERROR_OOM:                 return oom_error();
                case STR_UNESCAPE_ERROR_BAD_ESCAPE_SEQUENCE: return syntax_error("<%s> literal is escaped incorrectly", type_str);
            }

            // TODO: <char> literals that have '\0'(s) might be consumed even when they contain multiple characters
            if (is_single_quote && (quoted_sv.m_size == 2 || str_base_size(&temp.result) > 1 + 2))
                return syntax_error("<char> literal must represent 1 character");

            if (!is_single_quote && state.tokens.m_size > 0 && ((Token*)vec_base_at(&state.tokens, state.tokens.m_size - 1))->m_type == TOKEN_TYPE_STR_LIT){
                Token *last = vec_base_at(&state.tokens, state.tokens.m_size - 1);
                str_base_pop_back(&last->m_id);
                if (!str_base_append_str_view(&last->m_id, state.alloc, str_view_trim_left(quoted_sv, 1)))
                    return oom_error();
            }
            else if (!(
                str_base_assign_str_view(&temp.result, state.alloc, quoted_sv) &&
                vec_base_push_back(
                    &state.tokens,
                    state.alloc,
                    &(Token){.m_type = (is_single_quote) ? TOKEN_TYPE_CHAR_LIT : TOKEN_TYPE_STR_LIT, .m_id = temp.result, .m_pos = state.pos}
                )
            ))
                return oom_error();

            state.pos.m_column += quoted_sv.m_size;
            sv = str_view_trim_left(sv, quoted_sv.m_size);
        }
        else if (
            match_punct(TOKEN_TYPE_LPAREN               ) ||
            match_punct(TOKEN_TYPE_RPAREN               ) ||
            match_punct(TOKEN_TYPE_LBRACKET             ) ||
            match_punct(TOKEN_TYPE_RBRACKET             ) ||
            match_punct(TOKEN_TYPE_LBRACE               ) ||
            match_punct(TOKEN_TYPE_RBRACE               ) ||
            match_punct(TOKEN_TYPE_COMMA                ) ||
            match_punct(TOKEN_TYPE_COLON                ) ||
            match_punct(TOKEN_TYPE_SEMICOLON            ) ||
            match_punct(TOKEN_TYPE_DOT2                 ) ||
            match_punct(TOKEN_TYPE_DOT1                 ) ||
            match_punct(TOKEN_TYPE_PLUS                 ) ||
            match_punct(TOKEN_TYPE_MINUS                ) ||
            match_punct(TOKEN_TYPE_ASTERISK2            ) ||
            match_punct(TOKEN_TYPE_ASTERISK1            ) ||
            match_punct(TOKEN_TYPE_SLASH                ) ||
            match_punct(TOKEN_TYPE_PERCENT              ) ||
            match_punct(TOKEN_TYPE_LESS_THAN2           ) ||
            match_punct(TOKEN_TYPE_GREATER_THAN2        ) ||
            match_punct(TOKEN_TYPE_AMPERSAND            ) ||
            match_punct(TOKEN_TYPE_PIPE                 ) ||
            match_punct(TOKEN_TYPE_CARET                ) ||
            match_punct(TOKEN_TYPE_TILDE                ) ||
            match_punct(TOKEN_TYPE_EQUALS2              ) ||
            match_punct(TOKEN_TYPE_EXCL_EQUALS1         ) ||
            match_punct(TOKEN_TYPE_LESS_THAN1_EQUALS1   ) ||
            match_punct(TOKEN_TYPE_LESS_THAN1           ) ||
            match_punct(TOKEN_TYPE_GREATER_THAN1_EQUALS1) ||
            match_punct(TOKEN_TYPE_GREATER_THAN1        ) ||
            match_punct(TOKEN_TYPE_EQUALS1              )
        ){
            lparen_count   += (punct_token_type == TOKEN_TYPE_LPAREN  );
            rparen_count   += (punct_token_type == TOKEN_TYPE_RPAREN  );
            lbracket_count += (punct_token_type == TOKEN_TYPE_LBRACKET);
            rbracket_count += (punct_token_type == TOKEN_TYPE_RBRACKET);
            lbrace_count   += (punct_token_type == TOKEN_TYPE_LBRACE  );
            rbrace_count   += (punct_token_type == TOKEN_TYPE_RBRACE  );

            if (!token_push_back(punct_token_type, punct_token_id))
                return oom_error();

            usize punct_token_id_len = (usize)strlen(punct_token_id);
            state.pos.m_column += punct_token_id_len;
        }
        else if (isdigit(sv.m_str[0])){
            if (sv.m_size >= 2 && (tolower(sv.m_str[1]) == 'x' || tolower(sv.m_str[1]) == 'b')){
                bool is_hex = (tolower(sv.m_str[1]) == 'x');

                int (*is_fn)(int)     = isxdigit;
                usize digit_max_count = HEX_DIGIT_MAX_COUNT;
                int base              = 16;

                if (!is_hex){
                    is_fn           = is_bin_digit;
                    digit_max_count = BIN_DIGIT_MAX_COUNT;
                    base            = 2;
                }

                state.pos.m_column += 2;
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

                if (!token_push_back(TOKEN_TYPE_INT_LIT, int_buf))
                    return oom_error();

                state.pos.m_column += i;
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
                        if (!next || next == '_' || sv.m_str[i - 1] == '_' || ++dot_count > 1)
                            return syntax_error("Digit separator <_> must not come before or after <.>");
                        if (!str_base_push_back(&decimal_buf, state.alloc, '.'))
                            return oom_error();
                    }
                    else if (isdigit(sv.m_str[i]) && !str_base_push_back(&decimal_buf, state.alloc, sv.m_str[i]))
                        return oom_error();
                }

                char temp = sv.m_str[i - 1];
                if (temp == '_' || isalpha((temp = sv.m_str[i])))
                    return syntax_error("<%s> literal followed by <%c>", (dot_count > 0) ? "float" : "int", temp);

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

                if (!token_push_back((dot_count > 0) ? TOKEN_TYPE_FLOAT_LIT : TOKEN_TYPE_INT_LIT, data))
                    return oom_error();

                state.pos.m_column += i;
                sv = str_view_trim_left(sv, i);
            }
        }
        else if (isalpha(sv.m_str[0]) || sv.m_str[0] == '_'){
            usize id_end_pos = 0;
            while (++id_end_pos < sv.m_size && (isalnum(sv.m_str[id_end_pos]) || sv.m_str[id_end_pos] == '_'));
            Str_view id_sv = str_view_trim_right(sv, sv.m_size - id_end_pos);

            enum Token_type keyword_token_type;
            Str_view keyword_token_sv;
            #define match_keyword(keyword_token_type_val) \
                ( \
                    keyword_token_type = (keyword_token_type_val), \
                    keyword_token_sv = str_view_init(token_type_to_str(keyword_token_type)), \
                    cmp_eq_Str_view(&id_sv, &keyword_token_sv) \
                )

            if (
                match_keyword(TOKEN_TYPE_FALSE   ) ||
                match_keyword(TOKEN_TYPE_TRUE    ) ||
                match_keyword(TOKEN_TYPE_AS      ) ||
                match_keyword(TOKEN_TYPE_AND     ) ||
                match_keyword(TOKEN_TYPE_OR      ) ||
                match_keyword(TOKEN_TYPE_NOT     ) ||
                match_keyword(TOKEN_TYPE_FN      ) ||
                match_keyword(TOKEN_TYPE_LET     ) ||
                match_keyword(TOKEN_TYPE_DEFTYPE ) ||
                match_keyword(TOKEN_TYPE_VOID    ) ||
                match_keyword(TOKEN_TYPE_BOOL    ) ||
                match_keyword(TOKEN_TYPE_CHAR    ) ||
                match_keyword(TOKEN_TYPE_INT     ) ||
                match_keyword(TOKEN_TYPE_FLOAT   ) ||
                match_keyword(TOKEN_TYPE_STR     ) ||
                match_keyword(TOKEN_TYPE_IF      ) ||
                match_keyword(TOKEN_TYPE_ELSE    ) ||
                match_keyword(TOKEN_TYPE_WHILE   ) ||
                match_keyword(TOKEN_TYPE_FOR     ) ||
                match_keyword(TOKEN_TYPE_BREAK   ) ||
                match_keyword(TOKEN_TYPE_CONTINUE) ||
                match_keyword(TOKEN_TYPE_RETURN  )
            ){
                if (!token_push_back(keyword_token_type, keyword_token_sv.m_str))
                    return oom_error();
                state.pos.m_column += id_sv.m_size;
                sv = str_view_trim_left(sv, id_sv.m_size);
            }
            else{
                Str_base_result id = str_base_init_str_view(state.alloc, id_sv);
                if (!(id.success && vec_base_push_back(&state.tokens, state.alloc, &(Token){.m_type = TOKEN_TYPE_ID, .m_id = id.result, .m_pos = state.pos})))
                    return oom_error();
                usize id_size = str_base_size(&id.result);
                state.pos.m_column += id_size;
                sv = str_view_trim_left(sv, id_size);
            }
        }
        else{
            Str_base_result temp = str_base_init_fmt(state.alloc, "Found unknown token <%c>", sv.m_str[0]);
            return (temp.success) ? syntax_error(str_base_data(&temp.result)) : oom_error();
        }
    }

    state.pos.m_line = 0;
    if (lparen_count != rparen_count)
        return syntax_error("Number of opening and closing parentheses must match");
    if (lbracket_count != rbracket_count)
        return syntax_error("Number of opening and closing brackets must match");
    if (lbrace_count != rbrace_count)
        return syntax_error("Number of opening and closing braces must match");

    lexer_state_cleanup(&state);

    return (Lex_result){.tokens = {.m_size = state.tokens.m_size, .m_data = state.tokens.m_data}, .error = LEX_ERROR_NONE};
}
