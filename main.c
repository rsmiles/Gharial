#define _POSIX_C_SOURCE 199309L

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <libgen.h>
#include <setjmp.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <histedit.h>
#include <gc.h>

#include "gharial.h"
#include "y.tab.h"
#include "lex.yy.h"

#define INIT_FILE "init.ghar"

void init_globals();
void init_io();
void init_builtins();
void init_signals();
unsigned char insert_parens(EditLine *el, int ch);
unsigned char insert_newline(EditLine *el, int ch);
unsigned char next_closeparen(EditLine *el, int ch);
unsigned char insert_doublequotes(EditLine *el, int ch);
static unsigned char
complete(EditLine *el, int ch __attribute__((__unused__)));

void init_editline();
char *el_prompt(EditLine *el);
void cleanup();
void sig_stop(int signum);
void sig_interrupt(int signum);

char *hist_file;
char *PROGNAME;

bool eval_flag = TRUE;
bool print_flag = TRUE;
bool capture_flag = FALSE;
bool prompt_flag = TRUE;
bool interactive = FALSE;
struct sigaction default_action;
struct sigaction sigstop_action;
struct sigaction siginterrupt_action;
datum *subproc_nowait;
datum *pipe_err_to;
datum *pipe_err_append;
jmp_buf toplevel;

char *current_file = "<REPL>";

datum *gh_input = &LANG_NIL_VALUE;
datum *gh_result = &LANG_NIL_VALUE;
datum *globals = &LANG_NIL_VALUE;
datum *empty_locals = &LANG_NIL_VALUE;
datum **locals = &empty_locals;
datum *jobs;
datum *current_job;
EditLine *gh_editline;
History *gh_history;
HistEvent gh_last_histevent;

void sig_stop(int signum) {
	if (current_job != NULL) {
		job_signal(current_job, SIGTSTP);
		jobs = gh_cons(current_job, jobs);
		current_job = NULL;
	}
}

void sig_interrupt(int signum) {
	if (current_job != NULL) {
		job_signal(current_job, SIGINT);
		current_job = NULL;
	}
	
}

void init_globals(char **argv){
	PROGNAME = (char *)GC_MALLOC(sizeof(char) * (strlen(argv[0] + 1)));
	strcpy(PROGNAME, argv[0]);
	PROGNAME = basename(PROGNAME);
	

	hist_file = string_append(getenv("HOME"), "/");
	hist_file = string_append(hist_file, ".");
	hist_file = string_append(hist_file, PROGNAME);
	hist_file = string_append(hist_file, "_history");
}

void init_io() {
	symbol_set(&globals, "*STDIN*", gh_file(stdin));
	symbol_set(&globals, "*STDOUT*", gh_file(stdout));
	symbol_set(&globals, "*STDERR*", gh_file(stderr));
	symbol_set(&globals, "input-file", symbol_get(globals, "*STDIN*"));
	symbol_set(&globals, "output-file", symbol_get(globals, "*STDOUT*"));
	symbol_set(&globals, "error-file", symbol_get(globals, "*STDERR*"));
}

