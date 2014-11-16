#file:updateexpiry3.t
#--------------------
# Test if a master with one slave responds to an UPDATEEXPIRY with a broken
# query argument correctly: only a time is given, not a filename
#

use strict;
use warnings FATAL => 'all';

require MIME::Base64;
use Apache::Test;
use Apache::TestUtil;
use Apache::TestRequest qw( GET );
use GlobuleTest::Functions;

my $times='1089813099535030';
my $url = '/psodium/updateExpiry?'.$times;
my $userid = 'w8w00rd';
my $passwd = 'w8w00rd';
my $auth_header = create_auth( $userid, $passwd );

plan tests => 2;

ok 1; # simple load test
my $res = GET $url, redirect_ok => 0, Authorization => $auth_header;
ok t_cmp( 400, $res->code, "Master returned 400 to UPDATEEXPIRY" );
