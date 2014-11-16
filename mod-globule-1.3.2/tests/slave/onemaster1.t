#file:onemaster1.t
#-----------------
# Test if a slave performs slave registration (= slave sends GET
# with X-From-Replica: and Authorization: header)
#
# NOTE: onemaster1.t and onemaster2.t should be run separately, that is,
# a fresh server should be started for each test. If not, I get trouble when I
# run two tests that create threads and listen to sockets in the same
# test group (there are not run as separate perl processes, but some
# funky line by line eval). Arno, 2003-12-18
#
use strict;
use warnings FATAL => 'all';

require MIME::Base64;

use Apache::Test;
use Apache::TestUtil;
use Apache::TestRequest qw( GET );
use GlobuleTest::Functions;

#Uncomment next two lines to enable DEBUG
#LWP::Debug::level( '+' );
#Apache::TestRequest::user_agent( reset => 1 );

plan tests => ntests_onemaster1;

#
# Read filename with extra config info from httpd.conf
#
my $conffname = read_which_extra_conffname();

test_onemaster1( $conffname );
