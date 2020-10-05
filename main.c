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

datum GH_NIL_VALUE = { TYPE_NIL, { 0 } }; datum GH_TRUE_VALUE = { TYPE_TRUE, { 0 } };

datum *new_datum();

void print_datum(datum *expr);

datum *gh_cform(datum *(*addr)(datum **locals), datum *args);
datum *gh_cfunc(datum *(*addr)(datum **locals), datum *args);

datum *symbol_loc(datum *table, char *symbol);
datum *symbol_get(datum *table, char *symbol);
void  symbol_set(datum **table, char *symbol, datum *value);
void  symbol_unset(datum **table, char *symbol);

datum *combine(datum *lst1, datum *lst2);

datum *var_get(datum *locals, char *symbol);

datum *eval_arglist(datum *args, datum **locals);

datum *reverse(datum *lst);

datum *fold(datum *(*func)(datum *a, datum *b), datum *init, datum *list);

datum *map(datum *(*func)(datum *x), datum *lst);

datum *zip(datum *lst1, datum *lst2);

datum *car(datum *pair);

datum *cdr(datum *pair);

datum *add2(datum *a, datum *b);

datum *sub2(datum *a, datum *b);

datum *mul2(datum *a, datum *b);

datum *div2(datum *a, datum *b);

datum *dpow(datum *a, datum *b);

datum *do_unquotes(datum *expr, datum **locals);

datum *eval_form(datum* func, datum *args, datum **locals);

void gh_print(datum *expr);

datum *gh_set(datum **locals);

datum *gh_quote(datum **locals);

datum *gh_unquote(datum **locals);

datum *gh_cons(datum **locals);

datum *gh_car(datum **locals);

datum *gh_cdr(datum **locals);

datum *gh_reverse(datum **locals);

datum *gh_list(datum **locals);

datum *gh_equal(datum **locals);

datum *gh_add(datum **locals);

datum *gh_sub(datum **locals);

datum *gh_mul(datum **locals);

datum *gh_div(datum **locals);

datum *gh_pow(datum **locals);

datum *gh_lambda(datum **locals);

datum *gh_cond(datum **locals);

datum *gh_loop(datum **locals);

datum *gh_recur(datum **locals);

datum *gh_let(datum **locals);

datum *globals = &GH_NIL_VALUE;
datum *locals = &GH_NIL_VALUE;
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
		result = func(iterator->value.cons.car, result);
		iterator = iterator->value.cons.cdr;
	}

	gh_assert(iterator->type == TYPE_NIL, "Not a proper list");

	return result;
}

datum *map(datum *(*func)(datum *x), datum *lst) {
	datum *new_lst = &GH_NIL_VALUE;
	datum *iterator;

	iterator = lst;

	while (iterator->type != TYPE_NIL) {
		new_lst = cons(func(iterator->value.cons.car), new_lst);
		iterator = iterator->value.cons.cdr;
	}

	return reverse(new_lst);
}

datum *combine(datum *lst1, datum *lst2) {
	return fold(&cons, lst2, lst1);
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
	return var_get(*locals, "args");
}
datum *cons(datum *car, datum *cdr) {
	datum *c;
	c = new_datum();
	c->type = TYPE_CONS;
	c->value.cons.car = car;
	c->value.cons.cdr = cdr;
	return c;
}

datum *car(datum *pair) {
	return pair->value.cons.car;
}

datum *cdr(datum *pair) {
	return pair->value.cons.cdr;
}

int gh_assert(int cond, char *mesg) {
	if ( ! cond ) {
		fprintf(stderr, "ERROR: %s\n", mesg);
		exit(EXIT_FAILURE);
	} else {
		return TRUE;
	}
}

