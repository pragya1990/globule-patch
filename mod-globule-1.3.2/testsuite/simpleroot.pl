#!/usr/bin/perl -w

use strict;
use TestFunc qw($WIN32 $CWD $APACHE $SERVER $ORIGIN $REPLICA $REDIR $BACKUP
                $VHOST $ROLE $EXPDIR $IMPDIR $HOSTNAME $PORT $BASEDIR $PASSWORD
                $CONTAINS init_servers init_origin_docs start_server
                stop_server get_doc get_doc_full update_doc flood_test
                my_exit my_sleep @my_hostnames);

# Holds all your servers as array of hash references
# Place in the array determines directory use (e.g server[0] ==> /server0/..)
my @servers;

$| = 1;

# Cleaning up
`rm -rf server?`;


##############################################################################
# DEFINE YOUR TEST SETUP HERE

chomp(my $hostname = `hostname`);
if(!($hostname =~ m/\./)) { chomp($hostname .= "." . `domainname`); }

# Let's try two Apache servers
my $server0 = { $HOSTNAME => $hostname };
my $server1 = { $HOSTNAME => "globule.revolutionware.net" };

# Push them on the servers list
push @my_hostnames, $hostname;
push @my_hostnames, "globule.revolutionware.net";
push @servers, $server0;
push @servers, $server1;

# Let's set 1 origin, role MUST be set to $ORIGIN
my $origin0 = {$ROLE => $ORIGIN};

# And 1 replica, role MUST be set to $REPLICA
my $replica0 = {$ROLE => $REPLICA};

# NOTE: make sure that origin and its replica are NOT on the same server
#       AND replicas of one origin may NOT be on the same server
$server0->{$CONTAINS} = [$origin0];
$origin0->{$SERVER} = $server0;

# server1 contains replica0
$server1->{$CONTAINS} = [$replica0];
$replica0->{$SERVER} = $server1;

# origin0 has replica0
$origin0->{$REPLICA} = [$replica0];
$replica0->{$ORIGIN} = $origin0;

# Master export directories, make sure of leading and trailing slash
$origin0->{$EXPDIR} = "/";

# Slave import directories, make sure of leading and trailing slash
$replica0->{$IMPDIR} = "/";


###############################################################################
# DEFINE HERE WHAT TEST TO RUN/DOCS TO GET

# This will create the apache config files for you
init_servers(\@servers);

# Creates random sized documents in the origin->{$EXPDIR}
init_origin_docs($origin0);

# Start servers
start_server($server0);
start_server($server1);

# Sleep long enough for heartbeats to get through
my_sleep(60);

print "Getting origin 1.html...\n";
get_doc($origin0, "1.html");

print "Getting replica0 1.html...\n";
get_doc($replica0, "1.html");

# Some manual tests
get_doc_full($origin0, "/index.html");
get_doc($origin0, "1.html");
get_doc($replica0,  "1.html");
get_doc($origin0, "1.html");

# See if invalidates are coming through
# Update docs
print "Updating 1.html\n";
update_doc($origin0, "1.html");

# Sleep long enough for invalidate to get through
my_sleep(10);

# Check that replicas have new document
get_doc($origin0, "1.html");
get_doc($replica0,  "1.html");

# Does checkreplica get through, and are we redirected to them
my_sleep(10);

# Check that we are redirected
get_doc($origin0, "1.html");
get_doc($origin0, "1.html");
get_doc($origin0, "1.html");
get_doc($origin0, "1.html");

# Do some test with escaped characters
#get_doc_full($origin0, "/ex%70%6Frt/1.html");
#get_doc_full($replica2,  "/im%70%6Frt2/1.html");
#get_doc_full($origin0, "/ex%70%6Frt/1.html");
#get_doc_full($replica2,  "/im%70%6Frt2/1.html");

# Stop all servers
my_exit(\@servers);
