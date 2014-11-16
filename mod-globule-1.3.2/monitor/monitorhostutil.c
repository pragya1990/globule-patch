/*
Copyright (c) 2003-2006, Vrije Universiteit
All rights reserved.

Redistribution and use of the GLOBULE system in source and binary forms,
with or without modification, are permitted provided that the following
conditions are met:

   * Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.

   * Redistributions in binary form must reproduce the above
     copyright notice, this list of conditions and the following
     disclaimer in the documentation and/or other materials provided
     with the distribution.

   * Neither the name of Vrije Universiteit nor the names of the software
     authors or contributors may be used to endorse or promote
     products derived from this software without specific prior
     written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS, AUTHORS, AND
CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL VRIJE UNIVERSITEIT OR ANY AUTHORS OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This product includes software developed by the Apache Software Foundation
<http://www.apache.org/>.
*/
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/poll.h>
#include <fcntl.h>
#include "monitorhost.h"

char* argv0;

int
main(int argc, char* argv[])
{
  int status, sock, bufidx;
  struct sockaddr_in sockaddr;
  struct pollfd polls[2];
  char buffer[65536];
  struct hostent *hent;
  size_t nbytes;
  ssize_t written;

  /* Get the name of the program */
  if((argv0 = strrchr(argv[0],'/')) == NULL)
    argv0 = argv[0];
  else
    ++argv0;

  if(argc != 3) {
    fprintf(stderr,"%s: bad number of arguments\n",argv0);
    printf("usage: %s hostname portnum\n",argv[0]);
    exit(1);
  }

  CHECK((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0);
  CHECK((hent = gethostbyname(argv[1])) == NULL);
  sockaddr.sin_family      = AF_INET;
  sockaddr.sin_addr        = *(struct in_addr *)hent->h_addr_list[0];
  sockaddr.sin_port        = htons(atoi(argv[2]));
  CHECK(connect(sock, (struct sockaddr *)&sockaddr, sizeof(sockaddr)));
  CHECK(fcntl(sock, F_SETFL, O_NONBLOCK));
  printf("> connected.\n");

  for(;;) {
    polls[0].fd = 0;
    polls[0].revents = 0;
    polls[0].events  = POLLIN|POLLERR|POLLHUP;
    polls[1].fd = sock;
    polls[1].revents = 0;
    polls[1].events  = POLLIN|POLLERR|POLLHUP;
    CHECK((status = poll(polls, 2, -1)) < 0);
    if((polls[0].revents&(POLLHUP|POLLERR)) ||
       (polls[1].revents&(POLLHUP|POLLERR)))
      break;
    if(polls[0].revents & POLLIN) {
      polls[0].revents &= ~POLLIN;
      nbytes = read(0,buffer,sizeof(buffer));
      CHECK(write(sock,buffer,nbytes) != nbytes);
    }
    if(polls[1].revents & POLLIN) {
      polls[1].revents &= ~POLLIN;
      if((nbytes = read(sock,buffer,sizeof(buffer))) == 0) {
        printf("> disconnected.\n");
        break;
      }
      printf("> ");
      fflush(stdout);
      for(bufidx=0; bufidx<nbytes; bufidx+=written)
        CHECK((written = write(1,&buffer[bufidx],nbytes-bufidx)) <= 0);
    }
    if(polls[0].revents)
      BUG(fprintf(stderr,"assertion error: still events on tty channel\n"));
    if(polls[1].revents)
      BUG(fprintf(stderr,"assertion error: still events on channel 0\n"));
  }

  if(sock) {
    shutdown(sock, SHUT_RDWR);
    close(sock);
    sock = -1;
  }

  exit(0);
}
