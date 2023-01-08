.PHONY: install clean

CFLAGS := -std=c89 -O0 -ggdb \
          -fsanitize=undefined \
          -fsanitize=address \
          -fvisibility=hidden
PREFIX := /usr/local
BINDIR := $(PREFIX)/bin

ptxt: ptxt.c
	$(CC) $(CFLAGS) -o $@ $<

install: ptxt
	install -d $(BINDIR)
	install ptxt $(BINDIR)

clean:
	rm -f ptxt
