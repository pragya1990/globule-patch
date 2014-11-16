#!/bin/sh

n=`ls -1 | sed -e 's/^debugshm.\([0-9]*\).gif$/\1/p' -e d | tail -1`
if [ -z "$n" ]; then
  n=0
else
  n=`expr $n + 1`
fi
n=`printf '%03d' $n`

cat << END | gnuplot
set terminal gif large
set output 'debugshm.$n.gif'
plot [0:8388608][0:1] 'debugshm.dat' \
  using ((\$3+\$4)/2):(1):(\$4-\$3>1024?\$4-\$3:1024) \
  title "$n" with boxes
#pause -1  "Hit return to continue"
#set terminal postscript landscape
#set output 'debugshm.ps'
#replot
#set terminal png large color
#set output 'debugshm.png'
#replot
#set terminal gif large
#set output 'debugshm.gif'
#replot
END

#plot [0:8388608][0:1] 'debugshm.dat' \
#  using (($3+$4)/2):(1):($4-$3>1024?$4-$3:1024) \
#  title "plot.gif" with boxes
