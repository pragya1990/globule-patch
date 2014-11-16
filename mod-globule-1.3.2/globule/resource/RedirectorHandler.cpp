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
#include <typeinfo>
#include <string>
#include <apr.h>
#include <apr_file_io.h>
#include <apr_strings.h>
#include <httpd.h>
#include <http_core.h>
#include "utilities.h"
#include "resource/RedirectorHandler.hpp"
#include "event/RedirectEvent.hpp"
#include "event/ReportEvent.hpp"
#include "event/RegisterEvent.hpp"
#include "redirect/dns_config.h"
#include "netw/HttpRequest.hpp"
#include "Storage.hpp"
#include "psodium.h"
#include "configuration.hpp"

using namespace std;

/*
  The redirection handler picks up any and all requests that are made to
  the base url for which it is responsible (_uri).  The browsing client
  is always redirected to a different server.  When the requested URL
  is in the list of replicated documents (no wildcards or prefixes), then
  the client is replicated to one of the replica servers.
  If it is not in the list, the client is redirected to the master.
  
  The upsteam server (location and password) need to be specified in the
  configuration.  Initially the redirection handler attempts to load the
  list of documents and replica list from a locally stored file.
  When shutting down, these information is stored in that same file.
*/

RedirectorHandler::RedirectorHandler(Handler* parent, apr_pool_t* p,
                                     Context* ctx,
                                     const Url& uri, const char* path,
                                     const char* upstream,const char* password)
  throw()
  : BaseHandler(parent, p, ctx, uri, path),
    _upstream_location(p, upstream), _redirect_policy(0)
{
  _upstream_password = apr_pstrdup(p, password);
  _lastshipmentsize = logsize(p); // FIXME
  if(!(_redirect_policy = RedirectPolicy::defaultPolicy()))
    DIAG(ctx, (MONITOR(fatal),&_monitor), ("Could not install default redirection policy"));
  _peers.add(ctx, p, Contact(p,Peer::ORIGIN,upstream,password));
  reload(p, ctx);
}

RedirectorHandler::RedirectorHandler(Handler* parent, apr_pool_t* p,
                                     Context* ctx,
                                     const Url& uri, const char* path)
  throw()
  : BaseHandler(parent, p, ctx, uri, path),
    _upstream_location(uri), _upstream_password(0), _redirect_policy(0)
{
  if(!(_redirect_policy = RedirectPolicy::defaultPolicy()))
    DIAG(ctx, (MONITOR(fatal),&_monitor), ("Could not install default redirection policy"));
}

RedirectorHandler::~RedirectorHandler() throw()
{
}

void
RedirectorHandler::initialize(apr_pool_t* p,
                              int naliases, const char** aliases)
  throw(SetupException)
{
  BaseHandler::initialize(p, naliases, aliases);
  for(int i=0; i<naliases; i++) {
    const char* urlstring = apr_pstrcat(p,"http://",aliases[i],"/",NULL);
    try {
      _aliases.push_back(gUrl(p, urlstring));
    } catch(UrlException ex) {
      string errstring = mkstring() << "ServerAlias address " << urlstring << " is invalid";
      throw SetupException(errstring);
    }
  }
}

bool
RedirectorHandler::initialize(apr_pool_t* p, Context* ctx,
			      HandlerConfiguration* cfg)
  throw(SetupException)
{
  if(BaseHandler::initialize(p, ctx, cfg))
    return true;
  _redirect_mode = cfg->get_redirect_mode();
  _reevaluateinterval = cfg->get_reevaluateinterval();
  if(_reevaluateinterval) {
    RegisterEvent ev(p, this, _reevaluateinterval);
    if(!GlobuleEvent::submit(ev)) {
      throw "ResourcePool cannot register to HeartBeat";
    }
  }
  return false;
}

Peer*
RedirectorHandler::addReplicaFast(Context* ctx, apr_pool_t* p,
                                  const char* location, const char* secret) throw()
{
  Peer* newpeer = 0;
  if(_redirect_policy) {
    vector<Peer*> replicas;
    for(Peers::iterator iter=_peers.begin(Peer::REPLICA|Peer::MIRROR);
        iter != _peers.end();
        ++iter)
      if(iter->enabled > 0)
        replicas.push_back(&*iter);
    newpeer = BaseHandler::addReplicaFast(ctx, p, location, secret);
    _redirect_policy->sync(replicas, newpeer);
  } else
    newpeer = BaseHandler::addReplicaFast(ctx, p, location, secret);
  return newpeer;
}

