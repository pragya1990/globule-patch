TESTPROG:=../t/TEST
TESTOPTS:=-top_dir `pwd` -t_dir `pwd` -verbose -maxclients 15

TESTLOGFILE:=/dev/null

FLOODFILEDIR:=htdocs/flexport
FLOODFILESOURCE:=/dev/urandom
FLOODLOAD:=floodload.xml
PORT:=8057


all:: test

clean::
	-rm core core.* httpd.core.*
	-$(TESTPROG) $(TESTOPTS) -clean
	-rm -r globule
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
	rm -rf globule ; \
	$(TESTPROG) $(TESTOPTS) -postamble "$$POSTAMBLE" $$t 2>&1 | tee -a $(TESTLOGFILE); \
	done ;) \
	done

## For master+slave testing

start::
	@-rm -rf globule
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
	$(MAKE) FLOODFILESET=../floodA-fileset.txt TESTNO=$@ floodtest

floodtest:: 
	@echo "# Generating files for flood test in $(FLOODFILEDIR)"
	$(MAKE) FLOODFILESET=$(FLOODFILESET) floodfilesgen
	@echo "# Generating load definition in $(FLOODLOAD)"
	$(MAKE) FLOODFILESET=$(FLOODFILESET) floodloadgen
	$(MAKE) CONF=conf/$(TESTNO).conf PORT=$(PORT) start
	@echo "# Starting flood test $@"
	$(MAKE) floodrun
	$(MAKE) CONF=conf/$(TESTNO).conf PORT=$(PORT) stop
	@echo "# Deleting load definition"
	$(MAKE) FLOODFILESET=$(FLOODFILESET) floodloaddel
	@echo "# Deleting generated files"
	$(MAKE) FLOODFILESET=$(FLOODFILESET) floodfilesdel

floodrun::
	flood $(FLOODLOAD) | awk '{ print $$1, $$NF, tolower($$6);}'

