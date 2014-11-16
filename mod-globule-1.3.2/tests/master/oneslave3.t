#file:oneslave3.t
#----------------
# Test if a master with one slave handles a slave registration (= slave sends GET
# with X-From-Replica: and Authorization: header)
#
#

use strict;
use warnings FATAL => 'all';

use Apache::Test;
use Apache::TestUtil;
use Apache::TestRequest qw( GET );
use GlobuleTest::Functions;

plan tests => 4;

my $conffname;
my $export_path;
my $slave_uri_prefix;
my $userid;
my $mypasswd;


#
# Read filename with extra config info from httpd.conf
#
$conffname = read_which_extra_conffname();

#
# Read slave info from master's httpd.conf to create headers.
#
( $export_path, $slave_uri_prefix, $userid, $mypasswd ) = read_master_slave_info( $conffname );

my $cfg = Apache::Test::config();
my $hostport = $cfg->{hostport}; 

ok 1;   # got info correctly

#
# Create header values
#
my $url;
if ($export_path eq '/')
{
    # Special case, if export is root, make sure we request a file in a subdir 
    # of htdocs. We cannot use /index.html because the "ok 1" test uses that
    # and we put in place special measures (DoNotReplicate) to prevent that request
    # from being processed by Globule, as we want to control when we test that.
    $url = '/export/index.html';
}
else
{
    $url = $export_path.'index.html';
}
my $meta_uri = create_meta_uri( $export_path, $slave_uri_prefix, $url ); 
#print "X-From-Replica: ",$meta_uri,"\n";
my $auth_header = create_auth( $userid, $mypasswd );

#
# Do request
#
my $res = GET $url, redirect_ok => 0, 'X-From-Replica' => $meta_uri, Authorization => $auth_header;

ok_returns_http_ok( $res );
ok_returns_right_mimetype( $url, $res );
ok_returns_right_body( $url, $res );