void
RedirectorHandler::changeReplica(Peer* p) throw()
{
  if(_redirect_policy) {
    vector<Peer*> replicas;
    for(Peers::iterator iter=_peers.begin(Peer::REPLICA|Peer::MIRROR);
        iter != _peers.end();
        ++iter)
      if(iter->enabled > 0)
        replicas.push_back(&*iter);
    _redirect_policy->sync(replicas, p);
  }
}

bool
RedirectorHandler::handle(GlobuleEvent& evt) throw()
{
  if(evt.type() == GlobuleEvent::REDIRECT_EVT) {
    RedirectEvent& ev = (RedirectEvent&)evt;
    if(_redirect_mode&CFG_MODE_DNS && !ev.getRequest()) {
      string remainder;
      for(gvector<gUrl>::iterator iter = _aliases.begin();
          iter != _aliases.end();
          ++iter)
        if(evt.match(*iter, remainder)) {
          evt.target(this);
          break;
        }
    }
  }
  return Handler::handle(evt);
}

bool
RedirectorHandler::handle(GlobuleEvent& evt, const string& remainder) throw()
{
  switch(evt.type()) {
  case GlobuleEvent::REDIRECT_EVT: {
    gstring workaround(remainder.c_str());
    RedirectEvent& ev = (RedirectEvent&)evt;
    _lock.lock_shared();
    if((_redirect_mode&CFG_MODE_HTTP && ev.getRequest()) ||
       (_redirect_mode&CFG_MODE_DNS && !ev.getRequest())) {
      redirect(ev, remainder);
    }
    _lock.unlock();
    break;
  };
  case GlobuleEvent::UPDATE_EVT: {
    _lock.lock_exclusively();
    reload(evt.pool, evt.context);
    _lock.unlock();
    break;
  };
  case GlobuleEvent::HEARTBEAT_EVT: {
    _lock.lock_shared();
    if(fetchcfg(evt.pool, evt.context, _upstream_location, _upstream_password,
		"redirection")) {
      _lock.unlock();
      _lock.lock_exclusively();
      reload(evt.pool, evt.context);
    }
    _lock.unlock();
    _lock.lock_shared();
    checkreplicas(evt.pool, evt.context);
    _lock.unlock();
    shiplog(evt.context, evt.pool, _lastshipmentsize);
    return true;
  };
  case GlobuleEvent::INVALID_EVT: {
    HttpMetaEvent& ev = (HttpMetaEvent&) evt;
    if(evt.target() == this || remainder == "") {
      request_rec* r = ev.getRequest();
      if(r) {
        const char* protversion = apr_pstrdup(r->pool, mkstring::format("%d",
                                                                        PROTVERSION).c_str());
        apr_table_set(r->headers_out, "X-Globule-Protocol", protversion);
      }
      ev.setStatus(HTTP_NO_CONTENT);
      return true;
    } else {
      _lock.lock_shared();
      fetchcfg(evt.pool, evt.context, _upstream_location, _upstream_password,
               "redirection");
      _lock.unlock();
      ev.setStatus(HTTP_NO_CONTENT);
    }
    return true;
  };
  case GlobuleEvent::REPORT_EVT: {
    DEBUGACTION(RedirectorHandler,report,begin);
    ReportEvent& ev = (ReportEvent&)evt;
    ReportEvent subevt("section", &ev);
    subevt.setProperty("uri",_uri(evt.pool));
    subevt.setProperty("site",siteaddress(evt.pool));
    subevt.setProperty("type","redirector");
    subevt.setProperty("path",_path);
    subevt.setProperty("servername",_servername);
    subevt.setProperty("refreshinterval",mkstring()<<_reevaluateinterval);
    subevt.setProperty("redirectpolicy",_redirect_policy->policyName());
    subevt.setProperty("redirectmode", (_redirect_mode==CFG_MODE_DNS ?"dns" :
                                        _redirect_mode==CFG_MODE_HTTP?"http":
                                        _redirect_mode==CFG_MODE_BOTH?"both":
                                                                      "none"));
    subevt.setProperty("redirectttl",mkstring()<<_redirect_policy->ttl());
    BaseHandler::handle(subevt, remainder);
    DEBUGACTION(RedirectorHandler,report,end);
    return false;
  }
  default:
    ; // log some error
  }
  return true;
}

