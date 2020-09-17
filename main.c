#include <stdio.h>
#include <stdlib.h>
#include <gc.h>

#include "gharial.h"

#include "y.tab.h"

datum NIL_VALUE = { TYPE_NIL, { 0 } };

datum *input;

datum *eval(datum *expr) {
	return expr;
}

void print(datum *expr) {
	switch (expr->type) {
		case TYPE_NIL:
			printf("NIL\n");
			break;
		default:
			fprintf(stderr, "Error: Unkown data type: %d\n", expr->type);
	}
}

void ep(datum *expr) {
	print(eval(expr));
}

int main(int argc, char **argv) {
	yyparse();
	return 0;
}

