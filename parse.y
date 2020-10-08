%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gharial.h"

int yyerror(char *ps, ...);
int yylex();
extern int depth;
extern char* yytext;

%}

%union{
	struct datum *t_datum;
	int integer;
	double decimal;
	char character;
	char *string;
	char *symbol;
}

%token <gh_datum>  TOK_NIL
%token <gh_datum>  TOK_TRUE
%token <gh_datum>  TOK_EOF
%token <decimal>   TOK_DECIMAL
%token <integer>   TOK_INTEGER
%token <character> TOK_OPENPAREN
%token <character> TOK_CLOSEPAREN
%token <character> TOK_OPENSQUARE
%token <character> TOK_CLOSESQUARE
%token <character> TOK_OPENCURLY
%token <character> TOK_CLOSECURLY
%token <character> TOK_DOT
%token <character> TOK_QUOTE
%token <character> TOK_UNQUOTE
%token <string>    TOK_EMPTYSTRING
%token <string>    TOK_STRING
%token <string>    TOK_SYMBOL

%type <t_datum> atom list expr term dotted_body list_body

%start prog

%%

prog: terms;

terms: term terms| /* empty */ ;

term: expr {  
				if (depth == 0) {
					gh_input = $$;
					if (repl) {
						gh_print(eval($$, &locals));
						prompt();
					}
					YYACCEPT;
				}
			};

expr: atom
	| list
	| TOK_QUOTE expr { $$ = cons(gh_symbol("quote"), cons($2, &GH_NIL_VALUE)); }
	| TOK_UNQUOTE expr { $$ = cons(gh_symbol("unquote"), cons($2, &GH_NIL_VALUE)); };

atom: TOK_NIL         { $$ = &GH_NIL_VALUE; }
	| TOK_TRUE        { $$ = &GH_TRUE_VALUE; }
	| TOK_EOF         { $$ = &GH_EOF_VALUE; }
	| TOK_DECIMAL     { $$ = gh_decimal(atof(yytext)); }
	| TOK_INTEGER     { $$ = gh_integer(atoi(yytext)); }
	| TOK_EMPTYSTRING { $$ = gh_string(""); }
	| TOK_STRING      { $$ = gh_substring(1, strlen(yytext) - 2, yytext); }
	| TOK_SYMBOL      { $$ = gh_symbol(yytext); };

list: TOK_OPENPAREN TOK_CLOSEPAREN               { $$ = &GH_NIL_VALUE; }
	| TOK_OPENPAREN dotted_body TOK_CLOSEPAREN   { $$ = $2; }
	| TOK_OPENPAREN list_body TOK_CLOSEPAREN     { $$ = $2; }
    | TOK_OPENSQUARE TOK_CLOSESQUARE             { $$ = &GH_NIL_VALUE; }
	| TOK_OPENSQUARE dotted_body TOK_CLOSESQUARE { $$ = $2; }
	| TOK_OPENSQUARE list_body TOK_CLOSESQUARE   { $$ = $2; }
    | TOK_OPENCURLY TOK_CLOSECURLY              { $$ = &GH_NIL_VALUE; }
	| TOK_OPENCURLY dotted_body TOK_CLOSECURLY   { $$ = $2; }
	| TOK_OPENCURLY list_body TOK_CLOSECURLY     { $$ = $2; };

dotted_body: expr TOK_DOT expr { $$ = cons($1, $3); }
		   | expr dotted_body { $$ = cons($1, $2); };

list_body: expr { $$ = cons($1, &GH_NIL_VALUE); }
		 | expr list_body { $$ = cons($1, $2); };

%%

int yyerror(char *ps, ...){
	printf("%s\n", ps);
	return 0;
}