void
RedirectorHandler::checkreplicas(apr_pool_t* pool, Context* ctx) throw()
{
  for(Peers::iterator iter = _peers.begin(Peer::MIRROR);
      iter != _peers.end();
      ++iter)
    {
      if(*iter != *this) {
        if(iter->enabled == 0 || iter->enabled == 1) {
	  Url checkurl(pool, iter->uri().uri());
          try {
            globule::netw::HttpRequest req(pool, checkurl);
            req.setConnectTimeout(3);
            req.setMethod("HEAD");
            req.setHeader("X-Globule-Protocol", PROTVERSION);
            globule::netw::HttpResponse* res = req.response(pool);
            if(res->getStatus() >= 200 || res->getStatus() < 400) {
	      if(!iter->enabled) {
		DIAG(ctx,(MONITOR(availability),&_monitor,iter->_monitor,MONITOR(enabled),MONITOR(change)),(0));
		iter->enabled = 1;
	      } else {
		DIAG(ctx,(MONITOR(availability),&_monitor,iter->_monitor,MONITOR(enabled)),(0));
	      }
            } else if(iter->enabled) {
              DIAG(ctx,(MONITOR(availability),&_monitor,iter->_monitor,MONITOR(disabled),MONITOR(change),MONITOR(response),res->getStatus()),("wrong response (%d)",res->getStatus()));
              iter->enabled = 0;
            } else {
              DIAG(ctx,(MONITOR(availability),&_monitor,iter->_monitor,MONITOR(disabled),MONITOR(response),res->getStatus()),("wrong response (%d)",res->getStatus()));
            }
          } catch(globule::netw::HttpException ex) {
            if(iter->enabled) {
              DIAG(ctx,(MONITOR(availability),&_monitor,iter->_monitor,MONITOR(disabled),MONITOR(change),MONITOR(timeout)),("timeout: %s",ex.getMessage().c_str()));
              iter->enabled = 0;
            } else {
              DIAG(ctx,(MONITOR(availability),&_monitor,iter->_monitor,MONITOR(disabled),MONITOR(timeout)),("timeout: %s",ex.getMessage().c_str()));
            }
          }
	}
      }
    }
  for(Peers::iterator iter = _peers.begin(Peer::REPLICA|Peer::ORIGIN|Peer::KEEPER|Peer::REDIRECTOR);
      iter != _peers.end();
      ++iter)
    {
      /* If this replica isn't the master site itself (prevent checking
       * whether we are up of ourself).
       */
      if(*iter != *this) {
        if(iter->enabled == 0 || iter->enabled == 1) {
          Url baseurl(pool, iter->uri().uri());
          Url poolurl(pool, apr_pstrcat(pool, baseurl(pool), "?status", NULL));
          baseurl.normalize(pool);
          poolurl.normalize(pool);
          try {
            globule::netw::HttpRequest req(pool, poolurl);
            req.setConnectTimeout(3);
            req.setMethod("SIGNAL");
            if(iter->type() == Peer::ORIGIN) {
              if(_upstream_password) 
                req.setAuthorization(_upstream_password, _upstream_password);
            } else
              req.setAuthorization(iter->secret(), iter->secret());
            req.setHeader("X-Globule-Protocol", PROTVERSION);
            req.setHeader("X-Globule-Serial", iter->serial());
            globule::netw::HttpResponse* res = req.response(pool);
            if(res->getStatus() == globule::netw::HttpResponse::HTTPRES_OK ||
               res->getStatus() == globule::netw::HttpResponse::HTTPRES_NO_CONTENT) {
              int protversion = atoi(res->getHeader("X-Globule-Protocol").c_str());
              if(protversion <= 0) {
                if(iter->enabled) {
                  DIAG(ctx,(MONITOR(availability),&_monitor,iter->_monitor,MONITOR(disabled),MONITOR(change),MONITOR(protocol)),("unset protocol version"));
                  iter->enabled = 0;
                } else {
                  DIAG(ctx,(MONITOR(availability),&_monitor,iter->_monitor,MONITOR(disabled),MONITOR(protocol)),("unset protocol version"));
                }
              } else if(protversion < PROTVERSION) {
                if(iter->enabled) {
                  DIAG(ctx,(MONITOR(availability),&_monitor,iter->_monitor,MONITOR(disabled),MONITOR(change),MONITOR(protocol)),("incompatible protocol version"));
                  iter->enabled = 0;
                } else {
                  DIAG(ctx,(MONITOR(availability),&_monitor,iter->_monitor,MONITOR(disabled),MONITOR(protocol)),("incompatible protocol version"));
                }
              } else {
                if(!iter->enabled) {
                  DIAG(ctx,(MONITOR(availability),&_monitor,iter->_monitor,MONITOR(enabled),MONITOR(change)),(0));
                  iter->enabled = 1;
                } else {
                  DIAG(ctx,(MONITOR(availability),&_monitor,iter->_monitor,MONITOR(enabled)),(0));
                }
              }
              if(atoi(res->getHeader("X-Globule-Serial").c_str()) > iter->serial()) {
                DIAG(ctx,(MONITOR(notice)),("Availability check notices site %s is future state, flushing all peers",baseurl(pool)));
                setserial(apr_time_now());
              }
            } else if(iter->enabled) {
              // disabling because failed response (e.g. not globule site)
              DIAG(ctx,(MONITOR(availability),&_monitor,iter->_monitor,MONITOR(disabled),MONITOR(change),MONITOR(response),res->getStatus()),("wrong response (%d)",res->getStatus()));
              iter->enabled = 0;
            } else {
              DIAG(ctx,(MONITOR(availability),&_monitor,iter->_monitor,MONITOR(disabled),MONITOR(response),res->getStatus()),("wrong response (%d)",res->getStatus()));
            }
          } catch(globule::netw::HttpException ex) {
            if(iter->enabled) {
              DIAG(ctx,(MONITOR(availability),&_monitor,iter->_monitor,MONITOR(disabled),MONITOR(change),MONITOR(timeout)),("timeout: %s",ex.getMessage().c_str()));
              // disabling because request failed (e.g. timeout)
              iter->enabled = 0;
            } else {
              DIAG(ctx,(MONITOR(availability),&_monitor,iter->_monitor,MONITOR(disabled),MONITOR(timeout)),("timeout: %s",ex.getMessage().c_str()));
            }
          }
        } else if(iter->enabled > 1) {
          // Availability check skipped site because of received log file
          DIAG(ctx,(MONITOR(availability),&_monitor,iter->_monitor,MONITOR(enabled)),(0));
          iter->enabled -= 1;
        }
      }
    }
}

