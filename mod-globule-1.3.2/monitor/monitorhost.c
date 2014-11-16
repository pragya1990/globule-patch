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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/poll.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <signal.h>
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#include <string.h>
#include <syslog.h>
#include <assert.h>
#include "monitorhost.h"

extern char **environ;
char* argv0;

struct buffer {
  char *buffer;               /* unprocessed input from peer */
  ssize_t nbytes;             /* number of unprocessed bytes in buffer,
                                 or -1 for connect in progress */
  int size;                   /* current maximum size of buffer */
};

struct peer {
  struct peer *next, *prev;
  int fd;                     /* two-way connection channel with peer */
  int interval;               /* watchdog update requested every n millisecs */
  struct timeval lastupdate;  /* last time (gettimeofday) when sent watchdog */
  char *remaddr;
  int remport;
  struct buffer input;
  struct buffer output;
  void *user;
  void (*alarmfn)(void*);
  void (*connectfn)(void*);
  void (*disconnectfn)(void*);
  size_t (*consumefn)(void*,const char*,size_t);
};

struct peer *peers = 0;
int npeers = 0;
int wantsyslog = 0;
int loginterval = 0;
char *logfilename = 0;
int logfiledes;
int running = 1;
int debugging = 0;
int watchdoginterval = 0;
int reconnectinterval = 0;
char *watchdogcommand = NULL;
int port = DEFAULTPORT;
char *netdevice = "eth0";
int keepalive = 0;
int logcmdinterval = -1;
char *logcommand = NULL;

/****************************************************************************/

int getinteger(const char* arg, int *valptr);
const char *daemonize(char* rootdir, char* workdir, char* piddir);
const int milliseconds(struct timeval *t1, struct timeval *t2, int round);
const char *mktmpstring(const char* fmt, ...);

const int
milliseconds(struct timeval *t1, struct timeval *t2, int round)
{
  int msecs;
  if(round > 0)
    round = 999;
  else if(round < 0)
    round = 0;
  else if(round == 0)
    round = 500;
  if(t2->tv_sec - t1->tv_sec >= 86400)
    return INT_MAX;
  if(t2->tv_usec > t1->tv_usec) {
    msecs  = (t2->tv_usec - t1->tv_usec +  round) / 1000;
    msecs += (t2->tv_sec - t1->tv_sec) * 1000;
  } else {
    msecs  = (1000000 - t2->tv_usec + t1->tv_usec +  round) / 1000;
    msecs += (t2->tv_sec - t1->tv_sec - 1) * 1000;
  }
  return msecs;
}

const char *
mktmpstring(const char* fmt, ...)
{
  va_list ap;
  static char fname[FILENAME_MAX];
  va_start(ap, fmt);
  vsnprintf(fname, sizeof(fname), fmt, ap);
  va_end(ap);
  return fname;
}

const char *
daemonize(char* rootdir, char* workdir, char* piddir)
{
  int i, fd;
  pid_t pid;
  const char *s;
  if(getppid() == 1)
    return NULL; /* already a daemon */
  if((pid = fork()) < 0) {
    fprintf(stderr, "%s: cannot fork into daemon mode\n", argv0);
    return "cannot fork into daemon mode";
  }
  if(pid > 0)
    exit(0);
  pid = getpid();
  /* parent has now left the building, daemon child continues */
  setsid(); /* obtain a new process group */
  for(i=getdtablesize(); i>=0; --i)
    close(i);
  fd = open("/dev/null", O_RDWR);
  if(fd >= 0) {
    if(fd != 0) dup2(fd, 0);
    if(fd != 1) dup2(fd, 1);
    if(fd != 2) dup2(fd, 2);
    if(fd > 2) close(fd);
  }
  umask(027);
  s = mktmpstring("%s%s%s.lck",(piddir?piddir:""),(piddir?"/":""),argv0);
  if((fd = open(s, O_WRONLY|O_CREAT, 0666)) >= 0) {
    if(lockf(fd, F_TLOCK, 0))
      return "lockfile already locked";
    s = mktmpstring("%d\n",pid);
    write(fd,s,strlen(s));
  }
  if(rootdir)
    chroot(rootdir);
  if(workdir)
    chdir(workdir);
  signal(SIGTSTP,SIG_IGN); /* ignore tty signals */
  signal(SIGTTOU,SIG_IGN);
  signal(SIGTTIN,SIG_IGN);
  return NULL;
}

