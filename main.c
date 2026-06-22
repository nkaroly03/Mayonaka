#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdalign.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wchar.h>

#include "hdrs/Allocator/Aligned_malloc.h"
#include "hdrs/Allocator/Allocator.h"
#include "hdrs/Allocator/Arena.h"
#include "hdrs/Allocator/Page.h"
#include "hdrs/Allocator/Raw_malloc.h"
#include "hdrs/Data_structure/Slist.h"
#include "hdrs/Data_structure/Slist_base.h"
#include "hdrs/Data_structure/Str.h"
#include "hdrs/Data_structure/Str_base.h"
#include "hdrs/Data_structure/Str_view.h"
#include "hdrs/Data_structure/Umap.h"
#include "hdrs/Data_structure/Umap_base.h"
#include "hdrs/Data_structure/Vec.h"
#include "hdrs/Data_structure/Vec_base.h"
#include "hdrs/Utils/Cmp.h"
#include "hdrs/Utils/Hash.h"
#include "hdrs/Utils/Num.h"
#include "hdrs/Utils/Utils.h"

#include "hdrs/Lang/Compiler.h"
#include "hdrs/Lang/Lexer.h"
#include "hdrs/Lang/Parser.h"

int main(const int argc, const char *const *const argv){
    if (argc < 2){
        fprintf(stderr, "<%s> requires 1 argument <filepath>\n", argv[0]);
        return 1;
    }

    Arena arena = arena_init(raw_malloc_allocator());

    Str_base error_info;

    Lex_result lex_result = lex(&arena, argv[1]);
    switch (lex_result.error){
        case LEX_ERROR_NONE:
            break;
        case LEX_ERROR_OOM:
            goto oom_error;
        case LEX_ERROR_FILE:
            fprintf(stderr, "\x1b[38;2;255;0;0m");
            fprintf(stderr, "%s\n", str_base_data(&lex_result.error_info));
            if (errno != 0){
                fprintf(stderr, "perror msg:\n\t");
                perror(argv[1]);
            }
            fprintf(stderr, "\x1b[0m");
            arena_deinit(&arena);
            return 1;
        case LEX_ERROR_SYNTAX:
            error_info = lex_result.error_info;
            goto syntax_error;
    }

    token_slice_print(lex_result.tokens);
    printf("------------------------------------------------------------------------------------------------\n");

    Parse_result parse_result = parse(&arena, lex_result.tokens);
    switch (parse_result.error){
        case PARSE_ERROR_NONE:
            break;
        case PARSE_ERROR_OOM:
            goto oom_error;
        case PARSE_ERROR_SYNTAX:
            error_info = parse_result.error_info;
            goto syntax_error;
    }

    ast_node_ptr_slice_print(parse_result.ast_nodes);
    printf("------------------------------------------------------------------------------------------------\n");

    Compile_to_IR_result compile_to_IR_result = compile_to_IR(&arena, parse_result.ast_nodes);

    switch (compile_to_IR_result.error){
        case COMPILE_ERROR_NONE:
            break;
        case COMPILE_ERROR_OOM:
            goto oom_error;
        case COMPILE_ERROR_SYNTAX:
            error_info = compile_to_IR_result.error_info;
            goto syntax_error;
    }

    printf("%s", str_base_data(&compile_to_IR_result.IR));
    printf("------------------------------------------------------------------------------------------------\n");

    arena_deinit(&arena);

    printf("\n---test success---\n");
    return 0;

oom_error:
    fprintf(stderr, "\x1b[38;2;255;0;0mOut of memory\n\x1b[0m");
    arena_deinit(&arena);
    return 1;
syntax_error:
    fprintf(stderr, "\x1b[38;2;255;0;0m%s\x1b[0m", str_base_data(&error_info));
    arena_deinit(&arena);
    return 1;
}
