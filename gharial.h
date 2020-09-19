#ifndef GHARIAL_H
#define GHARIAL_H

#define TRUE  1
#define FALSE 0

#define TYPE_NIL     0
#define TYPE_INTEGER 1
#define TYPE_DECIMAL 2
#define TYPE_STRING  3
#define TYPE_CONS    4

typedef struct datum {
	int type;
	union {
		int integer;
		double decimal;
		char *string;
		struct {
			struct datum *car;
			struct datum *cdr;
		} cons;
	} value;
} datum;

extern datum GH_NIL_VALUE;

datum* gh_integer(int value);

datum* gh_decimal(double value);

datum* gh_string(char *value);

datum *gh_cons(datum *car, datum *cdr);

datum* eval(datum *expr);

void print(datum *expr);

void ep(datum *expr);

#endif

