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
#define TYPE_PID        17
#define TYPE_CAPTURE    18
#define TYPE_JOB        19

#define gh_assert(test, type, fmt, fmt_args) \
	do { \
		if (test) \
			; \
		else return gh_eval(gh_exception(type, fmt, fmt_args), locals); \
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
			char *name;
		} c_code; /* C function or special form */

		struct {
			struct datum *car;
			struct datum *cdr;
		} cons;

		struct {
			struct datum *lambda_list;
			struct datum *body;
			struct datum *closure;
			char *name;
		} func; /* Function or macro */

		struct {
			struct datum *bindings;
		} recur;

		struct {
			char *type;
			char *fmt;
			struct datum *fmt_args;
			int lineno;
		} exception;

		struct {
			char *path;
		} executable;

		struct {
			struct datum *commands;
			struct datum *values;
			struct datum *input_file;
			struct datum *output_file;
			struct datum *error_file;
		} job;
		
	} value;
} datum;

extern bool eval_flag;
extern bool print_flag;
extern bool capture_flag;
extern bool interactive;
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
extern datum *subproc_nowait;
extern datum *pipe_err_to;
extern datum *pipe_err_append;
extern datum *jobs;
extern datum *current_job;
extern struct sigaction default_action;
extern struct sigaction sigstop_action;
extern struct sigaction siginterrupt_action;

datum *symbol_loc_one(datum *table, char *symbol);

datum *symbol_loc(datum *table, char *symbol);

datum *symbol_get(datum *table, char *symbol);

char  *symbol_set(datum **table, char *symbol, datum *value);

void  symbol_unset(datum **table, char *symbol);

datum *gh_integer(int value);

datum *gh_decimal(double value);

datum *gh_string(char *value);

datum *gh_substring(int start, int end, char *value);

datum *gh_symbol(char *value);

datum *gh_file(FILE *fptr);

datum *gh_cons(datum *car, datum *cdr);

void gh_print(FILE *file, datum *expr);

void print_exception(FILE *file, datum *expr, datum **locals);

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

datum *list_copy(datum *lst);

datum *append(datum *lst1, datum *lst2);

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

datum *lang_append(datum **locals);

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

char *string_copy(const char *str);

datum *string_split(const char *str, const char *delim);

datum *lang_string_split(datum **locals);

datum *gh_exception(char *type, char *fmt, datum *fmt_args);

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

datum *lang_subproc_nowait(datum **locals);

datum *lang_subproc(datum **locals);

datum *lang_cd(datum **locals);

datum *job_start(datum *commands, datum *input_file, datum *output_file, datum *error_file);

datum *job_wait(datum *job);

datum *job_signal(datum *job, int sig);

datum *lang_pipe(datum **locals);

bool gh_stream_to(FILE *input, FILE *output);

datum *gh_capture(datum *result, datum *status);

datum *lang_capture(datum **locals);

datum *gh_redirect(datum *file_symbol, datum *path, char *file_mode,  bool in_pipe, datum *commands, datum **locals);

datum *lang_err_to(datum **locals);

datum *lang_err_append(datum **locals);

datum *lang_to(datum **locals);

datum *lang_to_append(datum **locals);

datum *lang_from(datum **locals);

datum *lang_pipe_err_to(datum **locals);

datum *lang_pipe_err_append(datum **locals);

datum *lang_import(datum **locals);

datum *lang_disown(datum **locals);

datum *lang_jobs(datum **locals);

datum *lang_bg(datum **locals);

datum *lang_fg(datum **locals);

void set_interactive(bool value);

datum *gh_format(FILE *output, char *str, datum *args);

datum *lang_format(datum **locals);

#endif