int
getinteger(const char* arg, int *valptr)
{
  char *endp;
  *valptr = strtol(arg, &endp, 0);
  if(arg == endp)
    return -1;
  while(*endp == ' ' || *endp == '\t')
    ++endp;
  if(*endp!='\0')
    return -1;
  return 0;
}

/****************************************************************************/

static struct peer *
allocpeer()
{
  struct peer *newpeer;
  CHECK(!(newpeer = malloc(sizeof(struct peer))));
  newpeer->next = peers;
  newpeer->prev = NULL;
  if(peers)
    peers->prev = newpeer;
  peers = newpeer;
  ++npeers;
  newpeer->fd       = -1;
  newpeer->interval = 0;
  CHECK(gettimeofday(&newpeer->lastupdate, NULL));
  newpeer->remaddr  = 0;
  newpeer->remport  = 0;
  newpeer->input.buffer = NULL;
  newpeer->input.size   = 0;
  newpeer->input.nbytes = 0;
  newpeer->output.buffer = NULL;
  newpeer->output.size   = 0;
  newpeer->output.nbytes = 0;
  newpeer->user         = newpeer;
  newpeer->alarmfn      = NULL;
  newpeer->connectfn    = NULL;
  newpeer->disconnectfn = NULL;
  newpeer->consumefn    = NULL;
  return newpeer;
}

static void
freepeer(struct peer *p)
{
  if(p->prev)
    p->prev->next = p->next;
  else
    peers = p->next;
  if(p->next)
    p->next->prev = p->prev;
  if(p->input.buffer)
    free(p->input.buffer);
  if(p->output.buffer)
    free(p->output.buffer);
  free(p);
}

static void
produce(struct peer* p, const char* buffer, size_t nbytes)
{
  if(!p->output.buffer) {
    CHECK(!(p->output.buffer = malloc(p->output.size = 2*4096)));
  } else if(p->output.size - p->output.nbytes < nbytes) {
    if(p->input.size + nbytes >= 65535) {
      close(p->fd);
      p->fd = -1;
      return;
    }
    CHECK(!(p->output.buffer = realloc(p->output.buffer,
                                       p->output.size += nbytes)));
  }
  memcpy(&p->output.buffer[p->output.nbytes], buffer, nbytes);
  p->output.nbytes += nbytes;
}

/****************************************************************************/

static void
watchdogcontact_connect(void *user)
{
  struct peer *p = user;
  char line[1024];
  sprintf(line,"watchdog %d\n",watchdoginterval);
  produce(p, line, strlen(line));
}

static size_t
watchdogcontact_consume(void *user, const char* buffer, size_t nbytes)
{
  int len, arg;
  size_t bytesdone = 0;
  struct peer *p = user;
  for(;;) {
    for(len=0; len<nbytes; len++)
      if(buffer[len] == '\n')
        break;
    if(len++ >= nbytes)
      break;
    if(buffer[0]>='A' && buffer[0]<='Z' && buffer[1]==' ') {
      if(logfilename)
        CHECK(write(logfiledes,buffer,nbytes) != nbytes);
      if(wantsyslog)
        syslog(LOG_USER,"%.*s",nbytes,buffer);
      if(debugging) {
        fprintf(stderr,"%.*s",nbytes,buffer);
      }
    } else {
      if(sscanf(buffer,"watchdog %d",&arg) == 1) {
        BUG(fprintf(stderr,"command received: watchdog request %d msecs\n",arg));
        p->interval = arg;
        /* force an immediate keep-alive */
        p->lastupdate.tv_sec = p->lastupdate.tv_usec = 0;
      } else if(!strncmp(buffer,"alive",strlen("alive"))) {
        BUG(fprintf(stderr,"command received: keep-alive\n"));
        if(++keepalive > 2)
          keepalive = 2;
      } else if(!strncmp(buffer,"shutdown",strlen("shutdown"))) {
        BUG(fprintf(stderr,"command received: shutdown\n"));
        keepalive = 0;
      } else if(!strncmp(buffer,"terminate",strlen("terminate"))) {
        BUG(fprintf(stderr,"command received: terminate\n"));
        running = 0;
      } else if(!strncmp(buffer,"echo",strlen("echo"))) {
        BUG(fprintf(stderr,"command received: echo\n"));
        produce(p,buffer,len); 
      } else {
        BUG(fprintf(stderr,"command received: unrecognized command\n"));
      }
    }
    buffer = &buffer[len];
    nbytes    -= len;
    bytesdone += len;
  }
  return bytesdone;
}

