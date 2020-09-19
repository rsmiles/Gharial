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

%type <t_datum> atom list expr

%start program

%%

program: expr program | /* empty */ ;

expr: atom { ep($$); }
	| list { ep($$); }

atom: TOK_NIL     { $$ = &GH_NIL_VALUE; }
	| TOK_DECIMAL { $$ = gh_decimal(atof(yytext)); }
	| TOK_INTEGER { $$ = gh_integer(atoi(yytext)); }
	| TOK_STRING  { $$ = gh_string(yytext); };

list: TOK_OPENPAREN TOK_CLOSEPAREN { $$ = &GH_NIL_VALUE; };

%%

int yyerror(char *ps, ...){
	printf("%s\n", ps);
	return 0;
}

