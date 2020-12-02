#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <termios.h>
#include <gc.h>
#include <histedit.h>

#include "gharial.h"
#include "y.tab.h"
#include "lex.yy.h"

#define BUFF_SIZE 512

extern char **environ;

datum *translate_binding(datum *let_binding);
datum *translate_bindings(datum *bindings);
bool is_path(char *str);
datum *gh_exec(char *name);
datum *exec_lookup(char *name);
char **build_argv(datum *ex, datum *arglist);
void set_proc_IO(FILE *input, FILE *output, FILE *error);
datum *gh_error();
datum *gh_pid(int num);
datum *run_exec_nowait(datum *ex, datum *args, datum **locals);
datum *run_exec(datum *ex, datum *args, datum **locals);
datum *gh_pipe();
datum *pipe_eval(datum *expr, datum **locals);
datum *eval_arglist(datum *args, datum **locals);
bool is_path_executable(const char *path);
void job_remove(datum *job);
char *gh_read_all(FILE *f);
datum *strip_stack_frames(datum *lst);
void gh_stacktrace(FILE *file, datum **locals);
datum *typecheck(datum *args, datum * types, char *mismatch_fmt,  datum **locals);
datum *gh_cons2(datum *car, datum *cdr, datum **locals);

datum LANG_NIL_VALUE = { TYPE_NIL, { 0 } };
datum LANG_TRUE_VALUE = { TYPE_TRUE, { 0 } };
datum LANG_EOF_VALUE = { TYPE_EOF, { 0 } };

datum *new_datum() {
	datum *d = GC_MALLOC(sizeof(datum));
	if (d == NULL) {
		fprintf(stderr, "memory-error: out of memory in new datum!");
		exit(EXIT_FAILURE);
	}
	return d;
}

