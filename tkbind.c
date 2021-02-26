#include <tcl.h>
#include <tk.h>

#include "gharial.h"
#include "tkbind.h"

extern int global_argc;
extern char **global_argv;
Tcl_Interp *tcl_interp;

int Tcl_AppInit(Tcl_Interp *tcl_interp) {
	int ret;

	ret = Tcl_Init(tcl_interp);
	if (ret != TCL_OK) {
		fprintf(stderr, "Could not initialize tcl interpreter\n");
		return ret;
	}

	ret = Tk_Init(tcl_interp);
	if (ret != TCL_OK) {
		fprintf(stderr, "Could not initialize TK\n");
		return ret;
	}
	return TCL_OK;
}

datum *tcl_eval(char *commands) {
	int ret;

	ret = Tcl_Eval(tcl_interp, commands);
	gh_assert(ret == TCL_OK, "tcl-error", "error occured when running commands: ~s", gh_cons(gh_string(commands), &LANG_NIL_VALUE));
	return &LANG_NIL_VALUE;
}

datum *lang_tk_main(datum **locals) {
	Tk_Main(global_argc, global_argv, &Tcl_AppInit);
	return &LANG_NIL_VALUE;
}


