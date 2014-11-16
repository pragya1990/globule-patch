#file:badpledge4.t
#-----------------
# Test if a master with one slave responds to a BADPLEDGE with a slave id
# but a partial, corrupt pledge correctly.
#
#

use strict;
use warnings FATAL => 'all';

require MIME::Base64;
use Apache::Test;
use Apache::TestUtil;
use Apache::TestRequest qw( GET );

my $slaveid = 'gromet.cs.vu.nl:3001';
my $pledge = "-----BEGIN PLEDGE-----\r\nSLAVID comet.cs.vu.nl:37001\r\n";

my $base64_pledge = MIME::Base64::encode( $pledge, "" );
my $query = $slaveid.'#'.$base64_pledge;
my $base64_query = MIME::Base64::encode( $query, "" );
my $url = '/psodium/badPledge?'.$base64_query;

plan tests => 2;

ok 1; # simple load test
my $res = GET $url, redirect_ok => 0;
ok t_cmp( 502, $res->code, "Master returned 502 to BADPLEDGE" );

my $line;
my @lines = split( /^/, $res->content );
my $flag = 0;
foreach $line (@lines)
{
    print "## ",$line,"\n";
    if ($line =~ /Count: 0/)
    {
        $flag=1;
    }
}
