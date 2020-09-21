%{
#include "y.tab.h"
int depth = 0;
%}

WHITESPACE [ \t\n\r\f]
DIGIT      [0-9]

%%

{WHITESPACE}+      { }
[nN][iI][lL]       { return TOK_NIL; }
{DIGIT}+\.{DIGIT}+ { return TOK_DECIMAL; }
{DIGIT}+           { return TOK_INTEGER; }
\"[^\"]*[^\\]\"    { return TOK_STRING; }
"("                { depth++; return TOK_OPENPAREN; }
")"                { if (depth > 0 ) depth--; return TOK_CLOSEPAREN; }
"."                { return TOK_DOT; }

%%

