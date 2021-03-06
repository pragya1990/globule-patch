#file:oneslave4.t
#----------------
# Test if a master with one slave handles the following case:
# 0. The default replication policy is Invalidate. 
# 1. The slave registers (= slave sends GET with X-From-Replica: and 
#    Authorization: header)
# 2. The document is updated at the master
# 3. The slave receives an invalidate from the master.
#

use strict;
use warnings FATAL => 'all';

use Apache::Test;
use Apache::TestUtil;
use Apache::TestRequest qw( GET );
use GlobuleTest::Functions;
use threads; 
use Config;
use vars qw( %SIG );

BEGIN 
{
    if (!$Config{useithreads}) 
    {
        die( "Must have thread support to run this test" );
    }  
}

plan tests => 6;

my $conffname;
my $slave_uri_prefix;
my $export_path;
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
##my $hostport = $cfg->{hostport}; 

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
# Start server thread
#
my $thr = threads->new(\&slave_server_invalidate_receiver_thread, $slave_uri_prefix, $meta_uri, $auth_header );


#
# Send registration request
#
my $res = GET $url, redirect_ok => 0, 'X-From-Replica' => $meta_uri, Authorization => $auth_header;
ok_returns_http_ok( $res );
ok_returns_right_mimetype( $url, $res );
ok_returns_right_body( $url, $res );

#
# Update file such that master sends invalidate
#
my $now = time;
utime $now, $now, url2file( $url );

# Now wait for the other thread to get the Invalidate
$thr->join; 
