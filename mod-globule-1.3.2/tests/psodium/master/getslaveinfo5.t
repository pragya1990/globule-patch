#file:getslaveinfo5.t
#--------------------
# Test if a master with one slave responds to a GETSLAVEINFO for that slave
# correctly
#

use strict;
use warnings FATAL => 'all';

require MIME::Base64;
use Apache::Test;
use Apache::TestUtil;
use Apache::TestRequest qw( GET );

my $slaveid = 'gromet.cs.vu.nl:3001';
my $shouldreturnkey=1;

my $base64_slaveid = MIME::Base64::encode( $slaveid, "" );
my $url = '/psodium/getSlaveInfo?'.$base64_slaveid;

plan tests => 3;

ok 1; # simple load test
my $res = GET $url, redirect_ok => 0;
ok t_cmp( 200, $res->code, "Master returned 200 to GETSLAVEINFO" );
my $line;
my @lines = split( /^/, $res->content );
my $flag = 0;
foreach $line (@lines)
{
    # print "LINE ",$line,"\n";
    if ($line =~ /BEGIN CERTIFICATE/)
    {
        $flag=1;
    }
}
ok t_cmp( $shouldreturnkey, $flag, "Master returned slave's public key" );
