#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gc.h>

#include "gharial.h"

#include "y.tab.h"

datum GH_NIL_VALUE = { TYPE_NIL, { 0 } };

datum *new_datum();


int length(datum *list);

void print_datum(datum *expr);

datum *gh_cfunc(datum *(*addr)(datum **locals));

datum *symbol_loc(datum *table, char *symbol);
datum *symbol_get(datum *table, char *symbol);
void  symbol_set(datum **table, char *symbol, datum *value);
void  symbol_unset(datum **table, char *symbol);

datum *globals = &GH_NIL_VALUE;

datum *new_datum() {
	datum *d = GC_MALLOC(sizeof(datum));
	gh_assert(d != NULL, "Out of memory!");
	return d;
}

datum *gh_integer(int value) {
	datum *i;
	i = new_datum();
	i->type = TYPE_INTEGER; i->value.integer = value;
	return i;
}

datum *gh_decimal(double value) {
	datum *d;
	d = new_datum();
	d->type = TYPE_DECIMAL;
	d->value.decimal = value;
	return d;
}

datum *gh_string(char* value) {
	datum *s;
	s = new_datum();
	s->type = TYPE_STRING;
	s->value.string = GC_MALLOC(sizeof(char) * (strlen(value) + 1));
	strcpy(s->value.string, value);
	return s;
}

datum *gh_substring(int start, int end, char *value) {
	datum *s;
	int len;
	len = end - start + 1;
	s = new_datum();
	s->type = TYPE_STRING;
	s->value.string = GC_MALLOC(sizeof(char) * (len + 1));
	strncpy(s->value.string, value + start, len);
	s->value.string[len] = '\0';
	return s;
}

datum *gh_symbol(char* value) {
	datum *s;
	s = new_datum();
	s->type = TYPE_SYMBOL;
	s->value.symbol = GC_MALLOC(sizeof(char) * (strlen(value) + 1));
	strcpy(s->value.symbol, value);
	return s;
}

datum *gh_cons(datum *car, datum *cdr) {
	datum *c;
	c = new_datum();
	c->type = TYPE_CONS;
	c->value.cons.car = car;
	c->value.cons.cdr = cdr;
	return c;
}

int gh_assert(int cond, char *mesg) {
	if ( ! cond ) {
		fprintf(stderr, "ERROR: %s\n", mesg);
		exit(EXIT_FAILURE);
	} else {
		return TRUE;
	}
}

datum *gh_eval(datum *expr) {
	char  *symbol;
	datum *value;

	switch (expr->type) {
		case TYPE_CONS:
			gh_assert(expr->value.cons.car->type == TYPE_SYMBOL, "Not a function");
			symbol = expr->value.cons.car->value.symbol;
			value = symbol_get(globals, symbol);
			gh_assert(value->type == TYPE_CFUNC, "Not a function");
			return value->value.cfunc(&expr->value.cons.cdr);
			break;
		case TYPE_SYMBOL:
			symbol = expr->value.symbol;
			value = symbol_get(globals, symbol);
			gh_assert(value != NULL, "Undefined variable");
			return value;
		default:
			return expr;
	}
}

void print_datum(datum *expr) {
	datum *iterator;
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
		case TYPE_CFUNC:
			printf("<c_function>");
			break;
		case TYPE_CONS:
			iterator = expr;
	
			printf("(");

			print_datum(iterator->value.cons.car);
			while(iterator->value.cons.cdr->type == TYPE_CONS) {
				printf(" ");
				iterator = iterator->value.cons.cdr;
				print_datum(iterator->value.cons.car);
			}

			if (iterator->value.cons.cdr->type != TYPE_NIL) {
				printf(" . ");
				print_datum(iterator->value.cons.cdr);
			}

			printf(")");
			break;
		default:
			fprintf(stderr, "Error: Unkown data type: %d\n", expr->type);
	}
}

void gh_print(datum *expr){
	print_datum(expr);
	printf("\n");
}

datum *symbol_loc(datum *table, char *symbol) {
	datum *iterator;

	iterator = table;
	while (iterator->type != TYPE_NIL) {
		datum *current;

		current = iterator->value.cons.car;
		if (strcmp(current->value.cons.car->value.symbol, symbol) == 0)
			return current;
		iterator = iterator->value.cons.cdr;
	}
	return NULL;
}

datum *symbol_get(datum *table, char *symbol) {
	datum *loc;
	loc = symbol_loc(table, symbol);
	if (loc)
		return loc->value.cons.cdr;
	else
		return loc;
}

void symbol_set(datum **table, char *symbol, datum *value) {
	datum *loc;
	loc = symbol_loc(*table, symbol);
	if (loc) {
		loc->value.cons.cdr = value;
	} else {
		*table = gh_cons(gh_cons(gh_symbol(symbol), value), *table);
	}
}

void symbol_unset(datum **table, char *symbol) {
	datum *iterator = NULL;
	datum *prev = NULL;

	iterator = *table;
	while (iterator->type != TYPE_NIL) {
		datum *current = NULL;
		prev = current;
		current = iterator->value.cons.car;
		if (current->value.cons.car->value.symbol == symbol) {
			if (prev) {
				prev->value.cons.cdr = iterator->value.cons.cdr;
			} else
				*table = iterator->value.cons.cdr;
		}
	}
}

datum *gh_set(datum **locals) {
	char *symbol;
	datum *value;

	printf("gh_set\n");
	symbol = symbol_get(*locals, "symbol")->value.symbol;
	value = symbol_get(*locals, "value");

	symbol_set(&globals, symbol, value);
	return &GH_NIL_VALUE;
}

datum *gh_cfunc(datum *(*addr)(datum **)) {
	datum *cf = new_datum(sizeof(datum));
	cf->type = TYPE_CFUNC;
	cf->value.cfunc = addr;
	return cf;
}

int main(int argc, char **argv) {
	symbol_set(&globals, "set", gh_cfunc(&gh_set));
	yyparse();
	return 0;
}

