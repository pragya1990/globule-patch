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
#include <apr.h>
#include <string>
#include "utilities.h"
#include "resource/ImportHandler.hpp"
#include "netw/HttpRequest.hpp"
#include "event/RedirectEvent.hpp"
#include "event/PostRequestEvent.hpp"
#include "event/ReportEvent.hpp"
#include "event/RegisterEvent.hpp"
#include "configuration.hpp"

using namespace std;
using namespace globule::netw;

ImportHandler::ImportHandler(Handler* parent, apr_pool_t* p, Context* ctx,
                             const Url& uri, const char* path,
                             const char* upstream, const char* password)
  throw()
  : BaseHandler(parent, p, ctx, uri, path)
{
  _peers.add(ctx, p, Contact(p, Peer::ORIGIN, upstream, password));
  _lastshipmentsize = logsize(p); // FIXME
}

ImportHandler::~ImportHandler() throw()
{
}

bool
ImportHandler::initialize(apr_pool_t* p, Context* ctx,
			  HandlerConfiguration* cfg)
  throw(SetupException)
{
  if(BaseHandler::initialize(p, ctx, cfg))
    return true;
  DIAG(ctx,(MONITOR(info)),("Initializing import section url=%s\n",_uri(p)));
  _reevaluateinterval = cfg->get_reevaluateinterval();
  if(_reevaluateinterval) {
    RegisterEvent ev(p, this, _reevaluateinterval);
    if(!GlobuleEvent::submit(ev)) {
      throw "ResourcePool cannot register to HeartBeat";
    }
  }
  BaseHandler::initialize(p, ctx, cfg->get_contacts()); // FIXME? only keepers
  return false;
}

