#file:comb-pms-A1.t
#------------------
# Combined proxy/master/slave test where we ask the proxy to retrieve
# the same doc twice, and test that the results are the same. Assumption
# is that the second retrieval goes through the slave
#
## BROKEN!!!!!!!!!!!!!!!!!!!!!!
## Arno: I can't get the perl crap to contact the proxy as a proxy
## (i.e., send req to proxy with full URI (http://master/) in GET request)
##

use strict;
use warnings FATAL => 'all';

use Apache::Test;
use Apache::TestUtil;
use Apache::TestRequest qw( GET );
use GlobuleTest::Functions;
use threads; 
use Config;
use vars qw( %SIG );

# Check if it returns the right body

sub ok_returns_right_body2
{
    my $file = shift;
    my $res = shift;
    my @lines = split( /^/, $res->content );
    my $count=0;

    open( STREAM, $file ) || die( "cannot open original HTML file" );
    my $flag=1;
    while( <STREAM> )
    {
        if ($lines[$count] ne $_)
        {
            $flag=0;
            last;
        }
        $count++;
    }

    close STREAM;

    # Check if less or more output than file
    if ($#lines+1 != $count)
    {
        $flag=0;
    }

    # Arno: ok $flag == 1, "test name"; appears to be broken, It will complain it was 
    # expecting "test name" :-(
    ok t_cmp( 1, $flag, 'Body same as original file' );

    if ($flag == 0)
    {
        print $res->content;
    }
}

my $proxy_url = 'http://localhost:8056/export/index.html';
my $file = '../master/htdocs/export/index.html';

plan tests => 8;


ok 1;
#
# First retrieval, assuming it is handled by the master
#
my $res = GET $proxy_url, redirect_ok => 0;
ok_returns_http_ok( $res );
ok_returns_right_mimetype( $proxy_url, $res );
ok_returns_right_body2( $file, $res );

exit -1;

#
# Second retrieval, assuming it is redirected to the slave
#
my $res2;
do
{
    print "# Attempt to get redirected to slave\n";
    $res2 = GET $proxy_url, redirect_ok => 0;
    #sleep 1; # No more than 1, funky stuff with long sleeps, hence this code.
} until $res2->code == 302 || $res2->code != 200;
print "# Redirected!\n";
ok t_cmp( 302, $res2->code, "Request to master returns HTTP redirect");
my $slave_url = $res2->headers()->header('Location');

#
# Actual retrieval from slave
#
my $res3 = GET $slave_url, redirect_ok => 0;
ok_returns_http_ok( $res3 );
ok_returns_right_mimetype( $slave_url, $res3 );
ok_returns_right_body2( $file, $res3 );