floodrunendless::
	while true; do flood $(FLOODLOAD) | awk '{ print $$1, $$NF, tolower($$6);}'; done


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
	@echo '<?xml version="1.0"?>' > $(FLOODLOAD)
	@echo '<!DOCTYPE flood SYSTEM "flood.dtd">' >> $(FLOODLOAD)
	@echo '<!-- Hi, I am a flood config file.  They call me "round-robin.xml"' >> $(FLOODLOAD)
	@echo '     I am an example of a "round-robin" configuration.  All of the URLs in' >> $(FLOODLOAD)
	@echo '     the urllist "Test Hosts" will be hit in sequential order.  ' >> $(FLOODLOAD)
	@echo '     Floods syntax is XML based.  ' >> $(FLOODLOAD)
	@echo '     After compiling flood (./configure && make), you can execute me as:' >> $(FLOODLOAD)
	@echo '     ./flood examples/round-robin.xml' >> $(FLOODLOAD)
	@echo '     (The path to me.)' >> $(FLOODLOAD)
	@echo '' >> $(FLOODLOAD)
	@echo '     Flood will then output data to stdout in the format as specified below ' >> $(FLOODLOAD)
	@echo '     (relative_times) for each URL that it hits:' >> $(FLOODLOAD)
	@echo '     998951597871780 1389 57420 7960273 7960282 OK  4 http://www.apache.org/' >> $(FLOODLOAD)
	@echo '     The columns are as follows:' >> $(FLOODLOAD)
	@echo '     - Absolute Time in usec that the request was started by flood' >> $(FLOODLOAD)
	@echo '     - Relative Time in usec (to first column) that it took to open the socket' >> $(FLOODLOAD)
	@echo '       to the specified server' >> $(FLOODLOAD)
	@echo '     - Relative Time in usec (to first column) that it took to write the' >> $(FLOODLOAD)
	@echo '       generated request to the server.' >> $(FLOODLOAD)
	@echo '     - Relative Time in usec (to first column) that it took to read the' >> $(FLOODLOAD)
	@echo '       generated response from the server.' >> $(FLOODLOAD)
	@echo '     - Relative Time in usec (to first column) that it took to close the' >> $(FLOODLOAD)
	@echo '       socket.  On a keepalive socket, it may not close the socket.' >> $(FLOODLOAD)
	@echo '     - OK/FAIL indicates what the verification module (in this case, ' >> $(FLOODLOAD)
	@echo '       verify_200) thought of the request.' >> $(FLOODLOAD)
	@echo '     - The thread/process id of the farmer that made the request.' >> $(FLOODLOAD)
	@echo '     - The URL that was hit (without query strings)' >> $(FLOODLOAD)
	@echo '' >> $(FLOODLOAD)
	@echo '     To get a "nice" summary of the output (as well as an idea of how to' >> $(FLOODLOAD)
	@echo '     further process this info), try out:' >> $(FLOODLOAD)
	@echo '     ./flood examples/round-robin.xml > my-output' >> $(FLOODLOAD)
	@echo '     ./examples/analyze-relative my-output' >> $(FLOODLOAD)
	@echo '     ' >> $(FLOODLOAD)
	@echo '-->' >> $(FLOODLOAD)
	@echo '<flood configversion="1">' >> $(FLOODLOAD)
	@echo '  <!-- A urllist describes which hosts and which methods we want to hit. -->' >> $(FLOODLOAD)
	@echo '  <urllist>' >> $(FLOODLOAD)
	@echo '    <name>Test Hosts</name>' >> $(FLOODLOAD)
	@echo '    <description>A bunch of hosts we want to hit</description>' >> $(FLOODLOAD)
	@echo '    <!-- We first send a POST request to search.apache.org looking for "foo" ' >> $(FLOODLOAD)
	@echo '	 Notice the payload attribute - this allows us to specify the POST' >> $(FLOODLOAD)
	@echo '	 content.  -->' >> $(FLOODLOAD)
	@cat $(FLOODFILESET) | gawk -v PORT=$(PORT) -v HTDOCS=`basename $(FLOODFILEDIR)` '{printf( "<url>http://localhost:%s/%s/%s</url>\n", PORT, HTDOCS, $$1 );}' >> $(FLOODLOAD)
	@echo '</urllist>' >> $(FLOODLOAD)
	@echo '' >> $(FLOODLOAD)
	@echo '  <!-- The profile describes how we will hit the urllists.  For this' >> $(FLOODLOAD)
	@echo '       example, we execute with the round_robin profile.' >> $(FLOODLOAD)
	@echo '       Round robin runs all of the URLs in the urllist in order once. -->' >> $(FLOODLOAD)
	@echo '  <profile>' >> $(FLOODLOAD)
	@echo '    <name>Example Profile</name>' >> $(FLOODLOAD)
	@echo '    <description>A Test Round Robin Configuration</description>' >> $(FLOODLOAD)
	@echo '' >> $(FLOODLOAD)
	@echo '    <!-- See above.  This indicates which URLs we will hit.  -->' >> $(FLOODLOAD)
	@echo '    <useurllist>Test Hosts</useurllist>' >> $(FLOODLOAD)
	@echo '' >> $(FLOODLOAD)
	@echo '    <!-- Specifies that we will use round_robin profile logic -->' >> $(FLOODLOAD)
	@echo '    <profiletype>round_robin</profiletype>' >> $(FLOODLOAD)
	@echo '    <!-- Specifies that we will use generic socket logic ' >> $(FLOODLOAD)
	@echo '	 We also have "keepalive" as an option - this option indicates that' >> $(FLOODLOAD)
	@echo '	 we should attempt to use HTTP Keepalive when available.  -->' >> $(FLOODLOAD)
	@echo '    <socket>generic</socket>' >> $(FLOODLOAD)
	@echo '    <!-- Specifies that we will use verify_200 for response verification ' >> $(FLOODLOAD)
	@echo '	 This verification step ensures that we received a 2xx or 3xx' >> $(FLOODLOAD)
	@echo '	 status code from the server.  -->' >> $(FLOODLOAD)
	@echo '    <verify_resp>verify_200</verify_resp>' >> $(FLOODLOAD)
	@echo '    <!-- Specifies that we will use the "relative_times" report generation ' >> $(FLOODLOAD)
	@echo '	 We also have "easy" - this option is similar to "relative_times",' >> $(FLOODLOAD)
	@echo '	 but the times posted are absolute instead of relative to the start' >> $(FLOODLOAD)
	@echo '	 of the request.  -->' >> $(FLOODLOAD)
	@echo '    <report>relative_times</report>' >> $(FLOODLOAD)
	@echo '' >> $(FLOODLOAD)
	@echo '  </profile>' >> $(FLOODLOAD)
	@echo '' >> $(FLOODLOAD)
	@echo '  <!-- A farmer runs one profile a certain number of times.  This farmer is' >> $(FLOODLOAD)
	@echo '       called Joe.  -->' >> $(FLOODLOAD)
	@echo '  <farmer>' >> $(FLOODLOAD)
	@echo '    <name>Joe</name>' >> $(FLOODLOAD)
	@echo '    <!-- We run the Joe farmer 5 times' >> $(FLOODLOAD)
	@echo '	 Note that we have the "time" option which indicates for how many' >> $(FLOODLOAD)
	@echo '	 seconds a farmer should run.  The "time" and "count" elements are' >> $(FLOODLOAD)
	@echo '	 exclusive.  -->' >> $(FLOODLOAD)
	@echo '    <count>5</count>' >> $(FLOODLOAD)
	@echo '    <!-- Farmer Joe uses this profile -->' >> $(FLOODLOAD)
	@echo '    <useprofile>Example Profile</useprofile>' >> $(FLOODLOAD)
	@echo '  </farmer>' >> $(FLOODLOAD)
	@echo '' >> $(FLOODLOAD)
	@echo '  <!-- A farm contains a bunch of farmers - each farmer is an independent ' >> $(FLOODLOAD)
	@echo '       worker (i.e. a thread or child process).  -->' >> $(FLOODLOAD)
	@echo '  <farm>' >> $(FLOODLOAD)
	@echo '    <!-- We call our farm "Bingo" - note that the farm must be called Bingo' >> $(FLOODLOAD)
	@echo '	 in the current implementation. -->' >> $(FLOODLOAD)
	@echo '    <name>Bingo</name>' >> $(FLOODLOAD)
	@echo '    <!-- We will start 5 farmers named Joe.  We will start 2 farmers every 5 ' >> $(FLOODLOAD)
	@echo '	 seconds.  -->' >> $(FLOODLOAD)
	@echo '    <usefarmer count="5" startcount="2" startdelay="5">Joe</usefarmer>' >> $(FLOODLOAD)
	@echo '  </farm>' >> $(FLOODLOAD)
	@echo '' >> $(FLOODLOAD)
	@echo '  <!-- Set the seed to a known value so we can reproduce the same tests.' >> $(FLOODLOAD)
	@echo '       Flood uses a seeded PRNG - this allows the tests which use random' >> $(FLOODLOAD)
	@echo '       numbers to be reproduced.  -->' >> $(FLOODLOAD)
	@echo '  <seed>1</seed>' >> $(FLOODLOAD)
	@echo '</flood>' >> $(FLOODLOAD)
	@echo '<!-- That is all folks! -->' >> $(FLOODLOAD)

floodloaddel::
	-rm $(FLOODLOAD)
	
