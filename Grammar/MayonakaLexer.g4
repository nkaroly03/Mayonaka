lexer grammar MayonakaLexer;

KW_FALSE    : 'false'    ;
KW_TRUE     : 'true'     ;
KW_NOT      : 'not'      ;
KW_AS       : 'as'       ;
KW_AND      : 'and'      ;
KW_OR       : 'or'       ;
KW_FN       : 'fn'       ;
KW_LET      : 'let'      ;
KW_DEFTYPE  : 'deftype'  ;
KW_VOID     : 'void'     ;
KW_BOOL     : 'bool'     ;
KW_CHAR     : 'char'     ;
KW_INT      : 'int'      ;
KW_FLOAT    : 'float'    ;
KW_STR      : 'str'      ;
KW_IF       : 'if'       ;
KW_ELSE     : 'else'     ;
KW_WHILE    : 'while'    ;
KW_FOR      : 'for'      ;
KW_BREAK    : 'break'    ;
KW_CONTINUE : 'continue' ;
KW_RETURN   : 'return'   ;

PUNCT_LPAREN    : '('  ;
PUNCT_RPAREN    : ')'  ;
PUNCT_LBRACKET  : '['  ;
PUNCT_RBRACKET  : ']'  ;
PUNCT_LBRACE    : '{'  ;
PUNCT_RBRACE    : '}'  ;
PUNCT_COMMA     : ','  ;
PUNCT_COLON     : ':'  ;
PUNCT_SEMICOLON : ';'  ;
PUNCT_PLUS      : '+'  ;
PUNCT_MINUS     : '-'  ;
PUNCT_TILDE     : '~'  ;
PUNCT_DOT2      : '..' ;
PUNCT_DOT1      : '.'  ;
PUNCT_ASTERISK2 : '**' ;
PUNCT_ASTERISK1 : '*'  ;
PUNCT_SLASH     : '/'  ;
PUNCT_PERCENT   : '%'  ;
PUNCT_LT2       : '<<' ;
PUNCT_LT1_EQ1   : '<=' ;
PUNCT_LT1       : '<'  ;
PUNCT_GT2       : '>>' ;
PUNCT_GT1_EQ1   : '>=' ;
PUNCT_GT1       : '>'  ;
PUNCT_EQ2       : '==' ;
PUNCT_EQ1       : '='  ;
PUNCT_EXCL_EQ1  : '!=' ;
PUNCT_AMPERSAND : '&'  ;
PUNCT_CARET     : '^'  ;
PUNCT_PIPE      : '|'  ;

IDENTIFIER : Identifier_letter (Identifier_letter | Digit)* ;

CHAR_LITERAL  : '\'' Char_char '\'' ;
INT_LITERAL   : Int_literal_bin | Int_literal_hex | Int_literal_dec ;
FLOAT_LITERAL : Int_literal_dec '.' Int_literal_dec ;
STR_LITERAL   : '"' Str_char* '"' ([ \n\r\t]+ '"' Str_char* '"')* ;

fragment Identifier_letter : [A-Za-z_] ;
fragment Digit : [0-9] ;
fragment Oct_digit : [0-7] ;
fragment Hex_digit : Digit | [A-Fa-f] ;

fragment Escape_sequence : '\\' (['"?\\abfnrtv] | [0-3] Oct_digit Oct_digit | Oct_digit Oct_digit? | 'x' Hex_digit+) ;

fragment Char_char : ~['\\\r\n] | Escape_sequence ;
fragment Str_char  : ~["\\\r\n] | Escape_sequence ;

fragment Int_literal_bin : '0' [bB] [01] ([01_]* [01] | [01]?) ;
fragment Int_literal_hex : '0' [xX] Hex_digit ((Hex_digit | '_')* Hex_digit | Hex_digit?) ;
fragment Int_literal_dec : Digit ((Digit | '_')* Digit | Digit?) ;

WS : [ \n\r\t]+ -> skip ;

COMMENT : ('/*/' .*? '/*/' | '//' ~[\r\n]*) -> skip ;
