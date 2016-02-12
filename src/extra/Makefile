CFLAGS=-O2
SOURCEFILE=checkpasswd.c
SOURCEDIR=src
OUTFILE=checkpasswd
DESTDIR=$(E_PREFIX)/bin
OS=$(shell uname -s)

ifeq ($(OS),OpenBSD)

else
CFLAGS+=-lcrypt
endif

default:
	$(CC) $(CFLAGS) "$(SOURCEDIR)/$(SOURCEFILE)" -o $(OUTFILE)
all:
	$(CC) $(CFLAGS) "$(SOURCEDIR)/$(SOURCEFILE)" -o $(OUTFILE)
install:
	cp $(OUTFILE) $(DESTDIR)
	chmod +xs "$(DESTDIR)/$(OUTFILE)"
clean:
	rm $(OUTFILE)
