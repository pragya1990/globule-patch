#file:oneslave2.t
#----------------
# Test if a master with one slave when asked for a Globulized doc 
# returns either:
# a) a HTTP redirect 
# b) the doc itself, as the master may choose to handle the request himself
#
# We have round-robin redirection in place and one slave defined, so we also 
# check that if we send two requests, one is a redirect.
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

my $url;
if ($exportpath eq '/')
{
    # Special case, if export is root, make sure we request a file in a subdir 
    # of htdocs. We cannot use /index.html because the "ok 1" test uses that
    # and we put in place special measures (DoNotReplicate) to prevent that request
    # from being processed by Globule, as we want to control when we test that.
    $url = '/export/index.html';
}
else
{
    $url = $exportpath.'index.html';
}

plan tests => 1+2*ntests_get_doc( $url )+1;

ok 1; # simple load test

my $code1 = ok_get_doc_or_302( $url );
my $code2 = ok_get_doc_or_302( $url );
ok t_cmp( 1, ($code1 == 302 && $code2 == 200) || ($code1 == 200 && $code2 == 302), "One of two requests must be 302" );
