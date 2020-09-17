#ifndef GHARIAL_H
#define GHARIAL_H

#define TRUE  1
#define FALSE 0

#define TYPE_NIL 0

typedef struct datum {
	int type;
	union {
		int integer;
	} value;
} datum;

extern datum NIL_VALUE;

extern datum* input;

datum* eval(datum *expr);

void print();

void ep(datum *expr);

#endif

