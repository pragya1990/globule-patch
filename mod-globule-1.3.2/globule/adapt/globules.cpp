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

#include <stdio.h>
#include <stdarg.h>
#include <math.h>
#include <time.h>
#include <iostream>
#include <string>
#include <set>
#include <vector>
#ifdef DMALLOC
#include <dmalloc.h>
#endif

#include <apr.h>
#include <apr_strings.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#ifndef MAIN
#include "policy/ReplPolicy.hpp"
#include "resource/OriginHandler.hpp"
#include "event/SwitchEvent.hpp"
#endif
#include "reportreader.h"
#include "utilities.h"
#ifdef MAIN
const char* servernamestr;
const char* servername() { return servernamestr; };
#define DEFAULT_TTL    10
#define DEFAULT_MAXTTL 86400
class GlobuleEvent {
public:
  apr_pool_t* pool;
  GlobuleEvent(apr_pool_t*p) : pool(p) { };
};
extern void ap_log_assert();
void ap_log_assert(char *szExp, const char *szFile, int nLine)
{
  //assert(!szExp);
}
std::string
mkstring::format(const char *fmt, va_list ap)
  throw()
{
  va_list ap2;
#ifdef WIN32
  ap2 = ap;
#else
  va_copy(ap2, ap);
#endif /* WIN32 */
  // vsnprintf is a common but non-standard extension.
  // I am assuming C99 behaviour when the first two args
  // are zero: not all implementations have this behaviour.
#ifdef MANDRAKE10_PATCH
  size_t sz = vsnprintf(NULL, 0, fmt, ap);
#else
  size_t sz = apr_vsnprintf(NULL, 0, fmt, ap);
#endif
  // If your string implementation guarantees contiguity
  // of storage, you can use that.  AFAIK, the standard
  // doesn't guarantee this for strings but does for vectors
  std::vector<char> v(sz+1, '\0');
#ifdef MANDRAKE10_PATCH
  vsnprintf(&v[0], sz+1, fmt, ap2);
#else
  apr_vsnprintf(&v[0], sz+1, fmt, ap2);
#endif
  return std::string(&v[0], sz);
}

std::string
mkstring::format(const char *fmt, ...)
  throw()
{
  va_list ap;
  va_start(ap, fmt);
  // vsnprintf is a common but non-standard extension.
  // I am assuming C99 behaviour when the first two args
  // are zero: not all implementations have this behaviour.
#ifdef MANDRAKE10_PATCH
  size_t sz = vsnprintf( NULL, 0, fmt, ap );
#else
  size_t sz = apr_vsnprintf( NULL, 0, fmt, ap );
#endif
#ifdef MANDRAKE10_PATCH
  va_end(ap);
  va_start(ap, fmt);
#endif
  // If your string implementation guarantees contiguity
  // of storage, you can use that.  AFAIK, the standard
  // doesn't guarantee this for strings but does for vectors
  std::vector<char> v(sz+1, '\0');
#ifdef MANDRAKE10_PATCH
  vsnprintf(&v[0], sz+1, fmt, ap);
#else
  apr_vsnprintf(&v[0], sz+1, fmt, ap);
#endif
  va_end(ap);
  return std::string(&v[0], sz);
}
#endif

using std::make_pair;
using std::string;

#undef BUG
#define BUG(ARG)

class server;
typedef std::set<server *> collection;
typedef std::set<server *>::const_iterator iterator;
class document
{
public:
  long lastmod;
  long lastupd;
  int docsize;
  int cost;
  collection replicas;
  document() : lastmod(0), lastupd(-1), docsize(0) { };
};

