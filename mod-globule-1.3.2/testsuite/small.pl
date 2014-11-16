#!/usr/bin/perl -w
use TestFunc qw($WIN32 $CWD $APACHE $SERVER $ORIGIN $REPLICA $REDIR $BACKUP
                $VHOST $ROLE $EXPDIR $IMPDIR $HOSTNAME $PORT $BASEDIR $PASSWORD
                $CONTAINS init_servers init_origin_docs start_server
                stop_server get_doc get_doc_full update_doc flood_test
                my_exit my_sleep);

# Holds all your servers as array of hash references
# Place in the array determines directory use (e.g server[0] ==> /server0/..)
my @servers;

# Cleaning up
`rm -rf server?`;

# Let's try three Apache servers
my $server0 = {};
my $server1 = {};
my $server2 = {};

# Push them on the servers list
push @servers, $server0;
push @servers, $server1;
push @servers, $server2;

# Let's set 2 origins, role MUST be set to $ORIGIN
my $origin0 = {$ROLE => $ORIGIN};
my $origin1 = {$ROLE => $ORIGIN};

# And 4 replicas, role MUST be set to $REPLICA
my $replica0 = {$ROLE => $REPLICA};
my $replica1 = {$ROLE => $REPLICA};
my $replica2 = {$ROLE => $REPLICA};
my $replica3 = {$ROLE => $REPLICA};

# NOTE: make sure that origin and its replica are NOT on the same server
#       AND replicas of one origin may NOT be on the same server
# server0 contains origin0, replica0
$server0->{$CONTAINS} = [$origin0, $replica0];
$origin0->{$SERVER} = $server0;
$replica0->{$SERVER} = $server0;

# server1 contains origin1, replica1
$server1->{$CONTAINS} = [$origin1, $replica1];
$origin1->{$SERVER} = $server1;
$replica1->{$SERVER} = $server1;

# server2 contains replica2, replica3
$server2->{$CONTAINS} = [$replica2, $replica3];
$replica2->{$SERVER} = $server2;
$replica3->{$SERVER} = $server2;

# origin0 has replica1, replica2
$origin0->{$REPLICA} = [$replica1, $replica2];
$replica1->{$ORIGIN} = $origin0;
$replica2->{$ORIGIN} = $origin0;

# origin1 => (replica0, replica3)
$origin1->{$REPLICA} = [$replica0, $replica3];
$replica0->{$ORIGIN} = $origin1;
$replica3->{$ORIGIN} = $origin1;

# Master export directories, make sure of leading and trailing slash
$origin0->{$EXPDIR} = "/export/";  
$origin1->{$EXPDIR} = "/export999/"; 

# Slave import directories, make sure of leading and trailing slash
$replica0->{$IMPDIR} = "/import/";
$replica1->{$IMPDIR} = "/import1/";
$replica2->{$IMPDIR} = "/import2/";
$replica3->{$IMPDIR} = "/import3/";


###############################################################################
# DEFINE HERE WHAT TEST TO RUN/DOCS TO GET

# This will create the apache config files for you
init_servers(\@servers);

# Creates random sized documents in the origin->{$EXPDIR}
init_origin_docs($origin0);
init_origin_docs($origin1);

# Start servers
start_server($server0);
start_server($server1);
start_server($server2);

# Sleep long enough for heartbeats to get through
my_sleep(60);

print "Getting origin 1.html...\n";
get_doc($origin0, "1.html");

print "Getting replica1 1.html...\n";
get_doc($replica1, "1.html");

get_doc($replica2, "1.html");

=cut
# Some manual tests
get_doc($origin0, "1.html");
get_doc($replica0,  "1.html");
get_doc($replica2,  "1.html", 200);
get_doc($origin0, "1.html");

# See if invalidates are coming through
# Update docs
update_doc($origin0, "1.html");
update_doc($origin1, "1.html");

# Sleep long enough for invalidate to get through
my_sleep(10);

# Check that replicas have new document
get_doc($origin0, "1.html");
get_doc($replica2,  "1.html");
get_doc($replica0,  "1.html");

# Does checkreplica get through, and are we redirected to them
my_sleep(10);

# Check that we are redirected
get_doc($origin0, "1.html");
get_doc($origin0, "1.html");
get_doc($origin0, "1.html");
get_doc($origin0, "1.html");

# Do some test with escaped characters
get_doc_full($origin0, "/ex%70%6Frt/1.html");
get_doc_full($replica2,  "/im%70%6Frt2/1.html");
get_doc_full($origin0, "/ex%70%6Frt/1.html");
get_doc_full($replica2,  "/im%70%6Frt2/1.html");

# More redirect test on other origin
get_doc($origin1, "1.html");
get_doc($origin1, "1.html");
get_doc($origin1, "1.html");
get_doc($origin1, "1.html");

# Start floodtest, just add a origin/replica multiple times for multiple threads
# Docs are NOT checked with original, just if they return 200
#flood_test($origin0, $origin0, $origin1, $replica0, 
#           $replica1, $replica2, $replica2, $replica3);

=cut

# Stop all servers
my_exit(\@servers);

