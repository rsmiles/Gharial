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
#include "lex.yy.h"

datum LANG_NIL_VALUE = { TYPE_NIL, { 0 } };
datum LANG_TRUE_VALUE = { TYPE_TRUE, { 0 } };
datum LANG_EOF_VALUE = { TYPE_EOF, { 0 } };

datum *new_datum() {
	datum *d = GC_MALLOC(sizeof(datum));
	if (d == NULL) {
		fprintf(stderr, "Out of memory in new datum!");
		exit(EXIT_FAILURE);
	}
	return d;
}

void prompt() {
	if (isatty(fileno(stdin))) {
		printf("$ ");
	}
}

datum *fold(datum *(*func)(datum *a, datum *b), datum *init, datum *lst) {
	datum *iterator;
	datum *result;

	gh_assert(lst->type == TYPE_CONS || lst->type == TYPE_NIL, "TYPE-ERROR", "Argument 3 of fold is not a list", lst);

	iterator = lst;
	result = init;

	while (iterator->type == TYPE_CONS) {
		result = func(iterator->value.cons.car, result);
		iterator = iterator->value.cons.cdr;
	}

	gh_assert(iterator->type == TYPE_NIL, "TYPE_ERROR", "argument 3 of fold is an improper list", lst);

	return result;
}

datum *map(datum *(*func)(datum *x), datum *lst) {
	datum *new_lst = &LANG_NIL_VALUE;
	datum *iterator;

	iterator = lst;

	while (iterator->type != TYPE_NIL) {
		new_lst = gh_cons(func(iterator->value.cons.car), new_lst);
		iterator = iterator->value.cons.cdr;
	}

	return reverse(new_lst);
}

datum *combine(datum *lst1, datum *lst2) {
	if (lst1->type == TYPE_NIL)
		return lst2;
	else if (lst2->type == TYPE_NIL)
		return lst1;
	else
		return fold(&gh_cons, lst2, lst1);
}

datum *add2(datum *a, datum *b) {
	int result_type;

	gh_assert(a->type == TYPE_INTEGER || a->type == TYPE_DECIMAL, "TYPE-ERROR", "Attempt to add a non-number", a);
	gh_assert(b->type == TYPE_INTEGER || b->type == TYPE_DECIMAL, "TYPE-ERROR", "Attempt to add a non-number", b);

	if (a->type == TYPE_DECIMAL || b->type == TYPE_DECIMAL)
		result_type = TYPE_DECIMAL;
	else
		result_type = TYPE_INTEGER;

	if (result_type == TYPE_INTEGER)
		return gh_integer(a->value.integer + b->value.integer);
	else {
		return gh_decimal((b->type == TYPE_INTEGER ? b->value.integer : b->value.decimal) +
							(a->type == TYPE_INTEGER ? a->value.integer : a->value.decimal));
	}
}

datum *sub2(datum *a, datum *b) {
	int result_type;

	gh_assert(a->type == TYPE_INTEGER || a->type == TYPE_DECIMAL, "TYPE-ERROR", "Attempt to subtract a non-number", a);
	gh_assert(b->type == TYPE_INTEGER || b->type == TYPE_DECIMAL, "TYPE-ERROR", "ATTEMPT to sulbltract a non-number", b);

	if (a->type == TYPE_DECIMAL || b->type == TYPE_DECIMAL)
		result_type = TYPE_DECIMAL;
	else
		result_type = TYPE_INTEGER;

	if (result_type == TYPE_INTEGER)
		return gh_integer(b->value.integer - a->value.integer);
	else {
		return gh_decimal((b->type == TYPE_INTEGER ? b->value.integer : b->value.decimal) -
							(a->type == TYPE_INTEGER ? a->value.integer : a->value.decimal));
	}

}

