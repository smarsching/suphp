SHELL = /bin/sh

top_srcdir = ../..
srcdir = .

prefix = /usr/local
exec_prefix = ${prefix}
sbindir = ${exec_prefix}/sbin
CC = gcc -s
CFLAGS = -g -O2
DEFS = -DHAVE_CONFIG_H
LDFLAGS = 

INSTALL = /bin/install -c
INSTALL_PROGRAM = ${INSTALL}
APXS = /notfound/

DESTDIR =

MAKEDEFS = CC='$(CC)' DEFS='$(DEFS)' CFLAGS='$(CFLAGS)' \
           LDFLAGS='$(LDFLAGS)' prefix='$(prefix)' \
           sbindir='$(sbindir)' DESTDIR='$(DESTDIR)' \
           srcdir='$(srcdir)'

RM = rm -f
INCLUDES = -I$(top_srcdir)/src
EXTRADEFS = -DPATH_TO_SUPHP=\\\"$(sbindir)/suphp\\\"

all: suphp.mod

suphp.mod: mod_suphp.c
	$(APXS) -c $(DEFS) $(EXTRADEFS) $(INCLUDES) -Wc,"$(CFLAGS)" mod_suphp.c

install: suphp.mod
	$(APXS) -i -a mod_suphp.so

clean:
	$(RM) mod_suphp.so mod_suphp.o

