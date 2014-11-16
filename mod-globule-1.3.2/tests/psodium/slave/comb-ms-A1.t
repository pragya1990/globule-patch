#file:comb-ms-A1.t
#-----------------
# Combined test to see if a single slave with a real master returns a pledge
# that checks out and passes the double check test with the master.
#
# TODO: crypto checks of pledge (single server test, really)
#

use strict;
use warnings FATAL => 'all';

require MIME::Base64;
use Apache::Test;
use Apache::TestUtil;
use Apache::TestRequest qw( GET );
use GlobuleTest::Functions;

my $master_uri_path = '/export/index.html';
my $slaveid = 'localhost.localdomain:8057';
my $slave_uri_path = '/3rdparty/poolname/index.html';
my $userid = 'w8w00rd';
my $passwd = 'w8w00rd';

my $from_replica_header = 'http://'.$slaveid.$slave_uri_path;
my $auth_header = create_auth( $userid, $passwd );

my $query = $slaveid.'#'.$slave_uri_path;
my $base64_query = MIME::Base64::encode( $query, "" );
my $url = '/psodium/getDigests?'.$base64_query;
my $master_url = 'http://localhost:8056'.$url;

plan tests => 6;

ok 1; # simple load test
my $res = GET $slave_uri_path, 'X-pSodium-Want-Pledge' => 'True';
ok t_cmp( 200, $res->code, "Slave returned 200 to GET" );
my $line;
my @lines = split( /^/, $res->content );
my $flag = 0;
my $slave_base64_digest;
foreach $line (@lines)
{
    #print "LINE ",$line,"\n";
    if ($line =~ /DIGEST/)
    {
    	my @words = split( /\s/, $line );
    	$slave_base64_digest = $words[1];
	$flag=1;
    }
}
ok t_cmp( 1, $flag, "Slave returned pledge" );
	
my $res2 = GET $master_url;
ok t_cmp( 200, $res2->code, "Master returned 200 to GETDIGESTS" );
my $line2;
my @lines2 = split( /^/, $res2->content );
my $flag2 = 0;
my $master_base64_digest;
foreach $line2 (@lines2)
{
    # print "LINE ",$line2,"\n";
    if ($line2 =~ /Count: 1/)
    {
	$flag2=1;
    }
    if (!($line2 =~ /END DIGESTS/))
    {
	($master_base64_digest ) = $line2 =~ /^(.*)\r\n$/;
    }
}
ok t_cmp( 1, $flag2, "Master returned 1 digests" );
ok t_cmp( $slave_base64_digest, $master_base64_digest, "Digest from slave equals digest from master" );
