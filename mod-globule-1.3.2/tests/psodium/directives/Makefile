#
# Makefile to test valid and invalid Apache configurations for pSodium
#
# Do make TESTLOGFILE=/tmp/log to see verbose output of the tests
#

TESTPROG:=../t/TEST
TESTOPTS:=-t_dir `pwd`
TESTLOGFILE:=/tmp/testlog

all:: start valid invalid stop

start::
	@echo "# Be very VERY VERY patient (up to 60 secs per test)."
	@echo "# Edit Apache::TestServer to change this parameter."
	-rm $(TESTLOGFILE)

stop::
	$(TESTPROG) $(TESTOPTS) -stop-httpd 2>&1 >> $(TESTLOGFILE)

clean::
	-$(TESTPROG) $(TESTOPTS) -clean
	-rm -r globule psodium-temp core*

invalid::
	@echo "# Test INvalid configs"
	@for conf in conf/i*.in; do \
		(BASE=`echo $$conf | sed -e 's/\.in$$//1'`; \
		if $(TESTPROG) $(TESTOPTS) -postamble "Include $$BASE" -start-httpd 2>&1 | tee -a $(TESTLOGFILE) | awk '{ print "#",$$0}' | egrep -e 'Syntax error|server failed'; \
		then \
			echo $$BASE....ok; \
		else \
			echo $$BASE....FAILED test 1; \
		fi;); \
	done	



# tee must come before awk, for unknown reasons.
valid::
	@echo "# Test valid configs"
	@for conf in conf/v*.in; do \
		(BASE=`echo $$conf | sed -e 's/\.in$$//1'`; \
		if $(TESTPROG) $(TESTOPTS) -postamble "Include $$BASE" -start-httpd 2>&1 | tee -a $(TESTLOGFILE) | awk '{ print "#",$$0}' | egrep 'server.*started$$'; \
		then \
			echo $$BASE....ok; \
		else \
			echo $$BASE....FAILED test 1; \
		fi;); \
 	done	
