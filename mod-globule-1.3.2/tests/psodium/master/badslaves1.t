#file:badslaves1.t
#--------------------
# Test if a master with one slave responds to a "PUT /psodium/addBADSLAVES"
# with no body correctly.
#
#

use strict;
use warnings FATAL => 'all';

use Apache::Test;
use Apache::TestUtil;
use Apache::TestRequest qw( PUT );
use GlobuleTest::Functions;

my $url = '/psodium/addBADSLAVES';
my $userid = 'auditor';
my $passwd = 'w8w00rd';
my $auth_header = create_auth( $userid, $passwd );

plan tests => 2;

ok 1; # simple load test
my $res = PUT $url, Authorization => $auth_header;
ok t_cmp( 200, $res->code, "Master returned 200 to ADDBADSLAVES" );
