#
# Main Makefile for Globule tests, generated from Makefile.in
#

TESTSCRIPT:=t/TEST.PL
GENTESTSCRIPT:=t/TEST

FLOODCOMBFILESET:=../floodA-fileset.txt

SUBDIRS:=master slave

all::
	@echo "Run 'make test' to run the tests. Only works with Apache test framework installed"

test:: master slave combined flood floodcomb

master::
	@$(MAKE) $(GENTESTSCRIPT) 
	@(cd $@; $(MAKE) TESTLOGFILE=/dev/null; ) 

slave:: 
	@$(MAKE) $(GENTESTSCRIPT) 
	@(cd $@; $(MAKE) TESTLOGFILE=/dev/null; ) 

flood::
	@$(MAKE) $(GENTESTSCRIPT) 
	@(cd master; $(MAKE) TESTLOGFILE=/dev/null $@; )
	@(cd slave; $(MAKE) TESTLOGFILE=/dev/null $@; )

combined:: 
	@$(MAKE) $(GENTESTSCRIPT) 
	@(cd master; $(MAKE) CONF=conf/combinedA.conf PORT=8056 TESTLOGFILE=/dev/null start )
	@(cd slave; $(MAKE) CONF=conf/combinedA.conf PORT=8057 TESTLOGFILE=/dev/null start )
	@-(cd master; $(MAKE) TESTGROUPS=combinedA PORT=8056 TESTLOGFILE=/dev/null combined)
	@(cd slave; $(MAKE) CONF=conf/combinedA.conf TESTLOGFILE=/dev/null stop)
	@(cd master; $(MAKE) CONF=conf/combinedA.conf TESTLOGFILE=/dev/null stop)

floodcomb::
	$(MAKE) FLOODCOMBRUNMODE=floodrun floodcombdo

floodcombendless::
	$(MAKE) FLOODCOMBRUNMODE=floodrunendless floodcombdo

floodcombdo::
	@$(MAKE) $(GENTESTSCRIPT) 
	@echo "# Generating files for floodcomb test in master"
	@(cd master; $(MAKE) FLOODFILESET=$(FLOODCOMBFILESET) floodfilesgen )
	@echo "# Generating load definition in slave"
	@(cd slave; $(MAKE) FLOODFILESET=$(FLOODCOMBFILESET) floodloadgen )
	@echo "# Starting master server for floodcomb"
	@(cd master; $(MAKE) CONF=conf/floodcombA.conf PORT=8056 TESTLOGFILE=/dev/null start )
	@echo "# Starting slave server for floodcomb"
	@(cd slave; $(MAKE) CONF=conf/floodcombA.conf PORT=8057 TESTLOGFILE=/dev/null start )
	@echo "# Running floodcomb tests in slave"
	@-(cd slave; $(MAKE) TESTGROUPS=floodcombA PORT=8057 TESTLOGFILE=/dev/null $(FLOODCOMBRUNMODE))
	@echo "# Stopping slave server for floodcomb"
	@(cd slave; $(MAKE) CONF=conf/floodcombA.conf TESTLOGFILE=/dev/null stop)
	@echo "# Stopping master server for floodcomb"
	@(cd master; $(MAKE) CONF=conf/floodcombA.conf TESTLOGFILE=/dev/null stop)
	@echo "# Deleting load definition in slave"
	@(cd slave; $(MAKE) FLOODFILESET=$(FLOODCOMBFILESET) floodloaddel )
	@echo "# Deleting generated files in master"
	@(cd master; $(MAKE) FLOODFILESET=$(FLOODCOMBFILESET) floodfilesdel )


install::

install::


clean::
	@$(MAKE) $(GENTESTSCRIPT) 
	for dir in $(SUBDIRS); do  \
		(cd $$dir; $(MAKE) clean; ) \
	done
	-rm $(GENTESTSCRIPT)
	-rmdir */logs
	-rmdir slave/htdocs

# -I.. is necessary for the test script to find the code in "GlobuleTest"
$(GENTESTSCRIPT):: $(TESTSCRIPT)
	echo "#!/usr/bin/perl -I.." > $(GENTESTSCRIPT)
	echo "#" >> $(GENTESTSCRIPT)
	echo "# GENERATED FILE, DO NOT EDIT!" >> $(GENTESTSCRIPT)
	echo "#" >> $(GENTESTSCRIPT)
	echo '%Apache::TestConfig::Argv = qw(apxs /home/berry/globule/globule/../apache/bin/apxs);' >> $(GENTESTSCRIPT)
	cat $(TESTSCRIPT) >> $(GENTESTSCRIPT)
	chmod +x $(GENTESTSCRIPT)

# Autohell will call this Makefile with maintainer-clean and other targets,
# make sure they're handled.
%: force
	@echo "tests: Ignored make of unknown target"

force: ;
