#!/bin/sh
rm -f debugshm.dat debugshm.avail
rm -f debugshm.*.gif
./debugshm.pl < /tmp/shm.log

# Free size of more than 32 bytes
