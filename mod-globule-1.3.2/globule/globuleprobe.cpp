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
#include <map>
#include <stack>
#include <sstream>
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif
#include <string>
#include <apr.h>
#include <apr_lib.h>
#include <apr_strings.h>
#ifndef WIN32
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

#include "netw/Url.hpp"
#include "netw/HttpRequest.hpp"
#include "Storage.hpp"
//#include "resource/BaseHandler.hpp"

using namespace std;

const char* argv0;
int quit = 0;
int verbosity = 1; // FIXME set back to 0 when fully tested

/****************************************************************************/

extern "C" {
#ifdef NOTDEFINED
struct sharedstate_struct *sharedstate;
struct childstate_struct childstate;
#endif

void
ap_log_assert(const char *expression, const char *file, int line)
{
  fprintf(stderr,"assertion \"%s\" failed at %s line %d\n",expression,
          file,line);
  abort();
}

void
ap_log_perror(const char *file, int line, int level, apr_status_t status,
              apr_pool_t *p, const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr,fmt,ap);
  va_end(ap);
}

void
ap_log_rerror(const char *file, int line, int level, apr_status_t status,
              request_rec* r, const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr,fmt,ap);
  va_end(ap);
}

};

class Peer
{
public:
  static const int ORIGIN     = 0x10;
};

class BaseHandler
{
public:
  static const int PROTVERSION = 2;
};

/****************************************************************************/

class Server
{
private:
  std::string  _location;
  Url          _uri;
public:
  std::string  _secret;
  apr_uint16_t _type;
  apr_uint16_t _weight;
private:
  bool         _available;
  std::string  _message;
public:
  Server(apr_pool_t* pool, const char* uri, const char* secret,
         apr_uint16_t type, apr_uint16_t weight)
    : _location(uri), _uri(pool, uri), _secret(secret), _type(type),
      _weight(weight), _available(false)
  { };
  std::string location() { return _location; };
  bool available() { return _available; };
  void available(bool status, char *message = 0) {
    _available = false;
    _message = (message ? message : "");
  };
  const apr_uri_t* uri() {
    return _uri.uri();
  };
};

/****************************************************************************/

using std::string;
using std::map;
using namespace globule::netw;

static void
probe(apr_pool_t* pool, apr_interval_time_t timeout,
      map<string,Server*>& peers, char* password)
{
  HttpNonBlockingManager manager(pool);
  map<HttpRequest*,string> requests;
  for(map<string,Server*>::iterator iter = peers.begin();
      iter != peers.end();
      iter++)
    {
      Url baseurl(pool, iter->second->uri());
      Url poolurl(pool, apr_pstrcat(pool, baseurl(pool), "?status", NULL));
      poolurl.normalize(pool);
      HttpRequest* req = 0;
      try {
        req = new HttpRequest(pool, poolurl);
        req->setConnectTimeout(3);
        req->setMethod("SIGNAL");
        if(iter->second->_type == Peer::ORIGIN) {
            req->setAuthorization(password, password);
        } else
          req->setAuthorization(iter->second->_secret, iter->second->_secret);
        req->setHeader("X-Globule-Protocol", BaseHandler::PROTVERSION);
        // FIXME req->setHeader("X-Globule-Serial", ..serial..);
        req->connect(pool, &manager); /* manager will manage the connection */
        requests[req] = iter->first;
      } catch(HttpException ex) {
        iter->second->available(false, "request failed");
        if(req)
          delete req;
      }
    }
  while(!manager.empty()) {
    list<HttpResponse*> responselist;
    responselist = manager.poll(timeout);
    for(list<HttpResponse*>::iterator iter=responselist.begin();
        iter != responselist.end();
        ++iter)
      {
        HttpResponse* res = *iter;
        Server* peer = peers[requests[res->request()]];
        if(res->getStatus() == HttpResponse::HTTPRES_OK ||
           res->getStatus() == HttpResponse::HTTPRES_NO_CONTENT) {
          int protversion = atoi(res->getHeader("X-Globule-Protocol").c_str());
          if(protversion <= 0)
            peer->available(false);
          else if(protversion < BaseHandler::PROTVERSION)
            peer->available(false);
          else
            peer->available(true);
        } else if(res->getStatus() == HttpResponseCodes::HTTPRES_FAILURE)
          peer->available(false, "request failed");
        else
          peer->available(false, "not a globule site");
      }
  }
}