datum *mul2(datum *a, datum *b) {
	int result_type;

	gh_assert(a->type == TYPE_INTEGER || a->type == TYPE_DECIMAL, "TYPE-ERROR", "Attempt to multiply a non-number", a);
	gh_assert(b->type == TYPE_INTEGER || b->type == TYPE_DECIMAL, "TYPE-ERROR", "Attempt to multiply a non-number", b);

	if (a->type == TYPE_DECIMAL || b->type == TYPE_DECIMAL)
		result_type = TYPE_DECIMAL;
	else
		result_type = TYPE_INTEGER;

	if (result_type == TYPE_INTEGER)
		return gh_integer(a->value.integer * b->value.integer);
	else {
		return gh_decimal((b->type == TYPE_INTEGER ? b->value.integer : b->value.decimal) *
							(a->type == TYPE_INTEGER ? a->value.integer : a->value.decimal));
	}
}

datum *div2(datum *a, datum *b) {
	gh_assert(a->type == TYPE_INTEGER || a->type == TYPE_DECIMAL, "TYPE-ERROR", "Attempt to divide a non-number", a);
	gh_assert(b->type == TYPE_INTEGER || b->type == TYPE_DECIMAL, "TYPE-ERROR", "Attempt to divide a non-number", b);

	if (a->type == TYPE_INTEGER) {
		gh_assert(a->value.integer != 0, "MATH-ERROR", "Division by zero", a);
	} else {
		gh_assert(a->value.decimal != 0.0, "MATH-ERROR", "Division by zero", a);
	}

	return gh_decimal((b->type == TYPE_INTEGER ? b->value.integer : b->value.decimal) /
						(a->type == TYPE_INTEGER ? a->value.integer : a->value.decimal));
}

datum *dpow(datum *a, datum *b) {
	gh_assert(a->type == TYPE_INTEGER || a->type == TYPE_DECIMAL, "TYPE-ERROR", "Attempt to find exponent of non-number", a);
	gh_assert(b->type == TYPE_INTEGER || b->type == TYPE_DECIMAL, "TYPE-ERROR", "Use of non-number as exponent", b);

	return gh_decimal(pow(a->type == TYPE_INTEGER ? a->value.integer : a->value.decimal,
						b->type == TYPE_INTEGER ? b->value.integer : b->value.decimal));
}

datum *gh_integer(int value) {
	datum *i;
	i = new_datum();
	i->type = TYPE_INTEGER; i->value.integer = value;
	return i;
}

