.PHONY:		FORCE cleanest
.SILENT:

default:	configure

#Makefile::	configure
#		./configure
configure:	configure.in
		autoconf
configure.in Makefile.am globule/Makefile.am GNUmakefile:
		;

cleanest:	clean
		rm -f config.guess config.log config.status config.sub configure
		rm -rf autom4te.cache
		rm -f aclocal.m4 config.h stamp-h* *~
		rm -f compile missing mkinstalldirs ltmain.sh install-sh depcomp

clean:

%:		Makefile FORCE
		if [ "$@" = "default" -o "$@" = "GNUmakefile" ]; then \
		  $(MAKE) -f Makefile ; \
		else \
		  $(MAKE) -f Makefile $@ ; \
		fi
