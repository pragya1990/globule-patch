#file:oneslave5.t
#----------------
# Test if a master with one slave returns a 301 on a Globulize directory
# if we fail to include a '/' at the end of the URL.
#
use strict;
use warnings FATAL => 'all';

use Apache::Test;
use Apache::TestUtil;
use Apache::TestRequest qw( GET );
use GlobuleTest::Functions;

my $conffname;
my $exportpath;
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
( $exportpath, $slave_uri_prefix, $userid, $mypasswd ) = read_master_slave_info( $conffname );

if ($exportpath eq '/')
{
    # Special case, we need a subdir. URL cannot be "" (i.e., "/" without trailing /)
    $exportpath = '/export/';
}
my $url = substr( $exportpath, 0, length( $exportpath )-1);    # no trailing slash!

my $cfg = Apache::Test::config();
my $hostport = $cfg->{hostport}; 
my $expected_location = "http://".$hostport.$url."/";

plan tests => 3;

ok 1; # simple load test

my $res = GET $url, redirect_ok => 0;
ok_returns_http_301( $res );
my $sent_location = $res->header( 'Location' );
ok t_cmp( $expected_location, $sent_location, "Location of 301 is requested URL + /" );
