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
#include <httpd.h>
#include <apr.h>
#include <apr_time.h>
#include <apr_thread_proc.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <http_protocol.h>
#include <http_log.h>
#include <vector>
#ifndef WIN32
extern "C" {
#include <unistd.h>
#include <unixd.h>
};
#endif /* !WIN32 */
#include "utilities.h"
#include "monitoring.h"
#include "heartbeat.hpp"
#include "Constants.hpp"
#include "event/HeartBeatEvent.hpp"
#include "event/HttpReqEvent.hpp"
#include "event/HttpMetaEvent.hpp"
#include "event/RegisterEvent.hpp"
#include "netw/Url.hpp"
#include "netw/HttpRequest.hpp"
#include "resource/ConfigHandler.hpp"

/****************************************************************************/

extern "C" {

static int quit = 0;

void * APR_THREAD_FUNC
run(apr_thread_t *thr, void *arg)
{
  apr_allocator_t* allocer;
  apr_pool_t* mainpool;
  int status;
  int failures = 0; // num of consecutive failures, not total number of fails
  //unixd_setup_child();
  if(apr_allocator_create(&allocer)) {
    apr_thread_exit(thr, APR_EGENERAL);
    return NULL;
  }
  if(apr_pool_create_ex(&mainpool, NULL, NULL, allocer)) {
    apr_thread_exit(thr, APR_EGENERAL);
    return NULL;
  }
  apr_pool_tag(mainpool, "hbthread mainpool");
  Url hburl(mainpool, (char*)arg);
  apr_sockaddr_t* hbaddr;
  hburl.address(mainpool, &hbaddr, true); // force caching of local address

  do {
    apr_pool_t* pool;
    if(apr_pool_create_ex(&pool, mainpool, NULL, NULL))
      break;
    apr_pool_tag(mainpool, "hbthread subpool");
    /* Re-use this pool an arbitary times number over. */
    for(int i=0; i<64 && !quit; i++) {
      { // To let the HttpRequest object go out of scope and be destroyed.
        globule::netw::HttpRequest req(pool, hburl);
        try {
          req.setKeepAlive((apr_size_t)0); // Keep-alive with Content-Length zero
          req.setMethod("SIGNAL");
          /* The internal request doesn't identify itself with a proper
           * protocol version number:
           *     req.setHeader("X-Globule-Protocol", PROTVERSION);
           * mainly because we do not have access to it, but also because we are
           * guaranteed to be right.  This does mean that checking the protocol
           * version cannot be done on heartbeats.
           */
          req.setAuthorization(ConfigHandler::password, ConfigHandler::password);
          req.setConnectTimeout(2);
          status = req.getStatus(pool);
          if(status != globule::netw::HttpResponse::HTTPRES_OK &&
             status != globule::netw::HttpResponse::HTTPRES_NO_CONTENT) {
            if(++failures >= 10) {
              failures = 0;
              ap_log_perror(APLOG_MARK, APLOG_ERR, OK, pool, "unable to contact "
                            "own server for periodic processing on %s",
                            hburl(pool));
            }
          } else
            failures = 0;
        } catch(globule::netw::HttpException ex) {
          req.setKeepAlive(false);
          if(++failures >= 10) {
            failures = 0;
            ap_log_perror(APLOG_MARK, APLOG_ERR, OK, pool, "unable to contact "
                          "own server for periodic processing on %s",
                          hburl(pool));
          }
        }
      }
      apr_sleep(HeartBeat::HEART_BEAT_RESOLUTION);
    }
    apr_pool_destroy(pool);
  } while(!quit);
  apr_pool_destroy(mainpool);
  apr_allocator_destroy(allocer);
  apr_thread_exit(thr, APR_SUCCESS);
  return NULL;
}

}

/****************************************************************************/

