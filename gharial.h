#ifndef GHARIAL_H
#define GHARIAL_H

#define TRUE  1
#define FALSE 0

#define TYPE_NIL     0
#define TYPE_INTEGER 1

typedef struct datum {
	int type;
	union {
		int integer;
	} value;
} datum;

extern datum GH_NIL_VALUE;

datum* gh_integer(int value);

datum* eval(datum *expr);

void print();

void ep(datum *expr);

#endif

