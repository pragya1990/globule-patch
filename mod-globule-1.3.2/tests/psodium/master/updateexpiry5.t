#file:updateexpiry5.t
#--------------------
# Test if a master with one slave responds to a correct UPDATEEXPIRY.
#

use strict;
use warnings FATAL => 'all';

require MIME::Base64;
use Apache::Test;
use Apache::TestUtil;
use Apache::TestRequest qw( GET );
use GlobuleTest::Functions;

my $master_uri_path = '/export/index.html';

my $currenttime=time;
my $times=$currenttime.'000000'; # APR times are in microseconds, multiply gives 64-bit trouble
my $slaveid = 'localhost:8057';
my $slave_uri_path = '/3rdparty/poolname/index.html';
my $from_replica_header = 'http://'.$slaveid.$slave_uri_path;

my $update_url = '/psodium/updateExpiry?'.$times.$from_replica_header;
my $userid = 'w8w00rd';
my $passwd = 'w8w00rd';
my $auth_header = create_auth( $userid, $passwd );
my $auditor_auth_header = create_auth( "auditor", $passwd );


my $getmodif_url = '/psodium/getMODIFICATIONS?'.$times;

plan tests => 5;

ok 1; # simple load test

#
# 1. Act like a slave and retrieve a document from the master
#
my $res1 = GET $master_uri_path, redirect_ok => 0, 'X-From-Replica' => $from_replica_header, Authorization => $auth_header;
ok t_cmp( 200, $res1->code, "Master returned 200 to emulated slave retrieval" );

#
# 2. Act like a Globule master module talking to the pSodium master module
#

my $res2 = GET $update_url, redirect_ok => 0, Authorization => $auth_header;
ok t_cmp( 200, $res2->code, "Master returned 200 to UPDATEEXPIRY" );


#
# 3. To check the update, act like an auditor and do a GETMODIFICATIONS
#
my $res3 = GET $getmodif_url, redirect_ok => 0, Authorization => $auditor_auth_header;

ok t_cmp( 200, $res3->code, "Master returned 200 to GETMODIFICATIONS" );
my $line;
my @lines = split( /^/, $res3->content );
my $flag = 0;
foreach $line (@lines)
{
    print "LINE ",$line,"\n";
    if ($line =~ /EXPIRYDIGEST/)
    {
        my @words = split( /\s/, $line );
        if ($words[1] eq $times)
        {
            $flag=1;
        }
    }
}
ok t_cmp( 1, $flag, "Master updated expiry time for digest" );