void init_builtins() {
	symbol_set(&globals, "set", gh_cform(&lang_set, gh_cons(gh_symbol("#symbol"), gh_cons(gh_symbol("#value"), &LANG_NIL_VALUE))));
	symbol_set(&globals, "setenv", gh_cform(&lang_setenv, gh_cons(gh_symbol("#symbol"), gh_cons(gh_symbol("#value"), &LANG_NIL_VALUE))));
	symbol_set(&globals, "quote", gh_cform(&lang_quote, gh_cons(gh_symbol("#expr"), &LANG_NIL_VALUE)));
	symbol_set(&globals, "unquote", gh_cform(&lang_unquote, gh_cons(gh_symbol("#expr"), &LANG_NIL_VALUE)));
	symbol_set(&globals, "unquote-splice", gh_cform(&lang_unquote_splice, gh_symbol("#lst")));
	symbol_set(&globals, "cons", gh_cfunc(&lang_cons, gh_cons(gh_symbol("#car"), gh_cons(gh_symbol("#cdr"), &LANG_NIL_VALUE))));
	symbol_set(&globals, "car", gh_cfunc(&lang_car, gh_cons(gh_symbol("#pair"), &LANG_NIL_VALUE)));
	symbol_set(&globals, "cdr", gh_cfunc(&lang_cdr, gh_cons(gh_symbol("#pair"), &LANG_NIL_VALUE)));
	symbol_set(&globals, "reverse", gh_cfunc(&lang_reverse, gh_cons(gh_symbol("#lst"), &LANG_NIL_VALUE)));
	symbol_set(&globals, "list", gh_cfunc(&lang_list, gh_symbol("#args")));
	symbol_set(&globals, "append", gh_cfunc(&lang_append, gh_cons(gh_symbol("#lst1"), gh_cons(gh_symbol("#lst2"), &LANG_NIL_VALUE))));
	symbol_set(&globals, "is", gh_cfunc(&lang_is, gh_cons(gh_symbol("#a"), gh_cons(gh_symbol("#b"), &LANG_NIL_VALUE))));
	symbol_set(&globals, "=", gh_cfunc(&lang_equal, gh_cons(gh_symbol("#a"), gh_cons(gh_symbol("#b"), &LANG_NIL_VALUE))));
	symbol_set(&globals, "+", gh_cfunc(&lang_add, gh_symbol("#args")));
	symbol_set(&globals, "-", gh_cfunc(&lang_sub, gh_cons(gh_symbol("#first"), gh_symbol("#rest"))));
	symbol_set(&globals, "*", gh_cfunc(&lang_mul, gh_symbol("#args")));
	symbol_set(&globals, "/", gh_cfunc(&lang_div, gh_cons(gh_symbol("#first"), gh_symbol("#rest"))));
	symbol_set(&globals, "^", gh_cfunc(&lang_pow, gh_cons(gh_symbol("#a"), gh_cons(gh_symbol("#b"), &LANG_NIL_VALUE))));
	symbol_set(&globals, "lambda", gh_cform(&lang_lambda, gh_cons(gh_symbol("#lambda-list"), gh_symbol("#body"))));
	symbol_set(&globals, "macro", gh_cform(&lang_macro, gh_cons(gh_symbol("#lambda-list"), gh_symbol("#body"))));
	symbol_set(&globals, "cond", gh_cform(&lang_cond, gh_symbol("#conditions")));
	symbol_set(&globals, "loop", gh_cform(&lang_loop, gh_cons(gh_symbol("#bindings"), gh_symbol("#body"))));
	symbol_set(&globals, "recur", gh_cform(&lang_recur, gh_symbol("#bindings")));
	symbol_set(&globals, "let", gh_cform(&lang_let, gh_cons(gh_symbol("#bindings"), gh_symbol("#body"))));
	symbol_set(&globals, "open", gh_cfunc(&lang_open, gh_cons(gh_symbol("#fname"), gh_symbol("#mode"))));
	symbol_set(&globals, "close", gh_cfunc(&lang_close, gh_cons(gh_symbol("#file"), &LANG_NIL_VALUE)));
	symbol_set(&globals, "read", gh_cfunc(&lang_read, gh_symbol("#file")));
	symbol_set(&globals, "write", gh_cfunc(&lang_write, gh_cons(gh_symbol("#expr"), gh_symbol("#file"))));
	symbol_set(&globals, "load", gh_cfunc(&lang_load, gh_cons(gh_symbol("#path"), &LANG_NIL_VALUE)));
	symbol_set(&globals, "apply", gh_cfunc(&lang_apply, gh_cons(gh_symbol("#fn"), gh_cons(gh_symbol("#args"), &LANG_NIL_VALUE))));
	symbol_set(&globals, "macroexpand", gh_cform(&lang_macroexpand, gh_cons(gh_symbol("#expr"), &LANG_NIL_VALUE)));
	symbol_set(&globals, "gensym", gh_cform(&lang_gensym, gh_symbol("#name")));
	symbol_set(&globals, "try", gh_cform(&lang_try, gh_cons(gh_symbol("#action"), gh_symbol("#except"))));
	symbol_set(&globals, "exception", gh_cform(&lang_exception, gh_cons(gh_symbol("#type"), gh_cons(gh_symbol("#fmt"), gh_symbol("#fmt_args")))));
	symbol_set(&globals, "eval", gh_cfunc(&lang_eval, gh_cons(gh_symbol("#expr"), &LANG_NIL_VALUE)));
	symbol_set(&globals, "read-line", gh_cfunc(&lang_read_line, gh_symbol("#file")));
	symbol_set(&globals, "exit", gh_cfunc(&lang_exit, gh_symbol("#status")));
	symbol_set(&globals, "string-split", gh_cfunc(&lang_string_split, gh_cons(gh_symbol("#str"), gh_symbol("#delim"))));
	symbol_set(&globals, "return-code", gh_cfunc(&lang_return_code, gh_cons(gh_symbol("#num"), &LANG_NIL_VALUE)));
	symbol_set(&globals, "length", gh_cfunc(&lang_length, gh_cons(gh_symbol("#lst"), &LANG_NIL_VALUE)));
	symbol_set(&globals, "subproc", gh_cform(&lang_subproc, gh_symbol("#commands")));
	symbol_set(&globals, "--cd", gh_cfunc(&lang_cd, gh_symbol("#dir")));
	symbol_set(&globals, "|", gh_cform(&lang_pipe, gh_symbol("#commands")));
	symbol_set(&globals, "$", gh_cform(&lang_capture, gh_symbol("#commands")));
	symbol_set(&globals, "&", gh_cform(&lang_disown, gh_cons(gh_symbol("#command"), &LANG_NIL_VALUE)));
	symbol_set(&globals, "err-to", gh_cform(&lang_err_to, gh_cons(gh_symbol("#path"), gh_symbol("#commands"))));
	symbol_set(&globals, "err-to+", gh_cform(&lang_err_to, gh_cons(gh_symbol("#path"), gh_symbol("#commands"))));
	symbol_set(&globals, "to", gh_cform(&lang_to, gh_cons(gh_symbol("#path"), gh_symbol("#commands"))));
	symbol_set(&globals, "to+", gh_cform(&lang_to_append, gh_cons(gh_symbol("#path"), gh_symbol("#commands"))));
	symbol_set(&globals, "from", gh_cform(&lang_from, gh_cons(gh_symbol("#path"), gh_symbol("#commands"))));
	symbol_set(&globals, "import", gh_cfunc(&lang_import, gh_cons(gh_symbol("#table"), &LANG_NIL_VALUE)));
	symbol_set(&globals, "jobs", gh_cfunc(&lang_jobs, &LANG_NIL_VALUE));
	symbol_set(&globals, "bg", gh_cfunc(&lang_bg, gh_symbol("#job")));
	symbol_set(&globals, "fg", gh_cfunc(&lang_fg, gh_symbol("#job")));
	symbol_set(&globals, "format", gh_cfunc(&lang_format, gh_cons(gh_symbol("#output"), gh_cons(gh_symbol("#str"), gh_symbol("#args")))));
	symbol_set(&globals, "read-password", gh_cfunc(&lang_read_password, gh_symbol("#prompt")));
	symbol_set(&globals, "string-append", gh_cfunc(&lang_string_append, gh_symbol("#args")));
	symbol_set(&globals, "test-expr", gh_cfunc(&lang_test_expr, gh_cons(gh_symbol("#test-name"), gh_cons(gh_symbol("#expr"), gh_cons(gh_symbol("#fmt"), gh_symbol("#fmt_args"))))));
	symbol_set(&globals, "type", gh_cfunc(&lang_type, gh_cons(gh_symbol("#obj"), &LANG_NIL_VALUE)));
	symbol_set(&globals, "and", gh_cfunc(&lang_and, gh_symbol("#args")));
	symbol_set(&globals, "or", gh_cfunc(&lang_or, gh_symbol("#args")));
	symbol_set(&globals, "xor", gh_cfunc(&lang_xor, gh_symbol("#args")));
	symbol_set(&globals, "not", gh_cfunc(&lang_not, gh_cons(gh_symbol("#expr"), &LANG_NIL_VALUE)));
	symbol_set(&globals, "globals", gh_cfunc(&lang_globals, &LANG_NIL_VALUE));
	symbol_set(&globals, "locals", gh_cfunc(&lang_locals, &LANG_NIL_VALUE));
	symbol_set(&globals, "print", gh_cfunc(&lang_print, gh_cons(gh_symbol("#obj"), gh_symbol("#file"))));

	subproc_nowait = gh_cform(&lang_subproc_nowait, gh_symbol("#commands"));
	pipe_err_to = gh_cform(&lang_pipe_err_to, gh_cons(gh_symbol("#path"), gh_symbol("#commands")));
	pipe_err_append = gh_cform(&lang_pipe_err_append, gh_cons(gh_symbol("#path"), gh_symbol("#commands")));
}

