#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gc.h>

#include "gharial.h"

#include "y.tab.h"

datum GH_NIL_VALUE = { TYPE_NIL, { 0 } };

void print_datum(datum *expr);

datum *gh_integer(int value) {
	datum *i;
	i = GC_MALLOC(sizeof(datum));
	i->type = TYPE_INTEGER;
	i->value.integer = value;
	return i;
}

datum *gh_decimal(double value) {
	datum *d;
	d = GC_MALLOC(sizeof(datum));
	d->type = TYPE_DECIMAL;
	d->value.decimal = value;
	return d;
}

datum *gh_string(char* value) {
	datum *s;
	s = GC_MALLOC(sizeof(datum));
	s->type = TYPE_STRING;
	s->value.string = GC_MALLOC(sizeof(char) * (strlen(value) + 1));
	strcpy(s->value.string, value);
	return s;
}


datum *gh_symbol(char* value) {
	datum *s;
	s = GC_MALLOC(sizeof(datum));
	s->type = TYPE_SYMBOL;
	s->value.symbol = GC_MALLOC(sizeof(char) * (strlen(value) + 1));
	strcpy(s->value.symbol, value);
	return s;
}

datum *gh_cons(datum *car, datum *cdr) {
	datum *c;
	c = GC_MALLOC(sizeof(datum));
	c->type = TYPE_CONS;
	c->value.cons.car = car;
	c->value.cons.cdr = cdr;
	return c;
}

datum *eval(datum *expr) {
	return expr;
}

void print_datum(datum *expr) {
	switch (expr->type) {
		case TYPE_NIL:
			printf("NIL");
			break;
		case TYPE_INTEGER:
			printf("%d", expr->value.integer);
			break;
		case TYPE_DECIMAL:
			printf("%f", expr->value.decimal);
			break;
		case TYPE_STRING:
			printf("\"%s\"", expr->value.string);
			break;
		case TYPE_SYMBOL:
			printf("%s", expr->value.symbol);
			break;
		case TYPE_CONS:
			printf("(");
			print_datum(expr->value.cons.car);
			printf(" . ");
			print_datum(expr->value.cons.cdr);
			printf(")");
			break;
		default:
			fprintf(stderr, "Error: Unkown data type: %d\n", expr->type);
	}
}

void print(datum *expr){
	print_datum(expr);
	printf("\n");
}

int main(int argc, char **argv) {
	yyparse();
	return 0;
}

