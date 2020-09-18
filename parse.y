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
	double decimal;
	char *string;
}

%token <integer>   TOK_NIL
%token <decimal>   TOK_DECIMAL
%token <integer>   TOK_INTEGER
%token <string>    TOK_STRING

%start program

%%

program: expr program | /* empty */ ;

expr: atom;

atom: TOK_NIL     { ep(&GH_NIL_VALUE); }
	| TOK_DECIMAL { ep(gh_decimal(atof(yytext))); }
	| TOK_INTEGER { ep(gh_integer(atoi(yytext))); }
	| TOK_STRING  { ep(gh_string(yytext)); };

%%

int yyerror(char *ps, ...){
	printf("%s\n", ps);
	return 0;
}

