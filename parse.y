%{
#include <stdio.h>
#include "gharial.h"

int yyerror(char *ps, ...);
int yylex();
%}

%union {
	int integer;
}

%token <integer> TOK_NIL
%token TOK_INVALID

%start program

%%

program: expr program | /* empty */ ;

expr: atom;

atom: TOK_NIL { ep(&NIL_VALUE); }
	| TOK_INVALID { fprintf(stderr, "Error: invalid token\n"); };

%%

int yyerror(char *ps, ...){
	printf("%s\n", ps);
	return 0;
}

