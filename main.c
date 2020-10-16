#include "gharial.h"
#include "y.tab.h"
#include "lex.yy.h"

#define INIT_FILE "init.ghar"

void init_io();
void init_builtins();

int eval_flag = TRUE;
int print_flag = TRUE;

char *current_file = NULL;

datum *gh_input = &LANG_NIL_VALUE;
datum *gh_result = &LANG_NIL_VALUE;
datum *globals = &LANG_NIL_VALUE;
datum *empty_locals = &LANG_NIL_VALUE;

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
	symbol_set(&globals, "quote", gh_cform(&lang_quote, gh_cons(gh_symbol("#expr"), &LANG_NIL_VALUE)));
	symbol_set(&globals, "unquote", gh_cform(&lang_unquote, gh_cons(gh_symbol("#expr"), &LANG_NIL_VALUE)));
	symbol_set(&globals, "unquote-splice", gh_cform(&lang_unquote_splice, gh_symbol("#lst")));
	symbol_set(&globals, "cons", gh_cfunc(&lang_cons, gh_cons(gh_symbol("#car"), gh_cons(gh_symbol("#cdr"), &LANG_NIL_VALUE))));
	symbol_set(&globals, "car", gh_cfunc(&lang_car, gh_cons(gh_symbol("#pair"), &LANG_NIL_VALUE)));
	symbol_set(&globals, "cdr", gh_cfunc(&lang_cdr, gh_cons(gh_symbol("#pair"), &LANG_NIL_VALUE)));
	symbol_set(&globals, "reverse", gh_cfunc(&lang_reverse, gh_cons(gh_symbol("#lst"), &LANG_NIL_VALUE)));
	symbol_set(&globals, "list", gh_cfunc(&lang_list, gh_symbol("#args")));
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
	symbol_set(&globals, "is-eof-object", gh_cfunc(&lang_is_eof_object, gh_cons(gh_symbol("#expr"), &LANG_NIL_VALUE)));
	symbol_set(&globals, "open", gh_cfunc(&lang_open, gh_cons(gh_symbol("#fname"), gh_symbol("#mode"))));
	symbol_set(&globals, "close", gh_cfunc(&lang_close, gh_cons(gh_symbol("#file"), &LANG_NIL_VALUE)));
	symbol_set(&globals, "read", gh_cfunc(&lang_read, gh_symbol("#file")));
	symbol_set(&globals, "write", gh_cfunc(&lang_write, gh_cons(gh_symbol("#expr"), gh_symbol("#file"))));
	symbol_set(&globals, "load", gh_cfunc(&lang_load, gh_cons(gh_symbol("#path"), &LANG_NIL_VALUE)));
	symbol_set(&globals, "apply", gh_cfunc(&lang_apply, gh_cons(gh_symbol("#fn"), gh_cons(gh_symbol("#args"), &LANG_NIL_VALUE))));
	symbol_set(&globals, "macroexpand", gh_cform(&lang_macroexpand, gh_cons(gh_symbol("#expr"), &LANG_NIL_VALUE)));
}

int main(int argc, char **argv) {
	yylineno = 0;
	init_io();
	init_builtins();

	gh_load(INIT_FILE);

	yyin = stdin;
	yypush_buffer_state(yy_create_buffer(yyin, YY_BUF_SIZE));
	prompt();
	do {
		yyparse();
	} while(gh_result->type != TYPE_EOF);
	yylex_destroy();

	yypop_buffer_state();
	printf("\n");

	return 0;
}

