#file:getslaveinfo3.t
#--------------------
# Test if a master with one slave responds to a GETSLAVEINFO with bad slave ID
# correctly.
#
# At present it will act like it doesn't know the slave, and return 200 iso 
# 400, which is fine by me. The BASE64 decoder of APR is robust, so testing
# with non-base64 encoded slave ID proved not necessary.
#

use strict;
use warnings FATAL => 'all';

require MIME::Base64;
use Apache::Test;
use Apache::TestUtil;
use Apache::TestRequest qw( GET );

my $slaveid = 'bromet.cs.vu.nl,3001';
my $base64_slaveid = MIME::Base64::encode( $slaveid, "" );
my $url = '/psodium/getSlaveInfo?'.$base64_slaveid;

plan tests => 2;

ok 1; # simple load test
my $res = GET $url, redirect_ok => 0;
ok t_cmp( 200, $res->code, "Master returned 200 to GETSLAVEINFO" );
