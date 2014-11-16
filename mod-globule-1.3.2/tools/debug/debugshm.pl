#!/usr/bin/env perl

$count = 0;
while(<>) {
  chomp;
  if(m/SHM [A-Z].*$/) {
    if(m/^SHM ([A-Z]+) ([^:]+):(\d+) ([^\(]*)\(used: (\d+) bytes in (\d+) objects\)\s*$/) {
      $fname      = $2;
      $fline      = $3;
      $_          = $1 . " " . $4;
      $bytesinuse = $5;
      $itemsinuse = $6;
    } else {
      die "bad input";
    }
    MATCH: {
      m/^STRING (\d+) (\d+)\s*$/ && do {
        $addr = $1;
        $size = $2;
        $mem{$addr} = $size;
	$who{$addr} = $fname . ":" . $fline;
        last MATCH;
      };
      m/^ALLOC (\d+) (\d+)\s*$/ && do {
        $addr = $1;
        $size = $2;
        $mem{$addr} = $size;
	$who{$addr} = $fname . ":" . $fline;
        last MATCH;
      };
      m/^REALLOC (\d+)->(\d+) (\d+)\s*$/ && do {
        $addr = $1;
        delete $mem{$addr};
	delete $who{$addr};
        $addr = $2;
        $size = $3;
        $mem{$addr} = $size;
	$who{$addr} = $fname . ":" . $fline;
        last MATCH;
      };
      m/^DEALLOC (\d+)\s*$/ && do {
        $addr = $1;
        delete $mem{$addr};
	delete $who{$addr};
        last MATCH;
      };
      die "bad input: $_";
    }
    if(($count++) % 1000 == 0) {
      $addrmin = -1;
      foreach $addr (keys %mem) {
        $addrmin = $addr  if($addrmin < 0 || $addrmin > $addr);
      }
      open(OUT,">debugshm.dat");
      foreach $addr (sort (keys %mem)) {
        $addr2 = $addr + $mem{$addr};
        print OUT $addr, " ", $mem{$addr}, " ",
        ($addr-$addrmin), " ", ($addr2-$addrmin), "\n";
      }
      close(OUT);
      system("sh ./debugshm.gp");
    }
  }
}

open(OUT,">debugshm.dat");
$addrmin = -1;
foreach $addr (keys %mem) {
  $addrmin = $addr  if($addrmin < 0 || $addrmin > $addr);
}
foreach $addr (sort (keys %mem)) {
  $addr2 = $addr + $mem{$addr};
  print OUT $addr, " ", $mem{$addr}, " ",
            ($addr-$addrmin), " ", ($addr2-$addrmin), " ", 
	    $who{$addr}, "\n";
}
close(OUT);

open(OUT,">debugshm.avail");
$addrlast=$addrmin;
foreach $addr (sort (keys %mem)) {
  print OUT $addrlast, " ", $addr, " ", $addr - $addrlast, "\n";
  $addrlast = $addr + $mem{$addr};
}
close(OUT);

#print "gnuplot" , "\n";
#print "plot [0:8388608][0:1] 'data' \\", "\n";
#print "  using ((\$3+\$4)/2):(1):(\$4-\$3>1024?\$4-\$3:1024) \\", "\n";
#print "  with boxes", "\n";
#print "pause -1  \"Hit return to continue\"\n";

exit 0;


