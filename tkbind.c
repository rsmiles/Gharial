#include <tcl.h>
#include <tk.h>

#include "gharial.h"
#include "tkbind.h"

extern int global_argc;
extern char **global_argv;
Tcl_Interp *tcl_interp;
datum *callbacks;

int gh_callback(void *client_data, Tcl_Interp *tcl_interp, int argc, const char **argv);

int gh_callback(void *client_data, Tcl_Interp *tcl_interp, int argc, const char **argv) {
	datum *widget;
	datum *thunk;

	widget = symbol_get(callbacks, argv[1]);
	gh_assert(widget != NULL, "ref-error", "widget not found: ~a", gh_cons(gh_string(argv[1] + 1), &LANG_NIL_VALUE));
	thunk = symbol_get(widget, argv[2]);
	gh_assert(thunk != NULL, "ref-error", "callback not found: ~a", gh_cons(gh_string(argv[2]), &LANG_NIL_VALUE));
	

	apply(thunk, &LANG_NIL_VALUE, &empty_locals);
	return TCL_OK;
}

datum *lang_tk_init(datum **locals) {
	int ret;

	tcl_interp = Tcl_CreateInterp();
	ret = Tcl_Init(tcl_interp);
	gh_assert(ret == TCL_OK, "tk-error", "could not initialize tcl interpreter", &LANG_NIL_VALUE);

	ret = Tk_Init(tcl_interp);
	gh_assert(ret == TCL_OK, "tk-error", "could not initialize tk", &LANG_NIL_VALUE);

	Tcl_CreateCommand(tcl_interp, "gh-callback", &gh_callback, NULL, NULL);

	callbacks = &LANG_NIL_VALUE;

	return &LANG_TRUE_VALUE;
}

int tcl_eval(char *commands) { int ret; 
	ret = Tcl_Eval(tcl_interp, commands);
	return ret;
}

datum *lang_tk_main(datum **locals) {
	Tk_MainLoop();
	return &LANG_NIL_VALUE;
}

datum *lang_tk_eval(datum **locals) {
	datum *args;
	char *tcl_string;
	datum *iterator;
	datum *previous;
	int tcl_ret;
	datum *callback_result;

	args = var_get(locals, "#args");
	tcl_string = "";

	previous = NULL;
	for (iterator = args; iterator->type == TYPE_CONS; iterator = iterator->value.cons.cdr) {
		datum *current;

		current = iterator->value.cons.car;
		if (current->type == TYPE_FUNC || current->type == TYPE_CFUNC || current->type == TYPE_CFORM) {
			tcl_string = string_append(tcl_string, "{gh-callback ");
			tcl_string = string_append(tcl_string, gh_to_string(args->value.cons.cdr->value.cons.car));
			if (previous != NULL) {
				tcl_string = string_append(tcl_string, " ");
				tcl_string = string_append(tcl_string, gh_to_string(previous));
				callback_result = gh_cons(gh_cons(gh_symbol(gh_to_string(previous)), current), &LANG_NIL_VALUE);
			} else {
				gh_assert(FALSE, "tcl-error", "unnamed command", &LANG_NIL_VALUE);
			}
			tcl_string = string_append(tcl_string, "}");
			symbol_set(&callbacks, gh_to_string(args->value.cons.cdr->value.cons.car), callback_result);
		} else {
			tcl_string = string_append(tcl_string, gh_to_string(iterator->value.cons.car));
		}
		if (iterator->value.cons.cdr->type == TYPE_CONS) {
			tcl_string = string_append(tcl_string, " ");
		}
		previous = current;
	}

	tcl_ret = tcl_eval(tcl_string);
	gh_assert(tcl_ret == TCL_OK, "tcl-error", "~a", gh_cons(gh_string(Tcl_GetStringResult(tcl_interp)), &LANG_NIL_VALUE));
	
	return &LANG_NIL_VALUE;
}

