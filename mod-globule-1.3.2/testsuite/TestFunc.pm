package TestFunc;

use strict;
use warnings;
use threads;
use Thread::Queue;
use Thread::Semaphore;
use Time::HiRes qw(usleep ualarm gettimeofday tv_interval);
require LWP::UserAgent;
require HTTP::Request;

BEGIN {
  use Exporter ();
  our (@ISA, @EXPORT, @EXPORT_OK);
  @ISA=qw(Exporter);
  @EXPORT=qw(init_servers init_origin_docs generate_doc start_server stop_server
             get_doc get_doc_full update_doc flood_test my_exit my_sleep);
  @EXPORT_OK = qw($WIN32 $CWD $APACHE $SERVER $ORIGIN $REPLICA $VHOST 
                  $REDIR $BACKUP $ROLE $EXPDIR $IMPDIR $HOSTNAME $PORT
                  $BASEDIR $PASSWORD $CONTAINS $LOCALHOST $REDIR_MODE 
                  $REDIR_DNS $REDIR_HTTP $REDIR_BOTH $REDIR_OFF $NR_DOCS @my_hostnames);
}

our $SERVER   = "server";
our $ORIGIN   = "origin";
our $REPLICA  = "replica";
our $VHOST    = "vhost";
our $REDIR    = "redirector";
our $BACKUP   = "backup";
our $ROLE     = "role";
our $EXPDIR   = "exportdir";
our $IMPDIR   = "importdir";
our $HOSTNAME = "hostname";
our $PORT     = "port";
our $BASEDIR  = "base_dir";
our $PASSWORD = "password";
our $CONTAINS = "contains";
our $PID      = "pid";
our $BASEURI  = "base_uri";
our $NR_DOCS  = "nr_docs";
our $LOCALHOST= "localhost";
our $HAS_VHOST= "has_vhost";
our $REDIR_MODE ="redir_mode";
our $REDIR_DNS = "DNS";
our $REDIR_HTTP = "HTTP";
our $REDIR_BOTH = "BOTH";
our $REDIR_OFF = "OFF";

our @EXPORT_OK;
our $WIN32 = 0;
our $CWD = undef;             # See further down !
our $APACHE = undef;          # See further down !
our $MIME_TYPES_LOC = undef;  # See further down !
my $semaphore = new Thread::Semaphore;
my $default_nr_docs = 25;
my $max_nr_lines = 1000;
my $default_nr_iterations = 1000;

if (defined $ENV{WINDIR}) {
  $WIN32 = 1;
}

# PLEASE SET YOUR $CWD, $APACHE and $MIME_TYPES_LOC HERE
# $WIN32 is set by TestFunctions;
if ($WIN32) {
  # What is your current working directory, use windows notation, with /
  $CWD = "D:/Work/globule/testsuite";

  # Where is the apache binary, use cygwin notation
  $APACHE = "/cygdrive/d/ApacheInstallFiles/Apache2-2.0.52-Patch/bin/Apache.exe";
  $MIME_TYPES_LOC = "C:/cygwin/home/globule/mime.types";
} else {
  # Current working directory is fine.
  $CWD = `pwd`; chop($CWD);
  $APACHE = "/home/berry/globule/apache/bin/httpd";
  $MIME_TYPES_LOC = "/home/berry/globule/apache/conf/mime.types";
}


# Update this list if your computer has multiple hostnames that you use.
# This will determine if we can compare documents on disk or if we need
# to fetch them (via HTTP).
our @my_hostnames = ($LOCALHOST);


=cut
#############################################################################
Server Hash: $CONTAINS => ref of array of ref of Hahes          [* REQ]
             $BASEDIR  => path, $CWD/server$i default, no / end [OPT]
             $HOSTNAME => $LOCALHOST  (default)                 [OPT]
             $PORT     => 10000+$i (default)                    [OPT]
             $REDIR_MODE=> HTTP/DNS/BOTH                        [OPT]
             $PID      => set if $WIN32 and apache was forked   [NO TOUCH]
             $BASEURI  => http://hostname[:port] (autoset)      [NO TOUCH]
             $HAS_VHOST=> 0/1                                   [NO TOUCH]

Origin Hash: $ROLE     => $ORIGIN                               [* REQ]
             $SERVER   => ref to Server Hash                    [* REQ]
             $REPLICA  => ref to array of Replica Hashes        [* REQ]
             $REDIR    => ref to array of Redir Hashes          [OPT]
             $BACKUP   => ref to array of Backup Hashes         [OPT]
             $EXPDIR   => path string + '/' !                   [* REQ]
             $NR_DOCS  => how many generated docs               [OPT]

