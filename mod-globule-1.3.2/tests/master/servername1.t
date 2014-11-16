#file:servername1.t
#------------------
# Test a master with one slave and a ServerName different from its regular
# hostname. When asked for a Globulized doc it should return either:
# either:
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

my $servername_hostname = 'localhost.cs.vu.nl';
# port in servername is set from Apache test config
my $path = '/export/index.html';

my $cfg = Apache::Test::config();
my $hostport = $cfg->{hostport}; 
$hostport =~ s/localhost/$servername_hostname/g;
$hostport =~ s/.localdomain//g; # handle case localhost.localdomain
my $url = 'http://'.$hostport.$path;

print "TESTURL is ", $url, "\n";

plan tests => 1+(2*ntests_get_doc( $url ))+1;

ok 1; # simple load test

my $code1 = ok_get_doc_or_302( $url );
my $code2 = ok_get_doc_or_302( $url );
ok t_cmp( 1, ($code1 == 302 && $code2 == 200) || ($code1 == 200 && $code2 == 302), "One of two requests must be 302" );