template <typename T>
class array
{
private:
  bool iscopy;
  std::vector<T>* elts;
public:
  array() {
    elts = new std::vector<T>();
    iscopy = false;
  };
  array(int initsize) {
    elts = new std::vector<T>(initsize);
    iscopy = false;
  };
  array(const array& cpy) {
    elts = cpy.elts;;
    iscopy = true;
  };
  array& operator=(const array& cpy) {
    elts = cpy.elts;;
    iscopy = true;
    return *this;
  };
  ~array() {
    if(elts && !iscopy)
      delete elts;
  };
  unsigned int count() {
    return elts->size();
  };
  T& operator[](int idx) { 
    // return elts->at(idx);
    return (*elts)[idx];
  };
};
typedef array<document> documents;

class replpolicy {
private:
  string replname;
protected:
  long maxttl;
  bool invalidation;
public:
  replpolicy() : replname(""), maxttl(-1), invalidation(false) { };
  replpolicy(char *name, long ttl=LONG_MAX, bool invalidate=false)
    : replname(name), maxttl(ttl), invalidation(invalidate) { };
  replpolicy(char *name, bool invalidate)
    : replname(name), maxttl(LONG_MAX), invalidation(invalidate) { };
  string& name() { return replname; };
  virtual ~replpolicy() { };
  virtual long ttl(document& doc) { return maxttl; };
  virtual bool invalidate(document& doc) { return invalidation; };
};
class alexpolicy : public replpolicy {
public:
  alexpolicy(char *name, long maxttl) : replpolicy(name, maxttl) { };
  virtual ~alexpolicy() { };
  virtual long ttl(document& doc) {
    long t = (doc.lastupd - doc.lastmod) / 5;
    if(t > maxttl)
      t = maxttl;
    return t;
  };
};
static long workaround1(replpolicy& policy, document& doc) {
  return policy.ttl(doc);
}
static bool workaround2(replpolicy& policy, document& doc) {
  return policy.invalidate(doc);
}
#include <fornax.h>

class server;
class dispatcher;

struct server_update {
  int docid;
  int size;
};
struct server_invalidate {
  int docid;
};
struct server_subscribe {
  int docid;
  server *replica;
};
struct server_retrieve {
  int docid;
  document _;
};
struct server_main {
  double theSetupTime;
  double theTransferTime;
  bool isTheMaster;
  server *theMaster;
  replpolicy thePolicy;
  documents theDocuments;
};
struct dispatcher_update {
  int docid;
  int size;
};
struct dispatcher_retrieve {
  int docid;
  int server;
};
struct dispatcher_main {
  documents theDocuments;
  server *peer;
  server **peers;
};

struct server_vars {
  replpolicy * policy;
  replpolicy _policy;
  double * transferTime;
  double _transferTime;
  double * setupTime;
  double _setupTime;
  bool * ismaster;
  bool _ismaster;
  server ** master;
  server *_master;
  documents * docs;
  documents _docs;
  int * requests;
  int _requests;
  fornax::Event<struct server_update> *_update;
  int update_docid;
  int update_size;
  int update_t2;
  int update_t1;
  server *update_peer;
  iterator update_iter;
  fornax::Event<struct server_invalidate> *_invalidate;
  int invalidate_docid;
  fornax::Event<struct server_subscribe> *_subscribe;
  int subscribe_docid;
  server *subscribe_replica;
  fornax::Event<struct server_retrieve> *_retrieve;
  int retrieve_docid;
  apr_int64_t retrieve_curtime;
  double main_theSetupTime;
  double main_theTransferTime;
  bool main_isTheMaster;
  server *main_theMaster;
  replpolicy main_thePolicy;
  documents main_theDocuments;
  fornax::Event<struct server_invalidate> *_5;
  fornax::Event<struct server_retrieve> *_20;
};
struct dispatcher_vars {
  long * reallastmod;
  long _reallastmod;
  server ** master;
  server *_master;
  server *** servers;
  server **_servers;
  documents * docs;
  documents _docs;
  fornax::Event<struct dispatcher_update> *_update;
  int update_docid;
  int update_size;
  fornax::Event<struct dispatcher_retrieve> *_retrieve;
  int retrieve_docid;
  int retrieve_server;
  document retrieve_doc;
  int retrieve_t2;
  int retrieve_t1;
  documents main_theDocuments;
  server *main_peer;
  server **main_peers;
  apr_uint32_t main_docid;
  fornax::Event<struct server_update> *_4;
  fornax::Event<struct server_update> *_10;
  fornax::Event<struct server_retrieve> *_14;
};