Replica Hash:$ROLE     => $REPLICA                              [* REQ]
             $SERVER   => ref to Server Hash                    [* REQ]
             $ORIGIN   => ref to Origin Hash                    [* REQ]
             $IMPDIR   => path string + '/' !                   [* REQ]
             $PASSWORD => "password$i" (default)                [OPT]

Redir Hash:  $ROLE     => $REDIR                                [* REQ]
             $SERVER   => ref to Server Hash                    [* REQ]
             $ORIGIN   => ref to Origin Hash                    [* REQ]
             $IMPDIR   => path string + '/' !                   [* REQ]
             $PASSWORD => "password$i" (default)                [OPT]

Backup Hash: $ROLE     => $BACKUP                               [* REQ]
             $SERVER   => ref to Server Hash                    [* REQ]
             $ORIGIN   => ref to Origin Hash                    [* REQ]
             $IMPDIR   => path string + '/' !                   [* REQ]
             $PASSWORD => "password$i" (default)                [OPT]

Virtual Hash:$ROLE     => $VHOST                                [* REQ]
             $SERVER   => ref to Server Hash                    [* REQ]
             $CONTAINS => ref of array of ref of o,r,r,b Hashes [* REQ]
             $BASEDIR  => past, $CWD/server$i/vhost.hostname    [OPT]
             $HOSTNAME => inherit from Server Hash (default)    [* REQ]
             $PORT     => inherit from Server Hash (default)    [OPT]
             $BASEURI => http://hostname[:port] (autoset)     [NO TOUCH]
            


#############################################################################
=cut


# For the floodtest

# Initializes the test case, generates httpd.conf and files for testing.
sub init_servers {

  my $temp = shift;
  my @servers = @$temp;
  my $port = 10000;   # Start counting from this port number
  my $cnt = 0; # password enumeration
  my $i=0;

  # Set some basic stuff and defaults
  for ($i=0; $i <= $#servers; $i++) {
    # Set the server's base directory
    if (not defined $servers[$i]{$BASEDIR}) {
      $servers[$i]{$BASEDIR} = "$CWD/server$i";
    }

    # check if this dir exist, if logs/conf/htdocs exist
    if (! -e $servers[$i]{$BASEDIR}) {
      mkdir($servers[$i]{$BASEDIR});
    }

    if (! -e $servers[$i]{$BASEDIR}."/conf") {
      mkdir($servers[$i]{$BASEDIR}."/conf");
    }

    if (! -e $servers[$i]{$BASEDIR}."/logs") {
      mkdir($servers[$i]{$BASEDIR}."/logs");
    }

    if (! -e $servers[$i]{$BASEDIR}."/htdocs") {
      mkdir($servers[$i]{$BASEDIR}."/htdocs");
    }

    # Set the server's hostname if not set;
    if (not defined $servers[$i]{$HOSTNAME}) {
      $servers[$i]{$HOSTNAME} = $LOCALHOST;
    }

    # Set the server's port number
    if (not defined $servers[$i]{$PORT}) {
      $servers[$i]{$PORT} = $port++;
    }

    # Set base_uri
    if ($servers[$i]{$PORT} != 80) {
      $servers[$i]{$BASEURI} = "http://".$servers[$i]{$HOSTNAME}.":".
                                $servers[$i]{$PORT};
    } else {
      $servers[$i]{$BASEURI} = "http://".$servers[$i]{$HOSTNAME};
    }

    # Set default REDIR_MODE to HTTP
    if (not defined $servers[$i]{$REDIR_MODE}) {
      $servers[$i]{$REDIR_MODE} = $REDIR_HTTP;
    }

    # Check for a virtual host section
    foreach my $role (@{$servers[$i]{$CONTAINS}}) {
      if ($role->{$ROLE} eq $VHOST) {
        $servers[$i]{$HAS_VHOST} = 1;
        last;
      }
    }
  }

  # Create the httpd.confs
  for ($i=0; $i <= $#servers; $i++) {
    # Create an index.html
    if (! -e $servers[$i]{$BASEDIR}."/htdocs/index.html") {
      open(DOC, '>', $servers[$i]{$BASEDIR}."/htdocs/index.html")
        or die "ERROR: Can't open ".$servers[$i]{$BASEDIR}."/htdocs/index.html: $!\n";
      print DOC "<HTML><BODY>\n";
      print DOC "TEST SUITE: ".$servers[$i]{$BASEDIR}."\n";
      print DOC "</BODY></HTML>\n";
      close(DOC);
    }

    # Copy the template over
    `cp httpd.template $servers[$i]{$BASEDIR}/conf/httpd.conf`;
   
    # Open the template to append to
    open(HTTPD, '>>', "$servers[$i]{$BASEDIR}/conf/httpd.conf")
      or die "Error: Can't open $servers[$i]{$BASEDIR}conf/httpd.conf: $!\n";

    # Write the directory locations, servername etc.
    print HTTPD "# Start generated httpd.conf ----------------------------\n";
    print HTTPD "ServerName   $servers[$i]{$HOSTNAME}:$servers[$i]{$PORT}\n";
    print HTTPD "Listen       $servers[$i]{$PORT}\n";
    print HTTPD "ServerRoot   $servers[$i]{$BASEDIR}\n";
    print HTTPD "DocumentRoot $servers[$i]{$BASEDIR}/htdocs\n";
    print HTTPD "PidFile      logs/httpd.pid\n";
    print HTTPD "ErrorLog     logs/error.log\n";
    print HTTPD "TransferLog  logs/access.log\n";
    print HTTPD "TypesConfig  $MIME_TYPES_LOC\n";
    print HTTPD "\n";
    print HTTPD "GlobuleRedirectionMode $servers[$i]{$REDIR_MODE}\n";
    print HTTPD "GlobuleDefaultRedirectPolicy RR\n";
    print HTTPD "GlobuleAdminUrl $servers[$i]{$BASEURI}/globulectl/\n";
    print HTTPD "GlobuleHeartBeatInterval 3secs\n";
    print HTTPD "GlobuleDebugProfile verbose\n";
#    print HTTPD "GlobuleMonitor 0 trace\n";
#    print HTTPD "GlobuleMonitor 1 trace\n";
#    print HTTPD "GlobuleMonitor 2 trace\n";
#    print HTTPD "GlobuleMonitor 3 trace\n";
#    print HTTPD "GlobuleMonitor 4 trace\n";
#    print HTTPD "GlobuleMonitor 5 trace\n";
#    print HTTPD "GlobuleMonitor 6 trace\n";
#    print HTTPD "GlobuleMonitor 7 trace\n";
    print HTTPD "\n";

    if (defined $servers[$i]{$HAS_VHOST} && $servers[$i]{$HAS_VHOST} == 1) {
      print HTTPD "NameVirtualHost *";
      if ($servers[$i]{$PORT} != 80) {
        print HTTPD ":$servers[$i]{$PORT}";
      }
      print HTTPD "\n\n";
    }

    # print out the sections
    foreach my $role (@{$servers[$i]{$CONTAINS}}) {
      print_role($role, \$cnt);
    }
    close(HTTPD);
  }
}


