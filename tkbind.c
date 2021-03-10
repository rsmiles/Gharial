#include <tcl.h>
#include <tk.h>

#include "gharial.h"
#include "tkbind.h"

extern int global_argc;
extern char **global_argv;
Tcl_Interp *tcl_interp;

datum *lang_tk_init(datum **locals) {
	int ret;

	tcl_interp = Tcl_CreateInterp();
	ret = Tcl_Init(tcl_interp);
	gh_assert(ret == TCL_OK, "tk-error", "could not initialize tcl interpreter", &LANG_NIL_VALUE);

	ret = Tk_Init(tcl_interp);
	gh_assert(ret == TCL_OK, "tk-error", "could not initialize tk", &LANG_NIL_VALUE);

	return &LANG_TRUE_VALUE;
}

int tcl_eval(char *commands) {
	int ret;

	ret = Tcl_Eval(tcl_interp, commands);
	gh_assert(ret == TCL_OK, "tcl-error", "error occured when running commands: ~s", gh_cons(gh_string(commands), &LANG_NIL_VALUE));
	return ret;
}

datum *lang_tk_main(datum **locals) {
	Tk_MainLoop();
	return &LANG_NIL_VALUE;
}