static void
watchdogcontact_alarm(void *user)
{
  struct peer *p = (struct peer *) user;
  produce(p, "alive\n", strlen("alive\n"));
}

/****************************************************************************/

static void
watchdogalarm(void *user)
{
  int status = -1;
  struct peer *p = (struct peer *) user;
  if(!keepalive--) {
    if(watchdogcommand) {
      status = system(watchdogcommand);
      if(status < 0) {
	if(debugging)
          fprintf(stderr, "%s: cannot execute watchdog command\n", argv0);
	else if(wantsyslog)
          syslog(LOG_ERR, "cannot execute watchdog command");
      } else {
        status = 0;
        if(WEXITSTATUS(status)) {
          p->interval = 0;
        }
      }
    }
    if(status < 0) {
      running = 0;
      p->interval = 0;
    }
  }
}


/****************************************************************************/

static char *logcmd_result = NULL;
static pid_t logcmd_child = 0;

static void
logcmd_alarm(void *user)
{
  int i, pipepair[2];
  struct peer *p = user;
  if(!logcmd_child) {
    pipe(pipepair);
    CHECK((logcmd_child = fork()) < 0);
    if(logcmd_child) {
      close(pipepair[1]);
      p->fd = pipepair[0];
    } else {
      /* keep stdin open, close all but output pipe */
      for(i=getdtablesize(); i>2; --i)
	if(i != pipepair[1])
	  close(i);
      close(1);
      if(pipepair[1] != 1)
	dup2(pipepair[1], 1);
      if(pipepair[1] != 1 && pipepair[1] != 2)
	close(pipepair[1]);
      execle("/bin/sh","/bin/sh","-c",logcommand,NULL,environ);
      exit(-1);
    }
  }
}

static size_t
logcmd_consume(void *user, const char* buffer, size_t size)
{
  int pos, end;

  for(end=pos=0; end<size; end++)
    if(buffer[end] == '\n' && end!=size-1)
      pos = end;
  if(buffer[end-1] == '\n')
    --end;
  if(logcmd_result)
    free(logcmd_result);
  logcmd_result = malloc(end-pos+1);
  memcpy(logcmd_result, buffer, end-pos);
  logcmd_result[end-pos] = '\0';
  return size;
}

static void
logcmd_disconnect(void *user)
{
  struct peer *p = user;
  int status;
  pid_t pid;
  close(p->fd);
  p->fd = -1;
  CHECK((pid = waitpid(logcmd_child, &status, WNOHANG)) < 0);
  if(pid == 0) {
    kill(logcmd_child, SIGKILL);
    CHECK((pid = waitpid(logcmd_child, &status, 0)) < 0);
  }
  if(WIFEXITED(status)) {
    if(WEXITSTATUS(status)) {
      BUG(fprintf(stderr,"log command exited with non-zero return status %d", WEXITSTATUS(status)));
    }
    logcmd_child = 0;
  } else if(WIFSIGNALED(status)) { 
    BUG(fprintf(stderr,"log command exited because of uncaught signal %d", WTERMSIG(status)));
    logcmd_child = 0;
  } else {
    BUG(fprintf(stderr,"log command with pid %d did still not exit",logcmd_child));
  }
}

/****************************************************************************/