datum *fold(datum *(*func)(datum *a, datum *b, datum **locals), datum *init, datum *lst, datum **locals) {
	datum *iterator;
	datum *result;

	gh_assert(lst->type == TYPE_CONS || lst->type == TYPE_NIL, "type-error", "argument 3 is not a list: ~s", gh_cons(lst, &LANG_NIL_VALUE));

	iterator = lst;
	result = init;

	while (iterator->type == TYPE_CONS) {
		result = func(iterator->value.cons.car, result, locals);
		iterator = iterator->value.cons.cdr;
	}

	gh_assert(iterator->type == TYPE_NIL, "type_error", "argument 3 is an improper list: ~s", gh_cons(lst, &LANG_NIL_VALUE));

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

datum *combine(datum *lst1, datum *lst2, datum **locals) {
	if (lst1->type == TYPE_NIL) {
		return lst2;
	} else if (lst2->type == TYPE_NIL) {
		return lst1;
	} else {
		datum *reversed1;

		reversed1 = reverse(lst1);
		return fold(&gh_cons2, lst2, reversed1, locals);
	}
}

datum *add2(datum *a, datum *b, datum **locals) {
	int result_type;

	gh_assert(a->type == TYPE_INTEGER || a->type == TYPE_DECIMAL, "type-error", "non-number argument: ~s", gh_cons(a, &LANG_NIL_VALUE));
	gh_assert(b->type == TYPE_INTEGER || b->type == TYPE_DECIMAL, "type-error", "non-number argument: ~s", gh_cons(b, &LANG_NIL_VALUE));

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

datum *sub2(datum *a, datum *b, datum **locals) {
	int result_type;

	gh_assert(a->type == TYPE_INTEGER || a->type == TYPE_DECIMAL, "type-error", "non-number argument ~s", gh_cons(a, &LANG_NIL_VALUE));
	gh_assert(b->type == TYPE_INTEGER || b->type == TYPE_DECIMAL, "type-error", "non-number argument ~s", gh_cons(b, &LANG_NIL_VALUE));

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

datum *mul2(datum *a, datum *b, datum **locals) {
	int result_type;

	gh_assert(a->type == TYPE_INTEGER || a->type == TYPE_DECIMAL, "type-error", "non-number argument ~s", gh_cons(a, &LANG_NIL_VALUE));
	gh_assert(b->type == TYPE_INTEGER || b->type == TYPE_DECIMAL, "type-error", "non-number argument ~s", gh_cons(b, &LANG_NIL_VALUE));

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

datum *div2(datum *a, datum *b, datum **locals) {
	gh_assert(a->type == TYPE_INTEGER || a->type == TYPE_DECIMAL, "type-error", "non-number argument ~s", gh_cons(a, &LANG_NIL_VALUE));
	gh_assert(b->type == TYPE_INTEGER || b->type == TYPE_DECIMAL, "type-error", "non-number argument ~s", gh_cons(b, &LANG_NIL_VALUE));

	if (a->type == TYPE_INTEGER) {
		gh_assert(a->value.integer != 0, "math-error", "division by zero", &LANG_NIL_VALUE);
	} else {
		gh_assert(a->value.decimal != 0.0, "math-error", "division by zero", &LANG_NIL_VALUE);
	}

	return gh_decimal((b->type == TYPE_INTEGER ? b->value.integer : b->value.decimal) /
						(a->type == TYPE_INTEGER ? a->value.integer : a->value.decimal));
}

datum *dpow(datum *a, datum *b, datum **locals) {
	gh_assert(a->type == TYPE_INTEGER || a->type == TYPE_DECIMAL, "type-error", "non-number argument ~s", gh_cons(a, &LANG_NIL_VALUE));
	gh_assert(b->type == TYPE_INTEGER || b->type == TYPE_DECIMAL, "type-error", "non-number argument ~s", gh_cons(b, &LANG_NIL_VALUE));

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
	if (value == NULL) {
		return NULL;
	}
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
	return var_get(locals, "#args");
}

datum *gh_cons(datum *car, datum *cdr) {
	datum *c;
	c = new_datum();
	c->type = TYPE_CONS;
	c->value.cons.car = car;
	c->value.cons.cdr = cdr;
	return c;
}

datum *gh_cons2(datum *car, datum *cdr, datum **locals) {
	return gh_cons(car, cdr);
}

datum *car(datum *pair) {
	return pair->value.cons.car;
}

datum *cdr(datum *pair) {
	return pair->value.cons.cdr;
}

datum *strip_stack_frames(datum *lst) {
	datum *result;
	datum *prev;
	datum *iterator;

	result = reverse(lst);
	prev = NULL;
	iterator = result;

	while (iterator->type == TYPE_CONS) {
		datum *current;

		current = iterator->value.cons.car;
		if (strcmp(current->value.cons.car->value.string, "#stack-frame")) {
			if (prev != NULL) {
				prev->value.cons.cdr = iterator->value.cons.cdr;
			} else {
				result = iterator->value.cons.cdr;
			}
		}
		prev = iterator;
		iterator = iterator->value.cons.cdr;
	}

	return result;
}

datum *lang_lambda(datum **locals) {
	datum *lambda_list;
	datum *body;
	datum *func;
	datum *iterator;

	lambda_list = var_get(locals, "#lambda-list");
	body = var_get(locals, "#body");

	iterator = lambda_list;

	while (iterator->type == TYPE_CONS) {
		gh_assert(iterator->value.cons.car->type == TYPE_SYMBOL, "type-error", "non-symbol in lambda list: ~s", gh_cons(iterator->value.cons.car, &LANG_NIL_VALUE));
		iterator = iterator->value.cons.cdr;
	}

	gh_assert(iterator->type == TYPE_NIL || iterator->type == TYPE_SYMBOL, "type-error", "non-symbol in lambda list: ~s", gh_cons(iterator, &LANG_NIL_VALUE));

	func = new_datum(sizeof(datum));
	func->type = TYPE_FUNC;
	func->value.func.lambda_list = lambda_list;
	func->value.func.body = body;
	func->value.func.closure = strip_stack_frames(*locals);
	func->value.func.name = "lambda";
	return func;	
}

datum *lang_macro(datum **locals) {
	datum *lambda_list;
	datum *body;
	datum *mac;
	datum *iterator;

	lambda_list = var_get(locals, "#lambda-list");
	body = var_get(locals, "#body");

	iterator = lambda_list;

	while (iterator->type == TYPE_CONS) {
		gh_assert(iterator->value.cons.car->type == TYPE_SYMBOL, "type-error", "non-symbol in lambda list: ~s", gh_cons(iterator->value.cons.car, &LANG_NIL_VALUE));
		iterator = iterator->value.cons.cdr;
	}

	gh_assert(iterator->type == TYPE_NIL || iterator->type == TYPE_SYMBOL, "type-error", "non-symbol in lambda list ~s", gh_cons(iterator, &LANG_NIL_VALUE));

	mac = new_datum(sizeof(datum));
	mac->type = TYPE_MACRO;
	mac->value.func.lambda_list = lambda_list;
	mac->value.func.body = body;
	mac->value.func.name = "macro";
	return mac;	
}

datum *lang_cond(datum **locals) {
	datum *iterator;

	iterator = var_get(locals, "#conditions");

	while (iterator->type != TYPE_NIL) {
		datum *current_cond;
		datum *eval_cond;

		current_cond = iterator->value.cons.car;
		eval_cond = gh_eval(current_cond->value.cons.car, locals);

		if (eval_cond->type == TYPE_NIL) {
			iterator = iterator->value.cons.cdr;
			continue;
		} else if ((eval_cond->type == TYPE_RETURNCODE || eval_cond->type == TYPE_INTEGER) && eval_cond->value.integer != 0) {
			iterator = iterator->value.cons.cdr;
			continue;
		} else {
			return gh_eval(current_cond->value.cons.cdr->value.cons.car, locals);
		}
	}

	return &LANG_NIL_VALUE;
}

datum *lang_is_eof_object(datum **locals) {
	datum *expr;

	expr = var_get(locals, "#expr");

	if (expr->type == TYPE_EOF)
		return &LANG_TRUE_VALUE;
	else 
		return &LANG_NIL_VALUE;
}

datum *lang_open(datum **locals) {
	datum *path;
	datum *mode;
	FILE *fptr;

	path = var_get(locals, "#fname");
	gh_assert(path->type == TYPE_STRING || path->type == TYPE_SYMBOL, "type-error", "argument 1 is neither a string nor a symbol", gh_cons(path, &LANG_NIL_VALUE));

	mode = var_get(locals, "#mode");

	if (mode->type == TYPE_NIL) {
		mode = gh_string("a+");
	} else {
		mode = mode->value.cons.car;
	}

	gh_assert(mode->type == TYPE_STRING || mode->type == TYPE_SYMBOL, "type-error", "argument 2 is neither a string nor a symbol: ~s", gh_cons(mode, &LANG_NIL_VALUE));

	fptr = fopen(path->value.string, mode->value.string);
	gh_assert(fptr != NULL, "file-error", "Could not open \"~a\": ~a", gh_cons(path, gh_cons(gh_error(), &LANG_NIL_VALUE)));

	return gh_file(fptr);
}

datum *lang_close(datum **locals) {
	datum *file;
	int result;

	file = var_get(locals, "#file");
	gh_assert(file->type == TYPE_FILE, "type-error", "argument is not a file: ~s", gh_cons(file, &LANG_NIL_VALUE));

	result = fclose(file->value.file);
	gh_assert(result == 0, "file-error", "error closing file: ~a", gh_cons(gh_error(), &LANG_NIL_VALUE));

	return &LANG_TRUE_VALUE;
}

datum *lang_read(datum **locals) {
	datum *file;
	FILE *oldyyin;

	file = var_get(locals, "#file");
	if (file->type == TYPE_CONS) {
		file = file->value.cons.car;
		gh_assert(file->type == TYPE_FILE, "type-error", "argument 1 is not a file: ~s", gh_cons(file, &LANG_NIL_VALUE));
	} else { 
		gh_assert(file->type == TYPE_FILE, "type-error", "argument 1 is not a file: ~s", gh_cons(file, &LANG_NIL_VALUE));
		file = var_get(locals, "input-file");
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

	expr = var_get(locals, "#expr");

	file = var_get(locals, "#file");
	if (file->type == TYPE_CONS) {
		file = file->value.cons.car;
		gh_assert(file->type == TYPE_FILE, "type-error", "argument 1 is not a file: ~s", gh_cons(file, &LANG_NIL_VALUE));
	} else { 
		file = var_get(locals, "output-file");
	}

	gh_print(file->value.file, expr);
	return expr;
}

datum *lang_load(datum **locals) {
	datum *path;

	path = var_get(locals, "#path");
	gh_assert(path->type == TYPE_STRING || path->type == TYPE_SYMBOL, "type-error", "argument 1 is neither a string nor a symbol: ~s", gh_cons(path, &LANG_NIL_VALUE));

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

datum *translate_bindings(datum *bindings) {
	datum *copy;
	datum *iterator;

	copy = list_copy(bindings);
	iterator = copy;

	while (iterator->type == TYPE_CONS) {
		datum *current;
		current = iterator->value.cons.car;
		gh_assert(current->type == TYPE_CONS, "type-error", "attempt to translate binding that is not a list: ~s", gh_cons(current, &LANG_NIL_VALUE));
		iterator->value.cons.car = translate_binding(current);
		iterator = iterator->value.cons.cdr;
	}
	return copy;
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

	bindings = var_get(locals, "#bindings");
	body = var_get(locals, "#body");

	translated_bindings = translate_bindings(bindings);
	gh_assert(translated_bindings->type == TYPE_CONS || translated_bindings->type == TYPE_NIL, "type-error", "error occured while translating bindings: ~s", gh_cons(bindings, &LANG_NIL_VALUE));
	new_locals = combine(translated_bindings, *locals, locals);

	result = &LANG_NIL_VALUE;

	iterator = body;
	while (iterator->type != TYPE_NIL) {
		result = gh_eval(iterator->value.cons.car, &new_locals);
		if (result->type == TYPE_EXCEPTION) {
			return result;
		} else if (result->type == TYPE_RECUR) {
			datum *recur_bindings;

			recur_bindings = bind_args(map(&car, translated_bindings), result->value.recur.bindings);
			new_locals = combine(recur_bindings, *locals, locals);
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

	bindings = var_get(locals, "#bindings");
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
	datum *iterator;

	bindings = var_get(locals, "#bindings");
	body = var_get(locals, "#body");


	translated_bindings = translate_bindings(bindings);
	gh_assert(translated_bindings->type == TYPE_CONS || translated_bindings->type == TYPE_NIL, "type-error", "error occured while translating bindings: ~s", gh_cons(bindings, &LANG_NIL_VALUE));

	iterator = translated_bindings;
	while (iterator->type == TYPE_CONS) {
		datum *current;

		current = iterator->value.cons.car;
		current->value.cons.cdr = gh_eval(current->value.cons.cdr, locals);
		iterator = iterator->value.cons.cdr;
	}

	new_locals = combine(translated_bindings, *locals, locals);

	return gh_begin(body, &new_locals);
}

void print_exception(FILE *file, datum *expr, datum **locals) {
	char *type;
	char *fmt;
	datum *fmt_args;

	type = expr->value.exception.type;
	fmt = expr->value.exception.fmt;
	fmt_args = expr->value.exception.fmt_args;

	if (gh_input != NULL) {
		fprintf(file, "Uncaught exception in expression: ");
		gh_print(file, gh_input);
	}

	fprintf(file, "Traceback:\n");
	gh_stacktrace(file, locals);

	fprintf(file, "%s: ", type);
	gh_format(file, fmt, fmt_args);
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
	datum *handlers;
	datum *result;
	datum *handler;

	switch (expr->type) {
		case TYPE_CONS:
			expanded = expr;
			while (is_macro_call(expanded, locals)) {
				expanded = macroexpand(expanded, locals);
			}

			value = gh_eval(expanded->value.cons.car, locals);

			result = apply(value, expanded->value.cons.cdr, locals);
			break;
		case TYPE_SYMBOL:
			symbol = expr->value.string;
			value = var_get(locals, symbol);
			result = value;
			break;
		case TYPE_EXCEPTION:
			handlers = symbol_get(*locals, "#handlers");
			if (handlers == NULL) {
				handler = NULL;
			} else {
				handler = symbol_get(handlers, expr->value.exception.type);
			}

			if (handler == NULL) {
				datum *out;

				out = var_get(locals, "error-file");
				print_exception(out->value.file, expr, locals);
				if (interactive) {
					depth = 0;
					longjmp(toplevel, TRUE);
				} else {
					exit(EXIT_FAILURE);
				}
			} else {
				result = apply(handler, expr, locals);
			}
			break;
		default:
			result = expr;
	}

	if (result->type == TYPE_RETURNCODE) {
		symbol_set(&globals, "*?*", gh_integer(result->value.integer));
	} else if(result->type == TYPE_CAPTURE) {
		symbol_set(&globals, "*?*", result->value.cons.cdr);
		return result->value.cons.car;
	} else {
		symbol_set(&globals, "*?*", result);
	}
	return result;
}

datum *lang_eval(datum **locals) {
	datum *expr;

	expr = var_get(locals, "#expr");
	return gh_eval(expr, locals);
}

void print_datum(FILE *file, datum *expr) {
	datum *iterator;
	switch (expr->type) {
		case TYPE_NIL:
			fprintf(file, "nil");
			break;
		case TYPE_TRUE:
			fprintf(file, "t");
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
		case TYPE_RETURNCODE:
			break;
		case TYPE_PID:
			fprintf(file, "<pid %d>", expr->value.integer);
			break;
		case TYPE_EXECUTABLE:
			fprintf(file, "<executable:%s>", expr->value.executable.path);
			break;
		case TYPE_CAPTURE:
			fprintf(file, "<captured_output>");
			break;
		case TYPE_JOB:
			fprintf(file, "<job:");
			print_datum(stdout, expr->value.job.commands);
			fprintf(file, ">");
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
	if (expr->type != TYPE_RETURNCODE) {
		fprintf(file, "\n");
	}
}

char *string_copy(const char *str) {
	char *copy;

	copy = GC_MALLOC(sizeof(char) * (strlen(str) + 1));
	strcpy(copy, str);
	return copy;
}

datum *symbol_loc_one(datum *table, char *symbol) {
	datum *iterator;


	if (symbol == NULL) {
		return NULL;
	}

	iterator = table;
	while (iterator->type == TYPE_CONS) {
		datum *current;

		current = iterator->value.cons.car;
		if (current->type != TYPE_CONS) {
			return NULL;
		}

		if (strcmp(current->value.cons.car->value.string, symbol) == 0)
			return current;
		iterator = iterator->value.cons.cdr;
	}
	return NULL;
}

datum *symbol_loc(datum *table, char *symbol) {
	char *sym_copy;
	char *current_sym;
	datum *loc;
	datum *current_table;

	sym_copy = string_copy(symbol);
	current_sym = strtok(sym_copy, ".");
	current_table = table;
	do {
		loc = symbol_loc_one(current_table, current_sym);
		if (loc == NULL) {
			return NULL;
		}
		current_table = loc->value.cons.cdr;
		current_sym = strtok(NULL, ".");
	} while (current_sym != NULL);

	return loc;
}

datum *symbol_get(datum *table, char *symbol) {
	datum *loc;
	loc = symbol_loc(table, symbol);
	if (loc == NULL)
		return NULL;
	else
		return loc->value.cons.cdr;
}

datum *var_get(datum **locals, char *symbol) {
	datum *var;
	char *env_var;

	var = symbol_get(*locals, symbol);
	if (var == NULL) {
		var = symbol_get(globals, symbol);
		if (var == NULL) {
			env_var = getenv(symbol);
			if (env_var != NULL) {
				var = gh_string(env_var);
			} else {
				var = exec_lookup(symbol);
				gh_assert(var != NULL, "ref-error", "Unbound variable: ~s", gh_cons(gh_symbol(symbol), &LANG_NIL_VALUE));
			}
		}
	}
	return var;
}

char *symbol_set(datum **table, char *symbol, datum *value) {
	char *sym_copy;
	char *current_sym;
	char *next_sym;
	datum *loc;
	datum *current_table;
	datum *prev_loc;

	prev_loc = NULL;
	sym_copy = string_copy(symbol);
	current_sym = strtok(sym_copy, ".");
	current_table = *table;

	if (strcmp(symbol, "*?*") != 0) {
		if (value->type == TYPE_FUNC || value->type == TYPE_MACRO) {
			value->value.func.name = symbol;
		} else if (value->type == TYPE_CFUNC || value->type == TYPE_CFORM) {
			value->value.c_code.name = symbol;
		}
	}

	do {
		loc = symbol_loc_one(current_table, current_sym);
		next_sym = strtok(NULL, ".");
		if (next_sym == NULL) {
			if (loc == NULL) {
				if (current_table == *table) {
					*table = gh_cons(gh_cons(gh_symbol(current_sym), value), current_table);
				} else {
					prev_loc->value.cons.cdr = gh_cons(gh_cons(gh_symbol(current_sym), value), current_table);
				}
			} else {
				loc->value.cons.cdr = value;
			}
		}

		if (loc == NULL) {
			return NULL;
		}

		current_table = loc->value.cons.cdr;
		prev_loc = loc;
		current_sym = next_sym;
	} while (current_sym != NULL);

	return symbol;
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
	gh_assert(file != NULL, "file-error", "could not open file \"~a\": ~a", gh_cons(gh_string(path), gh_cons(gh_error(), &LANG_NIL_VALUE)));

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
	} while (gh_result->type != TYPE_EOF && gh_result->type != TYPE_EXCEPTION);

	print_flag = old_print_flag;
	yypop_buffer_state();
	yyin = oldyyin;
	yylex_destroy();

	fclose(file);

	current_file = old_current_file;
	if (gh_result->type == TYPE_EXCEPTION) {
		return gh_result;
	} else {
		return &LANG_TRUE_VALUE;
	}
}

datum *do_unquotes(datum *expr, datum **locals) {
	datum *iterator;
	datum *current;
	datum *copy;
	datum *prev;

	if (expr->type == TYPE_CONS) {
		copy = list_copy(expr);
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

datum *list_copy(datum *lst) {
	datum *reversed;
	datum *copy;
	datum *iterator;

	reversed = &LANG_NIL_VALUE;
	iterator = lst;

	while (iterator->type == TYPE_CONS) {
		reversed = gh_cons(iterator->value.cons.car, reversed);
		iterator = iterator->value.cons.cdr;
	}

	copy = iterator;

	iterator = reversed;

	
	while (iterator->type == TYPE_CONS) {
		copy = gh_cons(iterator->value.cons.car, copy);
		iterator = iterator->value.cons.cdr;
	}

	return copy;
}

datum *append(datum *lst1, datum *lst2) {
	datum *result;
	datum *iterator;

	result = reverse(lst1);
	iterator = lst2;
	while (iterator->type == TYPE_CONS) {
		result = gh_cons(iterator->value.cons.car, result);
		iterator = iterator->value.cons.cdr;
	}

	return reverse(result);
}

datum *lang_append(datum **locals) {
	datum *lst1;
	datum *lst2;

	lst1 = var_get(locals, "#lst1");
	gh_assert(lst1->type == TYPE_CONS, "type-error", "argument 1 is not a list: ~s", gh_cons(lst1, &LANG_NIL_VALUE));

	lst2 = var_get(locals, "#lst2");
	gh_assert(lst2->type == TYPE_CONS, "type-error", "argument 2 is not a list: ~s", gh_cons(lst2, &LANG_NIL_VALUE));

	return append(lst1, lst2);
}

datum *lang_set(datum **locals) {
	datum *symbol;
	datum *value;
	datum *loc;

	symbol = var_get(locals, "#symbol");
	value = gh_eval(var_get(locals, "#value"), locals);

	loc = symbol_loc(*locals, symbol->value.string);
	if (loc)
		loc->value.cons.cdr = gh_eval(value, locals);
	else
		symbol_set(&globals, symbol->value.string, value);

	return symbol;
}

datum *lang_setenv(datum **locals) {
	datum *symbol;
	datum *value;

	symbol = var_get(locals, "#symbol");
	gh_assert(symbol->type == TYPE_STRING || symbol->type == TYPE_SYMBOL, "type-error", "argument 1 is neither a symbol nor a string: ~s", gh_cons(symbol, &LANG_NIL_VALUE));

	value = var_get(locals, "#value");
	value = gh_eval(value, locals);
	gh_assert(value->type == TYPE_STRING, "type-error", "Environment variables can only be set to string values: ~s", gh_cons(value, &LANG_NIL_VALUE));

	setenv(symbol->value.string, value->value.string, TRUE);

	return symbol;
}

datum *lang_quote(datum **locals) {
	datum *expr;

	expr = var_get(locals, "#expr");
	return do_unquotes(expr, locals);
}

datum *lang_unquote(datum **locals) {
	datum *expr;

	expr = var_get(locals, "#expr");
	return gh_eval(expr, locals);
}

datum *lang_unquote_splice(datum **locals) {
	gh_assert(TRUE, "runtime-error", "call to unquote-splice", &LANG_NIL_VALUE);
	return &LANG_NIL_VALUE;
}

datum *lang_cons(datum **locals) {
	datum *car;
	datum *cdr;

	car = var_get(locals, "#car");
	cdr = var_get(locals, "#cdr");
	return gh_cons(car, cdr);
}

datum *lang_car(datum **locals) {
	datum *pair;

	pair = var_get(locals, "#pair");
	gh_assert(pair->type == TYPE_CONS, "type-error", "not a pair nor a list: ~s", gh_cons(pair, &LANG_NIL_VALUE));

	return pair->value.cons.car;
}

datum *lang_cdr(datum **locals) {
	datum *pair;

	pair = var_get(locals, "#pair");
	gh_assert(pair->type == TYPE_CONS, "type-error", "not a pair nor a list: ~s", gh_cons(pair, &LANG_NIL_VALUE));

	return pair->value.cons.cdr;
}

datum *lang_reverse(datum **locals) {
	datum *lst;

	lst = var_get(locals, "#lst");

	return reverse(lst);
}

datum *lang_equal(datum **locals) {
	datum *a;
	datum *b;
	int result;

	a = var_get(locals, "#a");
	b = var_get(locals, "#b");

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

	args = var_get(locals, "#args");

	return fold(&add2, gh_integer(0), args, locals);
}

datum *lang_sub(datum **locals) {
	datum *first;
	datum *rest;

	first = var_get(locals, "#first");
	rest = var_get(locals, "#rest");

	return fold(&sub2, first, rest, locals);
}

datum *lang_mul(datum **locals) {
	datum *args;

	args = var_get(locals, "#args");

	return fold(&mul2, gh_integer(1), args, locals);
}

datum *lang_div(datum **locals) {
	datum *first;
	datum *rest;

	first = var_get(locals, "#first");
	rest = var_get(locals, "#rest");

	return fold(&div2, first, rest, locals);
}

datum *lang_pow(datum **locals) {
	datum *a;
	datum *b;

	a = var_get(locals, "#a");
	b = var_get(locals, "#b");

	return(dpow(a, b, locals));
}

datum *gh_cfunc(datum *(*addr)(datum **), datum *args) {
	datum *cf = new_datum(sizeof(datum));
	cf->type = TYPE_CFUNC;
	cf->value.c_code.func = addr;
	cf->value.c_code.lambda_list = args;
	cf->value.c_code.name = "cfunc";
	return cf;
}

datum *gh_cform(datum *(*addr)(datum **), datum *args) {
	datum *cf = new_datum(sizeof(datum));
	cf->type = TYPE_CFORM;
	cf->value.c_code.func = addr;
	cf->value.c_code.lambda_list = args;
	cf->value.c_code.name = "cform";
	return cf;
}

datum *eval_arglist(datum *args, datum **locals) {
	datum *iterator;
	datum *argscopy;

	argscopy = list_copy(args);	
	iterator = argscopy;

	while (iterator->type == TYPE_CONS) {
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

	result = GC_MALLOC(sizeof(char) * ((len1 - 1) + len2));
	if (result == NULL) {
		fprintf(stderr, "Memory error: line %d: Out of memory in string-append!", yylineno);
		exit(EXIT_FAILURE);
	}
	strcpy(result, str1);
	strcpy(result + len1 - 1, str2);

	return result;
}

datum *string_split(const char *str, const char *delim) {
	char *copy;
	char *tok;
	datum *result;

	copy = GC_MALLOC(sizeof(char) * (strlen(str) + 1));
	result = &LANG_NIL_VALUE;

	strcpy(copy, str);

	tok = strtok(copy, delim);

	while (tok != NULL) {
		result = gh_cons(gh_string(tok), result);
		tok = strtok(NULL, delim);
	}

	return reverse(result);
}

datum *lang_string_split(datum **locals) {
	datum *str;
	datum *delim;

	str = var_get(locals, "#str");
	gh_assert(str->type == TYPE_STRING, "type-error", "argument 1 is not a string ~s:", gh_cons(str, &LANG_NIL_VALUE));

	delim = var_get(locals, "#delim");
	if (delim->type == TYPE_NIL) {
		delim = gh_string(" ");
	} else {
		delim = delim->value.cons.car;
	}
	gh_assert(delim->type == TYPE_STRING, "type-error", "argument 2 is not a string ~s:", gh_cons(delim, &LANG_NIL_VALUE));

	return string_split(str->value.string, delim->value.string);
}

datum *gh_exception(char *type, char *fmt, datum *fmt_args) {
	datum *ex;

	ex = new_datum();
	ex->type = TYPE_EXCEPTION;

	ex->value.exception.type = GC_MALLOC(sizeof (char) * (strlen(type) + 1));
	if (ex->value.exception.type == NULL) {
		fprintf(stderr, "memory-error: out of memory in gh_exception!");
		exit(EXIT_FAILURE);
	}
	strcpy(ex->value.exception.type, type);
	
	ex->value.exception.fmt = GC_MALLOC(sizeof (char) * (strlen(fmt) + 1));
	if (ex->value.exception.fmt == NULL) {
		fprintf(stderr, "memory-error: out of memory in gh_exception!");
		exit(EXIT_FAILURE);
	}
	strcpy(ex->value.exception.fmt, fmt);

	ex->value.exception.fmt_args = fmt_args;

	return ex;
}

datum *gh_begin(datum *body, datum **locals) {
	datum *iterator;
	datum *result;

	iterator = body;
	result = &LANG_NIL_VALUE;

	while (iterator->type == TYPE_CONS) {
		result = gh_eval(iterator->value.cons.car, locals);
		if (result->type == TYPE_EXCEPTION) {
			return result;
		}
		iterator = iterator->value.cons.cdr;
	}

	return result;
}

datum *apply(datum *fn, datum *args, datum **locals) {
	datum *evaluated_args;
	datum *arg_bindings;
	datum *new_locals;
	datum *stack_frame;
	datum *result;
	char *sf_file;
	char *fname;

	gh_assert(fn->type == TYPE_CFORM || fn->type == TYPE_CFUNC ||
				fn->type == TYPE_FUNC || fn->type == TYPE_EXECUTABLE,
				"type-error", "attempt to call non-function: ~s", gh_cons(fn, &LANG_NIL_VALUE));

	if (current_file == NULL) {
		sf_file = "<REPL>";
	} else {
		sf_file = current_file;
	}

	if (fn->type == TYPE_FUNC || fn->type == TYPE_MACRO) {
		fname = fn->value.func.name;
	} else {
		fname = fn->value.c_code.name;
	}

	if (fn->type == TYPE_FUNC || fn->type == TYPE_MACRO || fn->type == TYPE_CFUNC || fn->type == TYPE_CFORM) {
		stack_frame = gh_cons(gh_symbol("#stack-frame"), gh_cons(gh_symbol(fname), gh_cons(gh_string(sf_file), gh_cons(gh_integer(yylineno), &LANG_NIL_VALUE))));
	}

	new_locals = gh_cons(stack_frame, *locals);

	if (fn->type == TYPE_CFORM || fn->type == TYPE_EXECUTABLE) { /* Do not evaluate arguments if special form */
		evaluated_args = args;
	} else {
		evaluated_args = eval_arglist(args, locals);
	}

	if (fn->type == TYPE_EXECUTABLE) {
		return run_exec(fn, evaluated_args, locals);
	}

	if (fn->type == TYPE_CFUNC || fn->type == TYPE_CFORM) {
		arg_bindings = bind_args(fn->value.c_code.lambda_list, evaluated_args);
	} else {
		arg_bindings = bind_args(fn->value.func.lambda_list, evaluated_args);
	}

	if (fn->type == TYPE_FUNC || fn->type == TYPE_CFORM) {
		new_locals = combine(fn->value.func.closure, new_locals, locals);
	}

	new_locals = combine(arg_bindings, new_locals, locals);

	if (fn->type == TYPE_CFUNC || fn->type == TYPE_CFORM) {
		result = fn->value.c_code.func(&new_locals);
	} else {
		result = gh_begin(fn->value.func.body, &new_locals);
	}

	return result;
}

datum *lang_apply(datum **locals) {
	datum *fn;
	datum *args;

	fn = var_get(locals, "#fn");
	args = var_get(locals, "#args");

	if (fn->type == TYPE_SYMBOL) {
		fn = var_get(locals, fn->value.string);
	}

	return apply(fn, args, locals);
}

datum *apply_macro(datum *macro, datum *args, datum **locals) {
	datum *arg_bindings;
	datum *new_locals;
	datum *result;

	arg_bindings = bind_args(macro->value.func.lambda_list, args);
	new_locals = combine(arg_bindings, *locals, locals);

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

	expr = var_get(locals, "#expr");

	return macroexpand(expr, locals);
}

unsigned int num_digits(unsigned int num) {
	unsigned int test;
	unsigned int count;

	test = num;
	count = 0;

	while (test > 0) {
		test = test / 10; count++;
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

	name = var_get(locals, "#name");
	if (name->type == TYPE_NIL) {
		name = gh_string("");
	} else {
		name = name->value.cons.car;
	}


	gh_assert(name->type == TYPE_SYMBOL || name->type == TYPE_STRING, "type-error", "not a symbol or string: ~s", gh_cons(name, &LANG_NIL_VALUE));

	return gensym(name->value.string);
}

datum *lang_try(datum **locals) {
	datum *action;
	datum *handlers;
	datum *new_locals;
	datum *iterator;

	action = var_get(locals, "#action");
	handlers = var_get(locals, "#except");
	handlers = translate_bindings(handlers);
	gh_assert(handlers->type == TYPE_CONS || handlers->type == TYPE_NIL, "type-error", "error occured while translating handler: ~s", gh_cons(var_get(locals, "#except"), &LANG_NIL_VALUE));

	/* Evaluate the exception handler functions so that they are functions, not symbols or lists */
	iterator = handlers;
	while (iterator->type == TYPE_CONS) {
		datum *handler;
		handler = iterator->value.cons.car;
		gh_assert(handler->type == TYPE_CONS, "type-error", "not an exception handler: ~s", gh_cons(handler, &LANG_NIL_VALUE));
		handler->value.cons.cdr = gh_eval(handler->value.cons.cdr, locals);
		iterator = iterator->value.cons.cdr;
	}

	new_locals = gh_cons(gh_cons(gh_symbol("#handlers"), handlers), *locals);

	return gh_eval(action, &new_locals);
}

datum *lang_exception(datum **locals) {
	datum *type;
	datum *fmt;
	datum *fmt_args;

	type = var_get(locals, "#type");
	gh_assert(type->type == TYPE_SYMBOL || type->type == TYPE_STRING,
				"type-error", "argument 1 is not a symbol nor a string: ~s",
				gh_cons(type, &LANG_NIL_VALUE));

	fmt = var_get(locals, "#fmt");
	gh_assert(fmt->type == TYPE_STRING,
				"type-error", "argumet 2 is not a string: ~s",
				fmt);

	fmt_args = var_get(locals, "#fmt_args");


	gh_assert(fmt_args->type == TYPE_CONS || fmt_args == TYPE_NIL, "type-error", "argument 3 is not a list: ~s", gh_cons(fmt_args, &LANG_NIL_VALUE));

	return gh_eval(gh_exception(type->value.string, fmt->value.string, fmt_args), locals);
}

char *gh_readline(FILE *file) {
	char *result;

	if (file == stdin && interactive) {
		int count;
		const char *el_str = el_gets(gh_editline, &count);

		if (count < 0) {
			return "";
		}

		if (count == 0) {
			if (el_str == NULL) {
				return NULL;
			} else {
				return "";
			}
		}

		result = GC_MALLOC(count);
		strncpy(result, el_str, count);
		result[count - 1] = '\0';
		history(gh_history, &gh_last_histevent, H_ENTER, result);
		return result;
	} else {
		char buff[sizeof(char) * BUFF_SIZE];
		int count;
		int total;
		result = "";

		total = 0;


		do {
			char *status;
			status = fgets(buff, BUFF_SIZE, file);

			if (status == NULL) {
				return NULL;
			}

			count = strlen(buff) + 1;

			if (count <= 2) {
				if (feof(file)) {
					return NULL;
				} else {
					return "";
				}
			}

			total += count - 1;
			result = string_append(result, buff);
		} while(result[total - 1] != '\n');

		result[total - 1] = '\0'; /* get rid of newline char at the end */
		return result;
	}
}

datum *lang_read_line(datum **locals) {
	datum *file;
	datum *result;
	bool old_prompt_flag;

	file = var_get(locals, "#file");

	if (file->type == TYPE_CONS) {
		file = file->value.cons.car;
		gh_assert(file->type == TYPE_FILE, "type-error", "not a file: ~s", gh_cons(file, &LANG_NIL_VALUE));
	} else {
		file = var_get(locals, "input-file");
	}

	old_prompt_flag = prompt_flag;
	prompt_flag = FALSE;
	result = gh_string(gh_readline(file->value.file));
	prompt_flag = old_prompt_flag;
	if (result == NULL) {
		return &LANG_EOF_VALUE;
	} else {
		return result;
	}
}

datum *lang_exit(datum **locals) {
	datum *status;

	status = var_get(locals, "#status");
	if (status->type == TYPE_NIL) {
		status = gh_integer(EXIT_SUCCESS);
	} else {
		status = status->value.cons.car;
		gh_assert(status->type == TYPE_INTEGER, "type-error", "not an integer: ~s", gh_cons(status, &LANG_NIL_VALUE));
	}
	exit(status->value.integer);
}

datum *gh_return_code(int num) {
	datum *rc;
	rc = gh_integer(num);
	rc->type = TYPE_RETURNCODE;
	return rc;
}

datum *lang_return_code(datum **locals) {
	datum *num;

	num = var_get(locals, "#num");
	gh_assert(num->type == TYPE_INTEGER, "type-error", "not an integer: ~s", gh_cons(num, &LANG_NIL_VALUE));
	return gh_return_code(num->value.integer);
}

bool is_path(char *str) {
	int i;
	for (i = 0; str[i] != '\0'; i++) {
		if (str[i] == '/') {
			return TRUE;
		}
	}
	return FALSE;
}

bool is_path_executable(const char *path) {
	int stat_status;
	struct stat statbuff;

	stat_status = stat(path, &statbuff);

	if (stat_status != 0) {
		return FALSE;
	}

	if (!(statbuff.st_mode & X_OK)) {
		return FALSE;
	}

	return S_ISREG(statbuff.st_mode);
}

datum *gh_exec(char *name) {
	datum *ex;

	ex = new_datum();
	ex->type = TYPE_EXECUTABLE;
	ex->value.executable.path = name;
	return ex;
}

datum *exec_lookup(char *name) {
	if (is_path(name)) {
		if (is_path_executable(name)) {
			return gh_exec(name);
		} else {
			return NULL;
		}
	} else {
		datum *iterator;
		iterator = var_get(locals, "*PATH*");
		while (iterator->type == TYPE_CONS) {
			datum *dir;
			char *path;

			dir = iterator->value.cons.car;
			path = string_append(dir->value.string, "/");
			path = string_append(path, name);
			if (is_path_executable(path)) {
				return gh_exec(path);
			}
			iterator = iterator->value.cons.cdr;
		}
		return NULL;
	}
}

int gh_length(datum *lst) {
	int count;
	datum *iterator;

	count = 0;
	iterator = lst;
	while (iterator->type == TYPE_CONS) {
		iterator = iterator->value.cons.cdr;
		count++;
	}
	return count;
}

datum *lang_length(datum **locals) {
	datum *lst;

	lst = var_get(locals, "#lst");
	gh_assert(lst->type == TYPE_CONS, "type-error", "not a list: ~s", gh_cons(lst, &LANG_NIL_VALUE));

	return gh_integer(gh_length(lst));
}

char **build_argv(datum *ex, datum *arglist) {
	char **argv;
	int len;
	datum *iterator;
	int i;

	len = gh_length(arglist) + 1;

	argv = GC_MALLOC(sizeof(char *) * len);

	argv[0] = ex->value.executable.path;

	iterator = arglist;
	i = 1;
	while (iterator->type == TYPE_CONS) {
		argv[i++] = iterator->value.cons.car->value.string;
		iterator = iterator->value.cons.cdr;
	}

	return argv;
}

void set_proc_IO(FILE *input, FILE *output, FILE *error) {
	if (input != NULL && input != stdin) {
		datum *gh_stdin;
		int dup_status;

		fclose(stdin);
		dup_status = dup2(fileno(input), 0);
		if (dup_status == -1) {
			fprintf(stderr, "Error: could not duplicate file descriptor %d for new stdin: %s\n",
					fileno(input), strerror(errno));
			exit(EXIT_FAILURE);
		}
		stdin = fdopen(0, "r");
		if (stdin == NULL) {
			fprintf(stderr, "Error: could not open file descriptor %d for new stdin: %s\n",
					fileno(input), strerror(errno));
			exit(EXIT_FAILURE);
		}
		gh_stdin = gh_file(stdin);
		symbol_set(&globals, "input-file", gh_stdin);
		symbol_set(&globals, "*STDIN*", gh_stdin);
	}
	if (output != NULL && output != stdout) {
		datum *gh_stdout;
		int dup_status;

		fclose(stdout);
		dup_status = dup2(fileno(output), 1);
		if (dup_status == -1) {
			fprintf(stderr, "Error: could not duplicate file descriptor %d for new stdout: %s\n",
					fileno(output), strerror(errno));
			exit(EXIT_FAILURE);
		}
		stdout = fdopen(1, "w");
		if (stdout == NULL) {
			fprintf(stderr, "Error: could not open file descriptor %d for new stdout: %s\n",
					fileno(output), strerror(errno));
			exit(EXIT_FAILURE);
		}
		gh_stdout = gh_file(stdout);
		symbol_set(&globals, "output-file", gh_stdout);
		symbol_set(&globals, "*STDOUT*", gh_stdout);
	}
	if (error != NULL && error != stderr) {
		datum *gh_stderr;
		int dup_status;

		fclose(stderr);
		dup_status = dup2(fileno(error), 2);
		if (dup_status == -1)
			exit(EXIT_FAILURE);
		stderr = fdopen(2, "w");
		if (stderr == NULL) {
			exit(EXIT_FAILURE);
		}
		gh_stderr = gh_file(stderr);
		symbol_set(&globals, "error-file", gh_stderr);
		symbol_set(&globals, "*STDERR*", gh_stderr);
	}
}

datum *gh_pid(int num) {
	datum *integer;

	integer = gh_integer(num);
	integer->type = TYPE_PID;

	return integer;
}

datum *gh_proc(datum *commands, datum **locals, FILE *input, FILE *output, FILE *error) {
	pid_t pid;
	datum *result;

	pid = fork();
	if (pid == 0) {
		set_interactive(FALSE);
		set_proc_IO(input, output, error);
		result = gh_begin(commands, locals);
		if (result->type == TYPE_EXCEPTION) {
			exit(EXIT_FAILURE);
		} else {
			exit(EXIT_SUCCESS);
		}
	} else {
		gh_assert(pid != -1, "runtime-error", "could not create child process: ~a", gh_cons(gh_error(), &LANG_NIL_VALUE));
		return gh_pid(pid);
	}
}


datum *lang_subproc_nowait(datum **locals) {
	datum *commands;
	datum *input_file;
	datum *output_file;
	datum *error_file;

	commands = var_get(locals, "#commands");
	gh_assert(commands->type == TYPE_CONS, "type-error", "argument 1 is not a list: ~s", gh_cons(commands, &LANG_NIL_VALUE));
	input_file = var_get(locals, "input-file");
	gh_assert(input_file->type == TYPE_FILE, "type-error", "argument 2 is not a file: ~s", gh_cons(input_file, &LANG_NIL_VALUE));
	output_file = var_get(locals, "output-file");
	gh_assert(output_file->type == TYPE_FILE, "type-error", "argument 3 is not a file: ~s", gh_cons(output_file, &LANG_NIL_VALUE));
	error_file = var_get(locals, "error-file");
	gh_assert(error_file->type == TYPE_FILE, "type-error", "argument 4 is not a file: ~s", gh_cons(error_file, &LANG_NIL_VALUE));

	return gh_proc(commands, locals, input_file->value.file, output_file->value.file, error_file->value.file);
}

datum *lang_subproc(datum **locals) {
	datum *pid;
	int status;
	
	pid = lang_subproc_nowait(locals);
	waitpid(pid->value.integer, &status, 0);
	return gh_return_code(status);
}

datum *gh_run(datum *command, datum *args, datum **locals, FILE *input, FILE *output, FILE *error) {
	pid_t pid;

	pid = fork();
	if (pid == 0) {
		char **argv;

		set_interactive(FALSE);
		set_proc_IO(input, output, error);
		argv = build_argv(command, args);
		execve(command->value.executable.path, argv, environ);
		fprintf(stderr, "exec failure: %s", strerror(errno));
		exit(EXIT_FAILURE);
	} else {
		gh_assert(pid != -1, "runtime-error", "could not create child process: ~s", gh_cons(gh_error(), &LANG_NIL_VALUE));
		return gh_pid(pid);
	}
}

datum *run_exec_nowait(datum *ex, datum *args, datum **locals) {
	datum *input_file;
	datum *output_file;
	datum *error_file;

	input_file = var_get(locals, "input-file");
	output_file = var_get(locals, "output-file");
	error_file = var_get(locals, "error-file");

	return gh_run(ex, args, locals, input_file->value.file, output_file->value.file, error_file->value.file);
}

datum *run_exec(datum *command, datum *args, datum **locals) {
	datum *job;
	datum *input_file;
	datum *output_file;
	datum *error_file;
	datum *commands;

	input_file = var_get(locals, "input-file");
	output_file = var_get(locals, "output-file");
	error_file = var_get(locals, "error-file");

	commands = gh_cons(gh_cons(command, args), &LANG_NIL_VALUE);

	job = job_start(commands, input_file, output_file, error_file);
	return job_wait(job);
}

datum *gh_error() {
	return gh_string(strerror(errno));
}

datum *lang_cd(datum **locals) {
	datum *dir;
	int result;

	dir = var_get(locals, "#dir");
	if (dir->type == TYPE_NIL) {
		dir = gh_string(getenv("HOME"));
	} else {
		dir = dir->value.cons.car;
	}
	gh_assert(dir->type == TYPE_STRING || dir->type == TYPE_SYMBOL, "type-error", "not a symbol nor a string: ~s", gh_cons(dir, &LANG_NIL_VALUE));

	result = chdir(dir->value.string);
	gh_assert(result == 0, "runtime-error", "could not change to directory \"~a\": ~a", gh_cons(dir, gh_cons(gh_error(), &LANG_NIL_VALUE)));
	return dir;
}


datum *pipe_eval(datum *expr, datum **locals) {
	datum *command;
	datum *args;

	gh_assert(expr->type == TYPE_CONS, "type-error", "atom in pipeline: ~s", gh_cons(expr, &LANG_NIL_VALUE));

	command = expr->value.cons.car;
	args = expr->value.cons.cdr;

	if (command->type == TYPE_SYMBOL) {
		command = gh_eval(command, locals);
	}
	if (command == var_get(locals, "subproc")) {
		return apply(subproc_nowait, args, locals);
	} else if (command == var_get(locals, "err-to")) {
		return apply(pipe_err_to, args, locals);
	} else if (command == var_get(locals, "err-to+")) {
		return apply(pipe_err_append, args, locals);
	}
	gh_assert(command != var_get(locals, "to"), "i/o-error", "cannot re-direct output within a pipeline: ~s", gh_cons(command, &LANG_NIL_VALUE));
	gh_assert(command != var_get(locals, "to+"), "i/o-error", "cannot re-direct output within a pipeline: ~s", gh_cons(command, &LANG_NIL_VALUE));
	gh_assert(command != var_get(locals, "from"), "i/o-error", "cannot re-direct input within a pipeline: ~s", gh_cons(command, &LANG_NIL_VALUE));

	if (command->type == TYPE_EXECUTABLE) {
		return run_exec_nowait(command, eval_arglist(args, locals), locals);
	} else {
		return apply(command, args, locals);
	}
}

bool gh_stream_to(FILE *input, FILE *output) {
	char buff[sizeof(char) * BUFF_SIZE];
	char *gets_status;
	int puts_status;

	do {
		gets_status = fgets(buff, BUFF_SIZE, input);
		if (gets_status != NULL) {
			puts_status = fputs(buff, output);
			if (puts_status == EOF) {
				fclose(input);
				fclose(output);
				return FALSE;
			}
		}
	} while (gets_status != NULL);

	if (errno != 0) {
		fclose(input);
		fclose(output);
		return FALSE;
	}

	fclose(input);
	fclose(output);
	return TRUE;
}

datum *gh_pipe() {
	int pipe_status;
	int pipe_fds[2];
	FILE *pipe_input;
	FILE *pipe_output;
	datum *gh_pipe_input;
	datum *gh_pipe_output;

	pipe_status = pipe(pipe_fds);
	gh_assert(pipe_status != -1, "i/o-error", "could not open pipe: ~s", gh_cons(gh_error(), &LANG_NIL_VALUE));
	pipe_input = fdopen(pipe_fds[1], "w");
	pipe_output = fdopen(pipe_fds[0], "r");
	gh_pipe_input = gh_file(pipe_input);
	gh_pipe_output = gh_file(pipe_output);
	return gh_cons(gh_pipe_output, gh_pipe_input);
}

datum *job_start(datum *commands, datum *input_file, datum *output_file, datum *error_file) {
	datum *job;
	datum *last_output;
	datum *iterator;

	job = new_datum();
	job->type = TYPE_JOB;
	job->value.job.commands = commands;
	job->value.job.values = &LANG_NIL_VALUE;

	last_output = input_file;

	iterator = commands;
	while (iterator->type == TYPE_CONS) {
		datum *command;
		datum *command_output;
		datum *command_locals;
		datum *command_input;

		command = iterator->value.cons.car;

		command_input = last_output;
		command_locals = gh_cons(gh_cons(gh_symbol("input-file"), command_input), &LANG_NIL_VALUE);

		if (iterator->value.cons.cdr->type != TYPE_CONS) {
			command_output = output_file;
		} else {
			datum *pipe_files;

			pipe_files = gh_pipe();
			command_output = pipe_files->value.cons.cdr;
			last_output = pipe_files->value.cons.car;
		}
		command_locals = gh_cons(gh_cons(gh_symbol("output-file"), command_output), command_locals);
		job->value.job.values = gh_cons(pipe_eval(command, &command_locals), job->value.job.values);
		if (command_input != input_file) {
			fclose(command_input->value.file);
		}
		if (command_output != output_file) {
			fclose(command_output->value.file);
		}

		iterator = iterator->value.cons.cdr;
	}

	return job;
}

void job_remove(datum *job) {
	datum *new_jobs;
	datum *iterator;

	new_jobs = &LANG_NIL_VALUE;
	iterator = jobs;
	while (iterator->type == TYPE_CONS) {
		datum *current;

		current = iterator->value.cons.car;
		if (current != job) {
			new_jobs = gh_cons(current, new_jobs);
		}

		iterator = iterator->value.cons.cdr;
	}

	jobs = reverse(new_jobs);
}

datum *job_wait(datum *job) {
	datum *last_value;
	datum *iterator;

	current_job = job;

	last_value = &LANG_NIL_VALUE;
	iterator = job->value.job.values;
	while (iterator->type == TYPE_CONS) {
		datum *current;
		int wait_status;
		int proc_status;

		proc_status = 0;

		current = iterator->value.cons.car;
		if (current->type == TYPE_PID) {
			wait_status = waitpid((pid_t)current->value.integer, &proc_status, WUNTRACED);
			if (wait_status == -1) {
				current_job = NULL;
				job_remove(job);
				return &LANG_NIL_VALUE;
			} else if (WIFSTOPPED(proc_status)) {
				job->value.job.values = iterator;
				current_job = NULL;
				return job;
			}

			last_value = gh_return_code(proc_status);
		} else {
			last_value = current;
		}

		iterator = iterator->value.cons.cdr;
	}
	current_job = NULL;

	job_remove(job);

	return last_value;
}

datum *job_signal(datum *job, int sig) {
	datum *iterator;

	iterator = job->value.job.values;

	while (iterator->type == TYPE_CONS) {
		datum *current;

		current = iterator->value.cons.car;
		if (current->type == TYPE_PID) {
			int kill_status;

			kill_status = kill((pid_t)current->value.integer, sig);
			gh_assert(kill_status != -1, "runtime-error", "error delivering signal to job ~s: ~a", gh_cons(job, gh_cons(gh_error(), &LANG_NIL_VALUE)));
			
		}

		iterator = iterator->value.cons.cdr;
	}
	return &LANG_TRUE_VALUE;
}

datum *lang_pipe(datum **locals) {
	datum *commands;
	datum *input_file;
	datum *output_file;
	datum *error_file;
	datum *job;

	commands = var_get(locals, "#commands");
	input_file = var_get(locals, "input-file");
	output_file = var_get(locals, "output-file");
	error_file = var_get(locals, "error-file");

	job = job_start(commands, input_file, output_file, error_file);
	return job_wait(job);
}

datum *gh_capture(datum *result, datum *status) {
	datum *cap;

	cap = gh_cons(result, status);
	cap->type = TYPE_CAPTURE;
	return cap;
}

datum *lang_capture(datum **locals) {
	datum *commands;
	datum *pipe_files;
	datum *pipe_in;
	datum *pipe_out;
	datum *command_locals;
	datum *status;
	datum *result;
	char *line;

	commands = var_get(locals, "#commands");

	pipe_files = gh_pipe();
	pipe_in = pipe_files->value.cons.cdr;
	pipe_out = pipe_files->value.cons.car;

	command_locals = gh_cons(gh_cons(gh_symbol("output-file"), pipe_in), *locals);
	
	status = gh_begin(commands, &command_locals);
	fclose(pipe_in->value.file);
	result = &LANG_NIL_VALUE;
	line = NULL;
	do {
		line = gh_readline(pipe_out->value.file);
		if (line != NULL) {
			result = gh_cons(gh_string(line), result);
		}
	} while (line != NULL);
	if (status->type == TYPE_RETURNCODE) {
		status->type = TYPE_INTEGER;
	}
	return gh_capture(reverse(result), status);
}

datum *gh_redirect(datum *file_symbol, datum *path, char *file_mode,  bool in_pipe, datum *commands, datum **locals) {
	FILE *file;
	datum *file_datum;
	datum *new_locals;

	path = gh_eval(path, locals);

	gh_assert(path->type == TYPE_STRING || path->type == TYPE_SYMBOL, "type-error", "neither a symbol, nor a string: ~a", gh_cons(path, &LANG_NIL_VALUE));
	file = fopen(path->value.string, file_mode);
	gh_assert(file != NULL, "i/o-error", "could not open file \"~a\"", gh_cons(path, gh_cons(gh_error(), &LANG_NIL_VALUE)));

	file_datum = gh_file(file);

	new_locals = gh_cons(gh_cons(file_symbol, file_datum), *locals);

	if (in_pipe) {
		datum *iterator;
		datum *result;

		iterator = commands;
		while (iterator->type == TYPE_CONS) {
			datum *command;

			command = iterator->value.cons.car;
			result = pipe_eval(command, &new_locals);
			iterator = iterator->value.cons.cdr;
		}
		return result;
	} else {
		return gh_begin(commands, &new_locals);
	}
}

datum *lang_pipe_err_to(datum **locals) {
	datum *path;
	datum *commands;

	path = var_get(locals, "#path");
	commands = var_get(locals, "#commands");

	return gh_redirect(gh_symbol("error-file"), path, "w", TRUE, commands, locals);
}

datum *lang_pipe_err_append(datum **locals) {
	datum *path;
	datum *commands;

	path = var_get(locals, "#path");
	commands = var_get(locals, "#commands");

	return gh_redirect(gh_symbol("error-file"), path, "a", TRUE, commands, locals);
}

datum *lang_err_to(datum **locals) {
	datum *path;
	datum *commands;

	path = var_get(locals, "#path");
	commands = var_get(locals, "#commands");

	return gh_redirect(gh_symbol("error-file"), path, "w", FALSE, commands, locals);
}

datum *lang_err_append(datum **locals) {
	datum *path;
	datum *commands;

	path = var_get(locals, "#path");
	commands = var_get(locals, "#commands");

	return gh_redirect(gh_symbol("error-file"), path, "a", FALSE, commands, locals);
}

datum *lang_to(datum **locals) {
	datum *path;
	datum *commands;

	path = var_get(locals, "#path");
	commands = var_get(locals, "#commands");

	return gh_redirect(gh_symbol("output-file"), path, "a", FALSE, commands, locals);

}

datum *lang_to_append(datum **locals) {
	datum *path;
	datum *commands;

	path = var_get(locals, "#path");
	commands = var_get(locals, "#commands");

	return gh_redirect(gh_symbol("output-file"), path, "a", FALSE, commands, locals);
	
}

datum *lang_from(datum **locals) {
	datum *path;
	datum *commands;

	path = var_get(locals, "#path");
	commands = var_get(locals, "#commands");

	return gh_redirect(gh_symbol("input-file"), path, "r", FALSE, commands, locals);
}

datum *lang_import(datum **locals) {
	datum *table;

	table = var_get(locals, "#table");

	globals = append(table, globals);

	return &LANG_TRUE_VALUE;
}

datum *lang_disown(datum **locals) {
	datum *command;
	datum *input_file;
	datum *output_file;
	datum *error_file;

	command = var_get(locals, "#command");

	if (command->type == TYPE_CONS) {
		datum *first;

		first = command->value.cons.car;
		if ((first->type == TYPE_SYMBOL && strcmp(first->value.string, "|") == 0) ||
			first == var_get(locals, "|")) {
			command = command->value.cons.cdr;
		} else {
			command = gh_cons(command, &LANG_NIL_VALUE);
		}
		
	}

	input_file = var_get(locals, "input-file");
	output_file = var_get(locals, "output-file");
	error_file = var_get(locals, "error-file");

	printf("command: ");

	return job_start(command, input_file, output_file, error_file);
}

datum *lang_jobs(datum **locals) {
	return jobs;
}

datum *lang_bg(datum **locals) {
	datum *job;

	job = var_get(locals, "#job");
	if (job->type == TYPE_NIL) {
		job = jobs->value.cons.car;
	} else {
		job = job->value.cons.car;
	}

	gh_assert(job->type == TYPE_JOB, "type-error", "not a job: ~s", gh_cons(job, &LANG_NIL_VALUE));

	job_signal(job, SIGCONT);
	job_remove(job);
	return &LANG_TRUE_VALUE;
}

datum *lang_fg(datum **locals) {
	datum *job;

	job = var_get(locals, "#job");
	if (job->type == TYPE_NIL) {
		job = jobs->value.cons.car;
	} else {
		job = job->value.cons.car;
	}

	gh_assert(job->type == TYPE_JOB, "type-error", "not a job: ~s", gh_cons(job, &LANG_NIL_VALUE));

	job_signal(job, SIGCONT);
	return job_wait(job);
}

void set_interactive(bool value) {
	int sigaction_status;

	interactive = value;

	if (value == TRUE) {
		sigaction_status = sigaction(SIGTSTP, &sigstop_action, NULL);
		if (sigaction_status == -1) {
			fprintf(stderr, "error setting signal handler for SIGSTP: %s", strerror(errno));
			exit(EXIT_FAILURE);
		}
		sigaction_status = sigaction(SIGINT, &siginterrupt_action, NULL);
		if (sigaction_status == -1) {
			fprintf(stderr, "error setting signal handler for SIGSTP: %s", strerror(errno));
			exit(EXIT_FAILURE);
		}
	} else {
		sigaction_status = sigaction(SIGTSTP, &default_action, NULL);
		if (sigaction_status == -1) {
			fprintf(stderr, "Error restoring default SIGTSTP signal handler\n");
			exit(EXIT_FAILURE);
		}
		sigaction_status = sigaction(SIGINT, &default_action, NULL);
		if (sigaction_status == -1) {
			fprintf(stderr, "Error restoring default SIGINT signal handler\n");
			exit(EXIT_FAILURE);
		}
	}
}

datum *gh_format(FILE *output, char *str, datum *args) {
	int i;
	datum *iterator;

	iterator = args;

	for (i = 0; str[i] != '\0'; i++) {
		if (str[i] == '~') {
			gh_assert(iterator->type == TYPE_CONS, "runtime-error", "not enough args to format string: ~s", gh_cons(gh_string(str), &LANG_NIL_VALUE));
			switch(str[i + 1]) {
				case '~':
					fprintf(output, "~~");
					break;

				case '%':
					fprintf(output, "\n");
					break;

				case 'S':
				case 's':
					print_datum(output, iterator->value.cons.car);
					i++;
					break;

				case 'A':
				case 'a':
					if (iterator->value.cons.car->type == TYPE_STRING) {
						fprintf(output, "%s", iterator->value.cons.car->value.string);
					} else {
						print_datum(output, iterator->value.cons.car);
					}
					break;
			}
			iterator = iterator->value.cons.cdr;
		} else {
			fputc(str[i], output);
		}
	}
	gh_assert(iterator->type == TYPE_NIL, "runtime-error", "too many args to format string: ~s", gh_cons(gh_string(str), &LANG_NIL_VALUE));
	
	fprintf(output, "\n");
	return &LANG_NIL_VALUE;
}

datum *lang_format(datum **locals) {
	datum *output;
	datum *str;
	datum *args;

	output = var_get(locals, "#output");
	if (output->type == TYPE_TRUE) {
		output = var_get(locals, "output-file");
	}

	gh_assert(output->type == TYPE_FILE, "type-error", "not a file nor t: ~s", gh_cons(output, &LANG_NIL_VALUE));

	str = var_get(locals, "#str");
	gh_assert(str->type == TYPE_STRING, "type-error", "not a string: ~s", gh_cons(str, &LANG_NIL_VALUE));

	args = var_get(locals, "#args");

	return gh_format(output->value.file, str->value.string, args);
}

void gh_stacktrace(FILE *file, datum **locals) {
	datum *iterator;

	iterator = reverse(*locals);

	while (iterator->type == TYPE_CONS) {
		datum *current;
		datum *symbol;


		current = iterator->value.cons.car;
		symbol = current->value.cons.car;
		if (strcmp(symbol->value.string, "#stack-frame") == 0) {
			datum *fname;
			datum *ffile;
			datum *flineno;

			fname = current->value.cons.cdr->value.cons.car;
			ffile = current->value.cons.cdr->value.cons.cdr->value.cons.car;
			flineno = current->value.cons.cdr->value.cons.cdr->value.cons.cdr->value.cons.car;
			fprintf(file, "In function \"%s\" in \"%s\" line %d\n", fname->value.string, ffile->value.string, flineno->value.integer);
		}
		iterator = iterator->value.cons.cdr;
	}
}

datum *typecheck(datum *args, datum *types, char *mismatch_fmt, datum **locals) {
	datum *arg_iterator;

	arg_iterator = args->value.cons.car;
	while (arg_iterator->type == TYPE_CONS) {
		datum *current_arg;
		datum *type_iterator;

		current_arg = arg_iterator->value.cons.car;

		type_iterator = types;
		while (type_iterator->type == TYPE_CONS) {
			datum *current_type;

			current_type = type_iterator->value.cons.car;
			if (current_type->value.integer == current_arg->type) {
				goto pass_arg;
			}	
			type_iterator = type_iterator->value.cons.cdr;
		}
		gh_assert(TRUE, "type-error", mismatch_fmt, gh_cons(current_arg, &LANG_NIL_VALUE));
		pass_arg:

		arg_iterator = arg_iterator->value.cons.cdr;
	}
	return &LANG_NIL_VALUE;
}

datum *lang_read_password(datum **locals) {
	datum *prompt;
	datum *file;
	datum *result;
	bool old_interactive;
	struct termios terminfo;
	int old_flags;
	int fd;
	int status;

	old_interactive = interactive;
	interactive = FALSE;

	prompt = var_get(locals, "#prompt");
	if (prompt->type == TYPE_NIL) {
		prompt = gh_string("");
	} else {
		prompt = prompt->value.cons.car;
	}

	gh_assert(prompt->type == TYPE_STRING, "type-error", "not a string: ~s", gh_cons(prompt, &LANG_NIL_VALUE));

	file = var_get(locals, "input-file");
	gh_assert(file->type == TYPE_FILE, "type-error", "input-file is not a file: ~s", gh_cons(file, &LANG_NIL_VALUE));

	fd = fileno(file->value.file);
	if (isatty(fd)) {
		status = tcgetattr(fd, &terminfo);
		gh_assert(status != -1, "i/o-error", "~a", gh_cons(gh_error(), &LANG_NIL_VALUE));
		old_flags = terminfo.c_lflag;
		terminfo.c_lflag |= ECHO;
		status = tcsetattr(fd, TCSANOW, &terminfo);
		gh_assert(status != -1, "i/o-error", "~a", gh_cons(gh_error(), &LANG_NIL_VALUE));
	} 

	result = gh_string(gh_readline(file->value.file));

	if (isatty(fd)) {
		terminfo.c_lflag = old_flags;
		status = tcsetattr(fd, TCSANOW, &terminfo);
		gh_assert(status != -1, "i/o-error", "~a", gh_cons(gh_error(), &LANG_NIL_VALUE));
	}

	interactive = old_interactive;

	if (result == NULL) {
		return &LANG_EOF_VALUE;
	} else {
		return result;
	}
}

datum *lang_string_append(datum **locals) {
	datum *args;
	datum *iterator;
	char *result;

	args = var_get(locals, "#args");
	result = "";

	iterator = args;
	while (iterator->type == TYPE_CONS) {
		datum *current;

		current = iterator->value.cons.car;
		gh_assert(current->type == TYPE_STRING, "type-error", "not a string: ~s", gh_cons(current, &LANG_NIL_VALUE));
		result = string_append(result, current->value.string);

		iterator = iterator->value.cons.cdr;
	}

	return gh_string(result);
}
