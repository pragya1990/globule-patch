#file:combinedA1.t
#-----------------
# Test if a master with one slave returns the correct document (via a HTTP 
# direct if from slave), when asked for a Globulized doc.
#
use strict;
use warnings FATAL => 'all';

use Apache::Test;
use GlobuleTest::Functions;

my $url = '/export/index.html';

plan tests => 1+ntests_get_doc( $url );

ok 1; # simple load test
ok_get_doc( $url );
