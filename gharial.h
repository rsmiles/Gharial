#ifndef GHARIAL_H
#define GHARIAL_H

#include <stdio.h>

#define TRUE  1
#define FALSE 0

#define TYPE_NIL     0
#define TYPE_TRUE    1
#define TYPE_INTEGER 2
#define TYPE_DECIMAL 3
#define TYPE_STRING  4
#define TYPE_SYMBOL  5
#define TYPE_CONS    6
#define TYPE_CFORM   7
#define TYPE_CFUNC   8
#define TYPE_FUNC    9
#define TYPE_MACRO   10
#define TYPE_RECUR   11
#define TYPE_FILE    12
#define TYPE_EOF     13

typedef struct datum {
	int type;
	union {
		int integer;
		double decimal;
		char *string;
		FILE *file;
		struct {
			struct datum *(*func)(struct datum **);
			struct datum *lambda_list;
		} c_code;
		struct {
			struct datum *car;
			struct datum *cdr;
		} cons;
		struct {
			struct datum *lambda_list;
			struct datum *body;
			struct datum **closure;
		} func;
		struct {
			struct datum *bindings;
		} recur;
		
	} value;
} datum;

extern int repl;
extern int silent;
extern FILE *yyin;
extern datum GH_NIL_VALUE;
extern datum GH_TRUE_VALUE;
extern datum GH_EOF_VALUE;
extern datum *locals;
extern datum *gh_input;

datum *gh_integer(int value);

datum *gh_decimal(double value);

datum *gh_string(char *value);

datum *gh_substring(int start, int end, char *value);

datum *gh_symbol(char *value);

datum *cons(datum *car, datum *cdr);

int gh_assert(int cond, char *mesg);

void gh_print(FILE *file, datum *expr);

datum *eval(datum *expr, datum **locals);

void prompt();

#endif

