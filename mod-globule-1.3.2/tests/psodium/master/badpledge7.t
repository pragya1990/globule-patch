#file:badpledge7.t
#-----------------
# Test if a master with one slave responds to a doubly bad BADPLEDGE
# report (=pledge is bad, in particular, the digest is of a different piece of
# content, but the signature is valid), and the client supplied the wrong 
# slaveid (which is therefore lying).
#

use strict;
use warnings FATAL => 'all';

require MIME::Base64;
use Apache::Test;
use Apache::TestUtil;
use Apache::TestRequest qw( GET );
use GlobuleTest::Functions;

my $master_uri_path = '/export/index.html';
my $slaveid = 'localhost:8057';
my $slave_uri_path = '/3rdparty/poolname/index.html';
my $userid = 'w8w00rd';
my $passwd = 'w8w00rd';

my $queryslaveid = 'localhost:8056';
my $pledge = "-----BEGIN PLEDGE-----\r\nSLAVEID localhost:8058\r\nREQUEST_URI /3rdparty/poolname/index.html\r\nREQUEST_HOST localhost:8057\r\nTIMESTAMP 1089873878511894\r\nDIGEST tQUoo0qZ7Bq2fQAs7AtuuFAUchI=\r\nSIGNATURE lAOUVHV8EnT/gLLAZ2meRyChAtArNpSYVUPBP3cDzBdtq5+nUejgAHMdCUpyX/tHXMvT+0rfve9LhPkIbYrJyi6rK0QTwNUCR/iTqD/DFmhIsLv7XTlQOuM3AM9HplinQkmd1HWmjNQvkDbL1cquLA4IwQngz5InrPXf17ZBaXs=\r\n-----END PLEDGE-----\r\n";

my $base64_pledge = MIME::Base64::encode( $pledge, "" );
my $query = $queryslaveid.'#'.$base64_pledge;
my $base64_query = MIME::Base64::encode( $query, "" );
my $url = '/psodium/badPledge?'.$base64_query;
my $from_replica_header = 'http://'.$slaveid.$slave_uri_path;
my $auth_header = create_auth( $userid, $passwd );

plan tests => 4;

ok 1; # simple load test

#
# 1. Act like a slave and retrieve a document from the master
#
my $res2 = GET $master_uri_path, redirect_ok => 0, 'X-From-Replica' => $from_replica_header, Authorization => $auth_header;
ok t_cmp( 200, $res2->code, "Master returned 200 to emulated slave retrieval" );

#
# 2. Act like a client and send a BADPLEDGE to the master
#

my $res = GET $url, redirect_ok => 0;
ok t_cmp( 200, $res->code, "Master returned 200 to BADPLEDGE" );

my $line;
my @lines = split( /^/, $res->content );
my $flag = 0;
foreach $line (@lines)
{
    print "## ",$line,"\n";
    if ($line =~ /liar/)
    {
        $flag=1;
    }
}
ok t_cmp( 1, $flag, "Client was marked as liar" );