void init_signals() {
	default_action.sa_handler = SIG_DFL;
	sigemptyset(&default_action.sa_mask);
	default_action.sa_flags = 0;

	sigstop_action.sa_handler = &sig_stop;
	sigemptyset(&sigstop_action.sa_mask);
	sigstop_action.sa_flags = 0;

	siginterrupt_action.sa_handler = &sig_interrupt;
	sigemptyset(&siginterrupt_action.sa_mask);
	siginterrupt_action.sa_flags = 0;

	if (isatty(fileno(stdin)) && (yyin == stdin)) {
		set_interactive(TRUE);
	} else {
		set_interactive(FALSE);
	}
}

char *el_prompt(EditLine *el) {
	if (depth == 0 && prompt_flag == TRUE) {
		datum *prompt_fn;
		datum *prompt;
		datum *old_status;

		prompt_fn = symbol_get(globals, "*prompt*");
		old_status = symbol_get(globals, "*?*");
		prompt = apply(prompt_fn, &LANG_NIL_VALUE, &empty_locals);
		symbol_set(&globals, "*?*", old_status);
		return prompt->value.string;
	} else {
		return "";
	}
}

unsigned char insert_parens(EditLine *el, int ch){
	switch (ch){
		case '(':
			el_insertstr(el, "(");
			el_insertstr(el, ")");
			break;
		case '[':
			el_insertstr(el, "[");
			el_insertstr(el, "]");
			break;
		case '{':
			el_insertstr(el, "{");
			el_insertstr(el, "}");
			break;
	}
	el_cursor(el, -1);
	return CC_REFRESH;
}

