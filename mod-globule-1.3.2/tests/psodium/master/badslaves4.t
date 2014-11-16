#file:badslaves3.t
#--------------------
# Test if a master with one slave responds to a "PUT /psodium/addBADSLAVES"
# with a non-chunked encoded, correct body correctly.
#
#

use strict;
use warnings FATAL => 'all';

use Apache::Test;
use Apache::TestUtil;
use Apache::TestRequest qw( GET PUT );
use GlobuleTest::Functions;

my $badslave_url = '/psodium/addBADSLAVES';
my $userid = 'auditor';
my $passwd = 'w8w00rd';
my $auth_header = create_auth( $userid, $passwd );

my $content = "----- BEGIN IPADDRPORTVEC -----\r\nIPADDRPORTNO: localhost:8057\r\n----- END IPADDRPORTVEC -----\r\n";

my $get_url = '/export/index.html';

plan tests => 3;

ok 1; # simple load test
my $res = PUT $badslave_url, content => $content, Authorization => $auth_header;
ok t_cmp( 200, $res->code, "Master returned 200 to ADDBADSLAVES" );

print "# Sleeping 60 secs, so master will redirect";
sleep 60;

my $res2 = GET $get_url, redirect_ok => 0;
my $res3 = GET $get_url, redirect_ok => 0;

if ($res2->code == 302 || $res3->code == 302)
{
    if ($res2->code == 302)
    {
        print "# Redirect to ",$res2->headers()->header('Location'),"\n";    
    }
    else
    {
        print "# Redirect to ",$res3->headers()->header('Location'),"\n";    
    }
    ok t_cmp( 200, 302, "Master returned 200 to ADDBADSLAVES" );
}
else
{
    ok 3;
}
