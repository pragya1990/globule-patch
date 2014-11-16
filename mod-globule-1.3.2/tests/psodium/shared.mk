#
# Makefile shared by all entities.
#

TESTPROG:=../t/TEST
TESTOPTS:=-t_dir `pwd` -verbose

#TESTLOGFILE:=/dev/null
TESTLOGFILE:=/tmp/testlog.txt

FLOODFILEDIR:=htdocs/flexport
FLOODFILESOURCE:=/dev/urandom
# Two load generators: Apache test framework's Flood and WGet.
FLOODLOADFLOOD=floodload.xml
FLOODLOADWGET=floodload.wget
PORT:=8057


all:: test

clean::
	-rm core core.*
	-$(TESTPROG) $(TESTOPTS) -clean
	-rm -r psodium-temp `find . -name .htglobule`
	$(MAKE) floodfilesdel
	$(MAKE) floodloaddel 

## Huh >& is TCSH syntax, does this work in a BASH env?
test::
	@for tgroupconf in $(TESTGROUPCONFS) ; do		 \
	(tgroup=`echo $$tgroupconf | sed -e 's/,.*//g'` ; \
	conf=`echo $$tgroupconf | sed -e 's/.*,//g'` ; \
	POSTAMBLE="Include conf/$$conf.conf";		\
	echo "# Running group $$tgroup (be patient)"; \
	for t in ../`basename $$PWD`/$$tgroup* ; do \
	echo "# Running test $$t (be patient)"; \
	rm -rf globule psodium-temp; \
	$(TESTPROG) $(TESTOPTS) -postamble "$$POSTAMBLE" $$t 2>&1 | tee -a $(TESTLOGFILE); \
	done ;) \
	done

## For master+slave testing

start::
	@-rm -rf globule psodium-temp
	@echo "# Starting "`basename $$PWD`" with conf" $(CONF) "port " $(PORT)
	#@$(TESTPROG) $(TESTOPTS) -postamble "Include $(CONF)" -port $(PORT) -start-httpd >& $(TESTLOGFILE)
	@$(TESTPROG) $(TESTOPTS) -postamble "Include $(CONF)" -port $(PORT) -start-httpd
		
stop::
	@echo "# Stopping "`basename $$PWD`
	@$(TESTPROG) $(TESTOPTS) -stop-httpd >& $(TESTLOGFILE)
	
combined::
	@for tgroup in $(TESTGROUPS) ; do		 \
	($(TESTPROG) $(TESTOPTS) -port $(PORT) -run-tests ../`basename $$PWD`/$$tgroup* 2>&1 | tee -a $(TESTLOGFILE) ;) \
	done

# 
floodA1 floodA2::
	$(MAKE) FLOODFILESET=../floodA-fileset2.txt TESTNO=$@ floodtest

floodtest:: 
	@echo "# Generating files for flood test in $(FLOODFILEDIR)"
	$(MAKE) FLOODFILESET=$(FLOODFILESET) floodfilesgen
	@echo "# Generating load definition in $(FLOODLOADFLOOD)"
	$(MAKE) FLOODFILESET=$(FLOODFILESET) floodloadgen
	$(MAKE) CONF=conf/$(TESTNO).conf PORT=$(PORT) start
	@echo "# Starting flood test $@"
	$(MAKE) LOADER=flood floodrun
	$(MAKE) CONF=conf/$(TESTNO).conf PORT=$(PORT) stop
	@echo "# Deleting load definition"
	$(MAKE) FLOODFILESET=$(FLOODFILESET) floodloaddel
	@echo "# Deleting generated files"
	$(MAKE) FLOODFILESET=$(FLOODFILESET) floodfilesdel

