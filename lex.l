%{
#include "y.tab.h"
%}

WHITESPACE [\s\t\n\r\f]
DIGIT      [0-9]

%%

{WHITESPACE}+      { }
[nN][iI][lL]       { return TOK_NIL; }
{DIGIT}+\.{DIGIT}+ { return TOK_DECIMAL; }
{DIGIT}+           { return TOK_INTEGER; }
\"[^\"]*\"         { return TOK_STRING; }
%%

