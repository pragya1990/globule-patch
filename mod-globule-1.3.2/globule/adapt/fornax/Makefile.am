noinst_PROGRAMS = cfornaxc test
cfornaxc_SOURCES = parser.y lexical.l ir.c output.c codegen.c transform.c declarations.c
test_SOURCES = test.cpp fornax.cpp fornax.h

AM_YFLAGS=-v -d

SUFFIXES=.cfornax

.PRECIOUS:	test.cpp

MOSTLYCLEANFILES=*~ dmalloc.out parser.output cfornaxc.output
CLEANFILES=test.cpp parser.c lexical.c parser.h
DISTCLEANFILES=configure Makefile.in config.h.in confdefs.h
MAINTAINERCLEANFILES=

.cfornax.cpp:
	cfornaxc -o $@ $<

lexical.c: parser.c
