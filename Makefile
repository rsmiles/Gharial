.PHONY: clean

CFLAGS= --std=c89 -pedantic -Wall -Werror -g
LDFLAGS=-ll -lm -lgc -lcurses -ledit

all: gharial

gharial: main.o lib.o parse.o lex.o
	$(CC) -o $@ $^ $(LDFLAGS)

main.o: main.c gharial.h lib.o parse.o lex.o
	$(CC) $(CFLAGS) -c -o $@ $<

lib.o: lib.c gharial.h parse.o lex.o
	$(CC) $(CFLAGS) -c -o $@ $<

parse.o: parse.y gharial.h
	yacc --defines $<; $(CC) $(CFLAGS) -c -o $@ y.tab.c

lex.o: lex.l parse.o gharial.h
	lex --header-file=lex.yy.h $<; $(CC) $(CFLAGS) -c -o $@ lex.yy.c

clean:
	$(RM) gharial *.o *.yy.c *.tab.c *.yy.h *.tab.h .*.swp