datum *gh_lambda(datum **locals) {
	datum *lambda_list;
	datum *body;
	datum *func;
	datum *iterator;

	lambda_list = var_get(*locals, "lambda-list");
	body = var_get(*locals, "body");

	iterator = lambda_list;

	while (iterator->type == TYPE_CONS) {
		gh_assert(iterator->value.cons.car->type == TYPE_SYMBOL, "Non-symbol in lambda list");
	}

	gh_assert(iterator->type == TYPE_NIL || iterator->type == TYPE_SYMBOL, "Non-symbol in lambda list");

	func = new_datum(sizeof(datum));
	func->type = TYPE_FUNC;
	func->value.func.lambda_list = lambda_list;
	func->value.func.body = body;
	return func;	
}

datum *gh_cond(datum **locals) {
	datum *iterator;

	iterator = var_get(*locals, "conditions");

	while (iterator->type != TYPE_NIL) {
		datum *current_cond;

		current_cond = iterator->value.cons.car;
		if (eval(current_cond->value.cons.car, locals)->type != TYPE_NIL)
			return eval(current_cond->value.cons.cdr->value.cons.car, locals);

		iterator = iterator->value.cons.cdr;
	}
	return &GH_NIL_VALUE;
}

datum *translate_binding(datum *let_binding) {
	datum *iterator;
	datum *first;
	datum *second;

	iterator = let_binding;
	first = iterator->value.cons.car;
	iterator = iterator->value.cons.cdr;
	second = iterator->value.cons.car;

	return cons(first, second);
}

datum *zip(datum *lst1, datum *lst2) {
	datum *iterator1;
	datum *iterator2;
	datum *result;

	iterator1 = lst1;
	iterator2 = lst2;
	result = &GH_NIL_VALUE;

	while (iterator1->type != TYPE_NIL && iterator2->type != TYPE_NIL) {
		datum *val1;
		datum *val2;

		val1 = iterator1->value.cons.car;
		val2 = iterator2->value.cons.car;

		result = cons(cons(val1, val2), result);

		iterator1 = lst1->value.cons.cdr;
		iterator2 = lst2->value.cons.cdr;
	}

	return result;
}

datum *gh_loop(datum **locals) {
	datum *bindings;
	datum *body;
	datum *iterator;
	datum *new_locals;
	datum *result;
	datum *translated_bindings;

	bindings = var_get(*locals, "bindings");
	body = var_get(*locals, "body");

	translated_bindings = map(&translate_binding, bindings);
	new_locals = combine(translated_bindings, *locals);

	result = &GH_NIL_VALUE;

	iterator = body;
	while (iterator->type != TYPE_NIL) {
		result = eval(iterator->value.cons.car, &new_locals);
		if (result->type == TYPE_RECUR) {
			datum *recur_bindings;

			recur_bindings = zip(map(&car, translated_bindings), result->value.recur.bindings);
			new_locals = combine(recur_bindings, *locals);
			iterator = body;
			continue;
		}
		iterator = iterator->value.cons.cdr;
	}

	return result;
}

datum *gh_recur(datum **locals) {
	datum *bindings;
	datum *rec;

	bindings = var_get(*locals, "bindings");
	rec = new_datum();
	rec->type = TYPE_RECUR;
	rec->value.recur.bindings = eval_arglist(bindings, locals);

	return rec;
}

datum *gh_let(datum **locals) {
	datum *bindings;
	datum *body;
	datum *iterator;
	datum *new_locals;
	datum *result;
	datum *translated_bindings;

	bindings = var_get(*locals, "bindings");
	body = var_get(*locals, "body");

	translated_bindings = map(&translate_binding, bindings);
	new_locals = combine(translated_bindings, *locals);

	result = &GH_NIL_VALUE;

	iterator = body;
	while (iterator->type != TYPE_NIL) {
		result = eval(iterator->value.cons.car, &new_locals);
		iterator = iterator->value.cons.cdr;
	}

	return result;

}

