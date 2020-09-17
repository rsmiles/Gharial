.PHONY: clean

CFLAGS=-std=c89 -pedantic -Wall #-Werror
LDFLAGS=-ll

all: gharial

gharial: main.o parse.o lex.o
	$(CC) -o $@ $^ $(LDFLAGS)

main.o: main.c parse.o
	$(CC) $(CFLAGS) -c -o $@ main.c

parse.o: parse.y
	yacc --defines $<; $(CC) $(CFLAGS) -c -o $@ y.tab.c

lex.o: lex.l parse.o
	lex --header-file=lex.yy.h $<; $(CC) $(CFLAGS) -c -o $@ lex.yy.c

clean:
	$(RM) gharial *.o *.yy.c *.tab.c *.yy.h *.tab.h .*.swp

