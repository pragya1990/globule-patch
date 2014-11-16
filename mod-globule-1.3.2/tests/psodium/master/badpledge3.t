#file:badpledge3.t
#--------------------
# Test if a master with one slave responds to a BADPLEDGE with a slave id
# but no pledge correctly.
#
#

use strict;
use warnings FATAL => 'all';

require MIME::Base64;
use Apache::Test;
use Apache::TestUtil;
use Apache::TestRequest qw( GET );

my $slaveid = 'gromet.cs.vu.nl:3001';
my $query = $slaveid;

my $base64_query = MIME::Base64::encode( $query, "" );
my $url = '/psodium/badPledge?'.$base64_query;

plan tests => 2;

ok 1; # simple load test
my $res = GET $url, redirect_ok => 0;
ok t_cmp( 400, $res->code, "Master returned 400 to BADPLEDGE" );