datum *eval(datum *expr, datum **locals) {
	char  *symbol;
	datum *value;

	switch (expr->type) {
		case TYPE_CONS:
			value = eval(expr->value.cons.car, locals);

			gh_assert(value->type == TYPE_FUNC || value->type == TYPE_CFUNC || value->type == TYPE_CFORM, "Not a function, macro, or special form");
			return eval_form(value, expr->value.cons.cdr, locals);
			break;
		case TYPE_SYMBOL:
			symbol = expr->value.string;
			value = var_get(*locals, symbol);
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
		case TYPE_FUNC:
			printf("<function>");
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
	while (iterator->type == TYPE_CONS) {
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
	if (loc == NULL)
		return NULL;
	else
		return loc->value.cons.cdr;
}

datum *var_get(datum *locals, char *symbol) {
	datum *var;

	var = symbol_get(locals, symbol);
	if (var == NULL) {
		var = symbol_get(globals, symbol);
		gh_assert(var != NULL, "Unbound variable");
	}
	return var;
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

datum *do_unquotes(datum *expr, datum **locals) {
	datum *iterator;
	datum *current;

	iterator = expr;

	while (iterator->type == TYPE_CONS) {
		current = iterator->value.cons.car;
		if (current->type == TYPE_SYMBOL) {
			if (strcmp(current->value.string, "unquote") == 0)
				return eval(iterator, locals);
		} else if (current->type == TYPE_CONS) {
			iterator->value.cons.car = do_unquotes(iterator->value.cons.car, locals);
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

	symbol = var_get(*locals, "symbol");
	value = eval(var_get(*locals, "value"), locals);

	symbol_set(&globals, symbol->value.string, value);
	return symbol;
}

datum *gh_quote(datum **locals) {
	datum *expr;

	expr = var_get(*locals, "expr");
	return do_unquotes(expr, locals);
}

datum *gh_unquote(datum **locals) {
	datum *expr;

	expr = var_get(*locals, "expr");
	return eval(expr, locals);
}

datum *gh_cons(datum **locals) {
	datum *car;
	datum *cdr;

	car = var_get(*locals, "car");
	cdr = var_get(*locals, "cdr");
	return cons(car, cdr);
}

datum *gh_car(datum **locals) {
	datum *pair;

	pair = var_get(*locals, "pair");
	gh_assert(pair->type == TYPE_CONS, "Not a pair or list");

	return pair->value.cons.car;
}

datum *gh_cdr(datum **locals) {
	datum *pair;

	pair = var_get(*locals, "pair");
	gh_assert(pair->type == TYPE_CONS, "Not a pair or list");

	return pair->value.cons.cdr;
}

datum *gh_reverse(datum **locals) {
	datum *lst;

	lst = var_get(*locals, "lst");

	return reverse(lst);
}

datum *gh_equal(datum **locals) {
	datum *a;
	datum *b;
	int result;

	a = var_get(*locals, "a");
	b = var_get(*locals, "b");

	if ((a->type == TYPE_INTEGER || a->type == TYPE_DECIMAL) &&
		(b->type == TYPE_INTEGER || b->type == TYPE_DECIMAL)) {
		result = (float)(a->type == TYPE_INTEGER ? a->value.integer : a->value.decimal) ==
					(float)(b->type == TYPE_INTEGER ? b->value.integer : b->value.decimal);
	}

	if (result == TRUE)
		return &GH_TRUE_VALUE;
	else
		return &GH_NIL_VALUE;
}

datum *gh_add(datum **locals) {
	datum *args;

	args = var_get(*locals, "args");

	return fold(&add2, gh_integer(0), args);
}

datum *gh_sub(datum **locals) {
	datum *first;
	datum *rest;

	first = var_get(*locals, "first");
	rest = var_get(*locals, "rest");

	return fold(&sub2, first, rest);
}

datum *gh_mul(datum **locals) {
	datum *args;

	args = var_get(*locals, "args");

	return fold(&mul2, gh_integer(1), args);
}

datum *gh_div(datum **locals) {
	datum *first;
	datum *rest;

	first = var_get(*locals, "first");
	rest = var_get(*locals, "rest");

	return fold(&div2, first, rest);
}

datum *gh_pow(datum **locals) {
	datum *a;
	datum *b;

	a = var_get(*locals, "a");
	b = var_get(*locals, "b");

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

datum *eval_arglist(datum *args, datum **locals) {
	datum *iterator;
	datum *argscopy;

	argscopy = reverse(reverse(args));	
	iterator = argscopy;
	while (iterator->type != TYPE_NIL) {
		iterator->value.cons.car = eval(iterator->value.cons.car, locals);
		iterator = iterator->value.cons.cdr;
	}
	return argscopy;
}

datum *eval_form(datum *form, datum *args, datum **locals) {
	datum *arglist;
	datum *sym_iterator;
	datum *args_iterator;

	gh_assert(form->type == TYPE_CFUNC || form->type == TYPE_CFORM || form->type == TYPE_FUNC, "Not a function, macro or special form");

	arglist = &GH_NIL_VALUE;
	if (form->type == TYPE_CFUNC || form->type == TYPE_CFORM)
		sym_iterator = form->value.c_code.lambda_list;
	else
		sym_iterator = form->value.func.lambda_list;
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
				symbol_set(&arglist, current_sym->value.string, eval(current_arg, locals));
			
			args_iterator = args_iterator->value.cons.cdr;
		}

		sym_iterator = sym_iterator->value.cons.cdr;
	}
	if (sym_iterator->type != TYPE_NIL) {
		if (form->type == TYPE_CFORM)
			symbol_set(&arglist, sym_iterator->value.string, args_iterator);
		else
			symbol_set(&arglist, sym_iterator->value.string, eval_arglist(args_iterator, locals));
	}

	if (form->type == TYPE_CFUNC || form->type == TYPE_CFORM) {
		datum *new_locals;
		new_locals = combine(arglist, *locals);
		return form->value.c_code.func(&new_locals);
	} else {
		datum *iterator;
		datum *new_locals;
		datum *result;

		new_locals = combine(arglist, *locals);
		iterator = form->value.func.body;

		while (iterator->type == TYPE_CONS) {
			result = eval(iterator->value.cons.car, &new_locals);
			iterator = iterator->value.cons.cdr;
		}

		return result;
	}
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
	symbol_set(&globals, "=", gh_cfunc(&gh_equal, cons(gh_symbol("a"), cons(gh_symbol("b"), &GH_NIL_VALUE))));
	symbol_set(&globals, "+", gh_cfunc(&gh_add, cons(&GH_NIL_VALUE, gh_symbol("args"))));
	symbol_set(&globals, "-", gh_cfunc(&gh_sub, cons(gh_symbol("first"), gh_symbol("rest"))));
	symbol_set(&globals, "*", gh_cfunc(&gh_mul, cons(&GH_NIL_VALUE, gh_symbol("args"))));
	symbol_set(&globals, "/", gh_cfunc(&gh_div, cons(gh_symbol("first"), gh_symbol("rest"))));
	symbol_set(&globals, "^", gh_cfunc(&gh_pow, cons(gh_symbol("a"), cons(gh_symbol("b"), &GH_NIL_VALUE))));
	symbol_set(&globals, "lambda", gh_cform(&gh_lambda, cons(gh_symbol("lambda-list"), gh_symbol("body"))));
	symbol_set(&globals, "cond", gh_cform(&gh_cond, cons(&GH_NIL_VALUE, gh_symbol("conditions"))));
	symbol_set(&globals, "loop", gh_cform(&gh_loop, cons(gh_symbol("bindings"), gh_symbol("body"))));
	symbol_set(&globals, "recur", gh_cform(&gh_recur, cons(&GH_NIL_VALUE, gh_symbol("bindings"))));
	symbol_set(&globals, "let", gh_cform(&gh_let, cons(gh_symbol("bindings"), gh_symbol("body"))));
	prompt();
	yyparse();
	return 0;
}