class server
  : public fornax::Entity<struct server_vars, struct server_main>
{
private:
  struct server_main mainargs;
public:
  fornax::Queue<struct server_update> queue_update;
  fornax::Queue<struct server_invalidate> queue_invalidate;
  fornax::Queue<struct server_subscribe> queue_subscribe;
  fornax::Queue<struct server_retrieve> queue_retrieve;
  fornax::QueueBase *queues_2[4];
  server(fornax::System &system, std::string name)
    : fornax::Entity<struct server_vars, struct server_main>(system,name),
      queue_update(this, "update", 3),
      queue_invalidate(this, "invalidate", 15),
      queue_subscribe(this, "subscribe", 18),
      queue_retrieve(this, "retrieve", 19)
  {
    queues_2[0] = &queue_update;
    queues_2[1] = &queue_retrieve;
    queues_2[2] = &queue_invalidate;
    queues_2[3] = &queue_subscribe;
  };
  virtual ~server() {};
  virtual fornax::StateId step(fornax::State<struct server_vars> *);
  fornax::Event<struct server_update>* send_update(fornax::Time _time, int docid, int size) {
    struct server_update evtdata = { docid, size };
    return queue_update.deliver(_time, evtdata);
  };
  fornax::Event<struct server_invalidate>* send_invalidate(fornax::Time _time, int docid) {
    struct server_invalidate evtdata = { docid };
    return queue_invalidate.deliver(_time, evtdata);
  };
  fornax::Event<struct server_subscribe>* send_subscribe(fornax::Time _time, int docid, server *replica) {
    struct server_subscribe evtdata = { docid, replica };
    return queue_subscribe.deliver(_time, evtdata);
  };
  fornax::Event<struct server_retrieve>* send_retrieve(fornax::Time _time, int docid) {
    struct server_retrieve evtdata = { docid };
    return queue_retrieve.deliver(_time, evtdata);
  };
  // C++ sucks, this next function should be called main() but C++ does
  // not allow overloading of template functions
  void init(double theSetupTime, double theTransferTime, bool isTheMaster, server *theMaster, replpolicy thePolicy, documents theDocuments) {
    mainargs.theSetupTime = theSetupTime;
    mainargs.theTransferTime = theTransferTime;
    mainargs.isTheMaster = isTheMaster;
    mainargs.theMaster = theMaster;
    mainargs.thePolicy = thePolicy;
    mainargs.theDocuments = theDocuments;
    main(fornax::Event<struct server_main>(mainargs));
  };
};

class dispatcher
  : public fornax::Entity<struct dispatcher_vars, struct dispatcher_main>
{
private:
  struct dispatcher_main mainargs;
public:
  fornax::Queue<struct dispatcher_update> queue_update;
  fornax::Queue<struct dispatcher_retrieve> queue_retrieve;
  fornax::QueueBase *queues_2[2];
  dispatcher(fornax::System &system, std::string name)
    : fornax::Entity<struct dispatcher_vars, struct dispatcher_main>(system,name),
      queue_update(this, "update", 9),
      queue_retrieve(this, "retrieve", 13)
  {
    queues_2[0] = &queue_update;
    queues_2[1] = &queue_retrieve;
  };
  virtual ~dispatcher() {};
  virtual fornax::StateId step(fornax::State<struct dispatcher_vars> *);
  fornax::Event<struct dispatcher_update>* send_update(fornax::Time _time, int docid, int size) {
    struct dispatcher_update evtdata = { docid, size };
    return queue_update.deliver(_time, evtdata);
  };
  fornax::Event<struct dispatcher_retrieve>* send_retrieve(fornax::Time _time, int docid, int server) {
    struct dispatcher_retrieve evtdata = { docid, server };
    return queue_retrieve.deliver(_time, evtdata);
  };
  // C++ sucks, this next function should be called main() but C++ does
  // not allow overloading of template functions
  void init(documents theDocuments, server *peer, server **peers) {
    mainargs.theDocuments = theDocuments;
    mainargs.peer = peer;
    mainargs.peers = peers;
    main(fornax::Event<struct dispatcher_main>(mainargs));
  };
};


