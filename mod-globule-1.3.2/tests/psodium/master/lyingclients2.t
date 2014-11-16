#file:lyingclient2.t
#--------------------
# Test if a master with one slave responds to a "PUT /psodium/addLYINGCLIENTS"
# with a non-chunked encoded, incorrect body correctly.
#
#

use strict;
use warnings FATAL => 'all';

use Apache::Test;
use Apache::TestUtil;
use Apache::TestRequest qw( PUT );
use GlobuleTest::Functions;

my $url = '/psodium/addLYINGCLIENTS';
my $userid = 'auditor';
my $passwd = 'w8w00rd';
my $auth_header = create_auth( $userid, $passwd );

my $content = "----- BEGIN IPADDRPORTVEC -----\r\nIPADDRPORTNO: 231.231.231.231,123\r\n----- END IPADDRPORTVEC -----\r\n";

plan tests => 2;

ok 1; # simple load test
my $res = PUT $url, content => $content, Authorization => $auth_header;
ok t_cmp( 400, $res->code, "Master returned 400 to ADDLYINGCLIENTS" );
