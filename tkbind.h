#ifndef TKBIND_H
#define TKBIND_H

int tcl_eval(char *commands);

datum *lang_tk_init(datum **locals);

datum *lang_tk_main(datum **locals);

datum *lang_tk_eval(datum **locals);

datum *lang_tk_get(datum **locals);

#endif

