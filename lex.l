%{
#include <stdio.h>
#include "y.tab.h"
#include "gharial.h"
int fileno(FILE *stream);
int depth = 0;
%}

%option noinput
%option nounput

WHITESPACE [ \t\n\r\f]
DIGIT      [0-9]
LETTER     [A-Za-z]
SPECIAL    [\+\-\*\/\^=\?]

%%

"#".*$                { /* Ignore comments */ }
{WHITESPACE}+         { /* Ignore whitespace */ }
['`]                  { return TOK_QUOTE; }
","                   { return TOK_UNQUOTE; }
[tT]                  { return TOK_TRUE; }
[nN][iI][lL]          { return TOK_NIL; }
{DIGIT}+\.{DIGIT}+    { return TOK_DECIMAL; }
{DIGIT}+              { return TOK_INTEGER; }
\"[^\"]*[^\\]\"       { return TOK_STRING; }
"\"\""                { return TOK_EMPTYSTRING; }
"("                   { depth++; return TOK_OPENPAREN; }
")"                   { if (depth > 0 ) depth--; return TOK_CLOSEPAREN; }
"["                   { depth++; return TOK_OPENSQUARE; }
"]"                   { if (depth > 0 ) depth--; return TOK_CLOSESQUARE; }
"{"                   { depth++; return TOK_OPENCURLY; }
"}"                   { if (depth > 0 ) depth--; return TOK_CLOSECURLY; }
"."                   { return TOK_DOT; }
({LETTER}|{SPECIAL})+ { return TOK_SYMBOL; }
<<EOF>>               { if (!repl) return TOK_EOF; else yyterminate();}

%%

