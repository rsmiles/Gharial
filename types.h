#ifndef TYPES_H
#define TYPES_H

#define TRUE  1
#define FALSE 0

#define TYPE_NIL 0

typedef struct datum{
	int type;
	union value{
		int integer;
	}
} datum;

#endif
