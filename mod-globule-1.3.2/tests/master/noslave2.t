#file:noslave2.t
#---------------
# Test if a master with no slaves returns a Globulized document correctly.
#
use strict;
use warnings FATAL => 'all';

use Apache::Test;
use GlobuleTest::Functions;

my $url = '/export/index.html';

plan tests => 1+ntests_get_doc( $url );

ok 1; # simple load test
ok_get_doc( $url );
