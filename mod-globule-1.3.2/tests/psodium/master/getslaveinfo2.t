#file:getslaveinfo2.t
#--------------------
# Test if a master with one slave responds to a GETSLAVEINFO with empty
# query argument correctly
#

use strict;
use warnings FATAL => 'all';

require MIME::Base64;
use Apache::Test;
use Apache::TestUtil;
use Apache::TestRequest qw( GET );

my $url = '/psodium/getSlaveInfo?';

plan tests => 2;

ok 1; # simple load test
my $res = GET $url, redirect_ok => 0;
ok t_cmp( 400, $res->code, "Master returned 400 to GETSLAVEINFO" );
