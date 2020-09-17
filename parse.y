%{
#include <stdio.h>
#include <stdlib.h>
#include "gharial.h"

int yyerror(char *ps, ...);
int yylex();
extern char* yytext;
%}

%union {
	int integer;
}

%token <integer> TOK_NIL
%token <integer> TOK_INTEGER
%token TOK_INVALID

%start program

%%

program: expr program | /* empty */ ;

expr: atom;

atom: TOK_NIL     { ep(&GH_NIL_VALUE); }
	| TOK_INTEGER { ep(gh_integer(atoi(yytext))); }
	| TOK_INVALID { fprintf(stderr, "Error: invalid token\n"); };

%%

int yyerror(char *ps, ...){
	printf("%s\n", ps);
	return 0;
}

