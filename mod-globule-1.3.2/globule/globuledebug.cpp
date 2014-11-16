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
#include <httpd.h>
#include <apr.h>
#include <apr_lib.h>
#include <apr_strings.h>
#ifndef WIN32
#include <apr_anylock.h>
#include <apr_file_io.h>
#include <apr_file_info.h>
#include <stddef.h>
#include <stdio.h>
#include <apr_shm.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#endif
#include "alloc/Allocator.hpp"
#include "netw/Url.hpp"
#include "resource/BaseHandler.hpp"

static int verbosity = 0;
static char* argv0;

/****************************************************************************/

class MyBaseHandler : public BaseHandler
{
public:
  MyBaseHandler(apr_pool_t*, Context*, const Url&, const char*) throw();
  virtual std::string description() const throw();
  virtual bool ismaster() const throw();
  virtual bool isslave() const throw();
};

MyBaseHandler::MyBaseHandler(apr_pool_t* pool, Context* ctx,
                             const Url& uri, const char* path)
  throw()
  : BaseHandler(0, pool, ctx, uri, path)
{
}

std::string
MyBaseHandler::description() const throw()
{
  return mkstring() << "MyBaseHandler(" << _uri.host() << ":" << _uri.port()
                    << _uri.path()<<")";
}

bool
MyBaseHandler::ismaster() const throw()
{
  return false;
}
bool
MyBaseHandler::isslave() const throw()
{
  return false;
}

/****************************************************************************/

#ifndef CHECK
#define CHECK(ARG) \
  do{if(ARG){fprintf(stderr,"call " #ARG " failed\n");abort();}}while(0)
#endif

void
ap_log_assert(const char *expression, const char *file, int line)
{
  fprintf(stderr,"assertion \"%s\" failed at %s line %d\n",expression,
          file,line);
  abort();
}

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

int
main(int argc, char* argv[])
{
  int ch;
  apr_pool_t* pool;

#ifdef HAVE_GETOPT_LONG
  static struct option longopts[] = {
    { "verbose",          2, 0, 'v' },
    { "help",             0, 0, 'h' },
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
  while((ch = GNUGETOPT(argc, argv,"hv",longopts,NULL)) >= 0) {
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
      printf("\nUsage: %s [-hv] command arguments...\n\n",argv[0]);
      //printf("%*.*s \n\n",strlen(argv[0]),strlen(argv[0]),"");
      printf("  -h  --help\n");
      printf("      For this friendly reminder of options.\n\n");
      printf("  -v  --verbose[=<num>]\n");
      printf("      Increase the verbosity of the program.\n\n");
      printf("Different debugging operations are performed by different commands:\n");
      printf("  statecheck <url> <path>\n");
      printf("      Checks whethers the .htglobule/state in the specified path which\n");
      printf("      should represent the section at the specified url is in a proper\n");
      printf("      state and prints a summary or verbose output.\n\n");
      exit(0);
    case ':':
      fprintf(stderr,"%s: missing option argument to -%c\n",argv0,optopt);
      exit(1);
    case '?':
      fprintf(stderr,"%s: unrecognized option -%c\n",argv0,optopt);
      exit(1);
    }
  }

  if(optind >= argc) {
    fprintf(stderr,"%s: no command given\n",argv0);
    return 1;
  }
  if(!strcasecmp(argv[optind],"statecheck")) {
    if(argc+3 != argc) {
      fprintf(stderr,"%s: wrong number of arguments to statecheck command",
              argv0);
      return 1;
    }
  } else {
    fprintf(stderr,"%s: unrecognized command %s\n",argv0,argv[optind]);
    return 1;
  }

  if(apr_initialize() || apr_pool_create(&pool, NULL)) {
    fprintf(stderr,"%s: Cannot initialize apr library\n",argv0);
    return 1;
  }

  Context::initialize();
  //CHECK(apr_proc_mutex_create(&lock.lock.pm,tempfile,APR_LOCK_DEFAULT,pool));
  Context* ctx = new Context();

  if(!strcasecmp(argv[optind],"statecheck")) {
    Url url(pool, argv[1]);
    const char* path = argv[2];
    MyBaseHandler section(pool, ctx, url, path);
    section.restore(pool, ctx);
  }

  return 0;
badnumber:
  fprintf(stderr,"%s: bad argument to option -%c\n",argv0,optopt);
  exit(1);
}

/****************************************************************************/
