#file:noslave3.t
#----------------
# Test if a master with no slave can stand a GET from a slave that uses a
# wrong password.
#

use strict;
use warnings FATAL => 'all';

require MIME::Base64;

use Apache::Test;
use Apache::TestUtil;
use Apache::TestRequest qw( GET );
use GlobuleTest::Functions;
use Config;

plan tests => 2;

my $url = '/export/index.html';
my $slave_uri_prefix;
my $poolname;
my $mypasswd;

$slave_uri_prefix="http://localhost:8057/3rdparty/poolname";
$poolname = "poolname";
$mypasswd = "wrongSlavepassw0rd";
my $userid = "wrongMasterpassw0rd";
my $cfg = Apache::Test::config();
my $hostport = $cfg->{hostport}; 

ok 1;   # got info correctly

#
# Create X-From-Replica header
# Format http://titan.cs.vu.nl:8042/meta?pool=poolname&url=http://titan.cs.vu.nl:8041/export/index.html
#
my $from_header = $slave_uri_prefix."/meta?pool=".$poolname."&url=http://".$hostport.$url; 
#print "X-From-Replica: ",$from_header,"\n";
my $auth_header = "Basic ".MIME::Base64::encode( "$userid:$mypasswd", "" );

#
# Send registration request
#

my $res = GET $url, redirect_ok => 0, 'X-From-Replica' => $from_header, Authorization => $auth_header;
ok_returns_http_401( $res );


