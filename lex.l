%{
#include "y.tab.h"
%}

WHITESPACE [\s\t\n\r\f]

%%

{WHITESPACE}+ { }
[nN][iI][lL]  { return TOK_NIL; }
.+            { return TOK_INVALID; }
%%

