#ifndef GHARIAL_H
#define GHARIAL_H

#define TRUE  1
#define FALSE 0

#define TYPE_NIL     0
#define TYPE_INTEGER 1
#define TYPE_DECIMAL 2
#define TYPE_STRING  3
#define TYPE_SYMBOL  4
#define TYPE_CONS    5
#define TYPE_CFORM   6
#define TYPE_CFUNC   7

typedef struct datum {
	int type;
	union {
		int integer;
		double decimal;
		char *string;
		struct {
			struct datum *(*func)(struct datum **);
			struct datum *args;
		} c_code;
		struct {
			struct datum *car;
			struct datum *cdr;
		} cons;
		
	} value;
} datum;

extern datum GH_NIL_VALUE;

datum *gh_integer(int value);

datum *gh_decimal(double value);

datum *gh_string(char *value);

datum *gh_substring(int start, int end, char *value);

datum *gh_symbol(char *value);

datum *cons(datum *car, datum *cdr);

int gh_assert(int cond, char *mesg);

datum *gh_eval(datum *expr);

void gh_print(datum *expr);

void prompt();

#endif