unsigned char insert_newline(EditLine *el, int ch){
	const struct lineinfo *info = el_line(el);

	el_insertstr(el, "\n");
	if (info->cursor >= info->lastchar) {
		printf("\n");
		return CC_NEWLINE;
	} else {
		return CC_REFRESH;
	}
}

unsigned char next_closeparen(EditLine *el, int ch) {
	const struct lineinfo *info;
	do {
		el_cursor(el, 1);
		info = el_line(el);
	} while (*info->cursor != ')' && *info->cursor != ']' && *info->cursor != '}' && info->cursor != info->lastchar);
	return CC_CURSOR;
}

unsigned char insert_doublequotes(EditLine *el, int ch) {
	el_insertstr(el, "\"\"");
	el_cursor(el, -1);
	return CC_REFRESH;
}

unsigned char clear(EditLine *el, int ch) {
	system("clear");
	return CC_REDISPLAY;
}

/*
 Below function was modified from the "complete" function in example tc1.c found in EditLine examples. Original copyright notice:

 * Copyright (c) 1992, 1993
 *  The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Christos Zoulas of Cornell University.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.


*/

static unsigned char
complete(EditLine *el, int ch __attribute__((__unused__)))
{
	DIR *dd;
	struct dirent *dp;
	const char* ptr;
	const LineInfo *lf = el_line(el);
	size_t len;
	int res = CC_ERROR;
	char *copy1;
	char *copy2;
	char *dir;
	char *file;

	/*
	 * Find the last word
	 */
	for (ptr = lf->cursor - 1;
		*ptr != '\'' && *ptr != '"' && ptr > lf->buffer; ptr--)
		continue;
	len = lf->cursor - ++ptr;

	copy1 = GC_MALLOC(sizeof(char) * (len + 1));
	strncpy(copy1, ptr, len);
	copy1[len] = '\0';

	copy2 = GC_MALLOC(sizeof(char) * (len + 1));
	strncpy(copy2, ptr, len);
	copy2[len] = '\0';

	dir = dirname(copy1);
	dd = opendir(dir);

	file = basename(copy2);

	len = strlen(file);

	for (dp = readdir(dd); dp != NULL; dp = readdir(dd)) {
		if (len > strlen(dp->d_name))
			continue;
		if (strncmp(dp->d_name, file, len) == 0) {
			if (el_insertstr(el, &dp->d_name[len]) == -1)
				res = CC_ERROR;
			else
				res = CC_REFRESH;
			break;
		} else {
		}
	}

	closedir(dd);
	return res;
}

