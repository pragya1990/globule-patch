#file:onemaster2.t
#-----------------
# Test if a slave with one master correctly accepts an invalidate
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

plan tests => ntests_onemaster1()+1;

#
# Read filename with extra config info from httpd.conf
#
my $conffname = read_which_extra_conffname();

#
# Do onemaster1 test
#

test_onemaster1( $conffname );

#
# Now let master send invalidate (we use this thread)
#

my $invalidate_url = $GlobuleTest::Functions::meta_url;
my $res2 = GET $invalidate_url, Authorization => $GlobuleTest::Functions::master_auth_header;

ok_returns_http_ok( $res2 );

# TODO: Real test would be to send another request for the document to the slave
# and see if it would refetch from the master server.
