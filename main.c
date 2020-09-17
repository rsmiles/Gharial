#include <stdio.h>
#include <stdlib.h>

#include "types.h"

#include "y.tab.h"

const datum NIL_VALUE = { TYPE_NIL, { 0 } };

datum *gh_nil() {
	return &NIL_VALUE;
}

int main(int argc, char **argv) {
	while ( !feof(stdin) ) {
		yyparse();
	}
	return 0;
}

