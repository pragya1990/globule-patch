#file:getmodif3.t
#--------------------
# Test if a master with one slave responds to a correct GETMODIFICATIONS 
#
# There is no getmodif4.t, as the UPDATEEXPIRY test set already includes one
# good GETMODIFCATIONS test.
#

use strict;
use warnings FATAL => 'all';

require MIME::Base64;
use Apache::Test;
use Apache::TestUtil;
use Apache::TestRequest qw( GET );
use GlobuleTest::Functions;


my $currenttime=time;
my $times=$currenttime.'000000'; # APR times are in microseconds, multiply gives 64-bit trouble
my $url = '/psodium/getMODIFICATIONS?'.$times;
my $userid = 'auditor';
my $passwd = 'w8w00rd';
my $auth_header = create_auth( $userid, $passwd );

plan tests => 3;

ok 1; # simple load test
my $res = GET $url, redirect_ok => 0, Authorization => $auth_header;
ok t_cmp( 200, $res->code, "Master returned 200 to GETMODIFICATIONS" );
my $line;
my @lines = split( /^/, $res->content );
my $flag = 0;
foreach $line (@lines)
{
    print "LINE ",$line,"\n";
    if ($line =~ /VERSIONS/)
    {
        $flag=1;
    }
}
ok t_cmp( 1, $flag, "Master returned MODIFICATIONS" );