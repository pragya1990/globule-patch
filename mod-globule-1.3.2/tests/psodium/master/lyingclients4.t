#file:lyingclient4.t
#--------------------
# Test if a master with one slave responds to a "PUT /psodium/addLYINGCLIENTS"
# with a non-chunked encoded, correct body correctly, and if it refuses a
# BADPLEDGE request from the client afterwards.
#

use strict;
use warnings FATAL => 'all';

use Apache::Test;
use Apache::TestUtil;
use Apache::TestRequest qw( GET PUT );
use GlobuleTest::Functions;

my $liar_url = '/psodium/addLYINGCLIENTS';
my $userid1 = 'auditor';
my $passwd1 = 'w8w00rd';
my $auditor_auth_header = create_auth( $userid1, $passwd1 );

my $content = "----- BEGIN IPADDRPORTVEC -----\r\nIPADDRPORTNO: 127.0.0.1:123----- END IPADDRPORTVEC -----\r\n";

my $master_uri_path = '/export/index.html';
my $slaveid = 'localhost:8057';
my $slave_uri_path = '/3rdparty/poolname/index.html';
my $userid = 'w8w00rd';
my $passwd = 'w8w00rd';

my $queryslaveid = 'localhost:8057';
my $pledge = "-----BEGIN PLEDGE-----\r\nSLAVEID localhost:8057\r\nREQUEST_URI /3rdparty/poolname/index.html\r\nREQUEST_HOST localhost:8057\r\nTIMESTAMP 1089872928529354\r\nDIGEST tQUoo0qZ7Bq2fQAs7AtuuFAUchI=\r\nSIGNATURE 1UINRcGAQFfvB4sVS1xQT8fXabOQRGm/dN+WhnhEo+QvZrep/NRLcsNqJyVZnPmVcZ8xc5AIOnu1dHa8mV5GSjUstXVOeimhXihDlPyON1jddowS1gRJXjqjmNDUkUV7MiOA0PTRTPS5avGtVjoDLq9+MGiTUlpmlfoEizSb9XI=\r\n-----END PLEDGE-----\r\n";

my $base64_pledge = MIME::Base64::encode( $pledge, "" );
my $query = $queryslaveid.'#'.$base64_pledge;
my $base64_query = MIME::Base64::encode( $query, "" );
my $badpledge_url = '/psodium/badPledge?'.$base64_query;
my $from_replica_header = 'http://'.$slaveid.$slave_uri_path;
my $auth_header = create_auth( $userid, $passwd );

plan tests => 4;

ok 1; # simple load test

#
# 1. Act like a slave and retrieve a document from the master
#
my $res1 = GET $master_uri_path, redirect_ok => 0, 'X-From-Replica' => $from_replica_header, Authorization => $auth_header;
ok t_cmp( 200, $res1->code, "Master returned 200 to emulated slave retrieval" );

#
# 2. Act like an auditor and do a ADDLYINGCLIENT
#

my $res2 = PUT $liar_url, content => $content, Authorization => $auditor_auth_header;
ok t_cmp( 200, $res2->code, "Master returned 200 to ADDLYINGCLIENTS" );

#
# 3. Act like a client and send a BADPLEDGE to the master
#

my $res3 = GET $badpledge_url, redirect_ok => 0;
ok t_cmp( 502, $res3->code, "Master returned 502 to BADPLEDGE" );

my $line;
my @lines = split( /^/, $res3->content );
my $flag = 0;
foreach $line (@lines)
{
    print "## ",$line,"\n";
    if ($line =~ /bad content/)
    {
        $flag=1;
    }
}
