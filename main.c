#define _POSIX_C_SOURCE 200112L

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <math.h>

#include <unistd.h>

#include <gc.h>

#include "gharial.h"

#include "y.tab.h"

datum GH_NIL_VALUE = { TYPE_NIL, { 0 } };
datum GH_TRUE_VALUE = { TYPE_TRUE, { 0 } };

datum *new_datum();

void print_datum(datum *expr);

datum *gh_cform(datum *(*addr)(datum **locals), datum *args);
datum *gh_cfunc(datum *(*addr)(datum **locals), datum *args);

datum *symbol_loc(datum *table, char *symbol);
datum *symbol_get(datum *table, char *symbol);
void  symbol_set(datum **table, char *symbol, datum *value);
void  symbol_unset(datum **table, char *symbol);

datum *eval_arglist(datum *args);

datum *reverse(datum *lst);

datum *fold(datum *(*func)(datum *a, datum *b), datum *init, datum *list);

datum *add2(datum *a, datum *b);

datum *sub2(datum *a, datum *b);

datum *mul2(datum *a, datum *b);

datum *div2(datum *a, datum *b);

datum *dpow(datum *a, datum *b);

datum *do_unquotes(datum *expr);

datum *eval_form(datum* func, datum *args);

datum *gh_set(datum **locals);

datum *gh_quote(datum **locals);

datum *gh_unquote(datum **locals);

datum *gh_cons(datum **locals);

datum *gh_car(datum **locals);

datum *gh_cdr(datum **locals);

datum *gh_reverse(datum **locals);

datum *gh_list(datum **locals);

datum *gh_add(datum **locals);

datum *gh_sub(datum **locals);

datum *gh_mul(datum **locals);

datum *gh_div(datum **locals);

datum *gh_pow(datum **locals);

datum *globals = &GH_NIL_VALUE;

datum *new_datum() {
	datum *d = GC_MALLOC(sizeof(datum));
	gh_assert(d != NULL, "Out of memory!");
	return d;
}

datum *fold(datum *(*func)(datum *a, datum *b), datum *init, datum *lst) {
	datum *iterator;
	datum *result;

	gh_assert(lst->type == TYPE_CONS || lst->type == TYPE_NIL, "Not a list");

	iterator = lst;
	result = init;

	while (iterator->type == TYPE_CONS) {
		result = func(result, iterator->value.cons.car);
		iterator = iterator->value.cons.cdr;
	}

	gh_assert(iterator->type == TYPE_NIL, "Not a proper list");

	return result;
}

datum *add2(datum *a, datum *b) {
	int result_type;

	gh_assert(a->type == TYPE_INTEGER || a->type == TYPE_DECIMAL, "Not a number");
	gh_assert(b->type == TYPE_INTEGER || b->type == TYPE_DECIMAL, "Not a number");

	if (a->type == TYPE_DECIMAL || b->type == TYPE_DECIMAL)
		result_type = TYPE_DECIMAL;
	else
		result_type = TYPE_INTEGER;

	if (result_type == TYPE_INTEGER)
		return gh_integer(a->value.integer + b->value.integer);
	else {
		return gh_decimal((a->type == TYPE_INTEGER ? a->value.integer : a->value.decimal) +
							(b->type == TYPE_INTEGER ? b->value.integer : b->value.decimal));
	}
}

datum *sub2(datum *a, datum *b) {
	int result_type;

	gh_assert(a->type == TYPE_INTEGER || a->type == TYPE_DECIMAL, "Not a number");
	gh_assert(b->type == TYPE_INTEGER || b->type == TYPE_DECIMAL, "Not a number");

	if (a->type == TYPE_DECIMAL || b->type == TYPE_DECIMAL)
		result_type = TYPE_DECIMAL;
	else
		result_type = TYPE_INTEGER;

	if (result_type == TYPE_INTEGER)
		return gh_integer(a->value.integer - b->value.integer);
	else {
		return gh_decimal((a->type == TYPE_INTEGER ? a->value.integer : a->value.decimal) -
							(b->type == TYPE_INTEGER ? b->value.integer : b->value.decimal));
	}

}

