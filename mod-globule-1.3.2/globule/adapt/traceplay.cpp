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
#include <iostream>
#include <fstream>
#include <math.h>
#if (defined(MAIN) && defined(HAVE_GETOPT_H))
#include <getopt.h>
#endif
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "netw/Url.hpp"
#include "netw/HttpRequest.hpp"
#include "utilities.h"
#include "reportreader.h"
#include "traceplay.h"

using namespace std;
using namespace netw;

/****************************************************************************/

void
readMappings(Mappings &m)
{
  FILE *fp;
  char mapname[1024];
  char itemname[1024];
  int itemindex;
  fp = fopen("mappings", "r");
  while(!feof(fp)) {
    if(fscanf(fp,"%[a-zA-Z]\t%d\t%[^\n]\n",mapname,&itemindex,itemname) != 3)
      fprintf(stderr,"error reading mappings\n");
    if(itemindex == 0)
      m[mapname][0] = itemname;
    else
      m[mapname].insert(make_pair<string,int>(itemname,itemindex));
  }
  fclose(fp);
}

void TracePlay::execute(struct instruction &instr) {
  int size;
  switch(instr.type) {
  case 'R': { // Retrieve document
    HttpRequest req(instr.location);
    HttpConnection *con = req.connect();
    HttpResponse *res = con->response();
    res->read(output);
    if(verbose)
      cout << "R url=" << instr.location << " status:"
           << res->getStatus() << " message=" << res->getMessage() << endl;
    break;
  }
  case 'U': { // Update document
    size = instr.size;
    if(verbose)
      cout << "U url=" << instr.location << " docsize:" << size << endl;
    ofstream ost(instr.location.c_str(), ios_base::out|ios_base::trunc);
    while(size-- > 0) {
      char ch = random() % 256;
      ost << ch;
    }
    ost.close();
    break;
  }
  }
}

void TracePlay::run() {
  struct instruction *instr = NULL;
  do {
    if(instr)
      delete instr;
    CHECK(sem_wait(&queuecounter));
    CHECK(pthread_mutex_lock(&queueaccess));
    instr = queuehead;
    queuehead = instr->next;
    if(queuehead == NULL)
      queuetailptr = &queuehead;
    CHECK(pthread_mutex_unlock(&queueaccess));
    if(instr->type != 'Q')
      execute(*instr);
  } while(instr->type != 'Q');
}

void *TracePlay::worker(void *arg)
{
  ((TracePlay *)arg)->run();
  return NULL;
}

TracePlay::TracePlay(int numberOfThreads, bool verboseActions)
  : time(-1), verbose(verboseActions), nthreads(numberOfThreads)
{
  output = ::open("/dev/null", O_WRONLY);
  if(nthreads > 0) {
    queuehead = NULL;
    queuetailptr = &queuehead;
    CHECK(sem_init(&queuecounter, 0, 0));
    CHECK(pthread_mutex_init(&queueaccess, NULL));
    threads = new pthread_t[nthreads];
    for(int i=0; i<nthreads; i++)
      CHECK(pthread_create(&threads[i], NULL, &worker, this));
  }
}

TracePlay::~TracePlay() {
  struct instruction instr;
  if(nthreads > 0) {
    instr.type = 'Q';
    instr.next = &instr;
    CHECK(pthread_mutex_lock(&queueaccess));
    *queuetailptr = &instr;
    CHECK(pthread_mutex_unlock(&queueaccess));
    for(int i=0; i<nthreads; i++)
      CHECK(sem_post(&queuecounter));
    for(int i=0; i<nthreads; i++)
      CHECK(pthread_join(threads[i], NULL));
    CHECK(sem_destroy(&queuecounter));
    CHECK(pthread_mutex_destroy(&queueaccess));
  }
  close(output);
}

void TracePlay::play(reportreader &in, Mappings mappings, double factor) {
  long long prev;
  long delta;
  unsigned long delay;
  int timeFieldID   = 0;
  int serverFieldID = 0;
  int docFieldID    = 0;
  int sizeFieldID   = 0;
  struct instruction instr, *instrptr;
  for(in.reset(); in.more(); in.next()) {
    prev  = time;
    time  = in.field(&timeFieldID, "t");
    if(prev < 0)
      prev = time;
    delta = time - prev;
    if(time != 0) {
      delay = lrint(delta*factor);
      if(verbose)
        cout << "W t:" << time << " delay="
             << mkstring::format("%d.%03d",delay/1000000,(delay/1000)%1000)
             << endl;
      usleep(delay);
    }
    switch(*(in.field())) {
    case 'R': { // Retrieve document
      instr.type     = 'R';
      instr.location = mappings["srvbase"][in.field(&serverFieldID,"server")]
                     + mappings["docpath"][in.field(&docFieldID,"path")];
      break;
    }
    case 'U': { // Update document
      instr.type     = 'U';
      instr.location = mappings["docpath"][0]
                     + mappings["docpath"][in.field(&docFieldID,"path")];
      instr.size     = in.field(&sizeFieldID,"docsize");
      break;
    }
    default:
      // Unsupported type
      instr.type = 'Q';
    }
    if(nthreads == 0) {
      execute(instr);
    } else if(instr.type != 'Q') {
      instrptr = new struct instruction;
      *instrptr = instr;
      CHECK(pthread_mutex_lock(&queueaccess));
      instrptr->next = NULL;
      *queuetailptr = instrptr;
      CHECK(pthread_mutex_unlock(&queueaccess));
      CHECK(sem_post(&queuecounter));
    }
  }
}

void TracePlay::play(int nthreads, double factor) {
  Mappings maps;
  int docFieldID = 0;
  int urlFieldID = 0;
  char *fields[] = { "t", "server", "docsize", "path", NULL };
  maps.insert(make_pair<string,Mapping>("",Mapping(false)));
  for(int i=0; fields[i]; i++)
    maps[""] += fields[i];
  readMappings(maps);
  reportreader input(maps);
  TracePlay player(nthreads, true);
  //maps.insert(make_pair<string,Mapping>("server",Mapping()));
  //maps.insert(make_pair<string,Mapping>("document",Mapping()));
  //maps.insert(make_pair<string,Mapping>("location",Mapping()));
  player.play(input, maps, factor);
}

/****************************************************************************/

#ifdef MAIN
static char *argv0;

int
main(int argc, char *argv[])
{
  int nthreads = 0;
  double factor = 1.0;
  int ch;

#ifdef HAVE_GETOPT_LONG
  static struct option options[] = {
    { "nthreads",    required_argument, NULL, 'n' }
    { "time-factor", optional_argument, NULL, 'x' }
  };
#endif

  /* Get the name of the program without path */
  if((argv0 = strrchr(argv[0],'/')) == NULL)
    argv0 = argv[0];
  else
    ++argv0;

#ifdef HAVE_GETOPT_LONG
  while((ch = getopt_long(argc, argv, "n:x::", options, NULL)) >= 0) {
#else
  while((ch = getopt(argc, argv, "n:x::")) >= 0) {
#endif
    switch(ch) {
    case 'n':
      nthreads = atoi(optarg);
      break;
    case 'x':
      factor = atof(optarg);
      break;
    case '?':
    default:
      fprintf(stderr, "%s: unrecognized option %c\n",argv0,optopt);
      exit(1);
    }
  }
  if(optind < argc) {
    fprintf(stderr, "%s: wrong number of arguments\n", argv0);
    exit(1);
  }

  TracePlay::play(nthreads, factor);

  exit(0);
}
#endif

/****************************************************************************/
