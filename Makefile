.PHONY: install uninstall clean

CFLAGS := -std=c89 -Wall -Werror -pedantic -fvisibility=hidden
ifndef NDEBUG
CFLAGS += -O0 -ggdb \
          -fsanitize=undefined \
          -fsanitize=address
else
CFLAGS += -O2 -DNDEBUG=1
endif
PREFIX := /usr/local
BINDIR := $(PREFIX)/bin/
MANDIR := $(PREFIX)/man/man1/

ptxt: ptxt.c
	$(CC) $(CFLAGS) -o $@ $<

install: ptxt
	install -d $(BINDIR) $(MANDIR)
	install ptxt $(BINDIR)
	install ptxt.1 $(MANDIR)

uninstall:
	rm $(BINDIR)ptxt
	rm $(MANDIR)ptxt.1

clean:
	rm -f ptxt
