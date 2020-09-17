%{
#include <stdio.h>
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

atom: TOK_NIL { printf("nil\n"); }
	| TOK_INVALID { printf("invalid\n"); };

%%

int yyerror(char *ps, ...){
	printf("%s\n", ps);
	return 0;
}