static void
process(apr_pool_t *pool, apr_interval_time_t timeout,
        char *password, int numpaths, char *paths[])
{
  char *fname;
  int pathidx, i, nreplicas;
  apr_uint16_t type, weight;
  string policy, location, secret;
  map<string,Server*> peers;
  for(pathidx=0; pathidx<numpaths; pathidx++) {
    fname = apr_pstrcat(pool, paths[pathidx], "/.htglobule/redirection", NULL);
    try {
      FileStore store(pool, 0, fname, true);
      store >> policy >> nreplicas;
      for(i=0; i<nreplicas; i++) {
        store >> type >> weight >> location >> secret;
        peers[location] = new Server(pool, location.c_str(), secret.c_str(),
                                     type, weight);
      }
    } catch(FileError) {
    }
  }
  probe(pool, timeout, peers, password);
  for(pathidx=0; pathidx<numpaths; pathidx++) {
    fname = apr_pstrcat(pool, paths[pathidx], "/.htglobule/redirection", NULL);
    try {
      FileStore store(pool, 0, fname, true);
      store >> policy >> nreplicas;
      for(i=0; i<nreplicas; i++) {
        store >> type >> weight >> location >> secret;
      }
      if(verbosity > 0) {
        map<string,Server*>::iterator iter = peers.find(location);
        printf("%s\t%s\t%s\n", paths[pathidx], location.c_str(),
               (iter != peers.end() ? ( iter->second->available() ? "available"
                                                              : "unavailable" )
                                    : "unknown" ) );
      }
    } catch(FileError) {
    }
  }
  for(map<string,Server*>::iterator iter = peers.begin();
      iter != peers.end();
      ++iter)
    delete iter->second;
}

/****************************************************************************/

static int
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

extern "C" {
static void
quithandler(int signalno)
{
  quit = 1;
}
}

int
main(int argc, char *argv[])
{
  apr_pool_t* pool;
  int interval = 60;
  int daemonize = -1; /* as yet unset */
  int ch;
  apr_time_t starttime;

#ifdef HAVE_GETOPT_LONG
  static struct option longopts[] = {
    { "verbose",          2, 0, 'v' },
    { "help",             0, 0, 'h' },
    { "interval",         1, 0, 'i' },
    { "no-daemon",        0, 0, 'd' },
    { "daemon",           0, 0, 'D' },
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
  while((ch = GNUGETOPT(argc, argv,"hv::i:dD",longopts,NULL)) >= 0) {
    switch(ch) {
    case 'v':
      if(optarg) {
        if(getinteger(optarg,&verbosity))
          goto badnumber;
      } else
	++verbosity;
      break;
    case 'h':
             /*34567890123456789012345678901234567890123456789012345678901234567890123456*/
      printf("\nUsage: %s [-hvdD] -i<interval> <path>...\n\n",argv[0]);
      //printf("%*.*s \n\n",strlen(argv[0]),strlen(argv[0]),"");
      printf("  -h  --help\n");
      printf("      For this friendly reminder of options.\n\n");
      printf("  -v  --verbose[=<num>]\n");
      printf("      Increase the verbosity of the program.\n\n");
      printf("  -i  --interval=<seconds>\n");
      printf("      Probing interval.\n\n");
      printf("  -d  --no-daemon\n");
      printf("      Do not fork into background as a daemon process\n");
      printf("  -D  --daemon\n");
      printf("      Fork off into background as a daemon process\n\n");
      exit(0);
    case 'i':
      if(getinteger(optarg,&interval))
        goto badnumber;
      break;
    case 'd':
      daemonize=0;
      break;
    case 'D':
      daemonize=1;
      break;
    case ':':
      fprintf(stderr,"%s: missing option argument to -%c\n",argv0,optopt);
      exit(1);
    case '?':
      fprintf(stderr,"%s: unrecognized option -%c\n",argv0,optopt);
      exit(1);
    }
  }

  if(daemonize == -1)
    daemonize = (verbosity > 0 ? 0 : 1);
  if(daemonize) {
    int fd, i;
    pid_t pid;
    if(getppid() != 1) { /* not already a daemon */
      if((pid = fork()) < 0) {
        fprintf(stderr, "%s: cannot fork into daemon mode\n", argv0);
        exit(1);
      }
      if(pid > 0)
        _exit(0);
      /* parent has now left the building, daemon child contines */
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
    }
    signal(SIGTSTP,SIG_IGN); /* ignore tty signals */
    signal(SIGTTOU,SIG_IGN);
    signal(SIGTTIN,SIG_IGN);
  } else {
    signal(SIGINT,quithandler);
  }
  signal(SIGTERM,quithandler);  

  if(apr_initialize() || apr_pool_create(&pool, NULL)) {
    fprintf(stderr,"%s: Cannot initialize apr library\n",argv0);
    return 1;
  }

  while(!quit) {
    starttime = apr_time_now();
    process(pool, apr_time_from_sec(interval), "", argc-1, &argv[1]);
    apr_sleep(apr_time_from_sec(interval) - (apr_time_now()-starttime));
  }

  apr_pool_destroy(pool);
  exit(0);

badnumber:
  fprintf(stderr,"%s: bad argument to option -%c\n",argv0,optopt);
  exit(1);
}
