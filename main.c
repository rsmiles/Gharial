#include <stdio.h>
#include <stdlib.h>
#include <gc.h>

#include "gharial.h"

#include "y.tab.h"

datum GH_NIL_VALUE = { TYPE_NIL, { 0 } };

datum *gh_integer(int value) {
	datum *i = GC_MALLOC(sizeof(datum));
	i->type = TYPE_INTEGER;
	i->value.integer = value;
	return i;
}

datum *gh_decimal(double value) {
	datum *d = GC_MALLOC(sizeof(datum));
	d->type = TYPE_DECIMAL;
	d->value.decimal = value;
	return d;
}

datum *eval(datum *expr) {
	return expr;
}

void print(datum *expr) {
	switch (expr->type) {
		case TYPE_NIL:
			printf("NIL\n");
			break;
		case TYPE_INTEGER:
			printf("%d\n", expr->value.integer);
			break;
		case TYPE_DECIMAL:
			printf("%f\n", expr->value.decimal);
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

