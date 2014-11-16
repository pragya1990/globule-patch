EXTRA_PROGRAMS       = tracegen traceplay simulator
noinst_LIBRARIES     = libadapt.a
noinst_LTLIBRARIES   = libadapt.la
tracegen_SOURCES     = tracegen.cpp
#tracegen_CFLAGS     = -I..
tracegen_LDADD       = -lgsl -lgslcblas -lm
traceplay_SOURCES    = reportreader.cpp traceplay.cpp mapping.cpp ../netw/HttpRequest.cpp ../utilities.cpp
traceplay_CXXFLAGS   = -DMAIN
traceplay_LDADD      = -lapr-0 -lpthread -ldl
libadapt_la_CXXFLAGS = -Ifornax
libadapt_la_SOURCES  = reportreader.cpp mapping.cpp globules.cpp fornax/fornax.cpp reportreader.h mapping.h fornax/fornax.h
libadapt_a_SOURCES   = reportreader.cpp mapping.cpp globules.cpp fornax/fornax.cpp reportreader.h mapping.h fornax/fornax.h
libadapt_a_CXXFLAGS  = -Ifornax
simulator_SOURCES    = reportreader.cpp mapping.cpp globules.cpp fornax/fornax.cpp ../utilities.cpp reportreader.h mapping.h fornax/fornax.h
simulator_CXXFLAGS = -Ifornax -DMAIN
simulator_LDADD    = -lapr-0 -lpthread -ldl -lrt

SUFFIXES=.cfornax

.PRECIOUS:	globule.cpp

MOSTLYCLEANFILES=*~ dmalloc.out *.output
CLEANFILES=
DISTCLEANFILES=configure Makefile.in config.h.in confdefs.h
MAINTAINERCLEANFILES=globule.cpp

.cfornax.cpp:
	$(MAKE) -C fornax
	./fornax/cfornaxc -L -o $@ $<

install:

clean:

