#file:oneslave1.t
#----------------
# Test if a master with one slave returns a non-Globule document correctly.
#
use strict;
use warnings FATAL => 'all';

use Apache::Test;
use GlobuleTest::Functions;

my $url = '/index.html';

plan tests => 1+ntests_get_doc( $url );

ok 1; # simple load test
ok_get_doc( $url );
