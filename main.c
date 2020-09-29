#define _POSIX_C_SOURCE 1

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#include <gc.h>

#include "gharial.h"

#include "y.tab.h"

datum GH_NIL_VALUE = { TYPE_NIL, { 0 } };

datum *new_datum();

void print_datum(datum *expr);

datum *gh_cform(datum *(*addr)(datum **locals), datum *args);
datum *gh_cfunc(datum *(*addr)(datum **locals), datum *args);

datum *symbol_loc(datum *table, char *symbol);
datum *symbol_get(datum *table, char *symbol);
void  symbol_set(datum **table, char *symbol, datum *value);
void  symbol_unset(datum **table, char *symbol);

datum *do_unquotes(datum *expr);

datum *eval_form(datum* func, datum *args);

datum *gh_set(datum **locals);

datum *gh_quote(datum **locals);

datum *gh_unquote(datum **locals);

datum *gh_car(datum **locals);

datum *gh_cdr(datum **locals);

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
	s->value.string = GC_MALLOC(sizeof(char) * (strlen(value) + 1));
	strcpy(s->value.string, value);
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
			symbol = expr->value.cons.car->value.string;
			value = symbol_get(globals, symbol);
			return eval_form(value, expr->value.cons.cdr);
			break;
		case TYPE_SYMBOL:
			symbol = expr->value.string;
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
			printf("%s", expr->value.string);
			break;
		case TYPE_CFORM:
			printf("<c_form>");
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
		if (strcmp(current->value.cons.car->value.string, symbol) == 0)
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
		if (current->value.cons.car->value.string == symbol) {
			if (prev) {
				prev->value.cons.cdr = iterator->value.cons.cdr;
			} else
				*table = iterator->value.cons.cdr;
		}
	}
}

datum *do_unquotes(datum *expr) {
	datum *iterator;
	datum *current;

	iterator = expr;

	while (iterator->type == TYPE_CONS) {
		current = iterator->value.cons.car;
		if (current->type == TYPE_SYMBOL) {
			if (strcmp(current->value.string, "unquote") == 0)
				return gh_eval(iterator);
		} else if (current->type == TYPE_CONS) {
			iterator->value.cons.car = do_unquotes(iterator->value.cons.car);
		}
		iterator = iterator->value.cons.cdr;
	}
	return expr;
}

datum *gh_set(datum **locals) {
	datum *symbol;
	datum *value;

	symbol = symbol_get(*locals, "symbol");
	value = gh_eval(symbol_get(*locals, "value"));

	symbol_set(&globals, symbol->value.string, value);
	return symbol;
}

datum *gh_quote(datum **locals) {
	datum *expr;

	expr = symbol_get(*locals, "expr");
	return do_unquotes(expr);
}

datum *gh_unquote(datum **locals) {
	datum *expr;

	expr = symbol_get(*locals, "expr");
	return gh_eval(expr);
}

datum *gh_car(datum **locals) {
	datum *pair;

	pair = symbol_get(*locals, "pair");
	gh_assert(pair->type == TYPE_CONS, "Not a pair or list");

	return pair->value.cons.car;
}

datum *gh_cdr(datum **locals) {
	datum *pair;

	pair = symbol_get(*locals, "pair");
	gh_assert(pair->type == TYPE_CONS, "Not a pair or list");

	return pair->value.cons.cdr;
}

datum *gh_cfunc(datum *(*addr)(datum **), datum *args) {
	datum *cf = new_datum(sizeof(datum));
	cf->type = TYPE_CFUNC;
	cf->value.c_code.func = addr;
	cf->value.c_code.args = args;
	return cf;
}

datum *gh_cform(datum *(*addr)(datum **), datum *args) {
	datum *cf = new_datum(sizeof(datum));
	cf->type = TYPE_CFORM;
	cf->value.c_code.func = addr;
	cf->value.c_code.args = args;
	return cf;
}

datum *eval_form(datum *form, datum *args) {
	datum *arglist;
	datum *sym_iterator;
	datum *args_iterator;

	gh_assert(form->type == TYPE_CFUNC || form->type == TYPE_CFORM, "Not a function, macro or special form");

	arglist = &GH_NIL_VALUE;
	sym_iterator = form->value.c_code.args;
	args_iterator = args;


	while (sym_iterator->type != TYPE_NIL && args_iterator != TYPE_NIL) {
		datum *current_sym;
		datum *current_arg;

		current_sym = sym_iterator->value.cons.car;
		current_arg = args_iterator->value.cons.car;

		if (form->type == TYPE_CFORM)
			symbol_set(&arglist, current_sym->value.string, current_arg);
		else
			symbol_set(&arglist, current_sym->value.string, gh_eval(current_arg));
			

		sym_iterator = sym_iterator->value.cons.cdr;
		args_iterator = args_iterator->value.cons.cdr;
	}
	return form->value.c_code.func(&arglist);
}

void prompt() {
	if (isatty(fileno(stdin))) {
		printf("$ ");
	}
}

int main(int argc, char **argv) {
	symbol_set(&globals, "set", gh_cform(&gh_set, gh_cons(gh_symbol("symbol"), gh_cons(gh_symbol("value"), &GH_NIL_VALUE))));
	symbol_set(&globals, "quote", gh_cform(&gh_quote, gh_cons(gh_symbol("expr"), &GH_NIL_VALUE)));
	symbol_set(&globals, "unquote", gh_cform(&gh_unquote, gh_cons(gh_symbol("expr"), &GH_NIL_VALUE)));
	symbol_set(&globals, "car", gh_cfunc(&gh_car, gh_cons(gh_symbol("pair"), &GH_NIL_VALUE)));
	symbol_set(&globals, "cdr", gh_cfunc(&gh_cdr, gh_cons(gh_symbol("pair"), &GH_NIL_VALUE)));
	prompt();
	yyparse();
	return 0;
}

