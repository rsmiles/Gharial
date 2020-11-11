%{
#include <stdio.h>
#include "y.tab.h"
#include "gharial.h"

#define YY_INPUT(buf, result, max_size) result = gh_get_input(buf, max_size);

int gh_get_input(char *buf, int max_size) {
	static char *line;
	static int len = 0;
	static int chars_put = 0;


	if (chars_put >= len) {
		line = gh_readline(yyin);
		if (line == NULL) {
			return YY_NULL;
		}

		len = strlen(line);
		chars_put = 0;
	}


	while (chars_put < len) {
		buf[chars_put] = line[chars_put];
		chars_put++;
		if (chars_put >= max_size) {
			return chars_put;
		}
	}
	if (chars_put >= max_size) {
		return chars_put;
	}
	buf[chars_put++] = '\n';
	return chars_put;
}

int fileno(FILE *stream);

int depth = 0;

%}

%option noinput
%option nounput
%option yylineno

WHITESPACE [ \t\r\f\n]
DIGIT      [0-9]
LETTER     [A-Za-z]
SPECIAL    [\&\|\$\+\-\*\/\^=\?\!_\.]

%%

(#|;).*$              { /* Ignore comments */ }
{WHITESPACE}+         { /* Ignore whitespace */ }
['`]                  { return TOK_QUOTE; }
"@"                   { return TOK_UNQUOTESPLICE; }
","                   { return TOK_UNQUOTE; }
"t"                   { return TOK_TRUE; }
"nil"                 { return TOK_NIL; }
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
<<EOF>>               { return TOK_EOF; }
({LETTER}|{SPECIAL}|\.)+({LETTER}|{SPECIAL}|{DIGIT}|\.)* { return TOK_SYMBOL; }

%%