# Generate the origin test documents
sub init_origin_docs {

  my $origin = shift;
  if (not defined $origin->{$NR_DOCS}) {
    $origin->{$NR_DOCS} = $default_nr_docs;
  }

  my $baseloc = $origin->{$SERVER}{$BASEDIR}."/htdocs".
                $origin->{$EXPDIR};

  # Create directory if it does not exist yet
  if (! -e $baseloc) {
    mkdir($baseloc);
  }

  # Generate 1..nr_docs.html files in origin's export directory.
  for (my $i=0; $i < $origin->{$NR_DOCS}; $i++) {
    open(DOC, '>', "$baseloc/$i.html")
      or print STDERR "ERROR: Can't open $baseloc/$i.html: $!\n";

    print DOC "TEST SUITE: ".$origin->{$SERVER}{$BASEDIR}."\n";

    my $nr_of_lines = int(rand($max_nr_lines)) + 1;
    for (my $j=0; $j < $nr_of_lines; $j++) {
      print DOC "$j. AAAAAAAAAAAAAAAAAAAAAAAAAAIIIIIIIIIIIIIIIIIIIIIIIIIIII\r\n";
    }
    close(DOC);
  }
}

sub generate_doc {
  my $origin = shift;
  my $filename = shift;
  $filename = $origin->{$SERVER}{$BASEDIR} . "/htdocs" . $origin->{$EXPDIR}
            . $filename;
  system("mkdirhier `dirname $filename`");
  open(DOC, ">$filename") or print STDERR "ERROR: Can't open $filename: $!\n";
  print DOC "TEST SUITE: ".$origin->{$SERVER}{$BASEDIR}."\n";
  my $nr_of_lines = int(rand($max_nr_lines)) + 1;
  for (my $j=0; $j < $nr_of_lines; $j++) {
    print DOC "$j. AAAAAAAAAAAAAAAAAAAAAAAAAAIIIIIIIIIIIIIIIIIIIIIIIIIIII\r\n";
  }
  close(DOC);
}