floodrun::
	if [ "$(LOADER)" = "flood" ]; then \
		flood $(FLOODLOADFLOOD) | awk '{ print $$1, $$NF, tolower($$6);}'; \
        else \
		http_proxy=$(HTTP_PROXY) wget -i $(FLOODLOADWGET) -O /dev/null 2>&1 | gawk 'substr( $$2, 1, length( "http:" )) == "http:" { url=$$2 } $$1 == "Proxy" { if ($$6 != "200") { ret=2 } else { ret = 1 }}  $$1 == "Length:" { if ($$3 != "[application/octet-stream]") { mime=2 } else { mime=1 }} ret == 1 && mime == 1 { print url" OK"; ret=0;mime=0 } ret != 0 && mime !=0 { print url" FAIL"; ret=0; mime=0 } { print "# "$$0 }'; \
	fi


floodrunendless::
	while true; \
	do \
		$(MAKE) LOADER=$(LOADER) floodrun
	done


# Generate files for flood test
floodfilesgen::
	@(cat $(FLOODFILESET) | while read name size ; \
	do \
		fullname=$(FLOODFILEDIR)/$$name ; \
		if [ ! -d "`dirname $$fullname`" ]; then \
			mkdir -p `dirname $$fullname` ; \
		fi ; \
		dd of=$$fullname if=$(FLOODFILESOURCE) bs=$$size count=1 >& /dev/null ; \
	done )

#  Delete files generated for flood test
floodfilesdel::
	@-rm -r $(FLOODFILEDIR)

floodloadgen: 
	$(MAKE) FLOODFILESET=$(FLOODFILESET) floodloadgenwget 
	$(MAKE) FLOODFILESET=$(FLOODFILESET) floodloadgenflood 

floodloadgenwget:
	@cat $(FLOODFILESET) | gawk -v PORT=$(PORT) -v HTDOCS=`basename $(FLOODFILEDIR)` '{printf( "http://localhost:%s/%s/%s\n", PORT, HTDOCS, $$1 );}' > $(FLOODLOADWGET)