void init_editline(int argc, char **argv) {

	gh_history = history_init();
	history(gh_history, &gh_last_histevent, H_SETSIZE, 100);
	history(gh_history, &gh_last_histevent, H_SETUNIQUE, TRUE);
	history(gh_history, &gh_last_histevent, H_LOAD, hist_file);

	gh_editline = el_init(argv[0], stdin, stdout, stderr);
	el_set(gh_editline, EL_SIGNAL, 1);
	el_set(gh_editline, EL_EDITOR, "emacs");
	el_set(gh_editline, EL_PROMPT, &el_prompt);
	el_set(gh_editline, EL_HIST, history, gh_history);
	el_source(gh_editline, NULL);

	el_set(gh_editline, EL_ADDFN, "insert_parens", "insert matching closing parentheses", &insert_parens);
	el_set(gh_editline, EL_BIND, "(", "insert_parens", NULL);
	el_set(gh_editline, EL_BIND, "[", "insert_parens", NULL);
	el_set(gh_editline, EL_BIND, "{", "insert_parens", NULL);

	el_set(gh_editline, EL_ADDFN, "insert_newline", "insert newline at cursor position", &insert_newline);
	el_set(gh_editline, EL_BIND, "\n", "insert_newline", NULL);

	el_set(gh_editline, EL_ADDFN, "next_closeparen", "move cursor to next closing parenthesis or end of line", &next_closeparen);
	el_set(gh_editline, EL_BIND, "^n", "next_closeparen", NULL);

	el_set(gh_editline, EL_ADDFN, "insert_doublequotes", "insert a pair of double quotation marks", &insert_doublequotes);
	el_set(gh_editline, EL_BIND, "\"", "insert_doublequotes", NULL);

	el_set(gh_editline, EL_ADDFN, "clear", "clear terminal screen", &clear);
	el_set(gh_editline, EL_BIND, "^l", "clear", NULL);

	el_set(gh_editline, EL_ADDFN, "complete", "tab completion", &complete);
	el_set(gh_editline, EL_BIND, "^I", "complete", NULL);
}

void cleanup_editline() {
	history(gh_history, &gh_last_histevent, H_SAVE, hist_file);
	history_end(gh_history);
	el_end(gh_editline);
}

void cleanup(){
	cleanup_editline();
}

int main(int argc, char **argv) {
	FILE *input;

	yylineno = 0;
	init_globals(argv);
	init_io();
	init_builtins();
	init_editline(argc, argv);
	init_signals();
	atexit(&cleanup);

	jobs = &LANG_NIL_VALUE;

	current_job = NULL;

	symbol_set(&globals, "*?*", gh_integer(0));

	if (argc == 1) {
		input = stdin;
	} else if (argc == 2) {
		input = fopen(argv[1], "r");
		if (input == NULL) {
			fprintf(stderr, "%s: could not open file \"%s\": %s", argv[0], argv[1], strerror(errno));
		}
	} else {
		fprintf(stderr, "%s: usage: %s: [file]\n", argv[0], argv[0]);
		exit(EXIT_FAILURE);
	}

	if (input == stdin && isatty(fileno(stdin))) {
		set_interactive(TRUE);
	}

	if (setjmp(toplevel) != TRUE) {
		gh_load(INIT_FILE);
	}
	
	yyin = input;
	yypush_buffer_state(yy_create_buffer(yyin, YY_BUF_SIZE));
	setjmp(toplevel);
	do {
		yyparse();
	} while(gh_result->type != TYPE_EOF);
	yylex_destroy();

	yypop_buffer_state();
	printf("\n");
	return 0;
}