# Stop the test script
sub my_exit {
  my $temp = shift;
  my @servers = @$temp;

  if ($WIN32) {
    print "\nHit CTRL-C to kill all Apache servers and this script.\n";
    sleep();
  }

  foreach my $server (@servers) {
    stop_server($server);
  }
}

sub my_sleep {
  my $time = shift;
  $| = 1;
  print "Waiting $time seconds...";
  while ($time > 0) {
    sleep(1);
    $time--;
    print "\rWaiting $time seconds...";
  }
  print "\n";
}


# Start the Apache server
sub start_server {

  my $server = shift;
  print "Starting Apache: ".$server->{$BASEDIR}."\n";

  if ($WIN32) {
    my $pid = fork();
    if ($pid == 0) {
      `bash -c "$APACHE -f $server->{$BASEDIR}/conf/httpd.conf"`;
      print "Stopping Apache: ".$server->{$BASEDIR}."\n";
      exit(0);
    } else {
      $server->{'pid'} = $pid;
    }
  } else {
    `$APACHE -f $server->{$BASEDIR}/conf/httpd.conf -k start`;
  }
}


# Stop the Apache server
sub stop_server {
  
  my $server = shift;
  print "Stopping Apache: ".$server->{$BASEDIR}."\n";

  if ($WIN32) {
    # Can't seem to get the kill switch right...
    print STDERR "Apache: ".$server->{$BASEDIR}." was forked with PID: ";
    print STDERR $server->{'pid'}."\n";
    print STDERR "Try killing it...\n";
#    print STDERR "Sorry, you'll have to manually stop ".$server->{$BASEDIR}."\n";
#    kill(20, $server->{'pid'}); #otherwise 2
  } else {
    `$APACHE -f $server->{$BASEDIR}/conf/httpd.conf -k stop`;
  }
}

# Retrieve a document from a particular server and origin/replica relative
sub get_doc {

  my ($role, $doc, $expected) = @_;
  if (not defined $expected) {
    $expected = 200;
  }
  my $full_doc;
  if ($role->{$ROLE} eq $ORIGIN) {
    $full_doc = $role->{$EXPDIR}.$doc;
  } elsif ($role->{$ROLE} eq $REPLICA) {
    $full_doc = $role->{$IMPDIR}.$doc;
  } elsif ($role->{$ROLE} eq $REDIR) {
    $full_doc = $role->{$IMPDIR}.$doc;
  }
  get_doc_full($role, $full_doc, $expected);
}


# Retrieve a document from a particular server full path specified
sub get_doc_full {

  my ($role, $doc, $expected) = @_;

  # Default: expect normal doc retrieval
  if (not defined $expected) {
    $expected = 200;
  }

  # Create the UserAgent
  my $ua = LWP::UserAgent->new;

  # Do not follow redirects (simple_request should also work iso request).
  $ua->max_redirect(0);

  # Construct the URL to be retrieved
  my $url = $role->{$SERVER}{$BASEURI}.$doc;
  print STDERR "GET $url\n";

  # Create the request object
  my $request = HTTP::Request->new("GET", $url);
  
  # Request the document
  my $response = $ua->request($request);

  # Check out the response
  print STDERR "==> ".$response->code."\n";
  if ($response->code != $expected
      && !$response->is_redirect) {
    print STDERR "ERROR: Expected $expected, but got ".$response->code."\n";
    print STDERR "       for url: $url\n";
    return;
  }

  # Were we redirected ?
  if ($response->is_redirect) {

    print STDERR "    REDIRECT: to ".$response->header('Location')."\n";
    # Follow the redirect, expect 200 then
    $request = HTTP::Request->new("GET", $response->header('Location'));
    $response = $ua->request($request);
    print STDERR "    ==> ".$response->code."\n";

    if ($response->is_redirect) {

      print STDERR "WARNING: Redirected again\n";
      print STDERR "        REDIRECT: to ".$response->header('Location')."\n";
      $request = HTTP::Request->new("GET", $response->header('Location'));
      $response = $ua->request($request);
      print STDERR "        ==> ".$response->code."\n";

      if ($response->code != 200) {
          print STDERR "ERROR: Expected 200 after redirect, but got ".
                       $response->code."\n";
          return;
      }
    }
  }

  # Check the content with the original document
  doc_check_ok($role, $response, $doc);
}