#undef BUG
#define BUG(ARG) ARG
static long
evaluatePolicy(reportreader &input, long *docstate, replpolicy policy)
{
  const double setup = 3.0;       /* 3 seconds, which is huge */
  const double delay = 1.0/65536; /* 64Kbyte/second, which is 1/3-1/2 of
                                     cable downstream speed */
  int i, nservers, ndocuments;
  fornax::Time simtime;
  int timeFieldID   = 0;
  int serverFieldID = 0;
  int docFieldID    = 0;

  fornax::System system("globule");
  nservers   = input.count("server") - 1;
  ndocuments = input.count("path") - 1;

  BUG(fprintf(stderr, "ndocuments=%d nservers=%d\n", ndocuments, nservers));

  if(nservers > 0) {
    array<document> docs(ndocuments);
    array<document> *sdocs = new array<document>[nservers];
    for(i=0; i<nservers; i++)
      sdocs[i] = docs;
    for(i=0; i<ndocuments; i++) {
      docs[i].cost = 0;
      docs[i].docsize = docstate[i];
    }

    server **serverEntities = new server *[nservers];
    serverEntities[0] = new server(system, "master");
    for(i=1; i<nservers; i++)
      serverEntities[i] = new server(system, mkstring::format("slave#%d",i));
    dispatcher *dispatcherEntity = new dispatcher(system, "dispatcher");
    
    dispatcherEntity->init(docs, serverEntities[0], serverEntities);
    serverEntities[0]->init(setup,delay, true,serverEntities[0],
                            policy, docs);
    for(i=1; i<nservers; i++)
      serverEntities[i]->init(setup,delay, false,serverEntities[0],
                              policy, sdocs[i-1]);
    
    for(input.reset(); input.more(); input.next()) {
      switch(*(input.field())) {
      case 'U': // document was updated.
      case 'R': // document was retrieved
        if(docstate[input.field(&docFieldID,"path") - 1] >= 0) {
          switch(*(input.field())) {
          case 'U': {
            fornax::Event<struct dispatcher_update>* event;
            event = dispatcherEntity->send_update(
                                    input.field(&timeFieldID, "t"),
                                    input.field(&docFieldID,  "path") - 1,
                                    input.field(&docFieldID,  "docsize"));
            event->selfDispose();
            break;
          }
          case 'R': {
            fornax::Event<struct dispatcher_retrieve>* event;
            event = dispatcherEntity->send_retrieve(
                                    input.field(&timeFieldID,   "t"),
                                    input.field(&docFieldID,    "path") - 1,
                                    input.field(&serverFieldID, "server") - 1);
            event->selfDispose();
            break;
          }
          default:
            abort();
          }
        }
        break;
      case 'A': // policy of document was adapted
        break;
      default:
        break;
      }
    }

    system.start();
    simtime = system.time();
    if(!system.check())
      std::cerr << system;

    for(i=0; i<ndocuments; i++)
      docstate[i] = docs[i].cost;

    delete dispatcherEntity;
    for(i=0; i<nservers; i++)
      delete serverEntities[i];
    delete[] serverEntities;
    delete[] sdocs;
    return simtime;

  } else

    return 0;
}

#ifdef MAIN
static void
adapt(const GlobuleEvent& evt, const char* fname = "report.log",
      size_t beginoffset = 0, ssize_t endoffset = -1)