floodloadgenflood:
	@echo '<?xml version="1.0"?>' > $(FLOODLOADFLOOD)
	@echo '<!DOCTYPE flood SYSTEM "flood.dtd">' >> $(FLOODLOADFLOOD)
	@echo '<!-- Hi, I am a flood config file.  They call me "round-robin.xml"' >> $(FLOODLOADFLOOD)
	@echo '     I am an example of a "round-robin" configuration.  All of the URLs in' >> $(FLOODLOADFLOOD)
	@echo '     the urllist "Test Hosts" will be hit in sequential order.  ' >> $(FLOODLOADFLOOD)
	@echo '     Floods syntax is XML based.  ' >> $(FLOODLOADFLOOD)
	@echo '     After compiling flood (./configure && make), you can execute me as:' >> $(FLOODLOADFLOOD)
	@echo '     ./flood examples/round-robin.xml' >> $(FLOODLOADFLOOD)
	@echo '     (The path to me.)' >> $(FLOODLOADFLOOD)
	@echo '' >> $(FLOODLOADFLOOD)
	@echo '     Flood will then output data to stdout in the format as specified below ' >> $(FLOODLOADFLOOD)
	@echo '     (relative_times) for each URL that it hits:' >> $(FLOODLOADFLOOD)
	@echo '     998951597871780 1389 57420 7960273 7960282 OK  4 http://www.apache.org/' >> $(FLOODLOADFLOOD)
	@echo '     The columns are as follows:' >> $(FLOODLOADFLOOD)
	@echo '     - Absolute Time in usec that the request was started by flood' >> $(FLOODLOADFLOOD)
	@echo '     - Relative Time in usec (to first column) that it took to open the socket' >> $(FLOODLOADFLOOD)
	@echo '       to the specified server' >> $(FLOODLOADFLOOD)
	@echo '     - Relative Time in usec (to first column) that it took to write the' >> $(FLOODLOADFLOOD)
	@echo '       generated request to the server.' >> $(FLOODLOADFLOOD)
	@echo '     - Relative Time in usec (to first column) that it took to read the' >> $(FLOODLOADFLOOD)
	@echo '       generated response from the server.' >> $(FLOODLOADFLOOD)
	@echo '     - Relative Time in usec (to first column) that it took to close the' >> $(FLOODLOADFLOOD)
	@echo '       socket.  On a keepalive socket, it may not close the socket.' >> $(FLOODLOADFLOOD)
	@echo '     - OK/FAIL indicates what the verification module (in this case, ' >> $(FLOODLOADFLOOD)
	@echo '       verify_200) thought of the request.' >> $(FLOODLOADFLOOD)
	@echo '     - The thread/process id of the farmer that made the request.' >> $(FLOODLOADFLOOD)
	@echo '     - The URL that was hit (without query strings)' >> $(FLOODLOADFLOOD)
	@echo '' >> $(FLOODLOADFLOOD)
	@echo '     To get a "nice" summary of the output (as well as an idea of how to' >> $(FLOODLOADFLOOD)
	@echo '     further process this info), try out:' >> $(FLOODLOADFLOOD)
	@echo '     ./flood examples/round-robin.xml > my-output' >> $(FLOODLOADFLOOD)
	@echo '     ./examples/analyze-relative my-output' >> $(FLOODLOADFLOOD)
	@echo '     ' >> $(FLOODLOADFLOOD)
	@echo '-->' >> $(FLOODLOADFLOOD)
	@echo '<flood configversion="1">' >> $(FLOODLOADFLOOD)
	@echo '  <!-- A urllist describes which hosts and which methods we want to hit. -->' >> $(FLOODLOADFLOOD)
	@echo '  <urllist>' >> $(FLOODLOADFLOOD)
	@echo '    <name>Test Hosts</name>' >> $(FLOODLOADFLOOD)
	@echo '    <description>A bunch of hosts we want to hit</description>' >> $(FLOODLOADFLOOD)
	@echo '    <!-- We first send a POST request to search.apache.org looking for "foo" ' >> $(FLOODLOADFLOOD)
	@echo '	 Notice the payload attribute - this allows us to specify the POST' >> $(FLOODLOADFLOOD)
	@echo '	 content.  -->' >> $(FLOODLOADFLOOD)
	@cat $(FLOODFILESET) | gawk -v PORT=$(PORT) -v HTDOCS=`basename $(FLOODFILEDIR)` '{printf( "<url>http://localhost:%s/%s/%s</url>\n", PORT, HTDOCS, $$1 );}' >> $(FLOODLOADFLOOD)
	@echo '</urllist>' >> $(FLOODLOADFLOOD)
	@echo '' >> $(FLOODLOADFLOOD)
	@echo '  <!-- The profile describes how we will hit the urllists.  For this' >> $(FLOODLOADFLOOD)
	@echo '       example, we execute with the round_robin profile.' >> $(FLOODLOADFLOOD)
	@echo '       Round robin runs all of the URLs in the urllist in order once. -->' >> $(FLOODLOADFLOOD)
	@echo '  <profile>' >> $(FLOODLOADFLOOD)
	@echo '    <name>Example Profile</name>' >> $(FLOODLOADFLOOD)
	@echo '    <description>A Test Round Robin Configuration</description>' >> $(FLOODLOADFLOOD)
	@echo '' >> $(FLOODLOADFLOOD)
	@echo '    <!-- See above.  This indicates which URLs we will hit.  -->' >> $(FLOODLOADFLOOD)
	@echo '    <useurllist>Test Hosts</useurllist>' >> $(FLOODLOADFLOOD)
	@echo '' >> $(FLOODLOADFLOOD)
	@echo '    <!-- Specifies that we will use round_robin profile logic -->' >> $(FLOODLOADFLOOD)
	@echo '    <profiletype>round_robin</profiletype>' >> $(FLOODLOADFLOOD)
	@echo '    <!-- Specifies that we will use generic socket logic ' >> $(FLOODLOADFLOOD)
	@echo '	 We also have "keepalive" as an option - this option indicates that' >> $(FLOODLOADFLOOD)
	@echo '	 we should attempt to use HTTP Keepalive when available.  -->' >> $(FLOODLOADFLOOD)
	@echo '    <socket>generic</socket>' >> $(FLOODLOADFLOOD)
	@echo '    <!-- Specifies that we will use verify_200 for response verification ' >> $(FLOODLOADFLOOD)
	@echo '	 This verification step ensures that we received a 2xx or 3xx' >> $(FLOODLOADFLOOD)
	@echo '	 status code from the server.  -->' >> $(FLOODLOADFLOOD)
	@echo '    <verify_resp>verify_200</verify_resp>' >> $(FLOODLOADFLOOD)
	@echo '    <!-- Specifies that we will use the "relative_times" report generation ' >> $(FLOODLOADFLOOD)
	@echo '	 We also have "easy" - this option is similar to "relative_times",' >> $(FLOODLOADFLOOD)
	@echo '	 but the times posted are absolute instead of relative to the start' >> $(FLOODLOADFLOOD)
	@echo '	 of the request.  -->' >> $(FLOODLOADFLOOD)
	@echo '    <report>relative_times</report>' >> $(FLOODLOADFLOOD)
	@echo '' >> $(FLOODLOADFLOOD)
	@echo '  </profile>' >> $(FLOODLOADFLOOD)
	@echo '' >> $(FLOODLOADFLOOD)
	@echo '  <!-- A farmer runs one profile a certain number of times.  This farmer is' >> $(FLOODLOADFLOOD)
	@echo '       called Joe.  -->' >> $(FLOODLOADFLOOD)
	@echo '  <farmer>' >> $(FLOODLOADFLOOD)
	@echo '    <name>Joe</name>' >> $(FLOODLOADFLOOD)
	@echo '    <!-- We run the Joe farmer 5 times' >> $(FLOODLOADFLOOD)
	@echo '	 Note that we have the "time" option which indicates for how many' >> $(FLOODLOADFLOOD)
	@echo '	 seconds a farmer should run.  The "time" and "count" elements are' >> $(FLOODLOADFLOOD)
	@echo '	 exclusive.  -->' >> $(FLOODLOADFLOOD)
	@echo '    <count>5</count>' >> $(FLOODLOADFLOOD)
	@echo '    <!-- Farmer Joe uses this profile -->' >> $(FLOODLOADFLOOD)
	@echo '    <useprofile>Example Profile</useprofile>' >> $(FLOODLOADFLOOD)
	@echo '  </farmer>' >> $(FLOODLOADFLOOD)
	@echo '' >> $(FLOODLOADFLOOD)
	@echo '  <!-- A farm contains a bunch of farmers - each farmer is an independent ' >> $(FLOODLOADFLOOD)
	@echo '       worker (i.e. a thread or child process).  -->' >> $(FLOODLOADFLOOD)
	@echo '  <farm>' >> $(FLOODLOADFLOOD)
	@echo '    <!-- We call our farm "Bingo" - note that the farm must be called Bingo' >> $(FLOODLOADFLOOD)
	@echo '	 in the current implementation. -->' >> $(FLOODLOADFLOOD)
	@echo '    <name>Bingo</name>' >> $(FLOODLOADFLOOD)
	@echo '    <!-- We will start 5 farmers named Joe.  We will start 2 farmers every 5 ' >> $(FLOODLOADFLOOD)
	@echo '	 seconds.  -->' >> $(FLOODLOADFLOOD)
	@echo '    <usefarmer count="5" startcount="2" startdelay="5">Joe</usefarmer>' >> $(FLOODLOADFLOOD)
	@echo '  </farm>' >> $(FLOODLOADFLOOD)
	@echo '' >> $(FLOODLOADFLOOD)
	@echo '  <!-- Set the seed to a known value so we can reproduce the same tests.' >> $(FLOODLOADFLOOD)
	@echo '       Flood uses a seeded PRNG - this allows the tests which use random' >> $(FLOODLOADFLOOD)
	@echo '       numbers to be reproduced.  -->' >> $(FLOODLOADFLOOD)
	@echo '  <seed>1</seed>' >> $(FLOODLOADFLOOD)
	@echo '</flood>' >> $(FLOODLOADFLOOD)
	@echo '<!-- That is all folks! -->' >> $(FLOODLOADFLOOD)

floodloaddel::
	-rm $(FLOODLOADFLOOD) $(FLOODLOADWGET)
	