# Compare document from the origin with retrieved document.
sub doc_check_ok {

  my ($role, $response, $doc) = @_;

  # Decode hexadecimal numbers in document
  my $temp;
  while ($doc =~ /(%(\w{2}))/) {
    $temp = pack("H2", $2);
    $doc =~ s/$1/$temp/g;
  }

  if ($role->{$ROLE} eq $ORIGIN) {
    # We fetched a document from the origin
    if (origin_doc_is_local($role->{$SERVER}{$HOSTNAME})) {
      return check_local_doc($role, $response, $doc);
    } else {
      print STDERR "    Document assumed to be correct, request for origin doc
not on this server.\n";
      return 1;
    }
  } elsif ($role->{$ROLE} eq $REPLICA) {
    if (origin_doc_is_local($role->{$ORIGIN}{$SERVER}{$HOSTNAME})) {
      return check_local_doc($role, $response, $doc);
    } else {
      return check_remote_doc($role, $response, $doc);
    }
  } elsif ($role->{$ROLE} eq $REDIR) {
    if (origin_doc_is_local($role->{$ORIGIN}{$SERVER}{$HOSTNAME})) {
      return check_local_doc($role, $response, $doc);
    } else {
      return check_remote_doc($role, $response, $doc);
    }
  } elsif ($role->{$ROLE} eq $BACKUP) {
    if (origin_doc_is_local($role->{$ORIGIN}{$SERVER}{$HOSTNAME})) {
      return check_local_doc($role, $response, $doc);
    } else {
      return check_remote_doc($role, $response, $doc);
    }
  } else {
    print STDERR "ERROR: Unsupported role: ".$role->{$ROLE}."\n";
    return 0;
  }

  return 1;
}


