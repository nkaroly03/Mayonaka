parser grammar MayonakaParser;

options{
    tokenVocab=MayonakaLexer;
}

program : stmt* EOF ;

stmt : block_stmt | expr_stmt | decl_stmt | selection_stmt | iteration_stmt | jump_stmt ;

block_stmt : PUNCT_LBRACE stmt* PUNCT_RBRACE ;
expr_stmt : expr? PUNCT_SEMICOLON ;
decl_stmt : decl_fn | decl_var | decl_type ;

decl_fn : KW_FN IDENTIFIER PUNCT_LPAREN id_colon_type_list? PUNCT_RPAREN (type | KW_VOID) block_stmt ;
decl_var : KW_LET IDENTIFIER PUNCT_COLON type PUNCT_EQ1 expr PUNCT_SEMICOLON ;
decl_type : KW_DEFTYPE IDENTIFIER PUNCT_LBRACE id_colon_type_list? PUNCT_RBRACE ;

selection_stmt : KW_IF PUNCT_LPAREN expr PUNCT_RPAREN stmt (KW_ELSE stmt)? ;
iteration_stmt : while_stmt | for_stmt ;

while_stmt : label? KW_WHILE PUNCT_LPAREN expr PUNCT_RPAREN (PUNCT_COLON PUNCT_LPAREN (expr | block_stmt) PUNCT_RPAREN)? stmt ;
for_stmt : label? KW_FOR PUNCT_LPAREN expr PUNCT_DOT2 expr PUNCT_RPAREN PUNCT_PIPE IDENTIFIER PUNCT_PIPE stmt ;

label : IDENTIFIER PUNCT_COLON ;

jump_stmt : ((KW_BREAK | KW_CONTINUE) IDENTIFIER? | KW_RETURN expr?) PUNCT_SEMICOLON ;

assignment_expr : or_expr (PUNCT_EQ1 or_expr)* ;
or_expr : and_expr (KW_OR and_expr)* ;
and_expr : bor_expr (KW_AND bor_expr)* ;
bor_expr : xor_expr (PUNCT_PIPE xor_expr)* ;
xor_expr : band_expr (PUNCT_CARET band_expr)* ;
band_expr : cmp_eq_expr (PUNCT_AMPERSAND cmp_eq_expr)* ;
cmp_eq_expr : cmp_rel_expr ((PUNCT_EQ2 | PUNCT_EXCL_EQ1) cmp_rel_expr)* ;
cmp_rel_expr : shift_expr ((PUNCT_LT1_EQ1 | PUNCT_LT1 | PUNCT_GT1_EQ1 | PUNCT_GT1) shift_expr)* ;
shift_expr : add_expr ((PUNCT_LT2 | PUNCT_GT2) add_expr)* ;
add_expr : mul_expr ((PUNCT_PLUS | PUNCT_MINUS) mul_expr)* ;
mul_expr : as_expr ((PUNCT_ASTERISK1 | PUNCT_SLASH | PUNCT_PERCENT) as_expr)* ;
as_expr : unary_expr (KW_AS type unary_expr?)* ;
unary_expr : (PUNCT_PLUS | PUNCT_MINUS | PUNCT_TILDE | KW_NOT)* exp_expr ;
exp_expr : postfix_expr (PUNCT_ASTERISK2 unary_expr)? ;
postfix_expr : primary_expr (PUNCT_DOT1 IDENTIFIER | PUNCT_LBRACKET expr PUNCT_RBRACKET | PUNCT_LPAREN expr_list? PUNCT_RPAREN)* ;
primary_expr : IDENTIFIER | literal | PUNCT_LPAREN expr PUNCT_RPAREN ;

literal : KW_FALSE | KW_TRUE | CHAR_LITERAL | INT_LITERAL | FLOAT_LITERAL | STR_LITERAL | obj_literal | init_list ;

obj_literal : IDENTIFIER PUNCT_LBRACE obj_literal_field_init_list? PUNCT_RBRACE ;
obj_literal_field_init_list : obj_literal_field_init (PUNCT_COMMA obj_literal_field_init)* PUNCT_COMMA? ;
obj_literal_field_init : PUNCT_DOT1 IDENTIFIER PUNCT_EQ1 expr ;

init_list : PUNCT_LBRACKET expr_list? PUNCT_RBRACKET ;

expr_list : expr (PUNCT_COMMA expr)* PUNCT_COMMA? ;
expr : assignment_expr ;

id_colon_type_list : id_colon_type (PUNCT_COMMA id_colon_type)* PUNCT_COMMA? ;
id_colon_type : IDENTIFIER PUNCT_COLON type ;

type : (PUNCT_LBRACKET PUNCT_RBRACKET)* (KW_BOOL | KW_CHAR | KW_INT | KW_FLOAT | KW_STR | IDENTIFIER) ;
