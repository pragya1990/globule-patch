#file:badpledge5.t
#-----------------
# Test if a master with one slave responds to a false BADPLEDGE
# report (=pledge is good) from a client (which is therefore lying)
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

my $queryslaveid = 'localhost:8057';
my $pledge = "-----BEGIN PLEDGE-----\r\nSLAVEID localhost:8057\r\nREQUEST_URI /3rdparty/poolname/index.html\r\nREQUEST_HOST localhost:8057\r\nTIMESTAMP 1089813099535030\r\nDIGEST hMtwHpYikd/6NKdK2I1rmShNuAg=\r\nSIGNATURE eas19gq78Ertqeqkm3vfnUTgaIEomdPYq2rXQJsC86htdCZXmSM/YvoUZbHQDmNk5mEwhY8Ygan3Yu5iRSs0qYVNU9FtlpH2opcA92Ze4haJQtn4nd3hyLTBRpo4XJgjI53xhmqJiUraEEV4q34VFfUUwzsujrEjxwLqU1PzX2A=\r\n-----END PLEDGE-----\r\n";

my $base64_pledge = MIME::Base64::encode( $pledge, "" );
my $query = $queryslaveid.'#'.$base64_pledge;
my $base64_query = MIME::Base64::encode( $query, "" );
my $url = '/psodium/badPledge?'.$base64_query;
my $from_replica_header = 'http://'.$slaveid.$slave_uri_path;
my $auth_header = create_auth( $userid, $passwd );

plan tests => 4;

ok 1; # simple load test

#
# 1. Act like a slave and retrieve a document from the master
#
my $res2 = GET $master_uri_path, redirect_ok => 0, 'X-From-Replica' => $from_replica_header, Authorization => $auth_header;
ok t_cmp( 200, $res2->code, "Master returned 200 to emulated slave retrieval" );

#
# 2. Act like a client and send a BADPLEDGE to the master
#

my $res = GET $url, redirect_ok => 0;
ok t_cmp( 200, $res->code, "Master returned 200 to BADPLEDGE" );

my $line;
my @lines = split( /^/, $res->content );
my $flag = 0;
foreach $line (@lines)
{
    print "## ",$line,"\n";
    if ($line =~ /liar/)
    {
        $flag=1;
    }
}
ok t_cmp( 1, $flag, "Client was marked as liar" );