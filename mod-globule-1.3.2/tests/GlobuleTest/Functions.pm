package GlobuleTest::Functions;

require MIME::Base64;
use Apache::TestUtil;
use Apache::Test;
use Apache::TestRequest qw( GET );
use threads;
use Socket;
use Config;
use vars qw( %SIG );

use Exporter ();
@ISA = qw(Exporter);
@EXPORT=qw( ntests_get_doc ok_get_doc ok_get_doc_or_302 ok_returned_html_doc ok_returns_http_ok ok_returns_http_401 ok_returns_http_301 ok_returns_right_mimetype ok_returns_right_body url2file create_meta_uri create_auth read_which_extra_conffname read_master_slave_info slave_server_invalidate_receiver_thread onemaster_master_thread meta_url master_auth_header test_onemaster1 ntests_onemaster1);

sub ntests_get_doc
{
    my $url=shift;
    # Number of OKs for ok_get_html_doc
    return 3;
}

sub ok_get_doc
{
    my $url = shift;
    my $res = GET $url;

    if ($url =~ /.html$/)
    {
        ok_returned_html_doc( $url, $res );
    }
    else
    {
        print "not ok: unknown document type in URL",$url,"\n";
    }
}

sub ok_get_doc_or_302
{
    my $url = shift;

    my $res = GET $url, redirect_ok => 0;
    if ($res->code == 302)
    {
        ok t_cmp( $res->code, $res->code, "Request to master returns HTTP redirect or doc" );
        ok t_cmp( 1, 1, 'dummy test to have equal #tests in all cases' );
        ok t_cmp( 1, 1, 'dummy test to have equal #tests in all cases' );
        return $res->code;
    }
    elsif ($res->code != 200)
    {
        ok t_cmp( "200 or 302", $res->code, "Request to master returns HTTP redirect or doc" );
        ok t_cmp( 1, 0, 'dummy test to have equal #tests in all cases' );
        ok t_cmp( 1, 0, 'dummy test to have equal #tests in all cases' );
        return $res->code;
    }

    # 200 case
    ok_returns_http_ok( $res );
    ok_returns_right_mimetype( $url, $res );
    ok_returns_right_body( $url, $res );

    return $res->code;
}


sub ok_returned_html_doc
{
    my $url = shift;
    my $res = shift;
    ok_returns_http_ok( $res );
    ok_returns_right_mimetype( $url, $res );
    ok_returns_right_body( $url, $res );
}


sub ok_returns_http_ok
{
    my $res = shift;
    ok t_cmp( 200, $res->code, "Returns HTTP_OK" );
}


sub ok_returns_http_401
{
    my $res = shift;
    ok t_cmp( 401, $res->code, "Returns HTTP_UNAUTHORIZED" );
}


sub ok_returns_http_301
{
    my $res = shift;
    ok t_cmp( 301, $res->code, "Returns HTTP_MOVED_PERMANENTLY" );
}


sub ok_returns_right_mimetype
{
    my $url = shift;
    my $res = shift;

    if ($url =~ /.html$/)
    {
        ok t_cmp( 'text/html', $res->header( 'Content-Type' ), 'Has right MIME type' );
    }
    else
    {
        print "not ok: unknown document type in URL\n";
    }
}

sub ok_returns_right_body
{
    my $url = shift;
    my $res = shift;    
    my $file = url2file( $url );
    my @lines = split( /^/, $res->content );
    my $count=0;

    my $pwd = `pwd`;
    open( STREAM, $file ) || die( "cannot open original HTML file $file\nIn $pwd" );
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

    # Arno: ok $flag == 1, "test name"; appears to be broken, It will complain it was 
    # expecting "test name" :-(
    ok t_cmp( 1, $flag, 'Body same as original file' );

    if ($flag == 0)
    {
        print $res->content;
    }
}

#
# Create meta_uri for in X-From-Replica header
# Format http://titan.cs.vu.nl:8042/3rdparty/poolname/index.html
#
sub create_meta_uri
{
    my $export_path = shift;
    my $slave_uri_prefix = shift;
    my $new_url = shift;

    $new_url =~ s/$export_path/$slave_uri_prefix/;
    return $new_url; 
}


#
# Create authentication header value
#
sub create_auth
{
    my $userid=shift;
    my $passwd=shift;
    return "Basic ".MIME::Base64::encode( "$userid:$passwd", "" );
}

