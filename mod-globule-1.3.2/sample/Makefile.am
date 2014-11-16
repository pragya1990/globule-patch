EXTRA_DIST=	globuleadm html errordocument
EXTRA_DIST+=	javascript-example.html httpd.conf webalizer.conf

TARGETS=globuleadm/database.php globuleadm/filtering.php globuleadm/general.cgi  globuleadm/general.html globuleadm/general.php globuleadm/resources.php globuleadm/index.cgi globuleadm/index.html globuleadm/index.php globuleadm/namebinding.php globuleadm/peers.php globuleadm/section-report.php globuleadm/section.cgi globuleadm/section.php globuleadm/sections.cgi globuleadm/sections.php globuleadm/statistics.php globuleadm/diagnostics.html globuleadm/diagnostics.php

MOSTLYCLEANFILES=`find -name \*~`
MAINTAINERCLEANFILES=$(TARGETS)

.PRECIOUS:	$(TARGETS)

globuleadm/database.php:	globuleadm/templates/database.php.tpl		globuleadm/templates/header.inc globuleadm/templates/footer.inc
				bash ../tools/globulectl generate $@
globuleadm/filtering.php:	globuleadm/templates/filtering.php.tpl		globuleadm/templates/header.inc globuleadm/templates/footer.inc
				bash ../tools/globulectl generate $@
globuleadm/general.cgi:		globuleadm/templates/general.cgi.tpl		globuleadm/templates/header.inc globuleadm/templates/footer.inc
				bash ../tools/globulectl generate $@
globuleadm/general.php:		globuleadm/templates/general.php.tpl		globuleadm/templates/header.inc globuleadm/templates/footer.inc
				bash ../tools/globulectl generate $@
globuleadm/general.html:	globuleadm/templates/general.html.tpl		globuleadm/templates/header.inc globuleadm/templates/footer.inc
				bash ../tools/globulectl generate $@
globuleadm/resources.php:	globuleadm/templates/resources.php.tpl		globuleadm/templates/header.inc globuleadm/templates/footer.inc
				bash ../tools/globulectl generate $@
globuleadm/index.cgi:		globuleadm/templates/index.cgi.tpl		globuleadm/templates/header.inc globuleadm/templates/footer.inc
				bash ../tools/globulectl generate $@
globuleadm/index.html:		globuleadm/templates/index.html.tpl		globuleadm/templates/header.inc globuleadm/templates/footer.inc
				bash ../tools/globulectl generate $@
globuleadm/index.php:		globuleadm/templates/index.php.tpl		globuleadm/templates/header.inc globuleadm/templates/footer.inc
				bash ../tools/globulectl generate $@
globuleadm/namebinding.php:	globuleadm/templates/namebinding.php.tpl	globuleadm/templates/header.inc globuleadm/templates/footer.inc
				bash ../tools/globulectl generate $@
globuleadm/peers.php:		globuleadm/templates/peers.php.tpl		globuleadm/templates/header.inc globuleadm/templates/footer.inc
				bash ../tools/globulectl generate $@
globuleadm/section-report.php:	globuleadm/templates/section-report.php.tpl	globuleadm/templates/header.inc globuleadm/templates/footer.inc
				bash ../tools/globulectl generate $@
globuleadm/section.cgi:		globuleadm/templates/section.cgi.tpl		globuleadm/templates/header.inc globuleadm/templates/footer.inc
				bash ../tools/globulectl generate $@
globuleadm/section.php:		globuleadm/templates/section.php.tpl		globuleadm/templates/header.inc globuleadm/templates/footer.inc
				bash ../tools/globulectl generate $@
globuleadm/sections.cgi:	globuleadm/templates/sections.cgi.tpl		globuleadm/templates/header.inc globuleadm/templates/footer.inc
				bash ../tools/globulectl generate $@
globuleadm/sections.php:	globuleadm/templates/sections.php.tpl		globuleadm/templates/header.inc globuleadm/templates/footer.inc
				bash ../tools/globulectl generate $@
globuleadm/statistics.php:	globuleadm/templates/statistics.php.tpl		globuleadm/templates/header.inc globuleadm/templates/footer.inc
				bash ../tools/globulectl generate $@
globuleadm/diagnostics.html:	globuleadm/templates/diagnostics.html.tpl	globuleadm/templates/header.inc globuleadm/templates/footer.inc
				bash ../tools/globulectl generate $@
globuleadm/diagnostics.php:	globuleadm/templates/diagnostics.php.tpl	globuleadm/templates/header.inc globuleadm/templates/footer.inc
				bash ../tools/globulectl generate $@

all:	$(TARGETS)

install-data-hook:
if INSTGLOBULEADM
	if [ \! -d @INSTGLOBULEADM@ ] ; then mkdir @INSTGLOBULEADM@ ; chmod go+rx @INSTGLOBULEADM@ ; fi
	if [ \! -d @INSTGLOBULEADM@/../error ] ; then mkdir @INSTGLOBULEADM@/../error ; chmod go+rx @INSTGLOBULEADM@/../error ; fi
	cp -R globuleadm/. @INSTGLOBULEADM@
	fqhn=`@APXS@ -q SYSCONFDIR` ; fqhn=`sed < $$fqhn/httpd.conf -e 's/^[ \t]*[Ss][Ee][Rr][Vv][Ee][Rr][Nn][Aa][Mm][Ee][ \t][ \t]*\([^ \t:]*\).*$$/\1/p' -e d | head -1` ; sed < globuleadm/.htaccess > @INSTGLOBULEADM@/.htaccess "s/localhost/localhost $$fqhn/"
	cp -R errordocument/. @INSTGLOBULEADM@/../error
	if [ -d @INSTGLOBULEADM@/CVS ] ; then rm -rf @INSTGLOBULEADM@/CVS ; fi
	if [ -d @INSTGLOBULEADM@/../error/CVS ] ; then rm -rf @INSTGLOBULEADM@/../error/CVS ; fi
endif
	bash ../tools/globulectl installconf -sample

install-demo:
if INSTGLOBULEADM
	cp -R html/. @INSTGLOBULEADM@/..
	cp globuleadm/Globule.css @INSTGLOBULEADM@/..
	cp globuleadm/globule.png @INSTGLOBULEADM@/..
	cp globuleadm/Globule.css @INSTGLOBULEADM@/../globule/.
	cp globuleadm/globule.png @INSTGLOBULEADM@/../globule/.
	if [ -d @INSTGLOBULEADM@/../CVS ] ; then rm -rf @INSTGLOBULEADM@/../CVS ; fi
else
	@echo "You should configure globule with --enable-globuleadm also"
	@exit 1
endif
