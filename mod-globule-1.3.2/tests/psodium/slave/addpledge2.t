#file:addpledge3.t
#-----------------
# Test if a slave appends a pledge to its output when asked to.
# 
# To test this, we first test slave registration (= slave sends GET
# with X-From-Replica: and Authorization: header), a Globule test.
#
# NOTE: addpledge1.t and addpledge2.t should be run separately, that is,
# a fresh server should be started for each test. If not, I get trouble when I
# run two tests that create threads and listen to sockets in the same
# test group (there are not run as separate perl processes, but some
# funky line by line eval). Arno, 2004-07-15
#
use strict;
use warnings FATAL => 'all';

require MIME::Base64;

use Apache::Test;
use Apache::TestUtil;
use Apache::TestRequest qw( GET );
use GlobuleTest::Functions;
use Config;

#Uncomment next two lines to enable DEBUG
#LWP::Debug::level( '+' );
#Apache::TestRequest::user_agent( reset => 1 );



sub ok_returns_right_pledged_body
{
    my $url = shift;
    my $res = shift;    
    my $file = url2file( $url );
    my @lines = split( /^/, $res->content );
    my $count=0;

    open( STREAM, $file ) || die( "cannot open original HTML file" );
    my $flag=1;
    while( <STREAM> )
    {
        if ($lines[$count] ne $_)
        {
            $flag=0;
            last;
        }
        $count++;
    }

    close STREAM;

    print "COUNTER line ",$#lines, " count ",$count,"\n";

    # Check if more output than file, and pledge
    if ($count+7 == $#lines)
    {
        if ($lines[$count+0] =~ /-----BEGIN PLEDGE-----/ &&
            $lines[$count+1] =~ /SLAVEID/ &&
            $lines[$count+2] =~ /REQUEST_URI/ &&
            $lines[$count+3] =~ /REQUEST_HOST/ &&
            $lines[$count+4] =~ /TIMESTAMP/ &&
            $lines[$count+5] =~ /DIGEST/ &&
            $lines[$count+6] =~ /SIGNATURE/ &&
            $lines[$count+7] =~ /-----END PLEDGE-----/)
        {
        }
        else
        {
            $flag=0;
        }
    }
    else
    {
        $flag=0;
    }


    # Arno: ok $flag == 1, "test name"; appears to be broken, It will complain it was 
    # expecting "test name" :-(
    ok t_cmp( 1, $flag, 'Body same as original file' );

    if ($flag == 0)
    {
        print $res->content;
    }
}



#
# Copy of GlobuleTest::Functions::test_onemaster1
# with two diffs:
# * Extra header on GET request, to request pledge be appended
# * Different response test, as there will be a pledge appended
#
sub test_addpledge2
{
    my $conffname = shift;
    my $poolname;
    my $slave_passwd;
    my $slave_path;
    my $master_passwd;
    my $master_host;
    my $master_port;
    my $master_path;
    my $fake_path="/index.html";
    my $url;
    my $doc_base_url_path;

    if (!$Config{useithreads}) 
    {
        die( "Must have thread support to run this test" );
    }  

    #
    # Read info from httpd.conf to create headers.
    #
    open( CONFSTREAM, $conffname ) || die( "Cannot open config file ".$conffname );
    while( <CONFSTREAM> )
    {
        chop;
        my @words = split();
        if ($#words == -1)
        {
            next;
        }

        $words[0] =~ tr/A-Z/a-z/;
        if ($words[0] eq "<location")
        {
            # allow for close tag with spaces (i.e, "  >  " at end)
            
            print "words[1] is ", $words[1], "\n";
    
            ( $slave_path ) = $words[1] =~ /([^>\s]+)/;

            print "slave_path is ", $slave_path, "\n";
            # always / terminated
            if (rindex( $slave_path, "/" ) != (length( $slave_path )-1))
            {
                $slave_path .= '/'; 
            }
            $slave_path = normalize_path( $slave_path );
            ( $poolname ) = $slave_path;
        }
        if ($words[0] eq "globulereplicafor")
        {
            ($master_host, $master_port, $master_path ) = $words[1] =~ /(\w+):(\d+)(\/\S*)$/;
            $master_passwd = $words[2];
	    $slave_passwd = $master_passwd;

            # URI comparison rules, hostnames case insensitive, normalized
            # to lower case
            $master_host =~ tr/A-Z/a-z/;
            # always / terminated
            if (rindex( $master_path, "/" ) != (length( $master_path )-1))
            {
                $master_path .= '/'; 
            }
            $master_path = normalize_path( $master_path );
        }
    } 
    my $slave_userid = $master_passwd;
    my $master_userid = $slave_passwd;
    my $cfg = Apache::Test::config();
    my $slave_hostport = $cfg->{hostport}; 

    if ($slave_path eq '/')
    {
        # Special case, if export is root, make sure we request a file in a subdir 
        # of htdocs. We cannot use /index.html because the "ok 1" test uses that
        # and we put in place special measures (DoNotReplicate) to prevent that request
        # from being processed by Globule, as we want to control when we test that.
        $url = '/export/index.html';
        $doc_base_url_path = $master_path.'export/index.html';
    }
    else
    {
        $url = $slave_path.'index.html';
        $doc_base_url_path = $master_path.'index.html';
    }

    ok 1;   # got info correctly

    #
    # Create X-From-Replica header
    # Format http://titan.cs.vu.nl:8042/meta?pool=poolname&url=http://titan.cs.vu.nl:8041/export/index.html
    #
    my $master_auth_header = "Basic ".MIME::Base64::encode( "$master_userid:$master_passwd", "" );
    #$meta_url = "/meta?pool=".$poolname."&url=http://".$master_host.":".$master_port.$doc_base_url_path;
    my $meta_url = $url;

    my $meta_uri = "http://".$slave_hostport.$meta_url;
    #print "X-From-Replica: ",$meta_uri;
    my $slave_auth_header = "Basic ".MIME::Base64::encode( "$slave_userid:$slave_passwd", "" );
    


    #
    # Start master thread
    #

    my $thr = threads->new(\&onemaster_master_thread, $doc_base_url_path, $master_host, $master_port, $meta_uri, $slave_auth_header, $fake_path );

    # Give thread some time to start up, as the next GET will indirectly
    # connect to it. CAREFUL: don't sleep longer than master thread waits for connection
    sleep 2;

    my $res = GET $url, 'X-pSodium-Want-Pledge' => 'True';
    #my $res = GET $url;

    ok_returns_http_ok( $res );
    ok_returns_right_mimetype( $url, $res );
    ok_returns_right_pledged_body( $fake_path, $res );

    $thr->join;
}


plan tests => ntests_onemaster1;

#
# Read filename with extra config info from httpd.conf
#
my $conffname = read_which_extra_conffname();

#
# Emulate a Globule master, and request a URL from the slave
#
test_addpledge2( $conffname );
