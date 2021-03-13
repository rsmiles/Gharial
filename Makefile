.PHONY: clean install uninstall install-user uninstall-user

CFLAGS= --std=c99 -pedantic -Wall -Werror -g -I/usr/include/tcl
LDFLAGS=-ll -lm -lgc -lcurses -ledit -lsodium -ltk -ltcl -lX11

all: gharial

gharial: main.o lib.o parse.o lex.o tkbind.o
	$(CC) -o $@ $^ $(LDFLAGS)

main.o: main.c gharial.h lib.o parse.o lex.o
	$(CC) $(CFLAGS) -c -o $@ $<

lib.o: lib.c gharial.h parse.o lex.o
	$(CC) $(CFLAGS) -c -o $@ $<

tkbind.o: tkbind.c tkbind.h
	$(CC) --std=c99 -pedantic -Wall -Werror -g -I/usr/include/tcl -c -o $@ $<

parse.o: parse.y gharial.h
	yacc --defines $<; $(CC) $(CFLAGS) -c -o $@ y.tab.c

lex.o: lex.l parse.o gharial.h
	lex --header-file=lex.yy.h $<; $(CC) $(CFLAGS) -c -o $@ lex.yy.c

test: gharial test.ghar
	./gharial test.ghar

clean:
	$(RM) gharial *.o *.yy.c *.tab.c *.yy.h *.tab.h .*.swp

install: gharial
	install -m 555 gharial /usr/local/bin/
	install -m 555 init.ghar /usr/local/lib/

uninstall:
	$(RM) /usr/local/bin/gharial
	$(RM) /usr/local/lib/init.ghar

install-user: gharial
	install -m 555 gharial ~/.local/bin
	install -m 555 init.ghar ~/.local/lib

uninstall-user:
	$(RM) rm ~/.local/bin/gharial
	$(RM) rm ~/.local/lib/init.ghar