static void
logging(void *user)
{
  struct peer *p;
  char *s, line[1024], fpline[256];
  int nloadavgs, len = 0;
  double loadavgs[3];
  FILE *fp;

  len += snprintf(&line[len], sizeof(line)-len-2,"L t=%ld server=",
		  (long)time(NULL));
  CHECK(gethostname(&line[len],sizeof(line)-len-2));
  len += strlen(&line[len]);

  strncpy(&line[len],".",sizeof(line)-len-2);
  CHECK(getdomainname(&line[len+strlen(&line[len])],
                      sizeof(line)-(len+strlen(&line[len]))-2));
  if(strcmp(&line[len],".") && strcmp(&line[len],".(none)") &&
     strcmp(&line[len],".localdomain")) {
    len += strlen(&line[len]);
  } else
    line[len] = '\0';

  if(loginterval > 15*60*1000)
    nloadavgs = 3;
  else if(loginterval > 5*60*1000)
    nloadavgs = 2;
  else
    nloadavgs = 1;

  nloadavgs = getloadavg(loadavgs,nloadavgs);
  if(nloadavgs > 0)
    len += snprintf(&line[len],sizeof(line)-len-2," loadavg=%d",
		    (int)(loadavgs[nloadavgs-1]*100.0));

  /* The following section opens a Linux-kernel specific file in
   * the /proc filesystem to get statistics on the network usage.
   * there is no portable alternative, though the section below
   * will simply be skipped if a non-linux system is used.
   */
  if((fp = fopen("/proc/net/dev","r"))) {
    while((s = fgets(fpline,sizeof(line),fp))) {
      while(*s == ' ')
	++s;
      if(!strncmp(s, netdevice, strlen(netdevice)) && s[strlen(netdevice)] == ':') {
	long dummy;
	long long int rxbytes, rxpackets, txbytes, txpackets;
	s += strlen(netdevice) + 1;
	if(sscanf(s,"%Lu%Lu%lu%lu%lu%lu%lu%lu%Lu%Lu",
		  &rxbytes, &rxpackets,
		  &dummy, &dummy, &dummy, &dummy, &dummy, &dummy,
		  &txbytes, &txpackets) == 10)
	  len += snprintf(&line[len],sizeof(line)-len-2,
			  " rxpackets=%Lu txpackets=%Lu rxbytes=%Lu txbytes=%Lu",
			  rxpackets,txpackets,rxbytes,txbytes);
	break;
      }
    }
    fclose(fp);
  }

  if(logcmd_result) {
    len += snprintf(&line[len],sizeof(line)-len-2," %s",logcmd_result);
    free(logcmd_result);
    logcmd_result = NULL;
  }

  strcpy(&line[len++],"\n");
  if(logfilename)
    write(logfiledes,line,len);
  if(wantsyslog)
    syslog(LOG_USER,"%s",line);
  if(debugging) {
    fprintf(stderr,"%s",line);
    fflush(stderr);
  }
  for(p=peers; p; p=p->next)
    if(p->remaddr && p->fd >= 0) {
      produce(p,line,len);
    }
}

/****************************************************************************/

