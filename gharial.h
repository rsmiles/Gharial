#ifndef GHARIAL_H
#define GHARIAL_H

#define TRUE  1
#define FALSE 0

#define TYPE_NIL     0
#define TYPE_INTEGER 1
#define TYPE_DECIMAL 2
#define TYPE_STRING  3

typedef struct datum {
	int type;
	union {
		int integer;
		double decimal;
		char *string;
	} value;
} datum;

extern datum GH_NIL_VALUE;

datum* gh_integer(int value);

datum* gh_decimal(double value);

datum* gh_string(char *value);

datum* eval(datum *expr);

void print();

void ep(datum *expr);

#endif