class HeartBeat::Thread
{
  HeartBeat*        _heartbeat;
  apr_pool_t*       _pool;
  apr_thread_t*     _thread;
  apr_threadattr_t* _attr;
  Url                hburl;
public:
  Thread(apr_pool_t* p, HeartBeat *heartbeat, const Url& location)
    throw()
    : _pool(0), _thread(0), hburl(p, location)
  {
    apr_allocator_t* allocer;
    apr_allocator_create(&allocer);
    apr_pool_create_ex(&_pool, NULL, NULL, allocer);
    _heartbeat = heartbeat;
    quit = 0;
  };
  void start() throw()
  {
    if(apr_threadattr_create(&_attr, _pool) < 0) {
      DIAG(0,(MONITOR(fatal)),("GlobuleThread: cannot create thread attribute"));
      return;
    }
    if(apr_thread_create(&_thread,_attr,run,(void*)(hburl(_pool)),_pool) < 0) {
      DIAG(0,(MONITOR(fatal)),("GlobuleThread: cannot create thread"));
      return;
    }
  };
  void terminate() throw()
  {
    apr_status_t status;
    quit = true;
    apr_status_t thrstatus;
    //apr_sleep((HeartBeat::HEART_BEAT_RESOLUTION*3+1)/2);
    status = apr_thread_join(&thrstatus, _thread);
    if(status != APR_SUCCESS)
      return;
  };
};

/****************************************************************************/

HeartBeat::HeartBeat(Handler* parent, apr_pool_t* p, const Url& selfcontact)
  throw()
  : Handler(parent,p,selfcontact)
{
  _thread = new Thread(p, this, selfcontact);
  _thread->start();
}

HeartBeat::~HeartBeat() throw()
{
}

void
HeartBeat::flush(apr_pool_t* p) throw()
{
  _thread->terminate();
}

Lock*
HeartBeat::getlock() throw()
{
  return &_lock;
}

void
HeartBeat::add(Context* ctx, const apr_time_t t, Registration hbr) throw()
{
  _lock.lock();
  _recv.insert(gmultimap<const apr_time_t, Registration>::value_type(t, hbr));
  _lock.unlock();
}

bool
HeartBeat::handle(GlobuleEvent& evt) throw()
{
  int err;
  switch(evt.type()) {
  case GlobuleEvent::HEART_BEAT_REG: {
    // optionally check if(evt.target() == this)
    RegisterEvent& ev = (RegisterEvent&)evt;
    add(evt.context, apr_time_now() + ev.interval(),
	Registration(ev.interval(), ev.destination()));
    return true;
  }
  case GlobuleEvent::INVALID_EVT: {
    HttpMetaEvent& metaevt = (HttpMetaEvent&)evt;
    /* The check on r-method being SIGNAL is unnecessary, as a HttpMetaEvent
     * can only be a SIGNAL HTTP method.  However, for clarification and
     * future extension, we make the check.
     */
    request_rec* r = metaevt.getRequest();
    if(r && r->args && !strcmp(r->args,"heartbeat") &&
       r->method && !strcmp(r->method,"SIGNAL")) {
      DIAG(evt.context,(MONITOR(detail)),("Received heartbeat request"));
      if((err = ap_discard_request_body(r)) != OK) {
        metaevt.setStatus(HTTP_INTERNAL_SERVER_ERROR);
        return true;
      }
      /* To allow heart beat registrations during the sending of heartbeat
       * events, we go through the list in two phases, with more overhead
       * to insert events back into the queue.
       */
      apr_time_t now = apr_time_now();
      _lock.lock();
      std::vector<Registration> events;
      while(!_recv.empty()) {
        gmultimap<const apr_time_t, Registration>::iterator iter=_recv.begin();
        if(iter->first <= now) {
          events.push_back(iter->second);
          _recv.erase(iter);
        } else
          break;
      }
      _lock.unlock();
      for(std::vector<Registration>::iterator iter = events.begin();
          iter != events.end();
          ++iter)
        {
          HeartBeatEvent ev(metaevt.pool, evt.context, iter->listener());
          if(iter->interval() > 0) {
            if(GlobuleEvent::submit(ev))
              add(evt.context, now+iter->interval(), *iter);
          } else {
            if(!GlobuleEvent::submit(ev)) {
              DIAG(evt.context,(MONITOR(error)),
		   ("Failed to wake up or initialize %s",
		    iter->listener()->location(evt.pool)));
              add(evt.context, now+HEART_BEAT_RESOLUTION, *iter);
            }
          }
        }
      DIAG(evt.context,(MONITOR(heartbeat)),(0));
      metaevt.setStatus(HTTP_NO_CONTENT);
      return true;
    }
    return false;
  }
  default:
    return false;
  }
}

std::string
HeartBeat::description() const throw()
{
  return "HeartBeat";
}

/****************************************************************************/