#
# Read filename with extra config info from httpd.conf
#
sub read_which_extra_conffname
{
    my $conffname = "conf/httpd.conf";
    my $CONFSTREAM;
    my @words;
    my $extraconffname;

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
        if ($words[0] eq "include")
        {
            $extraconffname= $words[1];
        }
    } 
    close( CONFSTREAM );
    return $extraconffname;
}



#
# Read slave info from master's httpd.conf to create headers.
#
sub read_master_slave_info
{
    my $conffname = shift;
    my $CONFSTREAM;
    my @words;
    my $export_path;
    my $slave_uri_prefix;
    my $userid;
    my $passwd;

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
            ( $export_path ) = $words[1] =~ /^([^>]+)>*$/;
            # always / terminated
            if (rindex( $export_path, "/" ) != (length( $export_path )-1))
            {
                $export_path .= '/'; 
            }
        }
        elsif ($words[0] eq "exportto")
        {
            $slave_uri_prefix = $words[1];
            # always / terminated
            if (rindex( $slave_uri_prefix, "/" ) != (length( $slave_uri_prefix )-1))
            {
                $slave_uri_prefix .= '/'; 
            }
            $mypasswd = $words[2];
            $userid = $mypasswd;
        }
    } 
    close( CONFSTREAM );
    return ( $export_path, $slave_uri_prefix, $userid, $mypasswd );
}

#
# Method executed by slave server. It wait until it receives an
# invalidate from the master.
#
sub slave_server_invalidate_receiver_thread 
{ 
    my $uri_prefix = shift;
    my $meta_uri = shift;
    my $expected_master_auth_header = shift;
    my $addr;
    my $sent_metaurl;
    my $sent_master_auth_header;
    my $gotSIGNAL=0;
    my $gotAuth=0;
    
    print "# Starting server thread\n";

    my ($hostname, $port) = $uri_prefix =~ /(\w+):(\d+)/;
    #allow HTTP: too
    my ($expected_metaurl) = $meta_uri =~ /^\w+:\/\/[^\/]+(.*)$/;    
    
    my $AF_INET=2;
    my $SOCK_STREAM=1;
    my $sockaddr_packspec = 'S n a4 x8';
    my ($name, $aliases, $proto) = getprotobyname( 'tcp' );
    my $myaddr = pack( $sockaddr_packspec, $AF_INET, $port, "\0\0\0\0" );

    # What?
    #select(NS); $|=1; select(stdout);

    socket( S, $AF_INET, $SOCK_STREAM, $proto ) || die( "Cannot open socket" );
    bind( S, $myaddr ) || die( "Cannot bind to socket" );
    listen(S,5) || die( "cannot listen to socket" );

    # What?
    #select(S); S|=1; select(stdout);

    print "# Listening for connection\n";

    # For some reason this cr*p doesn't seem to work, although it's an example
    # from http://www.perldoc.com/perl5.8.0/pod/perlipc.html
    eval 
    {
        local $SIG{ALRM} = sub { die "alarm clock restart" };
        alarm 10;
        ( $addr = accept( NS,S)) || die( "cannot accept from socket" );
        alarm 0;
    };
    if ($@ and $@ !~ /alarm clock restart/) { die }  
    
    while( <NS> )
    {
	print "# Got line ",$_,"\n";
	
        if (/SIGNAL/)
        {
	    ($sent_metaurl) = $_ =~ /^SIGNAL (\S+)/;
	    ok t_cmp( $expected_metaurl, $sent_metaurl, "See if master sent correct invalidate" );
            $gotSIGNAL=1;
        }
        elsif (/Authorization/)
        {
            ($sent_master_auth_header ) = $_ =~ /^Authorization: (.+)\r\n$/;
            ok t_cmp( $expected_master_auth_header, $sent_master_auth_header, "See if master sends correct Authorization header" );
            $gotAuth=1;
        }
        elsif ($_ eq "\r\n")
        {
            # End of header
            last;
        }
    }
    if ($gotSIGNAL == 0)
    {
        ok t_cmp( $expected_metaurl, "nothing", "See if master sends SIGNAL" );
    }
    if ($gotAuth == 0)
    {
        ok t_cmp( $expected_master_auth_header, "nothing", "See if master sends correct Authorization header" );
    }

    # send reply
    print NS "HTTP/1.1 204 No Content\r\n\r\n";
    close( NS );
    close( S );
}  