datum *mul2(datum *a, datum *b) {
	int result_type;

	gh_assert(a->type == TYPE_INTEGER || a->type == TYPE_DECIMAL, "Not a number");
	gh_assert(b->type == TYPE_INTEGER || b->type == TYPE_DECIMAL, "Not a number");

	if (a->type == TYPE_DECIMAL || b->type == TYPE_DECIMAL)
		result_type = TYPE_DECIMAL;
	else
		result_type = TYPE_INTEGER;

	if (result_type == TYPE_INTEGER)
		return gh_integer(a->value.integer * b->value.integer);
	else {
		return gh_decimal((a->type == TYPE_INTEGER ? a->value.integer : a->value.decimal) *
							(b->type == TYPE_INTEGER ? b->value.integer : b->value.decimal));
	}
}

datum *div2(datum *a, datum *b) {
	gh_assert(a->type == TYPE_INTEGER || a->type == TYPE_DECIMAL, "Not a number");
	gh_assert(b->type == TYPE_INTEGER || b->type == TYPE_DECIMAL, "Not a number");

	return gh_decimal((a->type == TYPE_INTEGER ? a->value.integer : a->value.decimal) /
						(b->type == TYPE_INTEGER ? b->value.integer : b->value.decimal));
}

datum *dpow(datum *a, datum *b) {
	gh_assert(a->type == TYPE_INTEGER || a->type == TYPE_DECIMAL, "Not a number");
	gh_assert(b->type == TYPE_INTEGER || b->type == TYPE_DECIMAL, "Not a number");

	return gh_decimal(pow(a->type == TYPE_INTEGER ? a->value.integer : a->value.decimal,
						b->type == TYPE_INTEGER ? b->value.integer : b->value.decimal));
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

datum *gh_list(datum **locals){
	return symbol_get(*locals, "args");
}

datum *cons(datum *car, datum *cdr) {
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
		case TYPE_TRUE:
			printf("T");
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
	gh_assert(loc != NULL, "Unbound variable");
	return loc->value.cons.cdr;
}

void symbol_set(datum **table, char *symbol, datum *value) {
	datum *loc;
	loc = symbol_loc(*table, symbol);
	if (loc) {
		loc->value.cons.cdr = value;
	} else {
		*table = cons(cons(gh_symbol(symbol), value), *table);
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

datum *reverse(datum *lst) {
	datum *reversed;
	datum *iterator;

	reversed = &GH_NIL_VALUE;
	iterator = lst;

	while (iterator->type == TYPE_CONS) {
		reversed = cons(iterator->value.cons.car, reversed);
		iterator = iterator->value.cons.cdr;
	}

	return reversed;
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

datum *gh_cons(datum **locals) {
	datum *car;
	datum *cdr;

	car = symbol_get(*locals, "car");
	cdr = symbol_get(*locals, "cdr");
	return cons(car, cdr);
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

datum *gh_reverse(datum **locals) {
	datum *lst;

	lst = symbol_get(*locals, "lst");

	return reverse(lst);
}

datum *gh_add(datum **locals) {
	datum *args;

	args = symbol_get(*locals, "args");

	return fold(&add2, gh_integer(0), args);
}

datum *gh_sub(datum **locals) {
	datum *first;
	datum *rest;

	first = symbol_get(*locals, "first");
	rest = symbol_get(*locals, "rest");

	return fold(&sub2, first, rest);
}

datum *gh_mul(datum **locals) {
	datum *args;

	args = symbol_get(*locals, "args");

	return fold(&mul2, gh_integer(1), args);
}

datum *gh_div(datum **locals) {
	datum *first;
	datum *rest;

	first = symbol_get(*locals, "first");
	rest = symbol_get(*locals, "rest");

	return fold(&div2, first, rest);
}

datum *gh_pow(datum **locals) {
	datum *a;
	datum *b;

	a = symbol_get(*locals, "a");
	b = symbol_get(*locals, "b");

	return(dpow(a, b));
}

datum *gh_cfunc(datum *(*addr)(datum **), datum *args) {
	datum *cf = new_datum(sizeof(datum));
	cf->type = TYPE_CFUNC;
	cf->value.c_code.func = addr;
	cf->value.c_code.lambda_list = args;
	return cf;
}

datum *gh_cform(datum *(*addr)(datum **), datum *args) {
	datum *cf = new_datum(sizeof(datum));
	cf->type = TYPE_CFORM;
	cf->value.c_code.func = addr;
	cf->value.c_code.lambda_list = args;
	return cf;
}

datum *eval_arglist(datum *args) {
	datum *iterator;

	iterator = args;
	while (iterator->type != TYPE_NIL) {
		iterator->value.cons.car = gh_eval(iterator->value.cons.car);
		iterator = iterator->value.cons.cdr;
	}
	return args;
}

datum *eval_form(datum *form, datum *args) {
	datum *arglist;
	datum *sym_iterator;
	datum *args_iterator;

	gh_assert(form->type == TYPE_CFUNC || form->type == TYPE_CFORM, "Not a function, macro or special form");

	arglist = &GH_NIL_VALUE;
	sym_iterator = form->value.c_code.lambda_list;
	args_iterator = args;

	while (sym_iterator->type == TYPE_CONS && args_iterator->type == TYPE_CONS) {
		datum *current_sym;
		datum *current_arg;

		current_sym = sym_iterator->value.cons.car;
		current_arg = args_iterator->value.cons.car;

		if (current_sym->type == TYPE_SYMBOL) {

			if (form->type == TYPE_CFORM)
				symbol_set(&arglist, current_sym->value.string, current_arg);
			else
				symbol_set(&arglist, current_sym->value.string, gh_eval(current_arg));
			
			args_iterator = args_iterator->value.cons.cdr;
		}

		sym_iterator = sym_iterator->value.cons.cdr;
	}
	if (sym_iterator->type != TYPE_NIL) {
		if (form->type == TYPE_CFORM)
			symbol_set(&arglist, sym_iterator->value.string, args_iterator);
		 else 
			symbol_set(&arglist, sym_iterator->value.string, eval_arglist(args_iterator));
	}

	return form->value.c_code.func(&arglist);
}

void prompt() {
	if (isatty(fileno(stdin))) {
		printf("$ ");
	}
}

int main(int argc, char **argv) {
	symbol_set(&globals, "set", gh_cform(&gh_set, cons(gh_symbol("symbol"), cons(gh_symbol("value"), &GH_NIL_VALUE))));
	symbol_set(&globals, "quote", gh_cform(&gh_quote, cons(gh_symbol("expr"), &GH_NIL_VALUE)));
	symbol_set(&globals, "unquote", gh_cform(&gh_unquote, cons(gh_symbol("expr"), &GH_NIL_VALUE)));
	symbol_set(&globals, "cons", gh_cfunc(&gh_cons, cons(gh_symbol("car"), cons(gh_symbol("cdr"), &GH_NIL_VALUE))));
	symbol_set(&globals, "car", gh_cfunc(&gh_car, cons(gh_symbol("pair"), &GH_NIL_VALUE)));
	symbol_set(&globals, "cdr", gh_cfunc(&gh_cdr, cons(gh_symbol("pair"), &GH_NIL_VALUE)));
	symbol_set(&globals, "reverse", gh_cfunc(&gh_reverse, cons(gh_symbol("lst"), &GH_NIL_VALUE)));
	symbol_set(&globals, "list", gh_cfunc(&gh_list, cons(&GH_NIL_VALUE, gh_symbol("args"))));
	symbol_set(&globals, "+", gh_cfunc(&gh_add, cons(&GH_NIL_VALUE, gh_symbol("args"))));
	symbol_set(&globals, "-", gh_cfunc(&gh_sub, cons(gh_symbol("first"), gh_symbol("rest"))));
	symbol_set(&globals, "*", gh_cfunc(&gh_mul, cons(&GH_NIL_VALUE, gh_symbol("args"))));
	symbol_set(&globals, "/", gh_cfunc(&gh_div, cons(gh_symbol("first"), gh_symbol("rest"))));
	symbol_set(&globals, "^", gh_cfunc(&gh_pow, cons(gh_symbol("a"), cons(gh_symbol("b"), &GH_NIL_VALUE))));
	prompt();
	yyparse();
	return 0;
}

