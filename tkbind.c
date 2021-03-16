#include <tcl.h>
#include <tk.h>

#include "gharial.h"
#include "tkbind.h"

extern int global_argc;
extern char **global_argv;
Tcl_Interp *tcl_interp;

int gh_callback(ClientData client_data, Tcl_Interp tcl_interp, int argc, char *argv[]);

int gh_callback(ClientData client_data, Tcl_Interp tcl_interp, int argc, char *argv[]) {
	printf("Test\n");
	return TCL_OK;
}

datum *lang_tk_init(datum **locals) {
	int ret;
	Tcl_Command tk_callback;

	tcl_interp = Tcl_CreateInterp();
	ret = Tcl_Init(tcl_interp);
	gh_assert(ret == TCL_OK, "tk-error", "could not initialize tcl interpreter", &LANG_NIL_VALUE);

	ret = Tk_Init(tcl_interp);
	gh_assert(ret == TCL_OK, "tk-error", "could not initialize tk", &LANG_NIL_VALUE);

	tk_callback = Tcl_CreateCommand(tcl_interp, "gh-callback", &gh_callback, NULL, NULL);

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
	int tcl_ret;

	args = var_get(locals, "#args");
	tcl_string = "";

	for (iterator = args; iterator->type == TYPE_CONS; iterator = iterator->value.cons.cdr) {
		if (iterator->value.cons.car->type == TYPE_FUNC || iterator->value.cons.car->type == TYPE_CFUNC || iterator->value.cons.car->type == TYPE_CFORM) {
			tcl_string = string_append(tcl_string, "gh-callback");
		} else {
			tcl_string = string_append(tcl_string, gh_to_string(iterator->value.cons.car));
		}
		if (iterator->value.cons.cdr->type == TYPE_CONS) {
			tcl_string = string_append(tcl_string, " ");
		}
	}

	tcl_ret = tcl_eval(tcl_string);
	gh_assert(tcl_ret == TCL_OK, "tcl-error", "~a", gh_cons(gh_string(Tcl_GetStringResult(tcl_interp)), &LANG_NIL_VALUE));
	
	return &LANG_NIL_VALUE;
}

