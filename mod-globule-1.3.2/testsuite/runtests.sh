#!/bin/sh
cd `dirname $0`

rtcode=0
quiet=0
keep=0
while [ $# -gt 0 ] ; do
  case $1 in
  -k)
    keep=1
    shift
    ;;
  -q)
    quiet=1
    shift
    ;;
  -h)
    echo ""
    echo "Script to run all registered tests."
    echo "Usage: %0 [-h] [-q] [-k]"
    echo "  -h  displays this message"
    echo "  -q  script remains quiet for tests passed"
    echo "  -k  keeps output files also for passed tests"
    echo ""
    exit 0
    ;;
  *)
    echo "$0: unrecognized argument $1"
    exit 1
    ;;
  esac
done

for f in *.pl ; do
  o=`basename $f .pl`.out
  rm -f $o
  if [ -x $f ] ; then
    pids=`ps auxww | grep apache/bin/httpd | grep -v grep | awk '{print $2}'`
    if [ -n "$pids" ]; then
      kill -9 $pids 2> /dev/null
    fi
    ids=`ipcs -s | grep $USER | awk '{print $2}'`
    if [ -n "$ids" ]; then
      ipcrm sem $ids 2> /dev/null > /dev/null
    fi
    sleep 3
    coredumps=0
    coredumps=`find . -name core\* -print | wc -l`
    ./$f 2>&1 | tee $o 2>/dev/null | grep -q ERROR
    if [ $? -eq 0 ] ; then
      echo "test $f FAILED; output in $o"
      rtcode=1
    else
      if [ $quiet -eq 0 ]; then
        grep -q WARNING $o
        if [ $? -eq 0 ] ; then
	  if [ $coredumps -ne 0 ]; then
            echo "test $f passed, with warnings and core dumps"
	  else
            echo "test $f passed, with warnings"
	  fi
        else
	  if [ $coredumps -ne 0 ]; then
            echo "test $f passed, but core dumps where detected"
	  else
            echo "test $f passed"
	  fi
        fi
      fi
      if [ $keep -eq 0 ]; then
        rm $o
      fi
    fi
  fi
done

exit $rtcode
