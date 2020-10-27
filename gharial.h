#ifndef GHARIAL_H
#define GHARIAL_H

#include <stdio.h>
#include <histedit.h>

#define TRUE  1
#define FALSE 0

#define TYPE_NIL        0
#define TYPE_TRUE       1
#define TYPE_INTEGER    2
#define TYPE_DECIMAL    3
#define TYPE_STRING     4
#define TYPE_SYMBOL     5
#define TYPE_CONS       6
#define TYPE_CFORM      7
#define TYPE_CFUNC      8
#define TYPE_FUNC       9
#define TYPE_MACRO      10
#define TYPE_RECUR      11
#define TYPE_FILE       12
#define TYPE_EOF        13
#define TYPE_EXCEPTION  14
#define TYPE_RETURNCODE 15
#define TYPE_EXECUTABLE 16

#define gh_assert(test, type, description, info) \
	do { \
		if (test) \
			; \
		else return gh_eval(gh_exception(type, description, info, yylineno), locals); \
	} while(0);

typedef int bool;

typedef struct datum {
	int type;
	union {
		int integer;
		double decimal;
		char *string;
		FILE *file;

		struct {
			struct datum *(*func)(struct datum **);
			struct datum *lambda_list;
		} c_code; /* C function or special form */

		struct {
			struct datum *car;
			struct datum *cdr;
		} cons;

		struct {
			struct datum *lambda_list;
			struct datum *body;
			struct datum **closure;
		} func; /* Function or macro */

		struct {
			struct datum *bindings;
		} recur;

		struct {
			char *type;
			char *description;
			struct datum *info;
			int lineno;
		} exception;

		struct {
			char *path;
		} executable;
		
	} value;
} datum;

extern int eval_flag;
extern int print_flag;
extern int yylineno;
extern FILE *yyin;
extern datum LANG_NIL_VALUE;
extern datum LANG_TRUE_VALUE;
extern datum LANG_EOF_VALUE;
extern datum *empty_locals;
extern datum **locals;
extern datum *globals;
extern datum *gh_input;
extern datum *gh_result;
extern char *current_file;
extern EditLine *gh_editline;
extern History *gh_history;
extern HistEvent gh_last_histevent;
extern int depth;

datum *symbol_loc(datum *table, char *symbol);

datum *symbol_get(datum *table, char *symbol);

void  symbol_set(datum **table, char *symbol, datum *value);

void  symbol_unset(datum **table, char *symbol);

datum *gh_integer(int value);

datum *gh_decimal(double value);

datum *gh_string(char *value);

datum *gh_substring(int start, int end, char *value);

datum *gh_symbol(char *value);

datum *gh_file(FILE *fptr);

datum *gh_cons(datum *car, datum *cdr);

void gh_print(FILE *file, datum *expr);

void print_exception(FILE *file, datum *expr);

bool is_macro_call(datum *expr, datum **locals);

datum *gh_eval(datum *expr, datum **locals);

datum *lang_eval(datum **locals);

void prompt();

datum *new_datum();

void print_datum(FILE *file, datum *expr);

datum *gh_cform(datum *(*addr)(datum **locals), datum *args);

datum *gh_cfunc(datum *(*addr)(datum **locals), datum *args);

datum* gh_load(char *path);

datum *combine(datum *lst1, datum *lst2);

datum *var_get(datum **locals, char *symbol);

datum *eval_arglist(datum *args, datum **locals);

datum *reverse(datum *lst);

datum *fold(datum *(*func)(datum *a, datum *b), datum *init, datum *list);

datum *map(datum *(*func)(datum *x), datum *lst);

datum *bind_args(datum *lst1, datum *lst2);

datum *gh_car(datum *pair);

datum *gh_cdr(datum *pair);

datum *add2(datum *a, datum *b);

datum *sub2(datum *a, datum *b);

datum *mul2(datum *a, datum *b);

datum *div2(datum *a, datum *b);

datum *dpow(datum *a, datum *b);

datum *do_unquotes(datum *expr, datum **locals);

datum *eval_form(datum* func, datum *args, datum **locals);

datum *lang_set(datum **locals);

datum *lang_setenv(datum **locals);

datum *lang_quote(datum **locals);

datum *lang_unquote(datum **locals);

datum *lang_unquote_splice(datum **locals);

datum *lang_cons(datum **locals);

datum *lang_car(datum **locals);

datum *lang_cdr(datum **locals);

datum *lang_reverse(datum **locals);

datum *lang_list(datum **locals);

datum *lang_equal(datum **locals);

datum *lang_add(datum **locals);

datum *lang_sub(datum **locals);

datum *lang_mul(datum **locals);

datum *lang_div(datum **locals);

datum *lang_pow(datum **locals);

datum *lang_lambda(datum **locals);

datum *lang_macro(datum **locals);

datum *lang_cond(datum **locals);

datum *lang_loop(datum **locals);

datum *lang_recur(datum **locals);

datum *lang_let(datum **locals);

datum *lang_is_eof_object(datum **locals);

datum *lang_open(datum **locals);

datum *lang_close(datum **locals);

datum *gh_read(FILE *file);

datum *lang_read(datum **locals);

datum *lang_write(datum **locals);

datum *lang_load(datum **locals);

char *typestring(datum *obj);

char *string_append(char *str1, char *str2);

datum *string_split(const char *str, const char *delim);

datum *lang_string_split(datum **locals);

datum *gh_exception(char *type, char *description, datum *info, int lineno);

datum *gh_begin(datum *body, datum **locals);

datum *apply(datum *fn, datum *args, datum **locals);

datum *apply_macro(datum *macro, datum *args, datum **locals);

datum *lang_apply(datum **locals);

datum *macroexpand(datum *expr, datum **locals);

datum *lang_macroexpand_1(datum **locals);

datum *lang_macroexpand(datum **locals);

unsigned int num_digits(unsigned int num);

datum *gensym();

datum *lang_gensym(datum **locals);

datum *lang_try(datum **locals);

datum *lang_exception(datum **locals);

char *gh_readline(FILE *file);

datum *lang_read_line(datum **locals);

int gh_get_input(char *buf, int max_size);

datum *lang_exit(datum **locals);

datum *gh_return_code(int num);

datum *lang_return_code(datum **locals);

int gh_length(datum *lst);

datum *lang_length(datum **locals);

datum *gh_subproc(datum *commands);

datum *lang_subproc(datum **locals);

datum *lang_cd(datum **locals);

#endif

