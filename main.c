#include <stdio.h>
#include <stdlib.h>

#include "y.tab.h"

#define TRUE  1
#define FALSE 0

typedef char byte;

int main(int argc, char **argv){
	while ( !feof(stdin) ){
		yyparse();
	}
	return 0;
}

