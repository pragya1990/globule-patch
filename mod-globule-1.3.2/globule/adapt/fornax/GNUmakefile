.PHONY:		FORCE cleanest

PATH:=/usr/local/VU/globe/globe/Packages:$(PATH)

default:	all

Makefile::	configure
		./configure --disable-dmalloc --disable-debug --enable-optimize
configure:	configure.in Makefile.am
		if [ -d /usr/local/VU/globe/globe/Packages ] ; then aclocal --acdir=/usr/local/VU/globe/globe/Packages/automake/share/aclocal-1.7 -I /usr/local/VU/globe/globe/Packages/libtool/share/aclocal ; else aclocal ; fi
		autoconf
		libtoolize --force --copy --automake
		autoheader --force
		automake --add-missing --copy
configure.in Makefile.am globule/Makefile.am GNUmakefile:
		;
cleanest:
		( if [ -f Makefile ]; then $(MAKE) -f Makefile distclean ; fi ; exit 0 )
		rm -f stamp-h1 aclocal.m4 config.guess config.log config.status config.sub configure `find . -name \*~ -print`
		rm -f missing mkinstalldirs ltmain.sh install-sh depcomp libtool
		rm -f Makefile.in
		rm -f Makefile
		rm -rf autom4te.cache .deps

%:		Makefile FORCE
		if [ "$@" = "default" -o "$@" = "GNUmakefile" ]; then \
		  $(MAKE) -f Makefile ; \
		else \
		  $(MAKE) -f Makefile $@ ; \
		fi
