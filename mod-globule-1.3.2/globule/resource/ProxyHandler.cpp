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
#include <http_protocol.h>
#include <string>
#include "utilities.h"
#include "resource/ProxyHandler.hpp"
#include "netw/HttpRequest.hpp"
#include "event/RedirectEvent.hpp"
#include "event/PostRequestEvent.hpp"
#include "event/ReportEvent.hpp"
#include "event/HttpLogEvent.hpp"

using namespace std;
using namespace globule::netw;

ProxyHandler::ProxyHandler(Handler* parent, apr_pool_t* p, Context* ctx,
                           const Url& uri, const char* path,
                           const char* upstream)
  throw()
  : SourceHandler(parent, p, ctx, uri, path)
{
  _peers.add(ctx, p, Contact(p, Peer::ORIGIN, upstream, ""));
}

ProxyHandler::~ProxyHandler() throw()
{
}

bool
ProxyHandler::initialize(apr_pool_t* p, Context* ctx,
			 HandlerConfiguration* cfg)
  throw(SetupException)
{
  if(SourceHandler::initialize(p, ctx, cfg))
    return true;
  DIAG(ctx,(MONITOR(info)),("Initializing proxy section url=%s\n",_uri(p)));
  return false;
}

bool
ProxyHandler::handle(GlobuleEvent& evt, const string& remainder) throw()
{
  _lock.lock_shared();
  Handler* target = gettarget(evt.pool, evt.context, remainder, evt.target());

  switch(evt.type()) {
  case GlobuleEvent::REDIRECT_EVT: {
    RedirectEvent& ev = (RedirectEvent&) evt;
    request_rec* r = ev.getRequest();
    if((_redirect_mode&CFG_MODE_HTTP && r) ||
       (_redirect_mode&CFG_MODE_DNS && !r)) {
      if(!r || !r->header_only) {
        /* do not redirect on HEAD requests only (unneeded and solves problem
         * for PHP scripts to make sure they are locally available.
         */
        redirect(ev, remainder);
      }
    }
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
    checkreplicas(evt.pool, evt.context);
    return true;
  }
  case GlobuleEvent::INVALID_EVT: {
    _lock.unlock();
    HttpMetaEvent& ev = (HttpMetaEvent&) evt;
    request_rec* r = ev.getRequest();
    if(r) {
      const char* protversion = apr_pstrdup(r->pool, mkstring::format("%d",
                                                                      PROTVERSION).c_str());
      apr_table_set(r->headers_out,"X-Globule-Protocol", protversion);
    }
    ev.setStatus(HTTP_NO_CONTENT);
    return true;
  }
  case GlobuleEvent::WRAPUP_EVT: {
    PostRequestEvent& ev = (PostRequestEvent&) evt;
    if(ev.getRequestStatusPurging()) {
      DIAG(evt.context,(MONITOR(warning)),("Document %s not found (%d), purging replication information",remainder.c_str(),ev.getRequest()->status));
      if(target) {
        Lock* lock = getlock();
        ap_assert(lock != &_lock);
        lock->lock_exclusively();
        _lock.unlock();
        target->handle(ev);
        lock->unlock();
      } else
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
    return true;
  }
  case GlobuleEvent::REQUEST_EVT: {
    HttpReqEvent& ev = (HttpReqEvent&) evt;
    request_rec* req = ev.getRequest();
    if(req && apr_table_get(req->headers_in, "X-From-Replica")) {
      Peer* peer;
      if(!(peer = authorized(evt.context, req))) {
        DIAG(evt.context,(MONITOR(error)),("Got replica request from %s that cannot be authenticated",apr_table_get(ev.getRequest()->headers_in, "X-From-Replica")));
        _lock.unlock();
        ev.setStatus(HTTP_UNAUTHORIZED);
        return true;
      }
      ev.setPeer(peer);
      if(req->content_type &&
         !strcasecmp(req->content_type, "application/x-httpd-php")) {
        ap_set_content_type(req,"application/x-httpd-php-raw");
        for(ap_filter_t* f=req->output_filters; f; f=f->next)
          if(!strcmp(f->frec->name,"php")) {
            ap_remove_output_filter(f);
            break;
          }
      }
    }
    Lock* lock = (target ? target->getlock() : 0);
    ap_assert(lock != &_lock);
    if(!target) {
      _lock.unlock();
      _lock.lock_exclusively();
      target = gettarget(evt.pool, evt.context, remainder, "Invalidate");
      lock = target->getlock();
      lock->lock_exclusively();
      _lock.unlock();
    } else {
      lock->lock_exclusively();
      _lock.unlock();
    }
    target->handle(evt);
    lock->unlock();
    return true;
  }
  case GlobuleEvent::LOGGING_EVT: {
    _lock.unlock();
    HttpLogEvent& ev = (HttpLogEvent&)evt;
    receivelog(ev.getRequest());
    ev.setStatus(HTTP_NO_CONTENT);
    return true;
  }
  case GlobuleEvent::REPORT_EVT: {
    _lock.unlock();
    return false;
  }
  default:
    _lock.unlock();
    ReportEvent& ev = (ReportEvent&)evt;
    ReportEvent subevt("section", &ev);
    subevt.setProperty("uri",_uri(evt.pool));
    subevt.setProperty("type","origin");
    subevt.setProperty("path",_path);
    subevt.setProperty("servername",_servername);
    subevt.setProperty("refreshinterval",mkstring()<<_reevaluateinterval);
    subevt.setProperty("replicatepolicy",_default_policy);
    subevt.setProperty("redirectpolicy",_redirect_policy->policyName());
    BaseHandler::handle(subevt, remainder);
    return true;
  }
}

string
ProxyHandler::description() const throw()
{
  return mkstring() << "ProxyHandler(" << _uri.host() << ":" << _uri.port()
                    << _uri.path()<<")";
}

Fetch*
ProxyHandler::fetch(apr_pool_t* p, const char* path,
                    const Url& replicaUrl, apr_time_t ims,
                    const char* arguments, const char* method)
  throw()
  /* FIXME: arguments should be the first argument after path,
   * also make replicaUrl and ims and possible other args as single
   * parameter describing the preconditions or options in the request
   * as demanded by the policy.
   */
{
  using namespace globule::netw;
  Peer* peer = _peers.origin();
  Peers::iterator iter = _peers.begin(Peer::KEEPER);
  HttpRequest* req = 0;
  try {
    Url source(p, peer->uri(), path);
    req = new HttpRequest(p, source);
    if(method)
      req->setMethod(method);
    req->setHeader("X-Globule-Protocol", PROTVERSION);
    req->setHeader("X-From-Replica", replicaUrl(p));
    if(arguments)
      req->setHeader("Content-Length", mkstring()<<strlen(arguments));
    req->setConnectTimeout(10);
    if(ims != 0)
      req->setIfModifiedSince(ims);
    if(arguments) {
      HttpConnection* con = req->connect(p);
      if(con)
        con->rputs(arguments);
    }
    int status = req->getStatus(p);
    if(status == HttpResponse::HTTPRES_NO_CONTENT ||
       status == HttpResponse::HTTPRES_OK ||
       status == HttpResponse::HTTPRES_NOT_FOUND ||
       status == HttpResponse::HTTPRES_MOVED_PERMANENTLY ||
       status == HttpResponse::HTTPRES_MOVED_TEMPORARILY) {
      string recvdlocation = req->response(p)->getHeader("Location");
      if(recvdlocation != "") {
        const char* baselocation = peer->location(p);
        if(!strncmp(recvdlocation.c_str(),baselocation,strlen(baselocation)))
          req->response(p)->setHeader("Location", apr_pstrcat(p,location(p),
                          &recvdlocation.c_str()[strlen(baselocation)], NULL));
      }
      req->response(p)->setHeader("X-Globule-Policy", apr_pstrdup(p, _default_policy.c_str()));
      return new HttpFetch(p, req);
    }
    delete req;
    req = 0;
  } catch(HttpException) {
    if(req) {
      delete req;
      req = 0;
    }
    // log that server source isn't available
  }
  return new ExceptionFetch(Fetch::HTTPRES_SERVICE_UNAVAILABLE,
                            "Upstream not available");
}
