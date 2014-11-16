#file:getDIGESTS5.t
#--------------------
# Test if a master with one slave responds to a GETDIGESTS for a known
# slave and known URL correctly.
#
#

use strict;
use warnings FATAL => 'all';

require MIME::Base64;
use Apache::Test;
use Apache::TestUtil;
use Apache::TestRequest qw( GET );
use GlobuleTest::Functions;

my $master_uri_path = '/export/index.html';
my $slaveid = 'localhost:8057';
my $slave_uri_path = '/3rdparty/poolname/index.html';
my $userid = 'w8w00rd';
my $passwd = 'w8w00rd';

my $from_replica_header = 'http://'.$slaveid.$slave_uri_path;
my $auth_header = create_auth( $userid, $passwd );

my $query = $slaveid.'#'.$slave_uri_path;
my $base64_query = MIME::Base64::encode( $query, "" );
my $url = '/psodium/getDigests?'.$base64_query;

plan tests => 4;

ok 1; # simple load test

#
# 1. Act like a slave and retrieve a document from the master
#
my $res2 = GET $master_uri_path, redirect_ok => 0, 'X-From-Replica' => $from_replica_header, Authorization => $auth_header;
ok t_cmp( 200, $res2->code, "Master returned 200 to emulated slave retrieval" );

#
# 2. Act like a client and do a GETDIGESTS
#
my $res = GET $url, redirect_ok => 0;

ok t_cmp( 200, $res->code, "Master returned 200 to GETDIGESTS" );
my $line;
my @lines = split( /^/, $res->content );
my $flag = 0;
foreach $line (@lines)
{
    #print "LINE ",$line,"\n";
    if ($line =~ /Count: 1/)
    {
        $flag=1;
    }
}
ok t_cmp( 1, $flag, "Master returned 1 digests" );