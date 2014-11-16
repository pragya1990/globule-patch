#file:getDIGESTS3.t
#--------------------
# Test if a master with one slave responds to a GETDIGESTS for an unknown
# slave correctly.
#
#

use strict;
use warnings FATAL => 'all';

require MIME::Base64;
use Apache::Test;
use Apache::TestUtil;
use Apache::TestRequest qw( GET );

my $slaveid = 'bromet.cs.vu.nl:3001';
my $slave_uri_path = '/3rdparty/poolname/index.html';
my $query = $slaveid.'#'.$slave_uri_path;

my $base64_query = MIME::Base64::encode( $query, "" );
my $url = '/psodium/getDigests?'.$base64_query;

plan tests => 3;

ok 1; # simple load test
my $res = GET $url, redirect_ok => 0;
ok t_cmp( 200, $res->code, "Master returned 200 to GETDIGESTS" );
my $line;
my @lines = split( /^/, $res->content );
my $flag = 0;
foreach $line (@lines)
{
    # print "LINE ",$line,"\n";
    if ($line =~ /Count: 0/)
    {
        $flag=1;
    }
}
ok t_cmp( 1, $flag, "Master returned zero digests" );