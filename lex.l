%{
#include "y.tab.h"
%}

WHITESPACE [ \t\n\r\f]
DIGIT      [0-9]

%%

{WHITESPACE}+      { }
[nN][iI][lL]       { return TOK_NIL; }
{DIGIT}+\.{DIGIT}+ { return TOK_DECIMAL; }
{DIGIT}+           { return TOK_INTEGER; }
\"[^\"]*[^\\]\"    { return TOK_STRING; }
"("                { return TOK_OPENPAREN; }
")"                { return TOK_CLOSEPAREN; }
"."                { return TOK_DOT; }

%%