sub url2file
{
    my $url = shift;
    my $path;
    my @dummy_char_list = ();
 
    if ($url =~ /:\/\//)
    {
        ( $path ) = $url =~ /^[^\/]+\/\/[^\/]+(\/.*)$/;
    }
    else
    {
        $path=$url;
    }
    # Mirror Apache's unescape routine for URL 2 filenames = all
    # encoded chars are translated to their octet
    $path = unescape_path( $path, 0, \@dummy_char_list );

    return 'htdocs'.$path;
}


sub normalize_path
{
    my $path = shift;
    my @rfc2396bis_unreserved = ( 'a' , 'b' , 'c' , 'd' , 'e' , 'f' , 'g' , 'h' , 'i' ,
      'j' , 'k' , 'l' , 'm' , 'n' , 'o' , 'p' , 'q' , 'r' ,
      's' , 't' , 'u' , 'v' , 'w' , 'x' , 'y' , 'z' ,
      'A' , 'B' , 'C' , 'D' , 'E' , 'F' , 'G' , 'H' , 'I' ,
      'J' , 'K' , 'L' , 'M' , 'N' , 'O' , 'P' , 'Q' , 'R' ,
      'S' , 'T' , 'U' , 'V' , 'W' , 'X' , 'Y' , 'Z' ,
      '0' , '1' , '2' , '3' , '4' , '5' , '6' , '7' ,
      '8' , '9' , '-' , '.' , '_' , '~' );

      print "NORMALIZING PATH\n";

    return unescape_path( $path, 1, \@rfc2396bis_unreserved );
}

#
# Decode %02X escaped characters in path:
# - Either: all escaped characters in path
# - or: only escaped characters specified in char_list
#
sub unescape_path
{
    my $path = shift;
    my $decode_char_list = shift;
    my $char_list_ref = shift;
    my @char_list = @$char_list_ref;
    my $char;
    my $in_list;
    my $found=0;
    my $encoded_char;
    my $decoded_char;
    my $shadow_path = $path;

    print "# Encoded path is ", $path, "\n";
    do
    {
        $found = ( $encoded_char ) = $shadow_path =~ /(%[0-9A-Fa-f][0-9A-Fa-f])/;
        if ($found eq '1')
        {
            print "# Encoded char is ", $encoded_char, "\n";
            $decoded_char = pack( "H2", substr( $encoded_char, 1 ) );
            print "# Decoded char is ", $decoded_char, "\n";
            $in_list=0;
            if ($decode_char_list == 1)
            {
                #print "Char list has ", $#char_list, " elements.\n";
                foreach $char (@char_list)
                {
                    #print "Compare ",$char," to decoded ",$decoded_char,"\n";
                    if ($char eq $decoded_char)
                    {
                        $in_list=1;
                        last;
                    }
                }
            }
            else
            {
                $in_list=1;
            }
            if ($in_list == 1)
            {
                $path =~ s/$encoded_char/$decoded_char/g;
            }
            # always
            $shadow_path =~ s/$encoded_char/$decoded_char/g;
            #print "Shadow path is ", $shadow_path, "\n";	
        }
    } while( $found eq '1' );

    print "# Decoded path is ", $path, "\n";
    return $path;
}


sub ntests_onemaster1
{
    return 7;
}

# Global vars
#my $meta_url;
#my $master_auth_header;

sub test_onemaster1
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
            ( $slave_path ) = $words[1] =~ /([^>\s]+)/;

            # always / terminated
            if (rindex( $slave_path, "/" ) != (length( $slave_path )-1))
            {
                $slave_path .= '/'; 
            }
            $slave_path = normalize_path( $slave_path );
            ( $poolname ) = $slave_path;
        }
        if ($words[0] eq "import")
        {
            ($master_host, $master_port, $master_path ) = $words[2] =~ /(\w+):(\d+)(\/\S*)$/;
            $master_passwd = $words[3];
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
    $master_auth_header = "Basic ".MIME::Base64::encode( "$master_userid:$master_passwd", "" );
    #$meta_url = "/meta?pool=".$poolname."&url=http://".$master_host.":".$master_port.$doc_base_url_path;
    $meta_url = $url;

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

    my $res = GET $url;

    ok_returns_http_ok( $res );
    ok_returns_right_mimetype( $url, $res );
    ok_returns_right_body( $fake_path, $res );

    $thr->join;
}