std::string
RedirectorHandler::description() const throw()
{
  return mkstring() << "RedirectorHandler(" << _uri.host() << ":" << _uri.port()
                    << _uri.path() << ")";
}

void
RedirectorHandler::redirect(RedirectEvent& ev, const string& remainder) throw()
{
  int rtcount = ev.requestedCount();
  vector<Peer*> replicas;
  const char *from_replica_str = NULL;
  apr_uint32_t client_ipaddr;
  request_rec* r = ev.getRequest();

  client_ipaddr = ev.remoteAddress(); 
  if(client_ipaddr == 0L)
    return;

  if(r) {
    if(!r->hostname || r->main)
      // if r->main != NULL then we are doing a subrequest
      return;
    // Arno: don't redirect if request is from slave
    from_replica_str = apr_table_get(r->headers_in, "X-From-Replica");
    if(from_replica_str != NULL) {
      DIAG(ev.context,(MONITOR(info)),("Redirector: Request is from replica %s, not redirecting.\n", from_replica_str));
      return;
    }
  }
  
#ifdef PSODIUM
  /*
   * The redirector should not redirect to any slave which is
   * known to be bad. To make this happen I pretend the slaves are
   * down.
   */
   const char *bad_slaves_str = apr_table_get( r->notes, PSODIUM_BADSLAVES_NOTE );

   if (bad_slaves_str != NULL)
   {
        DOUT( DEBUG3, "G2P: Redirector: Bad slaves are " << bad_slaves_str );
   }
#endif

  for(Peers::iterator iter=_peers.begin(Peer::REPLICA|Peer::MIRROR|Peer::ORIGIN);
      iter != _peers.end();
      ++iter)
    {
#ifdef PSODIUM
      char *slave_id = apr_psprintf( r->pool, "%s:%hu", iter->uri().host(), iter->uri().port() );
      if (bad_slaves_str != NULL && strstr( bad_slaves_str, slave_id ) != NULL)
      {
        DOUT( DEBUG3, "G2P: Redirector: Disabling slave " << slave_id );
        iter->enabled = -1;
      }
#endif      
      DIAG(ev.context,(MONITOR(info)),("Replica candidate %s for master %s is %s with weight %hu\n", iter->location(ev.pool), location(ev.pool), (iter->enabled?"enabled":"disabled"), iter->weight()));
      if(iter->enabled > 0 && iter->weight() > 0)
        replicas.push_back(&*iter);
    }
  if(!_redirect_policy) {
    /* We can be without a redirection policy if trying to respond to a
     * DNS request.  Since this doesn't go through the apache configuration
     * merging, the initialize hasn't happened.  Initialization will be
     * forced at the first heartbeat, but this leaves the server uninitialized
     * for DNS requests during this period.
     * We cannot handle requests until then, so we will just return an
     * error
     */
    rtcount = -1;
  } else {
    _redirect_policy->run(client_ipaddr, rtcount, replicas, globule_dns_config, ev.pool);
    rtcount = replicas.size();
  }
  DIAG(ev.context,(MONITOR(info)),("Redirector: Redirection policy returned %d candidates.\n",rtcount));

  mkstring message;
  message << "t=" << apr_time_now() << " client;"
          << (client_ipaddr&0xff) << "."
          << ((client_ipaddr>>8)&0xff) << "."
          << ((client_ipaddr>>16)&0xff) << "."
          << ((client_ipaddr>>24)&0xff);
  if(replicas.size() > 1)
    message << " destination=" << replicas[0]->location(ev.pool);
  log(ev.pool, "P", message);

  ev.callback(ev.context, ev.pool, rtcount, replicas, _redirect_policy);

  /* If we're being redirect to the master (=this server) then just return
   * DECLINED.
   * In case when rtcount == 0 then we have an unsupported hostname, which
   * for DNS queries should result in a NXDOMAIN status.
   */
  if(rtcount > 0) {
    if(*replicas[0] == *this) {
      DIAG(ev.context,(MONITOR(info)),("Redirector: Candidate chosen is us, the master, handling request directly\n"));
    } else {
      if(r) {
        const char* location = apr_pstrcat(ev.pool,
                                           replicas[0]->location(ev.pool),
                                           remainder.c_str(), NULL);
        DIAG(ev.context,(MONITOR(info)),("Redirector: Redirecting to %s\n",location));
        apr_table_setn(r->headers_out, "Location", location);
      }
      ev.setStatus(HTTP_MOVED_TEMPORARILY);
    }
  }
}

void
RedirectorHandler::reload(apr_pool_t* ptemp, Context* ctx) throw()
{
  string policy;

  BaseHandler::reload(ptemp, ctx, _upstream_location, _upstream_password, policy);

  RedirectPolicy* newpolicy = RedirectPolicy::lookupPolicy(policy);
  if(newpolicy) {
    _redirect_policy->release();
    _redirect_policy = newpolicy;
  } else
    DIAG(ctx,(MONITOR(error),&_monitor),("No redirection policy named %s found, keeping existing policy",policy.c_str()));
}

void
RedirectorHandler::setAuthorization(globule::netw::HttpRequest* req,
                                    const char* uri, apr_pool_t* pool) throw()
{
  // ap_assert(!"RedirectorHandler::setAuthorization");
}
