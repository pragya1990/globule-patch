#!/usr/bin/perl -w

use strict;
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

# Let's set 1 origin, role MUST be set to $ORIGIN
my $origin0 = {$ROLE => $ORIGIN};

# And 1 replicas, role MUST be set to $REPLICA
my $replica0 = {$ROLE => $REPLICA};

# And 1 redirector, role MUST be set to $REDIR
my $redir0 = {$ROLE => $REDIR};


# NOTE: make sure that origin and its replica are NOT on the same server
#       AND replicas of one origin may NOT be on the same server
# server0 contains origin0
$server0->{$CONTAINS} = [$origin0];
$origin0->{$SERVER} = $server0;

# server1 contains replica0
$server1->{$CONTAINS} = [$replica0];
$replica0->{$SERVER} = $server1;

# server2 contains $redir0
$server2->{$CONTAINS} = [$redir0];
$redir0->{$SERVER} = $server2;

# origin0 has replica0
$origin0->{$REPLICA} = [$replica0];
$replica0->{$ORIGIN} = $origin0;

# origin0 has redirector0
$origin0->{$REDIR} = [$redir0];
$redir0->{$ORIGIN} = $origin0;

# Master export directories, make sure of leading and trailing slash
$origin0->{$EXPDIR} = "/export/";  

# Slave import directories, make sure of leading and trailing slash
$replica0->{$IMPDIR} = "/import/";

# Redirector import dir
$redir0->{$IMPDIR} = $redir0->{$ORIGIN}{$EXPDIR};



###############################################################################
# DEFINE HERE WHAT TEST TO RUN/DOCS TO GET

# This will create the apache config files for you
init_servers(\@servers);

# Creates random sized documents in the origin->{$EXPDIR}
init_origin_docs($origin0);

# Start servers
start_server($server0);
my_sleep(60);
start_server($server1);
start_server($server2);

# Sleep long enough for heartbeats to get through
my_sleep(60);

print "Gettting index.html from origin 0\n";
get_doc_full($origin0, "/index.html");

print "Getting origin 1.html...\n";
get_doc($origin0, "1.html");

print "Getting replica0 1.html...\n";
get_doc($replica0, "1.html");

# Check that we are redirected
print "Checking redirection...\n";
get_doc($redir0, "1.html");
get_doc($redir0, "1.html");
get_doc($redir0, "1.html");
get_doc($redir0, "1.html");
get_doc($redir0, "1.html");
get_doc($redir0, "1.html");
get_doc($redir0, "1.html");
get_doc($redir0, "1.html");
get_doc($redir0, "1.html");
get_doc($redir0, "1.html");
get_doc($redir0, "1.html");
get_doc($redir0, "1.html");
get_doc($redir0, "1.html");
get_doc($redir0, "1.html");
get_doc($redir0, "1.html");
get_doc($redir0, "1.html");


# Stop all servers
my_exit(\@servers);

