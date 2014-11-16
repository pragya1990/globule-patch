#!/usr/bin/perl -w

use strict;
use TestFunc qw($WIN32 $CWD $APACHE $SERVER $ORIGIN $REPLICA $REDIR $BACKUP
                $VHOST $ROLE $EXPDIR $IMPDIR $HOSTNAME $PORT $BASEDIR $PASSWORD
                $CONTAINS init_servers init_origin_docs generate_doc
		start_server stop_server get_doc get_doc_full update_doc
		flood_test my_exit my_sleep @my_hostnames );

# Holds all your servers as array of hash references
# Place in the array determines directory use (e.g server[0] ==> /server0/..)
my @servers;

# Cleaning up
`rm -rf server?`;

##############################################################################
# DEFINE YOUR TEST SETUP HERE

chomp(my $hostname = `hostname`);
push @my_f(!($hostname =~ m/\./)) { chomp($hostname .= "." . `domainname`); }

hostnames, $hostname;

my $server0 = { $HOSTNAME => $hostname };
my $server1 = { $HOSTNAME => $hostname };

push @servers, $server0;
push @servers, $server1;

my $origin0 = {$ROLE => $ORIGIN};
my $replica0 = {$ROLE => $REPLICA};

$server0->{$CONTAINS} = [$origin0];
$origin0->{$SERVER}   = $server0;

$server1->{$CONTAINS} = [$replica0];
$replica0->{$SERVER}  = $server1;

$origin0->{$REPLICA}  = [$replica0];
$replica0->{$ORIGIN}  = $origin0;

$origin0->{$EXPDIR}  = "/";  
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

my $url; my $i;
$url = "/";
for($i=0; $i<256; $i++) {
  $url .= "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
};
$url .= ".html";
get_doc_full($origin0, $url);
get_doc_full($origin0, $url);
get_doc_full($replica0, $url);

$url = "";
for($i=0; $i<256; $i++) {
  $url .= "/x/x/x/x/x/x/x/x/x/x/x/x/x/x/x/x/x/x/x/x/x/x/x/x/x/x/x/x/x/x/x/x";
};
$url .= ".html";
get_doc_full($origin0, $url);
get_doc_full($origin0, $url);
get_doc_full($replica0, $url);

my_sleep(15);

stop_server($server0);
stop_server($server1);

my_exit(\@servers);
