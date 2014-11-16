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

# And 1 backup, role MUST be set to $BACKUP
my $backup0 = {$ROLE => $BACKUP};


# NOTE: make sure that origin and its replica are NOT on the same server
#       AND replicas of one origin may NOT be on the same server
# server0 contains origin0
$server0->{$CONTAINS} = [$origin0];
$origin0->{$SERVER} = $server0;

# server1 contains replica0
$server1->{$CONTAINS} = [$replica0];
$replica0->{$SERVER} = $server1;

# server2 contains $backup0
$server2->{$CONTAINS} = [$backup0];
$backup0->{$SERVER} = $server2;

# origin0 has replica0
$origin0->{$REPLICA} = [$replica0];
$replica0->{$ORIGIN} = $origin0;

# origin0 has backup0
$origin0->{$BACKUP} = [$backup0];
$backup0->{$ORIGIN} = $origin0;

# Master export directories, make sure of leading and trailing slash
$origin0->{$EXPDIR} = "/export/";  

# Slave import directories, make sure of leading and trailing slash
$replica0->{$IMPDIR} = "/import/";

# Backup import dir
$backup0->{$IMPDIR} = $backup0->{$ORIGIN}{$EXPDIR};



###############################################################################
# DEFINE HERE WHAT TEST TO RUN/DOCS TO GET

# This will create the apache config files for you
init_servers(\@servers);

# Creates random sized documents in the origin->{$EXPDIR}
init_origin_docs($origin0);

# Start servers
start_server($server0);
#start_server($server1);
#start_server($server2);

# Sleep long enough for heartbeats to get through
my_sleep(40);

# Stop all servers
my_exit(\@servers);

