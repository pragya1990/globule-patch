#!/usr/bin/perl
use threads;
use Thread::Queue;
use Thread::Semaphore;
use LWP::UserAgent;
use Time::HiRes qw(usleep ualarm gettimeofday tv_interval);
#$q1 = Thread::Queue->new;
#$q2 = Thread::Queue->new;
#$thr->detach;

my $semaphore = new Thread::Semaphore;
$niters=2000;
$nworkers=64;
$ndocuments=10000;
$documentsize=100;

sub worker {
    my $i, $nbytes, $nretrievals, $nupdates;

    $nbytes = $nretrievals = $nupdates = 0;
    $ua = LWP::UserAgent->new;
    $ua->agent("FloodTest/0.1");
    
    #srand(166277); # 17845
    $semaphore->down;

    for($i=0; $i<$niters; $i++) {
	$document = sprintf("%d",int(rand()*$ndocuments));
	$url = "http://site.revolutionware.net/site/" . $document;
	if(rand() < 0.0) {
	    #$atime = $mtime = time;
	    #utime $atime,$mtime,"/home/berry/globule/apache/htdocs/site/master/$document";
	    #++$nupdates;
	    $url = "http://site.revolutionware.net/site/robots.txt" . $document;
	} else {
	    print $url . "\n";
	    my $req = HTTP::Request->new(GET => $url);
	    my $res = $ua->request($req);
	    if(!$res->is_success) {
		print $res->status_line, "\n";
	    } else {
		++$nretrievals;
		$nbytes += length($res->content);
	    };
	};
	usleep(10000);
    };
    return [ $nbytes, $nretrievals, $nupdates ];
}

#srand(31237);
#for($i=0; $i<$ndocuments; $i++) {
#    $size = int(rand()*$documentsize) + $documentsize;
#    system("dd","if=/dev/urandom","of=/home/berry/globule/apache/htdocs/site/master/$i","bs=256","count=$size");
#    close(OUT);
#};

#print "Sleeping\n";
#sleep(20);
#print "Awoken\n";

for($i=0; $i<$nworkers; $i++) {
    $thrs[$i] = threads->new(\&worker);
};
$t0 = [gettimeofday];
$semaphore->up($nworkers);
$totnbytes = $totnretrievals = $totnupdates = 0;
for($i=0; $i<$nworkers; $i++) {
    $rtndata = $thrs[$i]->join;
    ($nbytes,$nretrievals,$nupdates) = @$rtndata;
    $totnbytes      += $nbytes;
    $totnretrievals += $nretrievals;
    $totnupdates    += $nupdates;
};
$t1 = [gettimeofday];
$elapsed = tv_interval $t0, $t1;

print "Total elapsed time in seconds : ", int($elapsed+0.5), "\n";
print "Number of requests per second : ",
    sprintf("%.01f",($totnretrievals)/$elapsed), "\n";
print "Milliseconds per request      : ",
    sprintf("%.03f",$elapsed/($totnretrievals)*1000), "\n";
print "Number of documents retrieved : ", $totnretrievals, "\n";
print "Number of documents updated   : ", $totnupdates, "\n";
print "Bytes data transferred        : ", $totnbytes, "\n";
print "          overall KB/s        : ", ($totnbytes/$elapsed/1024), "\n";

exit 0;
