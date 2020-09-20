%{
#include <stdio.h>
#include <stdlib.h>
#include "gharial.h"

int yyerror(char *ps, ...);
int yylex();
extern char* yytext;

%}

%union{
	struct datum *t_datum;
	int integer;
	double decimal;
	char character;
	char *string;
}



%token <gh_datum> TOK_NIL
%token <decimal> TOK_DECIMAL
%token <integer> TOK_INTEGER
%token <string>  TOK_STRING
%token <character> TOK_OPENPAREN
%token <character> TOK_CLOSEPAREN
%token <character> TOK_DOT

%type <t_datum> atom list expr

%start prog

%%

prog: exprs

exprs: expr exprs | /* empty */ ;

expr: atom { print(eval($$)); }
	| list { print(eval($$)); };

atom: TOK_NIL     { $$ = &GH_NIL_VALUE; }
	| TOK_DECIMAL { $$ = gh_decimal(atof(yytext)); }
	| TOK_INTEGER { $$ = gh_integer(atoi(yytext)); }
	| TOK_STRING  { $$ = gh_string(yytext); };

list: TOK_OPENPAREN TOK_CLOSEPAREN { $$ = &GH_NIL_VALUE; }
	| TOK_OPENPAREN expr TOK_DOT expr TOK_CLOSEPAREN { $$ = gh_cons($2, $4); };

%%

int yyerror(char *ps, ...){
	printf("%s\n", ps);
	return 0;
}