bool
ImportHandler::handle(GlobuleEvent& evt, const string& remainder) throw()
{
#ifdef NOTDEFINED
  {
    Handler* target = gettarget(evt.pool,evt.context,remainder,evt.target());
    if(evt.type() == HTTP_META && target != this && target) {
      HttpMetaEvent& ev = (HttpMetaEvent&)evt;
      request_rec* r = ev.getRequest();
      if(!authorized(r)) {
        ev.setStatus(HTTP_UNAUTHORIZED);
        return true;
      }
      DIAG(evt.context,(MONITOR(info)),("Invalidate postponed, lock busy or could not try to aquire lock, retrying to invalidate document at later moment\n"));
      _invalidates.insert(remainder.c_str());
      ev.setStatus(HTTP_LOCKED);
      return true;
    }
  }
#endif

  _lock.lock_shared();
  Handler* target = gettarget(evt.pool, evt.context, remainder, evt.target());

  switch(evt.type()) {
  case GlobuleEvent::REDIRECT_EVT: {
    RedirectEvent& ev = (RedirectEvent&) evt;
    /* Caveat: by returning DECLINED we say that Apache should
     * do the ap_hook_translate_name(), that is, figure out the
     * local filename corresponding to the URL. This delegation can
     * cause a problem when running a Globule slave. If e.g. the
     * DocumentRoot is not readable for the user running the server,
     * the name translation returns forbidden, and our slave's request
     * handler is never called.
     * Only when we're slave, otherwise we need the URI->filename 
     * translation by Apache.
     */
    _lock.unlock();
    /* Setting the filename is needed to humor ap_directory_walk() such that
     * it will not indicate that the file isn't present.  This because
     * then actually serving the file we will do the actual fetching.
     */
    ev.getRequest()->filename = "Boe";
    return true;
  }
  case GlobuleEvent::HEARTBEAT_EVT: {
    _lock.unlock();
    shiplog(evt.context, evt.pool, _lastshipmentsize);
    return true;
  }
  case GlobuleEvent::INVALID_EVT: {
    HttpMetaEvent& ev = (HttpMetaEvent&)evt;
    request_rec* r = ev.getRequest();
    if(!authorized(evt.context, r)) {
      _lock.unlock();
      ev.setStatus(HTTP_UNAUTHORIZED);
      return true;
    }
    if(target == this) {
      const char* s;
      apr_time_t serial;
      if(r && (s = apr_table_get(r->headers_in, "X-Globule-Serial")) &&
         (serial = apr_atoi64(s))) {
        if(serial <= 0)
          serial = apr_time_now();
        if(_peers.origin()->serial() < serial) {
          /* We've got an old state compared to the origin server */
          DIAG(evt.context,(MONITOR(error)),("master has more recent state"));
          log(evt.pool, "F", "");
          delalltargets(evt.context, evt.pool);
          _peers.origin()->setserial(serial);
        }
      }
      _lock.unlock();
      if(r) {
        const char* protversion = apr_pstrdup(r->pool, mkstring::format("%d",
                                                                        PROTVERSION).c_str());
        apr_table_set(r->headers_out, "X-Globule-Protocol", protversion);
      }
      ev.setStatus(HTTP_NO_CONTENT);
      return true;
    }
    if(target) {
      Lock* lock = target->getlock();
      if(lock->try_lock_exclusively()) {
        _lock.unlock();
        target->handle(evt);
        lock->unlock();
      } else {
        /* Unfortunately, trylock() isn't implemented for global mutexes.
         * This means that the implementation foreseen, by returning
         * HTTP_LOCKED to the master when the lock is taken such that it can
         * re-try later, cannot be done at the moment.  The trylock() will
         * always fail, so we omit testing for that value now, and adopt the
         * scheme of adding the document to a set.  This set is inspected upon
         * retrieval of the same document to actually deliver the invalidate.
         * The invalidates and _rsrcpool_lock can be removed if we have a
         * better solution to this locking problem.
         */
        DIAG(evt.context,(MONITOR(info)),("Invalidate postponed, lock busy or could not try to aquire lock, retrying to invalidate document at later moment\n"));
        _invalidates.insert(remainder.c_str());
        _lock.unlock();
        ev.setStatus(HTTP_LOCKED);
      }
    } else {
      _lock.unlock();
      ev.setStatus(HTTP_NOT_FOUND);
    }
    return true;
  }
  case GlobuleEvent::WRAPUP_EVT: {
    PostRequestEvent& ev = (PostRequestEvent&) evt;
    if(ev.getRequestStatusPurging()) {
      DIAG(evt.context,(MONITOR(warning)),("Document %s not successfully delivered (%d), purging replica copy ",remainder.c_str(),ev.getRequest()->status));
      _lock.unlock();
      deltarget(evt.context, evt.pool, remainder);
    } else if(target) {
      Lock* lock = target->getlock();
      ap_assert(lock != &_lock);
      lock->lock_exclusively();
      _lock.unlock();
      target->handle(ev);
      lock->unlock();
    } else
      _lock.unlock();
    if(accounting())
      accounting()->conform(ev.context, ev.pool);
    return true;
  }
  case GlobuleEvent::EVICTION_EVT: {
    if(target) {
      Lock* lock = target->getlock();
      ap_assert(lock != &_lock);
      lock->lock_exclusively();
      _lock.unlock();
      target->handle(evt);
      lock->unlock();
    } else
      _lock.unlock();
    deltarget(evt.context, evt.pool, remainder);
    return true;
  }
  case GlobuleEvent::LOAD_EVT:
  case GlobuleEvent::REQUEST_EVT: {
    if(evt.type() == GlobuleEvent::REQUEST_EVT) {
      /* Send any invalidate signal that was pending for this document
       * We can only do this if we have a request_rec, so this implies that
       * we can only do it for HTTP_REQ events.
       */
      // FIXME: race condition in the entire handling of_invalidates.
      gset<gstring>::iterator iter = _invalidates.find(remainder.c_str());
      if(iter != _invalidates.end()) {
        DIAG(evt.context,(MONITOR(info)),("Pending invalidation of document detected, delivering now"));
        _invalidates.erase(iter);
        if(target) {
          Lock *lock = target->getlock();
          lock->lock_exclusively();
          /* One is tempted to build a HttpMetaEvent with as argument the 
           * original request, such that things such as request_rec can be
           * inherited (for the path for instance).  However, this will result
           * in the r->status being set to HTTP_NO_CONTENT.  One assumes
           * this is not a problem as in the request handling the r->status
           * will be overwritten as it will serve the request.  However we will
           * return DECLINED in many cases, and Apache (strangly enough) will
           * not reset the field.  Therefor we will use the final target
           * as parameter to HttpMetaEvent, which will work as we deliver the
           * request immediately to the target, and not through the normal
           * GlobuleEvent:submit() procedure.
           */
          HttpMetaEvent invalidationevt(evt.pool, evt.context, target);
          target->handle(invalidationevt);
          lock->unlock();
        }
      }
    }
    Lock* lock = (target ? target->getlock() : 0);
    if(!target) {
      _lock.unlock();
      _lock.lock_exclusively();
      target = gettarget(evt.pool, evt.context, remainder, "Special");
      lock = target->getlock();
      lock->lock_exclusively();
      _lock.unlock();
    } else {
      lock->lock_exclusively();
      _lock.unlock();
    }
    if(target)
      target->handle(evt);
    lock->unlock();
    return true;
  }
  case GlobuleEvent::REPORT_EVT: {
    _lock.unlock();
    ReportEvent& ev = (ReportEvent&)evt;
    ReportEvent subevt("section", &ev);
    subevt.setProperty("uri",_uri(evt.pool));
    subevt.setProperty("site",siteaddress(evt.pool));
    subevt.setProperty("type","replica");
    subevt.setProperty("path",_path);
    subevt.setProperty("servername",_servername);
    BaseHandler::handle(subevt, remainder);
    return false;
  }
  default:
    _lock.unlock();
    return true;
  }
}

bool
ImportHandler::authorized(Context* ctx, request_rec* r) throw()
{
  char *remote_ipaddr = 0, *secret = "";
  apr_port_t remote_port;
  if(!getCredentials(ctx, r, &remote_ipaddr, &remote_port, &secret))
    return false;    
  if(_peers.origin()->authenticate(remote_ipaddr, remote_port, secret)
#ifdef DEBUG
     || (secret && !strcmp(secret,"backdoor"))
#endif
  ) {
    return true;
  } else {
    DIAG(ctx,(MONITOR(error)),("Upstream server used incorrect password"));
    return false;
  }
}

void
ImportHandler::log(apr_pool_t* pool, string msg1, const string msg2) throw()
{
  msg1 += mkstring::format(" server;%s",_servername);
  BaseHandler::log(pool, msg1, msg2);
}

string
ImportHandler::description() const throw()
{
  return mkstring() << "ImportHandler(" << _uri.host() << ":" << _uri.port()
                    << _uri.path()<<")";
}