sub onemaster_master_thread 
{
    my $master_path = shift;
    my $master_host = shift;
    my $master_port = shift;
    my $meta_uri = shift;
    my $slave_auth_header = shift;
    my $fake_path = shift;
    my $addr;
    my $sent_path;
    my $sent_meta_uri;
    my $sent_slave_auth_header;
    
    print "# Starting master thread\n";

    my $hostname = $master_host;
    my $port = $master_port;

    my $AF_INET=2;
    my $SOCK_STREAM=1;
    my $sockaddr_packspec = 'S n a4 x8';
    my ($name, $aliases, $proto) = getprotobyname( 'tcp' );
    my $myaddr = pack( $sockaddr_packspec, $AF_INET, $port, "\0\0\0\0" );

    socket( S, $AF_INET, $SOCK_STREAM, $proto ) || die( "Cannot open socket" );
    setsockopt(S, SOL_SOCKET, SO_REUSEADDR,pack("l", 1)) || die( "setsockopt: $!" );
    bind( S, $myaddr ) || die( "Cannot bind to socket" );
    listen(S,5) || die( "cannot listen to socket" );

    print "# Listening for connection, TID=",threads->self->tid,"\n";

    # For some reason this cr*p doesn't seem to work, although it's an example
    # from http://www.perldoc.com/perl5.8.0/pod/perlipc.html
    eval 
    {
        local $SIG{ALRM} = sub { die "alarm clock restart" };
        alarm 10;
        ( $addr = accept( NS,S)) || die( "cannot accept from socket" );
        alarm 0;
    };
    if ($@ and $@ !~ /alarm clock restart/) { die }  
    
    my $gotGET=0;
    my $gotXFromReplica=0;
    my $gotAuth=0;
    while( <NS> )
    {
        if (/GET/)
        {
            ($sent_path) = $_ =~ /^GET (\S+)/;
            ok t_cmp( $master_path, $sent_path, "See if slave requests correct URL" );
            $gotGET=1;
        }
        elsif (/X-From-Replica/)
        {
            ($sent_meta_uri ) = $_ =~ /^X-From-Replica: (\S+)\r\n$/;
            ok t_cmp( $meta_uri, $sent_meta_uri, "See if slave sends correct X-From-Replica header" );
            $gotXFromReplica=1;
        }
        elsif (/Authorization/)
        {
            ($sent_slave_auth_header ) = $_ =~ /^Authorization: (.+)\r\n$/;
            ok t_cmp( $slave_auth_header, $sent_slave_auth_header, "See if slave sends correct Authorization header" );
            $gotAuth=1;
        }
        elsif ($_ eq "\r\n")
        {
            # End of header
            last;
        }
    }

    # Ensure equal number of tests in all cases.
    # MUST do this before sending reply, otherwise we get concurrency on the
    # test counter used by ok().
    if ($gotGET == 0)
    {
        ok t_cmp( $master_path, "nothing", "See if slave requests correct URL" );
    }
    if ($gotXFromReplica == 0)
    {
        ok t_cmp( $meta_uri, "nothing", "See if slave sends correct X-From-Replica header" );
    }
    if ($gotAuth == 0)
    {
        ok t_cmp( $slave_auth_header, "nothing", "See if slave sends correct Authorization header" );
    }

    # send reply header
    print NS "HTTP/1.1 200 OK\r\n";
    print NS "Date: Tue, 11 Nov 2003 14:46:10 GMT\r\n";
    print NS "Server: Apache/2.0.48 (Unix) mod_perl/1.99_10 Perl/v5.8.1\r\n";
    print NS "Last-Modified: Wed, 05 Nov 2003 12:17:40 GMT\r\n";
    print NS "ETag: \"8257d-99-32846100\"\r\n";
    print NS "Accept-Ranges: bytes\r\n";
    # Don't forget: master tells slave what the new policy is
    print NS "X-Globule-Policy: Invalidate\r\n";
    print NS "Content-Length: 153\r\n";
    print NS "Content-Type: text/html\r\n";
    print NS "\r\n";

    # send body
    open( STREAM, url2file( $fake_path ) ) || die( "Master: cannot open source HTML file" );
    my $flag=1;
    while( <STREAM> )
    {
        print NS $_;
        #print "Sending body line TID",threads->self->tid,"\n";
    }
    close( STREAM );

    close( NS );
    close( S );

    #print "Master done!\n";

    return 1;
}  




1;
