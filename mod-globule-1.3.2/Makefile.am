AUTOMAKE_OPTIONS = no-dependencies
SUBDIRS          = globule monitor sample
if PSODIUM
SUBDIRS         += psodium
endif
EXTRA_DIST       = tests testsuite doc tools apache globule.sln mod_globule globule/compat/Win32 psodium globule/adapt/globules.cfornax globule/adapt/fornax
bin_SCRIPTS     = tools/globulectl

clean-local:
	$(MAKE) -C testsuite clean
	$(MAKE) -C doc clean
	rm -f tests/psodium/Makefile

#install-exec-local:
#	cd $(DESTDIR)$(libdir) ; if [ -f mod_globule ]; then echo rm -f mod_globule mod_globule$(shrext) ; echo "$(LN_S) mod_globule-$(VERSION)$(shrext) mod_globule$(shrext)" ; fi
#	cd $(DESTDIR)$(libdir) && if [ -f mod_globule-$(VERSION) ]; then echo mv mod_globule$(VERSION) mod_globule$(VERSION)$(shrext) ; fi

install-data-hook:
if INSTDOCUMENTATION
	$(MAKE) -C doc install
endif
	@echo "----------------------------------------------------------------------"
	@echo "The Globule module has now been installed.  You will need to adapt"
	@echo "your Apache configuration file (httpd.conf) to make use of Globule."
	@echo "A sample configuration file is stored as httpd-globule.conf"
	@echo "An attractive way of doing composing a httpd.conf in a semi-automatical"
	@echo "manner, and manage your mutual co-operations is to use the Globule"
	@echo "Broker System.  You can reach this GBS at:  http://www.globeworld.net/"
	@echo "----------------------------------------------------------------------"

install-demo:
	$(MAKE) -C sample install-demo

dist-hook:
	$(MAKE) -C tests clean
	$(MAKE) -C testsuite clean
	$(MAKE) -C doc all
	$(MAKE) -C doc clean
	find $(distdir) -name CVS -prune -exec rm -rf \{\} \;
	pushd $(srcdir) ; s=`pwd` ; popd ; cd $(distdir) ; find . -path ./globule/compat -prune -o \( -iname \*.c -o -iname \*.cc -o -iname \*.cpp -o -iname \*.h -o -iname \*.hpp -o -iname \*.inl \) -exec sh -c 'head -4 $$1 | grep -qi "^[ \t\*\-\+o]*copyright[^a-zA-Z]" ; if [ $$? -ne 0 ] ; then echo "/*" > $$1 ; cat $$2/COPYING >> $$1 ; echo "*/" >> $$1 ; cat $$2/$$1 >> $$1 ; fi' sh \{\} $$s \;