static int
run()
{
  int acceptsock, peerfd;
  int yes = 1;
  struct sockaddr_in sockaddr;
  struct sockaddr_in remaddr;
  socklen_t addrlen;
  struct pollfd* polls;
  int maxnpolls, npolls;
  ssize_t nbytes;
  struct peer *p, *newpeer;
  int status, i;
  int msecs, deltat; /* milliseconds */
  struct timeval currenttv;
  struct hostent *hent;

  maxnpolls = 1;
  CHECK(!(polls = malloc(sizeof(struct pollfd))));

  CHECK((acceptsock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0);
  CHECK(setsockopt(acceptsock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)));
  sockaddr.sin_family      = AF_INET;
  sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  sockaddr.sin_port        = htons(port);
  CHECK(bind(acceptsock,(struct sockaddr *)&sockaddr,sizeof(sockaddr)) < 0);
  CHECK(fcntl(acceptsock, F_SETFL, O_NONBLOCK));
  CHECK(listen(acceptsock, 5));

  CHECK(gettimeofday(&currenttv, NULL));
  for(i=0,p=peers; i<npeers; i++,p=p->next) {
    if(p->remaddr) {
      p->lastupdate.tv_sec = p->lastupdate.tv_usec = 0; /* for immediate reconnect */
      p->output.nbytes = -1; /* indicate connect in progress */
      p->interval = (reconnectinterval?reconnectinterval:INT_MAX);
    } else if(p->interval) {
      p->lastupdate = currenttv;
    }
  }

  running = 1;
  while(running) {
    if(npeers+1 > maxnpolls) {
      maxnpolls = npeers+1;
      CHECK(!(polls = realloc(polls, sizeof(struct pollfd)*maxnpolls)));
    }
    polls[0].fd      = acceptsock;
    polls[0].events  = POLLIN;
    polls[0].revents = 0;
    msecs = -1;
    CHECK(gettimeofday(&currenttv, NULL));
    npolls = 1;
    BUG(fprintf(stderr,"\n"));
    for(p=peers; p; p=p->next) {
      BUG(fprintf(stderr,"waiting for %s%s%s interval %d msecs alarm in %d msecs\n",(p->remaddr?"peer ":"internal process"),(p->remaddr?p->remaddr:""),(p->fd>=0?", connected":""),p->interval,p->interval-milliseconds(&p->lastupdate,&currenttv,1)));
      if(p->interval > 0) {
        deltat = (p->interval!=INT_MAX ? p->interval - milliseconds(&p->lastupdate,&currenttv,1) : 0);
        if(deltat < 0)
          deltat = 0;
        /* if we have a remote peer, are in connecting mode and
         * timeout occured, then reconnect */
        if(p->remaddr && p->output.nbytes == -1 && deltat <= 0) {
          BUG(fprintf(stderr,"reconnecting to %s\n",p->remaddr));
          CHECK((p->fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0);
          CHECK(fcntl(p->fd, F_SETFL, O_NONBLOCK));
          CHECK(setsockopt(p->fd,SOL_SOCKET,SO_KEEPALIVE,&yes,sizeof(yes)));
          CHECK((hent = gethostbyname(p->remaddr)) == NULL);
          sockaddr.sin_family      = AF_INET;
          sockaddr.sin_addr        = *(struct in_addr *)hent->h_addr_list[0];
          sockaddr.sin_port        = htons(p->remport);
          CHECK(connect(p->fd, (struct sockaddr *)&sockaddr, sizeof(sockaddr)) &&
                errno!=EINPROGRESS && errno!=ECONNREFUSED);
          p->lastupdate = currenttv;
          p->interval = reconnectinterval;
          if(p->output.nbytes == 0)
            p->output.nbytes = -1; /* indicate connect in progress */
        }
        if((msecs == -1 || msecs > deltat) && p->interval != INT_MAX)
          msecs = deltat;
      }
      if(p->fd >= 0) {
        polls[npolls].fd      = p->fd;
        polls[npolls].revents = 0;
        polls[npolls].events  = POLLERR|POLLHUP;
        if(p->output.nbytes >= 0)
          polls[npolls].events |= POLLIN;
        if(p->output.nbytes != 0)
          polls[npolls].events |= POLLOUT;
        ++npolls;
      }
    }
    assert(msecs == -1 || msecs >= 0);
    BUG(fprintf(stderr,"poll for next event on %d channels and timeout %d msecs\n",npolls,msecs));
    CHECK((status = poll(polls, npolls, msecs)) < 0);
    CHECK(gettimeofday(&currenttv, NULL));
    if(status) {
      if(polls[0].revents & POLLIN) {
        BUG(fprintf(stderr,"accepting new incoming connection\n"));
        addrlen = sizeof(remaddr);
        CHECK((peerfd = accept(acceptsock, (struct sockaddr *) &remaddr,
                                           &addrlen)) < 0);
        newpeer = allocpeer();
        newpeer->fd = peerfd;
        newpeer->alarmfn      = watchdogcontact_alarm;
        newpeer->consumefn    = watchdogcontact_consume;
        polls[0].revents &= ~POLLIN;
      }
      if(polls[0].revents) {
	if(debugging)
          fprintf(stderr, "%s: event structure not empty for accepting socket\n", argv0);
      }
      for(i=1,p=peers; i<npolls; i++) {
        p=peers;
        while(p && polls[i].fd != p->fd)
          p = p->next;
        assert(p);
        if(polls[i].revents & POLLOUT) {
          polls[i].revents &= ~POLLOUT;
          if(p->output.nbytes > 0) {
            CHECK((nbytes = write(p->fd, p->output.buffer, p->output.nbytes)) < 0);
            assert(p->output.nbytes >= nbytes);
            p->output.nbytes -= nbytes;
            if(p->output.nbytes > 0)
              memmove(p->output.buffer, &p->output.buffer[nbytes], p->output.nbytes);
          } else {
            assert(p->output.nbytes < 0);
            p->output.nbytes = 0;
            addrlen = sizeof(status);
            CHECK(getsockopt(p->fd, SOL_SOCKET, SO_ERROR, &status, &addrlen));
            if(status == 0) {
              p->interval = 0;
              if(p->connectfn)
                p->connectfn(p->user);
            } else {
              if(p->disconnectfn)
                p->disconnectfn(p->user);
              if(p->fd >= 0) {
                shutdown(p->fd, SHUT_RDWR);
                close(p->fd);
                p->fd = -1;
              }
	      if(debugging)
                BUG(fprintf(stderr,"%s: error connecting to %s\n",argv0,p->remaddr));
            }
          }
        }
        if(polls[i].revents & POLLIN) {
          polls[i].revents &= ~POLLIN;
          if(!p->input.buffer) {
            CHECK(!(p->input.buffer = malloc(p->input.size = 2*65536)));
          } else if(p->input.size - p->input.nbytes < 65536) {
            if(p->input.size >= 65536*4) {
              if(p->disconnectfn)
                p->disconnectfn(p->user);
              if(p->fd >= 0) {
                shutdown(p->fd, SHUT_RDWR);
                close(p->fd);
                p->fd = -1;
              }
              continue;
            }
            CHECK(!(p->input.buffer = realloc(p->input.buffer,
                                              p->input.size += 65536)));
          }
          CHECK((nbytes = read(p->fd, &p->input.buffer[p->input.nbytes],
                               p->input.size-p->input.nbytes)) < 0);
          if(nbytes == 0) {
            if(p->disconnectfn)
              p->disconnectfn(p->user);
            if(p->fd >= 0) {
              shutdown(p->fd, SHUT_RDWR);
              close(p->fd);
              p->fd = -1;
            }
            if(p->remaddr && reconnectinterval > 0) {
              p->interval = reconnectinterval;
              p->output.nbytes = -1;
            } else if(!p->disconnectfn)
              freepeer(p);
          } else {
            p->input.nbytes += nbytes;
            if(p->consumefn)
              nbytes = p->consumefn(p, p->input.buffer, p->input.nbytes);
            else
              nbytes = p->input.nbytes;
            if(nbytes > 0) {
              memmove(p->input.buffer, &p->input.buffer[nbytes],
                      p->input.nbytes - nbytes);
              p->input.nbytes -= nbytes;
            }
          }
        }
        if(polls[i].revents & (POLLHUP|POLLERR)) {
          polls[i].revents &= ~(POLLHUP|POLLERR|POLLIN|POLLOUT);
          if(p->disconnectfn)
            p->disconnectfn(p->user);
          if(p->fd >= 0) {
            shutdown(p->fd, SHUT_RDWR);
            close(p->fd);
            p->fd = -1;
          }
          if(p->remaddr && reconnectinterval > 0) {
            p->interval = reconnectinterval;
            p->output.nbytes = -1;
          } else if(!p->disconnectfn)
            freepeer(p);
        }
        if(polls[0].revents) {
	  if(debugging)
            fprintf(stderr, "%s: event structure not empty\n", argv0);
        }
      }
    } else {
      BUG(fprintf(stderr,"poll interrupted by timeout\n"));
      for(p=peers; p; p=p->next) {
        if(p->interval > 0 && p->output.nbytes != -1) {
          deltat = milliseconds(&p->lastupdate,&currenttv,-1);
          if(deltat >= p->interval) {
            p->lastupdate = currenttv;
            if(p->alarmfn)
              p->alarmfn(p->user);
          }
        }
      }
    }
  }
  shutdown(acceptsock, SHUT_RDWR);
  close(acceptsock);
  exit(0);
}

int
main(int argc, char *argv[])
{
  int i, ch;
  const char *s;
  struct peer *newpeer;

#ifdef HAVE_GETOPT_LONG
  static struct option longopts[] = {
    { "debug",              0, 0, 'd' },
    { "port",               1, 0, 'p' },
    { "log-interval",       2, 0, 'l' },
    { "no-log-syslog",      0, 0, 1   },
    { "log-syslog",         0, 0, 2   },
    { "no-log-file",        0, 0, 3   },
    { "log-file",           2, 0, 'L' },
    { "server",             1, 0, 's' },
    { "help",               0, 0, 'h' },
    { "watchdog-interval",  1, 0, 'w' },
    { "watchdog-command",   1, 0, 'W' },
    { "network-device",     1, 0, 'i' },
    { "reconnect-interval", 1, 0, 'r' },
    { "check-command",      1, 0, 'c' },
    { "check-interval",     1, 0, 'C' },
    { 0, 0, 0, 0 }
  };
#define GNUGETOPT(X1,X2,X3,X4,X5) getopt_long(X1,X2,X3,X4,X5)
#else
#define GNUGETOPT(X1,X2,X3,X4,X5) getopt(X1,X2,X5)
#endif

  /* Get the name of the program */
  if((argv0 = strrchr(argv[0],'/')) == NULL)
    argv0 = argv[0];
  else
    ++argv0;

  /* Parse options */
  opterr = 0;
  while((ch = GNUGETOPT(argc, argv,"dhp:l::L::r:w:W:s:i:c:C:",longopts,NULL)) >= 0) {
    switch(ch) {
    case 'd':
      debugging = 1;
      break;
    case 'p':
      if(getinteger(optarg,&port))
          goto badnumber;
      break;
    case 'l':
      if(optarg) {
        if(getinteger(optarg,&loginterval))
          goto badnumber;
      } else
        loginterval = 60;
      break;
    case 1: /* --no-log-syslog */
      wantsyslog = 0;
      break;
    case 2: /* --log-syslog */
      wantsyslog = 1;
      break;
    case 3: /* --no-log-file */
      logfilename = NULL;
      break;
    case 'L': /* --log-file */
      if(!optarg)
        logfilename = strdup(mktmpstring("%s%s.log","",argv0));
      else
        logfilename = strdup(optarg);
      break;
    case 'w':
      if(getinteger(optarg,&watchdoginterval))
        goto badnumber;
      break;
    case 'r':
      if(getinteger(optarg,&reconnectinterval))
        goto badnumber;
      break;
    case 'C':
      if(getinteger(optarg,&logcmdinterval))
	goto badnumber;
      break;
    case 'c':
      logcommand = strdup(optarg);
      break;
    case 'W':
      watchdogcommand = strdup(optarg);
      break;
    case 'i':
      netdevice = strdup(optarg);
      break;
    case 's':
      newpeer = allocpeer();
      s = optarg;
      while(*s && *s != ':')
        ++s;
      if(*s == ':') {
        /* newpeer->remaddr = strndup(optarg,s-optarg); not available on Solaris */
        newpeer->remaddr = malloc(s-optarg+1);
          strncpy(newpeer->remaddr, optarg, s-optarg);
          newpeer->remaddr[s-optarg] = '\0';
        if(getinteger(s+1,&newpeer->remport)) {
          fprintf(stderr,"%s: bad or missing port for remote server\n",argv0);
          exit(1);
        }
      } else {
        newpeer->remaddr = strdup(optarg);
        newpeer->remport = DEFAULTPORT;
      }
      newpeer->connectfn = watchdogcontact_connect;
      newpeer->consumefn = watchdogcontact_consume;
      break;
    case 'h':
             /*34567890123456789012345678901234567890123456789012345678901234567890123456*/
      printf("\n%s [-hd] [-p port] [-l [interval]] [-s server[:port]]\n",argv[0]);
      printf("%*.*s [-w interval] [-W command]\n\n",strlen(argv[0]),strlen(argv[0]),"");
      printf("  -h  --help\n");
      printf("      For this friendly reminder of options.\n\n");
      printf("  -d  --debug\n");
      printf("      Do not become daemon process but run in foreground.\n\n");
      printf("  -p  --port <portnum>\n");
      printf("      Listen to indicated port instead of default port %d.\n\n",DEFAULTPORT);
      printf("  -i  --network-device <device>\n");
      printf("      Use network device specified instead of default %s.\n\n",netdevice);
      printf("  -l  --log-interval [<interval>]\n");
      printf("      Log monitor information to logfile.\n\n");
      printf("      --[no-]syslog\n");
      printf("      Turn on/off logging through the syslog facility.\n\n");
      printf("      --[no-]log-file [<filename>]\n");
      printf("      Turn on/off logging by appending to the specified file.\n\n");
      printf("  -r  --reconnect-interval <seconds>\n");
      printf("      Retry to reconnect after failing to connect after n seconds.\n\n");
      printf("  -w  --watchdog-interval <seconds>\n");
      printf("      Indicate to the server as specified by -s to receive a notification\n");
      printf("      every n seconds otherwise the watchdog alarm will go off.  When a\n");
      printf("      time-out occurs and no command is specified with -W then this program\n");
      printf("      will exit.\n\n");
      printf("  -W  --watchdog-command <command>\n");
      printf("      Execute the indicated command using a shell script without arguments.\n");
      printf("      when the command exists with a non-zero return status, the watchdog\n");
      printf("      timer is shut down, otherwise the timer interval is reset.\n\n");
      printf("  -c  --check-command <command>\n");
      printf("      Execute the indicated command periodically and append the output to the\n");
      printf("      log interval.  The command is not run in sync with logging.\n\n");
      printf("  -C  --check-interval <seconds>\n");
      printf("      Instead of using the log interval, use this interval to execute the\n");
      printf("      command specified by -c\n\n");
      exit(0);
    case ':':
      fprintf(stderr,"%s: missing option argument to -%c\n",argv0,optopt);
      exit(1);
    case '?':
      fprintf(stderr,"%s: unrecognized option -%c\n",argv0,optopt);
      exit(1);
    }
  }
  for(i=optind; i<argc; i++) {
    fprintf(stderr,"%s: unexpected argument %s\n",argv0,argv[i]);
    exit(1);
  }

  if(logcmdinterval < 0)
    logcmdinterval = loginterval;
  watchdoginterval *= 1000;
  reconnectinterval *= 1000;
  loginterval *= 1000;
  logcmdinterval *= 1000;
  if(loginterval > 0) {
    newpeer = allocpeer();
    newpeer->interval = loginterval;
    newpeer->alarmfn  = logging;
    newpeer->user     = NULL;
  }
  if(logcmdinterval > 0 && logcommand && *logcommand) {
    newpeer = allocpeer();
    newpeer->interval = logcmdinterval;
    //newpeer->lastupdate = ...
    newpeer->alarmfn      = logcmd_alarm;
    newpeer->consumefn    = logcmd_consume;
    newpeer->disconnectfn = logcmd_disconnect;
  }
  if(watchdoginterval > 0) {
    newpeer = allocpeer();
    newpeer->interval = watchdoginterval;
    newpeer->alarmfn  = watchdogalarm;
  }

  if(!debugging)
    s = daemonize(NULL,NULL,NULL);
  if(wantsyslog)
    openlog(argv0, LOG_ODELAY, LOG_DAEMON);
  if(!debugging && s) {
    if(debugging)
     fprintf(stderr, "failed to startup: %s\n", s);
    else if(wantsyslog)
     syslog(LOG_ERR, "failed to startup: %s", s);
    exit(1);
  }
  if(logfilename) {
    CHECK((logfiledes = open(logfilename,O_CREAT|O_APPEND|O_WRONLY,0777)) < 0);
  }

  run();

  closelog();
  exit(0);
badnumber:
  fprintf(stderr,"%s: bad argument to option -%c\n",argv0,optopt);
  exit(1);
}

/****************************************************************************/