datum *gh_decimal(double value) { datum *d;
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

datum *gh_file(FILE *fptr) {
	datum *f;

	f = new_datum();
	f->type = TYPE_FILE;
	f->value.file = fptr;

	return f;
}

datum *lang_list(datum **locals){
	return var_get(*locals, "#args");
}

datum *gh_cons(datum *car, datum *cdr) {
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

datum *lang_lambda(datum **locals) {
	datum *lambda_list;
	datum *body;
	datum *func;
	datum *iterator;

	lambda_list = var_get(*locals, "#lambda-list");
	body = var_get(*locals, "#body");

	iterator = lambda_list;

	while (iterator->type == TYPE_CONS) {
		gh_assert(iterator->value.cons.car->type == TYPE_SYMBOL, "TYPE-ERROR", "Non-symbol in lambda list", iterator->value.cons.car);
		iterator = iterator->value.cons.cdr;
	}

	gh_assert(iterator->type == TYPE_NIL || iterator->type == TYPE_SYMBOL, "TYPE-ERROR", "Non-symbol in lambda list", iterator);

	func = new_datum(sizeof(datum));
	func->type = TYPE_FUNC;
	func->value.func.lambda_list = lambda_list;
	func->value.func.body = body;
	return func;	
}

datum *lang_macro(datum **locals) {
	datum *lambda_list;
	datum *body;
	datum *mac;
	datum *iterator;

	lambda_list = var_get(*locals, "#lambda-list");
	body = var_get(*locals, "#body");

	iterator = lambda_list;

	while (iterator->type == TYPE_CONS) {
		gh_assert(iterator->value.cons.car->type == TYPE_SYMBOL, "TYPE-ERROR", "Non-symbol in lambda list", iterator->value.cons.car);
		iterator = iterator->value.cons.cdr;
	}

	gh_assert(iterator->type == TYPE_NIL || iterator->type == TYPE_SYMBOL, "TYPE-ERROR", "Non-symbol in lambda list", iterator);

	mac = new_datum(sizeof(datum));
	mac->type = TYPE_MACRO;
	mac->value.func.lambda_list = lambda_list;
	mac->value.func.body = body;
	return mac;	
}

datum *lang_cond(datum **locals) {
	datum *iterator;

	iterator = var_get(*locals, "#conditions");

	while (iterator->type != TYPE_NIL) {
		datum *current_cond;

		current_cond = iterator->value.cons.car;
		if (gh_eval(current_cond->value.cons.car, locals)->type != TYPE_NIL)
			return gh_eval(current_cond->value.cons.cdr->value.cons.car, locals);

		iterator = iterator->value.cons.cdr;
	}

	return &LANG_NIL_VALUE;
}

datum *lang_is_eof_object(datum **locals) {
	datum *expr;

	expr = var_get(*locals, "#expr");

	if (expr->type == TYPE_EOF)
		return &LANG_TRUE_VALUE;
	else 
		return &LANG_NIL_VALUE;
}

datum *lang_open(datum **locals) {
	datum *path;
	datum *mode;
	FILE *fptr;

	path = var_get(*locals, "#fname");
	gh_assert(path->type == TYPE_STRING || path->type == TYPE_SYMBOL, "TYPE-ERROR", "open requires string or symbol for first argument", path);

	mode = var_get(*locals, "#mode");

	if (mode->type == TYPE_NIL) {
		mode = gh_string("a+");
	} else {
		mode = mode->value.cons.car;
	}

	gh_assert(mode->type == TYPE_STRING || mode->type == TYPE_SYMBOL, "TYPE-ERROR", "Open requires string or symbol for second argument", mode);

	fptr = fopen(path->value.string, mode->value.string);
	gh_assert(fptr != NULL, "FILE-ERROR", "Could not open file", path);

	return gh_file(fptr);
}

datum *lang_close(datum **locals) {
	datum *file;
	int result;

	file = var_get(*locals, "#file");
	gh_assert(file->type == TYPE_FILE, "TYPE-ERROR", "Attempt to close a non-file", file);

	result = fclose(file->value.file);
	gh_assert(result == 0, "FILE-ERROR", "Error closing file", file);

	return &LANG_TRUE_VALUE;
}

datum *lang_read(datum **locals) {
	datum *file;
	FILE *oldyyin;

	file = var_get(*locals, "#file");
	if (file->type == TYPE_CONS) {
		file = file->value.cons.car;
		gh_assert(file->type == TYPE_FILE, "TYPE-ERROR", "read call on non-file", file);
	} else { 
		gh_assert(file->type == TYPE_NIL, "TYPE-ERROR", "read call on non-file", file);
		file = var_get(*locals, "input-file");
	}

	yypush_buffer_state(yy_create_buffer(yyin, YY_BUF_SIZE));
	oldyyin = yyin;
	yyin = file->value.file;
	eval_flag = FALSE;
	yyparse();
	eval_flag = TRUE;
	yyin = oldyyin;
	yypop_buffer_state();
	return gh_input;
}

datum *lang_write(datum **locals) {
	datum *expr;
	datum *file;

	expr = var_get(*locals, "#expr");

	file = var_get(*locals, "#file");
	if (file->type == TYPE_CONS) {
		file = file->value.cons.car;
		gh_assert(file->type == TYPE_FILE, "TYPE-ERROR", "read call on non-file", file);
	} else { 
		gh_assert(file->type == TYPE_NIL, "TYPE-ERROR", "read call on non-file", file);
		file = var_get(*locals, "output-file");
	}

	gh_print(file->value.file, expr);
	return expr;
}

datum *lang_load(datum **locals) {
	datum *path;

	path = var_get(*locals, "#path");
	gh_assert(path->type == TYPE_STRING || path->type == TYPE_SYMBOL, "TYPE-ERROR", "Load requires string or symbol for path argument", path);

	gh_load(path->value.string);
	
	return &LANG_TRUE_VALUE;
}

datum *translate_binding(datum *let_binding) {
	datum *iterator;
	datum *first;
	datum *second;

	iterator = let_binding;
	first = iterator->value.cons.car;
	iterator = iterator->value.cons.cdr;
	second = iterator->value.cons.car;

	return gh_cons(first, second);
}

datum *bind_args(datum *symbols, datum *values) {
	datum *symbol_iterator;
	datum *value_iterator;
	datum *result;

	symbol_iterator = symbols;
	value_iterator = values;
	result = &LANG_NIL_VALUE;

	while (symbol_iterator->type == TYPE_CONS && value_iterator->type == TYPE_CONS) {
		datum *current_symbol;
		datum *current_value;

		current_symbol = symbol_iterator->value.cons.car;
		current_value = value_iterator->value.cons.car;

		result = gh_cons(gh_cons(current_symbol, current_value), result);

		symbol_iterator = symbol_iterator->value.cons.cdr;
		value_iterator = value_iterator->value.cons.cdr;
	}

	if (symbol_iterator->type != TYPE_NIL) {
		result = gh_cons(gh_cons(symbol_iterator, value_iterator), result);
	}

	return result;
}

datum *lang_loop(datum **locals) {
	datum *bindings;
	datum *body;
	datum *iterator;
	datum *new_locals;
	datum *result;
	datum *translated_bindings;

	bindings = var_get(*locals, "#bindings");
	body = var_get(*locals, "#body");

	translated_bindings = map(&translate_binding, bindings);
	new_locals = combine(translated_bindings, *locals);

	result = &LANG_NIL_VALUE;

	iterator = body;
	while (iterator->type != TYPE_NIL) {
		result = gh_eval(iterator->value.cons.car, &new_locals);
		if (result->type == TYPE_RECUR) {
			datum *recur_bindings;

			recur_bindings = bind_args(map(&car, translated_bindings), result->value.recur.bindings);
			new_locals = combine(recur_bindings, *locals);
			iterator = body;
			continue;
		}
		iterator = iterator->value.cons.cdr;
	}

	return result;
}

datum *lang_recur(datum **locals) {
	datum *bindings;
	datum *rec;

	bindings = var_get(*locals, "#bindings");
	rec = new_datum();
	rec->type = TYPE_RECUR;
	rec->value.recur.bindings = eval_arglist(bindings, locals);

	return rec;
}

datum *lang_let(datum **locals) {
	datum *bindings;
	datum *body;
	datum *new_locals;
	datum *translated_bindings;

	bindings = var_get(*locals, "#bindings");
	body = var_get(*locals, "#body");

	translated_bindings = map(&translate_binding, bindings);
	new_locals = combine(translated_bindings, *locals);

	return gh_begin(body, &new_locals);
}

void print_exception(FILE *file, datum *expr) {
	char *type;
	char *description;
	datum *info;
	int lineno;

	type = expr->value.exception.type;
	description = expr->value.exception.description;
	lineno = expr->value.exception.lineno;
	info = expr->value.exception.info;

	if (current_file != NULL) {
		fprintf(file, "Uncaught EXCEPTION in file \"%s\":\n", current_file);
	}

	if (expr->value.exception.lineno >= 0) {
		fprintf(file, "EXCEPTION line %d: %s: %s", lineno, type, description);
	} else {
		fprintf(file, "EXCEPTION: %s: %s", type, description);
	}

	if (info != NULL) {
		fprintf(file, ": ");
		gh_print(file, info);
	} else {
		fprintf(file, "\n");
	}

	if (gh_input != NULL) {
		fprintf(file, "In expression: ");
		gh_print(file, gh_input);
	}
}

bool is_macro_call(datum *expr, datum **locals) {
	if (expr->type == TYPE_CONS) {
		datum *first;

		first = expr->value.cons.car;
		if (first->type == TYPE_SYMBOL) {
			char *str;
			datum *lookup;

			str = first->value.string;
			lookup = symbol_get(*locals, str);
			if (lookup == NULL) {
				lookup = symbol_get(globals, str);
			}

			if (lookup != NULL) {
				first = lookup;
			} else {
				return FALSE;
			}
		}
		if (first->type == TYPE_MACRO) {
			return TRUE;
		} else {
			return FALSE;
		}
	} else {
		return FALSE;
	}
}

datum *gh_eval(datum *expr, datum **locals) {
	char  *symbol;
	datum *value;
	datum *expanded;

	switch (expr->type) {
		case TYPE_CONS:
			expanded = expr;
			while (is_macro_call(expanded, locals)) {
				expanded = macroexpand(expanded, locals);
			}

			value = expanded->value.cons.car;

			if (value->type == TYPE_SYMBOL) {
				value = var_get(*locals, value->value.string);
			}

			return apply(value, expanded->value.cons.cdr, locals);
			break;
		case TYPE_SYMBOL:
			symbol = expr->value.string;
			value = var_get(*locals, symbol);
			gh_assert(value != NULL, "REF-ERROR", "Undefined variable", value);
			return value;
			break;
		case TYPE_EXCEPTION:
			print_exception(stderr, expr);
			return expr;
			break;
		default:
			return expr;
	}
}

void print_datum(FILE *file, datum *expr) {
	datum *iterator;
	switch (expr->type) {
		case TYPE_NIL:
			fprintf(file, "NIL");
			break;
		case TYPE_TRUE:
			fprintf(file, "T");
			break;
		case TYPE_INTEGER:
			fprintf(file, "%d", expr->value.integer);
			break;
		case TYPE_DECIMAL:
			fprintf(file, "%f", expr->value.decimal);
			break;
		case TYPE_STRING:
			fprintf(file, "\"%s\"", expr->value.string);
			break;
		case TYPE_SYMBOL:
			fprintf(file, "%s", expr->value.string);
			break;
		case TYPE_CFORM:
			fprintf(file, "<c_form>");
			break;
		case TYPE_CFUNC:
			fprintf(file, "<c_function>");
			break;
		case TYPE_FUNC:
			fprintf(file, "<function>");
			break;
		case TYPE_MACRO:
			fprintf(file, "<macro>");
			break;
		case TYPE_RECUR:
			fprintf(file, "<recur_object>");
			break;
		case TYPE_FILE:
			fprintf(file, "<file>");
			break;
		case TYPE_EOF:
			fprintf(file, "<EOF>");
			break;
		case TYPE_EXCEPTION:
			fprintf(file, "<exception>");
			break;
		case TYPE_CONS:
			iterator = expr;
	
			fprintf(file, "(");

			print_datum(file, iterator->value.cons.car);
			while(iterator->value.cons.cdr->type == TYPE_CONS) {
				fprintf(file, " ");
				iterator = iterator->value.cons.cdr;
				print_datum(file, iterator->value.cons.car);
			}

			if (iterator->value.cons.cdr->type != TYPE_NIL) {
				fprintf(file, " . ");
				print_datum(file, iterator->value.cons.cdr);
			}

			fprintf(file, ")");
			break;
		default:
			fprintf(stderr, "Error: Unkown data type: %d\n", expr->type);
	}
}

void gh_print(FILE *file, datum *expr){
	print_datum(file, expr);
	fprintf(file, "\n");
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
		gh_assert(var != NULL, "REF-ERROR", "Unbound variable", gh_symbol(symbol));
	}
	return var;
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

datum* gh_load(char *path) {
	FILE *file;
	FILE *oldyyin;
	int old_print_flag;
	char *old_current_file;
	
	file = fopen(path, "r");
	gh_assert(file != NULL, "FILE-ERROR", "Could not open file", gh_string(path));

	old_current_file = current_file;
	current_file = path;
	oldyyin = yyin;
	yyin = file;
	yypush_buffer_state(yy_create_buffer(yyin, YY_BUF_SIZE));
	old_print_flag = print_flag;
	print_flag = FALSE;

	yyrestart(yyin);
	do {
		yyparse();
	} while (gh_result->type != TYPE_EOF);

	print_flag = old_print_flag;
	yypop_buffer_state();
	yyin = oldyyin;
	yylex_destroy();

	fclose(file);

	current_file = old_current_file;
	return &LANG_TRUE_VALUE;
}

datum *do_unquotes(datum *expr, datum **locals) {
	datum *iterator;
	datum *current;
	datum *copy;
	datum *prev;

	if (expr->type == TYPE_CONS) {
		copy = reverse(reverse(expr));
	} else {
		copy = expr;
	}

	iterator = copy;

	prev = NULL;

	while (iterator->type == TYPE_CONS) {
		current = iterator->value.cons.car;
		if (current->type == TYPE_SYMBOL) {
			if (strcmp(current->value.string, "unquote") == 0) {
				return gh_eval(iterator->value.cons.cdr->value.cons.car, locals);
			} else if (strcmp(current->value.string, "unquote-splice") == 0) {
				datum *splice;
				splice = gh_eval(iterator->value.cons.cdr->value.cons.car, locals);
				if (splice->type != TYPE_CONS) {
					if (prev != NULL) {
						prev->value.cons.cdr = splice;
						return copy;
					} else {
						return splice;
					}
				}
				iterator->value.cons.car = splice->value.cons.car;
				iterator->value.cons.cdr = splice->value.cons.cdr;
			}
		} else if (current->type == TYPE_CONS) {
			iterator->value.cons.car = do_unquotes(iterator->value.cons.car, locals);
		}
		
		prev = iterator;
		iterator = iterator->value.cons.cdr;
	}

	return copy;
}

datum *reverse(datum *lst) {
	datum *reversed;
	datum *iterator;

	reversed = &LANG_NIL_VALUE;
	iterator = lst;

	while (iterator->type == TYPE_CONS) {
		reversed = gh_cons(iterator->value.cons.car, reversed);
		iterator = iterator->value.cons.cdr;
	}

	return reversed;
}

datum *lang_set(datum **locals) {
	datum *symbol;
	datum *value;
	datum *loc;

	symbol = var_get(*locals, "#symbol");
	value = gh_eval(var_get(*locals, "#value"), locals);

	loc = symbol_loc(*locals, symbol->value.string);
	if (loc)
		loc->value.cons.cdr = gh_eval(value, locals);
	else
		symbol_set(&globals, symbol->value.string, value);

	return symbol;
}

datum *lang_quote(datum **locals) {
	datum *expr;

	expr = var_get(*locals, "#expr");
	return do_unquotes(expr, locals);
}

datum *lang_unquote(datum **locals) {
	datum *expr;

	expr = var_get(*locals, "#expr");
	return gh_eval(expr, locals);
}

datum *lang_unquote_splice(datum **locals) {
	gh_assert(TRUE, "RUNTIME-ERROR", "Call to unquote-splice", NULL);
	return &LANG_NIL_VALUE;
}

datum *lang_cons(datum **locals) {
	datum *car;
	datum *cdr;

	car = var_get(*locals, "#car");
	cdr = var_get(*locals, "#cdr");
	return gh_cons(car, cdr);
}

datum *lang_car(datum **locals) {
	datum *pair;

	pair = var_get(*locals, "#pair");
	gh_assert(pair->type == TYPE_CONS, "TYPE-ERROR", "Not a pair or list", pair);

	return pair->value.cons.car;
}

datum *lang_cdr(datum **locals) {
	datum *pair;

	pair = var_get(*locals, "#pair");
	gh_assert(pair->type == TYPE_CONS, "TYPE-ERROR", "Not a pair or list", pair);

	return pair->value.cons.cdr;
}

datum *lang_reverse(datum **locals) {
	datum *lst;

	lst = var_get(*locals, "#lst");

	return reverse(lst);
}

datum *lang_equal(datum **locals) {
	datum *a;
	datum *b;
	int result;

	a = var_get(*locals, "#a");
	b = var_get(*locals, "#b");

	if ((a->type == TYPE_INTEGER || a->type == TYPE_DECIMAL) &&
		(b->type == TYPE_INTEGER || b->type == TYPE_DECIMAL)) {
		result = (float)(a->type == TYPE_INTEGER ? a->value.integer : a->value.decimal) ==
					(float)(b->type == TYPE_INTEGER ? b->value.integer : b->value.decimal);
	}

	if (result == TRUE)
		return &LANG_TRUE_VALUE;
	else
		return &LANG_NIL_VALUE;
}

datum *lang_add(datum **locals) {
	datum *args;

	args = var_get(*locals, "#args");

	return fold(&add2, gh_integer(0), args);
}

datum *lang_sub(datum **locals) {
	datum *first;
	datum *rest;

	first = var_get(*locals, "#first");
	rest = var_get(*locals, "#rest");

	return fold(&sub2, first, rest);
}

datum *lang_mul(datum **locals) {
	datum *args;

	args = var_get(*locals, "#args");

	return fold(&mul2, gh_integer(1), args);
}

datum *lang_div(datum **locals) {
	datum *first;
	datum *rest;

	first = var_get(*locals, "#first");
	rest = var_get(*locals, "#rest");

	return fold(&div2, first, rest);
}

datum *lang_pow(datum **locals) {
	datum *a;
	datum *b;

	a = var_get(*locals, "#a");
	b = var_get(*locals, "#b");

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
		iterator->value.cons.car = gh_eval(iterator->value.cons.car, locals);
		iterator = iterator->value.cons.cdr;
	}
	return argscopy;
}

char *typestring(datum *obj) {
	switch (obj->type) {
		case TYPE_NIL:
			return "NIL";
			break;
		case TYPE_TRUE:
			return "T";
			break;
		case TYPE_INTEGER:
			return "INTEGER";
			break;
		case TYPE_DECIMAL:
			return "DECIMAL";
			break;
		case TYPE_SYMBOL:
			return "SYMBOL";
			break;
		case TYPE_CONS:
			return "CONS";
			break;
		case TYPE_CFORM:
			return "C FORM";
			break;
		case TYPE_CFUNC:
			return "C FUNCION";
			break;
		case TYPE_FUNC:
			return "FUNCTION";
			break;
		case TYPE_MACRO:
			return "MACRO";
			break;
		case TYPE_RECUR:
			return "RECUR";
			break;
		case TYPE_FILE:
			return "FILE";
			break;
		case TYPE_EOF:
			return "END OF FILE";
			break;
		case TYPE_EXCEPTION:
			return "EXCEPTION";
			break;
		default:
			return "UNKOWN";
	}
}

char *string_append(char *str1, char *str2) {
	int len1;
	int len2;
	char *result;

	len1 = strlen(str1) + 1;
	len2 = strlen(str2) + 1;

	result = GC_MALLOC(sizeof(char) * (len1 + len2));
	if (result == NULL) {
		fprintf(stderr, "Memory error: line %d: Out of memory in string-append!", yylineno);
	}
	strcpy(result, str1);
	strcpy(result + len1, str2);

	return result;
}

datum *gh_exception(char *type, char *description, datum *info, int lineno) {
	datum *ex;

	ex = new_datum();
	ex->type = TYPE_EXCEPTION;

	ex->value.exception.type = GC_MALLOC(sizeof (char) * (strlen(type) + 1));
	gh_assert(ex->value.exception.type != NULL, "MEMORY-ERROR", "Out of memory in gh_exception!", NULL);
	strcpy(ex->value.exception.type, type);
	
	ex->value.exception.description = GC_MALLOC(sizeof (char) * (strlen(description) + 1));
	gh_assert(ex->value.exception.description != NULL, "MEMORY-ERROR", "Out of memory in gh_exception!", NULL);
	strcpy(ex->value.exception.description, description);

	ex->value.exception.info = info;

	ex->value.exception.lineno = lineno;

	return ex;
}

datum *gh_begin(datum *body, datum **locals) {
	datum *iterator;
	datum *result;

	iterator = body;
	result = &LANG_NIL_VALUE;

	while (iterator->type == TYPE_CONS) {
		result = gh_eval(iterator->value.cons.car, locals);
		iterator = iterator->value.cons.cdr;
	}

	return result;
}

datum *apply(datum *fn, datum *args, datum **locals) {
	datum *evaluated_args;
	datum *arg_bindings;
	datum *new_locals;

	gh_assert(fn->type == TYPE_CFORM || fn->type == TYPE_CFUNC ||
				fn->type == TYPE_FUNC,
				"TYPE-ERROR", "Attempt to call non-function", fn);

	if (fn->type == TYPE_CFORM) { /* Do not evaluate arguments if special form */
		evaluated_args = args;
	} else {
		evaluated_args = eval_arglist(args, locals);
	}

	if (fn->type == TYPE_CFUNC || fn->type == TYPE_CFORM) {
		arg_bindings = bind_args(fn->value.c_code.lambda_list, evaluated_args);
	} else {
		arg_bindings = bind_args(fn->value.func.lambda_list, evaluated_args);
	}

	new_locals = combine(arg_bindings, *locals);

	if (fn->type == TYPE_CFUNC || fn->type == TYPE_CFORM) {
		return fn->value.c_code.func(&new_locals);
	} else {
		return gh_begin(fn->value.func.body, &new_locals);
	}
}

datum *lang_apply(datum **locals) {
	datum *fn;
	datum *args;

	fn = var_get(*locals, "#fn");
	args = var_get(*locals, "#args");

	if (fn->type == TYPE_SYMBOL) {
		fn = var_get(*locals, fn->value.string);
	}

	gh_assert(fn->type == TYPE_FUNC || fn->type == TYPE_CFUNC, "TYPE-ERROR", "Attempt to apply non function", fn)
	return apply(fn, args, locals);
}

datum *apply_macro(datum *macro, datum *args, datum **locals) {
	datum *arg_bindings;
	datum *new_locals;
	datum *result;

	arg_bindings = bind_args(macro->value.func.lambda_list, args);
	new_locals = combine(arg_bindings, *locals);

	result = gh_begin(macro->value.func.body, &new_locals);
	return result;
}

datum *macroexpand(datum *expr, datum **locals) {
	if (expr->type == TYPE_CONS) {
		datum *first;

		first = expr->value.cons.car;
		if (first->type == TYPE_SYMBOL) {
			datum *value;

			value = symbol_get(*locals, first->value.string);
			if (value == NULL) {
				value = symbol_get(globals, first->value.string);
			}
			if (value == NULL) {
				return expr;
			}

			first = value;
		}
		if (first->type == TYPE_MACRO) {
			return apply_macro(first, expr->value.cons.cdr, locals);
		} else {
			return expr;
		}
	} else {
		return expr;
	}
}

datum *lang_macroexpand(datum **locals) {
	datum *expr;

	expr = var_get(*locals, "#expr");

	return macroexpand(expr, locals);
}

unsigned int num_digits(unsigned int num) {
	unsigned int test;
	unsigned int count;

	test = num;
	count = 0;

	while (test > 0) {
		test = test / 10;
		count++;
	}

	if (test <= 0) {
		return 1;
	} else {
		return count;
	}
}

datum *gensym(char *name) {
	unsigned int sym_len;
	char *sym_str;
	static unsigned long int count = 0;

	sym_len = strlen(name) + 3; /* +3 = 1 for null byte, 1 for '#' character, one for ':' character */
	sym_len += num_digits(count);

	sym_str = GC_MALLOC(sizeof(char) * sym_len);
	snprintf(sym_str, sym_len, "#%s:%ld", name, count++);

	return gh_symbol(sym_str);
}

datum *lang_gensym(datum **locals) {
	datum *name;

	name = var_get(*locals, "#name");
	if (name->type == TYPE_NIL) {
		name = gh_string("");
	} else {
		name = name->value.cons.car;
	}


	gh_assert(name->type == TYPE_SYMBOL || name->type == TYPE_STRING, "TYPE-ERROR", "Non symbol or string passed to gensym", name);

	return gensym(name->value.string);
}

datum *lang_try(datum **locals) {
	datum *action;
	datum *handlers;
	datum *new_locals;
	datum *iterator;

	action = var_get(*locals, "#action");
	handlers = var_get(*locals, "#except");
	handlers = map(&translate_binding, handlers);

	/* Evaluate the exception handler functions so that they are functions, not symbols or lists */
	iterator = handlers;
	while (iterator->type == TYPE_CONS) {
		datum *handler;
		handler = iterator->value.cons.car;
		gh_assert(handler->type == TYPE_CONS, "TYPE-ERROR", "Expected exception handler in try statement", handler);
		handler->value.cons.cdr = gh_eval(handler->value.cons.cdr, locals);
		iterator = iterator->value.cons.cdr;
	}

	new_locals = gh_cons(gh_cons(gh_symbol("#handlers"), handlers), *locals);

	return gh_eval(action, &new_locals);
}