#else
void
OriginHandler::adapt(const GlobuleEvent& evt,
                     const char *fname, size_t beginoffset, size_t endoffset) throw()
#endif
{
  long simtime;
  int i, d;
  char *fields[] = { "t", "server", "docsize", "path", NULL };

  BUG(fprintf(stderr,"policy adaptation run\n"));

  // Read input
  Mappings maps;
  maps.insert(make_pair<string,Mapping>("",Mapping(false)));
  for(i=0; fields[i]; i++)
    maps[""] += fields[i];
  maps["server"][servername()]; // for this to be element "1" (!)
  reportreader input(evt.pool, maps, fname, beginoffset, endoffset);


  const int npolicies = 4;
  replpolicy* policies[npolicies];
  // policies[0] = replpolicy("MirrorNoCons"); never choose this one
  policies[0] = new replpolicy("PureProxy", -1, false);
  policies[1] = new replpolicy("Ttl", DEFAULT_TTL, false);
  policies[2] = new replpolicy("Invalidate", true);
  policies[3] = new alexpolicy("Alex", DEFAULT_MAXTTL);

  // Simulate input
  int ndocuments   = input.count("path");
  long *docstate   = new long[ndocuments];
  long *bestcost   = new long[ndocuments];
  int  *bestpolicy = new int[ndocuments];
  for(d=0; d<ndocuments; d++)
    bestpolicy[d] = -1;
  for(i=0; i<npolicies; i++) {
    for(d=0; d<ndocuments; d++)
      docstate[d] = 0;
    simtime = evaluatePolicy(input, docstate, *(policies[i]));
    for(d=0; d<ndocuments; d++) {
      BUG(fprintf(stderr,"%s\t%d\t%ld\n",policies[i]->name().c_str(),d,docstate[d]));
      if((bestpolicy[d] < 0 || bestcost[d] > docstate[d]) && docstate[d] > 0) {
        bestpolicy[d] = i;
        bestcost[d]   = docstate[d];
      }
    }
  }
  for(d=0; d<ndocuments; d++)
    if(bestpolicy[d] >= 0) {
      BUG(fprintf(stderr,"%s\t%s\t%ld\n",maps["path"][d+1].c_str(),policies[bestpolicy[d]]->name().c_str(),bestcost[d]));
#ifndef MAIN
      switchpolicy(evt.pool, evt.context, maps["path"][d+1].c_str(),
               policies[bestpolicy[d]]->name().c_str());
#endif
    } else {
      BUG(fprintf(stderr,"%s\tnone\t\n",maps["path"][d+1].c_str()));
    }
  //DPRINTF(DEBUG3, ("evaluated policy of %d documents with %d policies with approx. %d requests.\n",ndocuments,npolicies,numofinputlines));

  for(i=0; i<npolicies; i++)
    delete policies[i];
  delete docstate;
  delete bestcost;
  delete bestpolicy;
}