# Check origin document that can be found on this host
sub check_local_doc {
  
  my ($role, $response, $doc) = @_;

  $doc .= "index.html"  if ($doc =~ m!/$!);

  # What is the real path
  my $fulldoc;
  if ($role->{$ROLE} eq $ORIGIN) {
    $fulldoc = $role->{$SERVER}{$BASEDIR}."/htdocs$doc";
  } else {
    $doc =~ s/$role->{$IMPDIR}/$role->{$ORIGIN}{$EXPDIR}/;
    $fulldoc = $role->{$ORIGIN}{$SERVER}{$BASEDIR}."/htdocs$doc";
  }
  
  open(DOC, $fulldoc)
    or print STDERR "ERROR: Could not open origin doc: $fulldoc\n";

  # Compare file contents
  my @lines = split(/^/, $response->content);
  my $count = 0;

  while (<DOC>) {
    if (not defined $lines[$count]) {
      # content has less lines than original
      print STDERR "ERROR: Original has MORE lines than retrieved document\n";
      return 0;
    }

    if ($lines[$count] ne $_) {
      print STDERR "ERROR: line $count of file $fulldoc mismatch\n";
      print STDERR "  Got: $lines[$count]";
      print STDERR " Orig: $_";
      return 0;
    }
    $count++;
  }
  close(DOC);

  if ($count != ($#lines+1)) {
    # content has more lines than original
    print STDERR "ERROR: Original has LESS lines than retrieved document\n";
    return 0;
  }

  # They are the same
  return 1;
}

# Check origin document not on this host
sub check_remote_doc {

  my ($role, $response, $doc) = @_;
  my $nr_of_tries = 5;  # try 5 times before giving up.

  # Try to retrieve the origin document;
  # Construct the URL to be retrieved
  $doc =~ s/$role->{$IMPDIR}/$role->{$ORIGIN}{$EXPDIR}/;
  my $url = $role->{$ORIGIN}{$SERVER}{$BASEURI}.$doc;

  # Create the UserAgent
  my $ua = LWP::UserAgent->new;

  # Do not follow redirects (simple_request should also work iso request).
  $ua->max_redirect(0);

  # Try to get the origin doc
  for (my $i=0; $i < $nr_of_tries; $i++) {
    # Create the request object
    my $request = HTTP::Request->new("GET", $url);
  
    # Request the document
    my $origin_resp = $ua->request($request);
    
    if ($origin_resp->code != 200) {
      next;
    }

    # compare
    my $cmp = $origin_resp->content cmp $response->content;
    if ($cmp == 0) {
      return 1;
    } elsif ($cmp < 0) {
      print STDERR "ERROR: Original has LESS bytes than retrieved document\n";
      return 0;
    } else {
      print STDERR "ERROR: Original has MORE bytes than retrieved document\n";
      return 0;
    }
  }

  print STDERR "ERROR: Was not able to get origin doc in $nr_of_tries tries\n";
  return 0;
}

# Given the hostname of the origin server, check if it is on this computer.
sub origin_doc_is_local {

  my ($hostname) = @_;

  foreach my $name (@my_hostnames) {
    if ($hostname eq $name) {
      return 1;
    }
  }
  return 0;
}

# Updates the origin document
sub update_doc {

  my ($origin, $doc) = @_;
  
  if (!origin_doc_is_local($origin->{$SERVER}{$HOSTNAME})) {
    print STDERR "ERROR: Can't update origin document, not on this server\n";
    return;
  }

  my $fulldoc = $origin->{$SERVER}{$BASEDIR}."/htdocs".
                $origin->{$EXPDIR}.$doc;

  open(DOC, ">>", $fulldoc)
    or print STDERR "ERROR: Could not open origin doc: $fulldoc\n";

  print DOC "Extra line\n";
  close(DOC);
}

# Actually flood the origin/replica etc.
sub flood_role {

  my $role = shift;
  my ($base_url);
  my ($nr_bytes, $nr_retrieved, $nr_updates, $nr_docs, $can_update);
  $nr_bytes = $nr_retrieved = $nr_updates = 0;

  my $ua = LWP::UserAgent->new;

  srand(28041977);

  if ($role->{$ROLE} eq $ORIGIN) {
    $base_url = $role->{$SERVER}{$BASEURI}.$role->{$EXPDIR};
    $nr_docs = $role->{$NR_DOCS};
    $can_update = 1;
  } elsif ($role->{$ROLE} eq $REPLICA) {
    $base_url = $role->{$SERVER}{$BASEURI}.$role->{$IMPDIR};
    $nr_docs = $role->{$ORIGIN}{$NR_DOCS};
    $can_update = 0;
  } elsif ($role->{$ROLE} eq $REDIR) {
    $base_url = $role->{$SERVER}{$BASEURI}.$role->{$IMPDIR};
    $nr_docs = $role->{$ORIGIN}{$NR_DOCS};
    $can_update = 0;
  }

  # Synchronize
  $semaphore->down();

  my ($doc, $url);
  my $t0 = [gettimeofday];
  for (my $i = 0; $i < $default_nr_iterations; $i++) {
    $doc = int(rand($nr_docs)).".html";
    $url = $base_url.$doc;
    if ($can_update && (rand() < 0.125)) {
      # update doc in $base_dir.$doc.html
      update_doc($role, $doc);
      $nr_updates++;
    } else {
      my $request = HTTP::Request->new("GET", $url);
      my $response = $ua->request($request);
      if ($response->is_success) {
        $nr_retrieved++;
        $nr_bytes += length($response->content);
      } else {
        print STDERR "ERROR [".threads->tid()."]: $url\n";
        print STDERR "ERROR [".threads->tid()."]: ".$response->status_line."\n";
      }
    }
  }
  my $t1 = [gettimeofday];
  my $elapsed = tv_interval $t0, $t1;

  return [$nr_bytes, $nr_retrieved, $nr_updates, $elapsed];
}

# Floodtest using multiple threads.
sub flood_test {

  my $temp = shift;
  my @to_flood = @$temp;
  my $nr_roles = $#to_flood+1;
  my @threads;
  my $thread_data;
  my ($tot_nr_bytes, $tot_nr_retrieved, $tot_nr_updates, $tot_elapsed);
  $tot_nr_bytes = $tot_nr_retrieved = $tot_nr_updates = $tot_elapsed = 0;

  for (my $i=0; $i < $nr_roles; $i++) {
    $threads[$i] = threads->create("flood_role", $to_flood[$i]);
  }

  # Synchronize
  my $t0 = [gettimeofday];
  $semaphore->up($nr_roles);
  for (my $i=0; $i < $nr_roles; $i++) {
    $thread_data = $threads[$i]->join();
    my ($nr_bytes, $nr_retrieved, $nr_updates, $elapsed) = @$thread_data;

    # Do something with it
    print "\nThread $i (".$to_flood[$i]{$ROLE}."):\n";
    print "- elapsed time: ", int($elapsed+0.5)," secs\n";
    print "- requests/sec: ",sprintf("%.01f",($default_nr_iterations/$elapsed)),"\n";
    print "- nr of bytes : $nr_bytes\n";
    print "- nr retrieved: $nr_retrieved\n";
    print "- nr updates  : $nr_updates\n";

    $tot_nr_bytes += $nr_bytes;
    $tot_nr_retrieved += $nr_retrieved;
    $tot_nr_updates += $nr_updates;
  }
  my $t1 = [gettimeofday];
  $tot_elapsed = tv_interval $t0, $t1;

  print "-----------------------------------------------------------------\n";
  print "Totals of $nr_roles threads:\n";
  print "- elapsed time: ",int($tot_elapsed+0.5)," secs\n";
  print "- requests/sec: ",sprintf("%.01f",(($default_nr_iterations*$nr_roles)/$tot_elapsed)),"\n";
  print "- nr of bytes : $tot_nr_bytes\n";
  print "- nr retrieved: $tot_nr_retrieved\n";
  print "- nr updates  : $tot_nr_updates\n";
}


sub print_role {

  my ($role, $cnt) = @_;

  if ($role->{$ROLE} eq $VHOST) {
    print_vhost_section_header($role, $cnt);
    foreach my $vrole (@{$role->{$CONTAINS}}) {
      print_role($vrole, $cnt);
    }
    print_vhost_section_footer($role, $cnt);
    return;
  }

  if ($role->{$ROLE} eq $ORIGIN) {
    # Write a origin section
    print_origin_section_header($role, $cnt);
    print_origin_replica_section($role, $cnt);
    print_origin_redirector_section($role, $cnt);
    print_origin_backup_section($role, $cnt);
    print_origin_section_footer($role, $cnt);
  } elsif ($role->{$ROLE} eq $REPLICA) {
    # Write a replica section
    print_replica_section_header($role, $cnt);
    print_replica_backup_section($role, $cnt);
    print_replica_section_footer($role, $cnt);
  } elsif ($role->{$ROLE} eq $REDIR) {
    # Write a redirector section
    print_redirector_section($role, $cnt);
  } elsif ($role->{$ROLE} eq $BACKUP) {
    # Write a backup section
    print_backup_section($role, $cnt);
  } else {
    print STDERR "ERROR: Role ".$role->{$ROLE}." not yet supported.\n";
  }
}


sub print_vhost_section_header {

  my ($vhost, $cnt) = @_;
 
  # No port number support yet...
  if (not defined $vhost->{$HOSTNAME}) {
    $vhost->{$HOSTNAME} = $vhost->{$SERVER}{$HOSTNAME};
  }
  if (not defined $vhost->{$PORT}) {
    $vhost->{$PORT} = $vhost->{$SERVER}{$PORT};
  }
  if (not defined $vhost->{$BASEDIR}) {
    $vhost->{$BASEDIR} = $vhost->{$SERVER}{$BASEDIR}."/";
    $vhost->{$BASEDIR} .= $vhost->{$HOSTNAME}."_".$vhost->{$PORT};
  }

  # Set base_uri
  if ($vhost->{$PORT} != 80) {
    $vhost->{$BASEURI} = "http://".$vhost->{$HOSTNAME}.":".
                                    $vhost->{$PORT};
  } else {
    $vhost->{$BASEURI} = "http://".$vhost->{$HOSTNAME};
  }
 
  if (! -e $vhost->{$BASEDIR}) {
    mkdir($vhost->{$BASEDIR});
  }

  if (! -e $vhost->{$BASEDIR}."/logs") {
    mkdir($vhost->{$BASEDIR}."/logs");
  }

  if (! -e $vhost->{$BASEDIR}."/htdocs") {
    mkdir($vhost->{$BASEDIR}."/htdocs");
  }

  # Create an index.html file for this VirtualHost
  if (! -e $vhost->{$BASEDIR}."/htdocs/index.html") {
    open(DOC, '>', $vhost->{$BASEDIR}."/htdocs/index.html")
      or die "ERROR: Can't open ".$vhost->{$BASEDIR}."/htdocs/index.html: $!\n";
    print DOC "<HTML><BODY>\n";
    print DOC "TEST SUITE: ".$vhost->{$BASEDIR}."\n";
    print DOC "</BODY></HTML>\n";
    close(DOC);
  }

  print HTTPD "<VirtualHost *";
  print HTTPD ":$vhost->{$PORT}";
  print HTTPD ">\n";
  print HTTPD "  ServerName   $vhost->{$HOSTNAME}:$vhost->{$PORT}\n";
  print HTTPD "  DocumentRoot $vhost->{$BASEDIR}/htdocs\n";
  print HTTPD "  ErrorLog     $vhost->{$BASEDIR}/logs/error_log\n";
  print HTTPD "  TransferLog  $vhost->{$BASEDIR}/logs/access_log\n";
}

sub print_vhost_section_footer {

  my ($vhost, $cnt) = @_;

  print HTTPD "</VirtualHost>\n\n";
}

sub print_origin_section_header {

  my ($origin, $cnt) = @_;

  print HTTPD "<Location ".$origin->{$EXPDIR}.">\n";
  print HTTPD "  GlobuleReplicate on\n";
}

sub print_origin_replica_section {

  my ($origin, $cnt) = @_;
 
  # Go over all the replicas for this origin
  foreach my $replica (@{$origin->{$REPLICA}}) {
    print HTTPD "  GlobuleReplicaIs ";
    print HTTPD $replica->{$SERVER}{$BASEURI};
    print HTTPD $replica->{$IMPDIR};
    if (not defined $replica->{$PASSWORD}) {
      $replica->{$PASSWORD} = "password$$cnt";
      $$cnt++;
    }
    print HTTPD " ".$replica->{$PASSWORD}."\n";
  }

}

sub print_origin_backup_section {

  my ($origin, $cnt) = @_;

  # Go over all backups for this origin
  foreach my $backup (@{$origin->{$BACKUP}}) {
    print HTTPD "  GlobuleBackupIs ";
    print HTTPD $backup->{$SERVER}{$BASEURI};
    print HTTPD $backup->{$IMPDIR};
    if (not defined $backup->{$PASSWORD}) {
      $backup->{$PASSWORD} = "password$$cnt";
      $$cnt++;
    }
    print HTTPD " ".$backup->{$PASSWORD}."\n";
  }
}

sub print_origin_redirector_section {

  my ($origin, $cnt) = @_;
  my $hasRedir = 0;

  # Go over all the redirectors for this origin
  foreach my $redir (@{$origin->{$REDIR}}) {
    print HTTPD "  GlobuleRedirectorIs ";
    print HTTPD $redir->{$SERVER}{$BASEURI};
    print HTTPD $redir->{$IMPDIR};
    if (not defined $redir->{$PASSWORD}) {
      $redir->{$PASSWORD} = "password$$cnt";
      $$cnt++;
    }
    print HTTPD " ".$redir->{$PASSWORD}."\n";
    $hasRedir = 1;
  }

  if ($hasRedir) {
    # Origin should add himself to the replica list for the redirector
    # To know about him.
    print HTTPD "  GlobuleReplicaIs ";
    print HTTPD $origin->{$SERVER}{$BASEURI};
    print HTTPD $origin->{$EXPDIR};
    print HTTPD " DummyPassWord$$cnt\n";
  }
}

sub print_origin_section_footer {

  my ($origin, $cnt) = @_;
  
  print HTTPD "  GlobuleDefaultReplicationPolicy Invalidate\n";
  print HTTPD "  GlobuleDefaultRedirectPolicy RR\n";
  print HTTPD "</Location>\n\n";
}


sub print_replica_section_header {

  my ($replica, $cnt) = @_;

  print HTTPD "<Location ".$replica->{$IMPDIR}.">\n";
  print HTTPD "  GlobuleReplicaFor ";
  print HTTPD $replica->{$ORIGIN}{$SERVER}{$BASEURI};
  print HTTPD $replica->{$ORIGIN}{$EXPDIR};
  if (not defined $replica->{$PASSWORD}) {
    $replica->{$PASSWORD} = "password$$cnt";
    $$cnt++;
  }
  print HTTPD " ".$replica->{$PASSWORD}."\n";

}

sub print_replica_backup_section {
  
  my ($replica, $cnt) = @_;
 
  # Go over all backups for this replica
  foreach my $backup (@{$replica->{$ORIGIN}{$BACKUP}}) {
    print HTTPD "  GlobuleBackupForIs ";
    print HTTPD $replica->{$ORIGIN}{$SERVER}{$BASEURI};
    print HTTPD $replica->{$ORIGIN}{$EXPDIR};
    print HTTPD " ".$backup->{$SERVER}{$BASEURI};
    print HTTPD $backup->{$IMPDIR}."\n";
  }
}

sub print_replica_section_footer {
 
  my ($replica, $cnt) = @_;

  print HTTPD "</Location>\n\n";
} 


sub print_redirector_section {
        
  my ($redir, $cnt) = @_;

  print HTTPD "<Location ".$redir->{$IMPDIR}.">\n";
  print HTTPD "  GlobuleRedirectorFor ";
  print HTTPD $redir->{$ORIGIN}{$SERVER}{$BASEURI};
  print HTTPD $redir->{$ORIGIN}{$EXPDIR};
  if (not defined $redir->{$PASSWORD}) {
    $redir->{$PASSWORD} = "password$$cnt";
    $$cnt++;
  }
  print HTTPD " ".$redir->{$PASSWORD}."\n";
  print HTTPD "</Location>\n\n";
}


sub print_backup_section {

  my ($backup, $cnt) = @_;

  print HTTPD "<Location ".$backup->{$IMPDIR}.">\n";
  print HTTPD "  GlobuleBackupFor ";
  print HTTPD $backup->{$ORIGIN}{$SERVER}{$BASEURI};
  print HTTPD $backup->{$ORIGIN}{$EXPDIR};
  if (not defined $backup->{$PASSWORD}) {
    $backup->{$PASSWORD} = "password$$cnt";
    $$cnt++;
  }
  print HTTPD " ".$backup->{$PASSWORD}."\n";
  print HTTPD "</Location>\n\n";
}

END {}

1;  # don't forget to return a true value from the file
