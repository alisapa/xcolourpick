CFLAGS=-O2 -std=c99
CFLAGS_DBG=-Og -g -std=c99
LDFLAGS=-lX11
PREFIX=/usr/local
BINDIR=$(PREFIX)/bin
MANDIR=$(PREFIX)/share/man/man1
xcolourpick: xcolourpick.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
debug: xcolourpick.c
	$(CC) $(CFLAGS_DBG) -o xcolourpick-dbg $^ $(LDFLAGS)
.PHONY: clean
clean:
	rm -f xcolourpick-dbg
	rm -f xcolourpick
.PHONY: install
install:
	mkdir -p $(BINDIR)
	mkdir -p $(MANDIR)
	cp xcolourpick $(BINDIR)/xcolourpick
	cp xcolourpick.1 $(MANDIR)/xcolourpick.1