#ifdef MAIN
#ifndef CHECK
#define CHECK(ARG) \
  do{if(ARG){fprintf(stderr,"call " #ARG " failed\n");exit(1);}}while(0)
#endif
int
main(int argc, char* argv[])
{
  apr_allocator_t *allocer;
  apr_pool_t* pool;
  servernamestr = "eendracht.cs.vu.nl:8081";
  CHECK(apr_allocator_create(&allocer));
  CHECK(apr_pool_create_ex(&pool, NULL, NULL, allocer));
  GlobuleEvent evt(pool);
  adapt(evt);
  apr_pool_destroy(pool);
  apr_allocator_destroy(allocer);
  fprintf(stderr,"done.\n");
  exit(0);
}
#endif

fornax::StateId
server::step(fornax::State<struct server_vars> *state)
{
  switch(state->id) {
  case 0: {
    state->vars.policy = &(state->vars._policy);
    state->vars.transferTime = &(state->vars._transferTime);
    state->vars.setupTime = &(state->vars._setupTime);
    state->vars.ismaster = &(state->vars._ismaster);
    state->vars.master = &(state->vars._master);
    state->vars.docs = &(state->vars._docs);
    state->vars.requests = &(state->vars._requests);
    struct server_main &evtdata = args();
    state->vars.main_theSetupTime = evtdata.theSetupTime;
    state->vars.main_theTransferTime = evtdata.theTransferTime;
    state->vars.main_isTheMaster = evtdata.isTheMaster;
    state->vars.main_theMaster = evtdata.theMaster;
    state->vars.main_thePolicy = evtdata.thePolicy;
    state->vars.main_theDocuments = evtdata.theDocuments;
    (*(state->vars.ismaster))      = (state->vars.main_isTheMaster);
    (*(state->vars.master))        = (state->vars.main_theMaster);
    (*(state->vars.policy))        = (state->vars.main_thePolicy);
    (*(state->vars.setupTime))     = (state->vars.main_theSetupTime);
    (*(state->vars.transferTime))  = (state->vars.main_theTransferTime);
    (*(state->vars.docs))          = (state->vars.main_theDocuments);
    (*(state->vars.requests))  =  0;
    return 2;
  }
  case 2: {
    recv(state, 4, queues_2);
    return 1;
  }
  case 1: {
    return 2;
  }
  case 3: {
    state->vars._update = queue_update.event();
    struct server_update &evtdata = state->vars._update->args();
    state->vars.update_docid = evtdata.docid;
    state->vars.update_size = evtdata.size;
    state->vars._update->reply();
    return 11;
  }
  case 7: {
    if((state->vars.update_iter) != (*(state->vars.docs)) [ (state->vars.update_docid) ] . replicas . end ( )) {
      (state->vars.update_peer)  =  * (state->vars.update_iter);
      { struct server_invalidate evtdata = {  };
      state->vars._5 = new fornax::Event<struct server_invalidate>(evtdata);
      (*(state->vars.update_peer)).queue_invalidate.send(state, state->vars._5);
      return 5; }
    }
    return 8;
  }
  case 4: {
    (state->vars.update_iter) ++;
    return 7;
  }
  case 5: {
    delete state->vars._5;
    return 6;
  }
  case 6: {
    return 4;
  }
  case 8: {
    (state->vars.update_t2)  = time();
    return 9;
  }
  case 9: {
    (*(state->vars.docs)) [ (state->vars.update_docid) ] . cost  += (state->vars.update_t2)  - (state->vars.update_t1);
    return 10;
  }
  case 10: {
    (*(state->vars.docs)) [ (state->vars.update_docid) ] . replicas . clear ( );
    return 0;
  }
  case 11: {
    (*(state->vars.docs)) [ (state->vars.update_docid) ] . lastmod  = time();
    return 12;
  }
  case 12: {
    (*(state->vars.docs)) [ (state->vars.update_docid) ] . lastupd  = time();
    return 13;
  }
  case 13: {
    (*(state->vars.docs)) [ (state->vars.update_docid) ] . docsize  = (state->vars.update_size);
    return 14;
  }
  case 14: {
    if(workaround2 ( (*(state->vars.policy)) , (*(state->vars.docs)) [ (state->vars.update_docid) ] )) {
      if(! (*(state->vars.docs)) [ (state->vars.update_docid) ] . replicas . empty ( )) {
        (state->vars.update_t1)  = time();
        (state->vars.update_iter) = (*(state->vars.docs)) [ (state->vars.update_docid) ] . replicas . begin ( );
        return 7;
      } else {
        return 0;
      }
      return 0;
    } else {
      return 0;
    }
    return 0;
  }
  case 15: {
    state->vars._invalidate = queue_invalidate.event();
    struct server_invalidate &evtdata = state->vars._invalidate->args();
    state->vars.invalidate_docid = evtdata.docid;
    state->wait(llrint ( (*(state->vars.setupTime)) * 1000000 ));
    return 16;  }
  case 16: {
    (*(state->vars.docs)) [ (state->vars.invalidate_docid) ] . lastupd  =  - 1;
    return 17;
  }
  case 17: {
    state->vars._invalidate->reply();
    return 0;
  }
  case 18: {
    state->vars._subscribe = queue_subscribe.event();
    struct server_subscribe &evtdata = state->vars._subscribe->args();
    state->vars.subscribe_docid = evtdata.docid;
    state->vars.subscribe_replica = evtdata.replica;
    (*(state->vars.docs)) [ (state->vars.subscribe_docid) ] . replicas . insert ( (state->vars.subscribe_replica) );
    state->vars._subscribe->reply();
    return 0;
  }
  case 19: {
    state->vars._retrieve = queue_retrieve.event();
   state->satisfy();
    struct server_retrieve &evtdata = state->vars._retrieve->args();
    state->vars.retrieve_docid = evtdata.docid;
    if(( (*(state->vars.docs)) [ (state->vars.retrieve_docid) ] . lastupd  <  0 )  || 
        ( time()  - (*(state->vars.docs)) [ (state->vars.retrieve_docid) ] . lastupd  > workaround1 ( (*(state->vars.policy)) , (*(state->vars.docs)) [ (state->vars.retrieve_docid) ] ) )) {
      if(! (*(state->vars.ismaster))) {
        { struct server_retrieve evtdata = {  };
        state->vars._20 = new fornax::Event<struct server_retrieve>(evtdata);
        (*(*(state->vars.master))).queue_retrieve.send(state, state->vars._20);
        return 20; }
      } else {
        return 24;
      }
      return 24;
    } else {
      return 25;
    }
    return 25;
  }
  case 29: {
    (state->vars.retrieve_curtime)  = time();
    return 30;
  }
  case 23: {
    return 24;
  }
  case 20: {
    (*(state->vars.docs)) [ (state->vars.retrieve_docid) ] = state->vars._20->args()._;
    delete state->vars._20;
    return 21;
  }
  case 21: {
    return 22;
  }
  case 22: {
    if(workaround2 ( (*(state->vars.policy)) , (*(state->vars.docs)) [ (state->vars.retrieve_docid) ] )) {
      { struct server_subscribe evtdata = { (state->vars.retrieve_docid) , this };
      fornax::Event<struct server_subscribe> evt(evtdata);
      (*(*(state->vars.master))).queue_subscribe.send(evt);
      return 23; }
    } else {
      return 23;
    }
    return 23;
  }
  case 24: {
    (*(state->vars.docs)) [ (state->vars.retrieve_docid) ] . lastupd  = time();
    return 25;
  }
  case 25: {
    
    ++ (*(state->vars.requests));
    return 26;
  }
  case 26: {
    (state->vars.retrieve_curtime)  = time();
    return 27;
  }
  case 27: {
    printf ( "%s: retrieve called at %lld, #active requests = %d\n" , name ( ) . c_str ( ) , (state->vars.retrieve_curtime) , (*(state->vars.requests)) );
    return 28;
  }
  case 28: {
    state->wait(llrint ( ( (*(state->vars.setupTime))  + (*(state->vars.transferTime))  * (*(state->vars.docs)) [ (state->vars.retrieve_docid) ] . docsize ) * 1000000 ));
    return 29;  }
  case 30: {
    printf ( "%s: retrieve complete at %lld, #active requests = %d\n" , name ( ) . c_str ( ) , (state->vars.retrieve_curtime) , (*(state->vars.requests)) );
    return 31;
  }
  case 31: {
    
    -- (*(state->vars.requests));
    return 32;
  }
  case 32: {
    state->vars._retrieve->args()._ = (*(state->vars.docs)) [ (state->vars.retrieve_docid) ];
    state->vars._retrieve->reply();
    return 0;
  }
  default:
    return state->error();
  }
}

fornax::StateId
dispatcher::step(fornax::State<struct dispatcher_vars> *state)
{
  switch(state->id) {
  case 0: {
    state->vars.reallastmod = &(state->vars._reallastmod);
    state->vars.master = &(state->vars._master);
    state->vars.servers = &(state->vars._servers);
    state->vars.docs = &(state->vars._docs);
    struct dispatcher_main &evtdata = args();
    state->vars.main_theDocuments = evtdata.theDocuments;
    state->vars.main_peer = evtdata.peer;
    state->vars.main_peers = evtdata.peers;
    (*(state->vars.master))   = (state->vars.main_peer);
    (*(state->vars.servers))  = (state->vars.main_peers);
    (*(state->vars.docs))     = (state->vars.main_theDocuments);
    (state->vars.main_docid) = 0;
    return 5;
  }
  case 2: {
    recv(state, 2, queues_2);
    return 1;
  }
  case 1: {
    return 2;
  }
  case 5: {
    if((state->vars.main_docid) < (*(state->vars.docs)) . count ( )) {
      { struct server_update evtdata = { (state->vars.main_docid) , (*(state->vars.docs)) [ (state->vars.main_docid) ] . docsize };
      state->vars._4 = new fornax::Event<struct server_update>(evtdata);
      (*(*(state->vars.master))).queue_update.send(state, state->vars._4);
      return 4; }
    }
    return 6;
  }
  case 3: {
    (state->vars.main_docid) ++;
    return 5;
  }
  case 4: {
    delete state->vars._4;
    return 3;
  }
  case 6: {
    return 7;
  }
  case 7: {
    return 2;
  }
  case 8: {
    return 0;
  }
  case 9: {
    state->vars._update = queue_update.event();
    struct dispatcher_update &evtdata = state->vars._update->args();
    state->vars.update_docid = evtdata.docid;
    state->vars.update_size = evtdata.size;
    (*(state->vars.reallastmod))  = time();
    { struct server_update evtdata = { (state->vars.update_docid) , (state->vars.update_size) };
    state->vars._10 = new fornax::Event<struct server_update>(evtdata);
    (*(*(state->vars.master))).queue_update.send(state, state->vars._10);
    return 10; }
  }
  case 10: {
    delete state->vars._10;
    return 11;
  }
  case 11: {
    return 12;
  }
  case 12: {
    state->vars._update->reply();
    return 0;
  }
  case 13: {
    state->vars._retrieve = queue_retrieve.event();
   state->satisfy();
    struct dispatcher_retrieve &evtdata = state->vars._retrieve->args();
    state->vars.retrieve_docid = evtdata.docid;
    state->vars.retrieve_server = evtdata.server;
    state->vars._retrieve->reply();
    return 15;
  }
  case 14: {
    (state->vars.retrieve_doc) = state->vars._14->args()._;
    delete state->vars._14;
    return 17;
  }
  case 17: {
    return 18;
  }
  case 15: {
    (state->vars.retrieve_t1)  = time();
    return 16;
  }
  case 16: {
    { struct server_retrieve evtdata = { (state->vars.retrieve_docid) };
    state->vars._14 = new fornax::Event<struct server_retrieve>(evtdata);
    (*(*(state->vars.servers)) [ (state->vars.retrieve_server) ]).queue_retrieve.send(state, state->vars._14);
    return 14; }
  }
  case 18: {
    (state->vars.retrieve_t2)  = time();
    return 19;
  }
  case 19: {
    if((state->vars.retrieve_doc) . lastmod  != (*(state->vars.docs)) [ (state->vars.retrieve_docid) ] . lastmod) {
      (*(state->vars.docs)) [ (state->vars.retrieve_docid) ] . cost  +=  3600;
    } else {
      (*(state->vars.docs)) [ (state->vars.retrieve_docid) ] . cost  += (state->vars.retrieve_t2)  - (state->vars.retrieve_t1);
    }
    return 0;
  }
  default:
    return state->error();
  }
}
