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
#include <apr_network_io.h>
#include <apr_lib.h>
#include <httpd.h>
#include <http_protocol.h>
#include "utilities.h"
#include "resource/OriginHandler.hpp"
#include "event/RedirectEvent.hpp"
#include "event/HttpLogEvent.hpp"
#include "event/PostRequestEvent.hpp"
#include "event/SwitchEvent.hpp"
#include "event/ReportEvent.hpp"
#include "redirect/dns_config.h"
#include "netw/HttpRequest.hpp"
#include "redirect/mod_netairt.h"
#include "Storage.hpp"
#include "psodium.h"
#include "configuration.hpp"

using namespace std;

OriginHandler::OriginHandler(Handler* parent, apr_pool_t* p, Context* ctx,
                             const Url& uri, const char* path,
                             const char* upstream, const char* password)
  throw(SetupException)
  : SourceHandler(parent, p, ctx, uri, path), _config_dirty(true),
    _filetree_timestamp(0), _filetree_numofdocs(0)
{
  _lastevaluationsize = logsize(p);
}

OriginHandler::~OriginHandler() throw()
{
}

bool
OriginHandler::initialize(apr_pool_t* p, Context* ctx,
			  HandlerConfiguration* cfg)
  throw(SetupException)
{
  if(SourceHandler::initialize(p, ctx, cfg))
    return true;
  _reevaluateadaptskip      = cfg->get_reevaluateadaptskip();
  _reevaluateadaptskipcnt   = cfg->get_reevaluateadaptskip();
  _reevaluatecontentskip    = cfg->get_reevaluatecontentskip();
  _reevaluatecontentskipcnt = cfg->get_reevaluatecontentskip();
  checkconfig(p,ctx);
  DIAG(ctx,(MONITOR(info)),("Initializing origin section url=%s\n",_uri(p)));
  return false;
}

bool
OriginHandler::handle(GlobuleEvent& evt, const string& remainder) throw()
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
    return true;
  };
  case GlobuleEvent::INVALID_EVT: {
    _lock.unlock();
    DIAG(evt.context,(MONITOR(detail)),("Received invalidation for document %s\n",remainder.c_str()));
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
  case GlobuleEvent::UPDATE_EVT: {
    if(target == this || remainder == "" || remainder == ".htglobule/redirection") {
      _lock.unlock();
      RedirectorHandler::handle(evt, remainder);
    } else {
      if(target) {
        Lock* lock = target->getlock();
        ap_assert(lock != &_lock);
        lock->lock_exclusively();
        _lock.unlock();
        target->handle(evt);
        lock->unlock();
      } else {
        _lock.unlock();
        ap_assert(!"File monitor update event to non-existent document");
      }
    }
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
      target = gettarget(evt.pool, evt.context, remainder, _default_policy.c_str());
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
  case GlobuleEvent::HEARTBEAT_EVT:
    _lock.unlock();
    checkreplicas(evt.pool, evt.context);
    checkconfig(evt.pool, evt.context);
    // Adapt policy of documents in this resourcepool
    // small race condition on counter 
    if(_reevaluatecontentskipcnt > 0) {
      if(--_reevaluatecontentskipcnt == 0) {
        _reevaluatecontentskipcnt = _reevaluatecontentskip;
        checkcontent(evt.pool, evt.context);
      }
    }
    // Adapt policy of documents in this resourcepool
    // small race condition on counter 
    if(_reevaluateadaptskipcnt > 0) {
      if(--_reevaluateadaptskipcnt == 0) {
        _reevaluateadaptskipcnt = _reevaluateadaptskip;
        evallog(evt);
      }
    }
    return true;
    break;
  case GlobuleEvent::WRAPUP_EVT: {
    PostRequestEvent& ev = (PostRequestEvent&) evt;
    if(ev.getRequestStatusPurging()) {
      DIAG(evt.context,(MONITOR(warning)),("Document %s not found (%d), purging replication information",remainder.c_str(),ev.getRequest()->status));
      if(target) {
        Lock* lock = target->getlock();
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
  case GlobuleEvent::LOGGING_EVT: {
    _lock.unlock();
    HttpLogEvent& ev = (HttpLogEvent&)evt;
    receivelog(ev.getRequest());
    ev.setStatus(HTTP_NO_CONTENT);
    return true;
  }
  case GlobuleEvent::REPORT_EVT: {
    ReportEvent& ev = (ReportEvent&)evt;
    ReportEvent subevt("section", &ev);

    for(ReportEvent::iterator iter=subevt.begin("peer");
        iter != subevt.end("peer");
        ++iter)
      {
        ReportEvent& peerevt = *iter;
        if(peerevt.getProperty("uri")) {
          Url peerurl(subevt.pool, peerevt.getProperty("uri"));
          Peers::iterator iter = _peers.find(peerurl);
          if(iter == _peers.end()) {
	    const char* secret = peerevt.getProperty("secret");
            if(!*secret)
	      secret = 0;
	    addReplicaFast(peerevt.context, peerevt.pool,
			   peerevt.getProperty("uri"),
			   peerevt.getProperty("secret"));
          } else {
            if(peerevt.getProperty("secret") && *peerevt.getProperty("secret")) {
              _config_dirty = true;
              iter->_secret = peerevt.getProperty("secret");
            }
            if(peerevt.getPropertyInt("weight", iter->_weight) ||
               peerevt.getPropertyInt("enabled", iter->enabled)) {
              _config_dirty = true;
              changeReplica(&*iter);
            }
          }
        }
      }
    if(subevt.getProperty("redirectpolicy")) {
      RedirectPolicy* newpolicy = RedirectPolicy::lookupPolicy(subevt.getProperty("redirectpolicy"));
      if(newpolicy) {
        _redirect_policy->release();
        _redirect_policy = newpolicy;
        _config_dirty = true;
      } else
        DIAG(evt.context, (MONITOR(error),&_monitor), ("No redirection policy named %s found, keeping existing policy",subevt.getProperty("redirectpolicy")));
    }

    _lock.unlock();

    subevt.setProperty("uri",_uri(evt.pool));
    subevt.setProperty("site",siteaddress(evt.pool));
    subevt.setProperty("type","origin");
    subevt.setProperty("path",_path);
    subevt.setProperty("servername",_servername);
    subevt.setProperty("refreshinterval",mkstring()<<_reevaluateinterval);
    subevt.setProperty("reevaluateinterval",mkstring()<<_reevaluateadaptskip);
    subevt.setProperty("reevaluateafter",mkstring()<<_reevaluateadaptskipcnt);
    subevt.setProperty("replicatepolicy",_default_policy);
    subevt.setProperty("redirectpolicy",_redirect_policy->policyName());
    subevt.setProperty("redirectmode", (_redirect_mode==CFG_MODE_DNS ?"dns" :
                                        _redirect_mode==CFG_MODE_HTTP?"http":
                                        _redirect_mode==CFG_MODE_BOTH?"both":
                                                                      "none"));
    subevt.setProperty("redirectttl",mkstring()<<_redirect_policy->ttl());
    BaseHandler::handle(subevt, remainder);
    return false;
  }
  default:
    _lock.unlock();
    return true;
  }
}

void
OriginHandler::checkcontent(apr_pool_t* pool, Context* ctx) throw()
{
  /*
   * The files present on the filesystem goes into .htglobule/files
   */
  map<string,string> docs;
  apr_time_t newest = traversedocs(docs, "", _path, pool);
  if(newest>_filetree_timestamp || docs.size()!=_filetree_numofdocs) {
    apr_int32_t ndocs = docs.size();
    string fname = mkstring() << _path << "/" << ".htglobule/files";
    string tmpfname = mkstring() << fname << "~";
    {
      FileStore store(pool, ctx, tmpfname, false);
      store << ndocs;
      for(map<string,string>::iterator iter = docs.begin();
          iter != docs.end();
          ++iter)
        store << iter->first;
    }
    apr_file_rename(tmpfname.c_str(), fname.c_str(), pool);
    _filetree_numofdocs = docs.size();
    if(_filetree_numofdocs == 0)
      _filetree_timestamp = apr_time_now();
    else
      _filetree_timestamp = newest;
  }
}

void
OriginHandler::checkconfig(apr_pool_t* pool, Context* ctx) throw()
{
  /* 
   * Redirection information goes into .htglobule/redirection
   */
  if(_config_dirty) {
    DIAG(ctx,(MONITOR(info)),("redirection information has changed\n"));
    _lock.lock_exclusively();
    apr_int32_t nreplicas = _peers.nreplicas();
    string fname = mkstring() << _path << "/" << ".htglobule/redirection";
    string tmpfname = mkstring() << fname << "~";
    {
      FileStore store(pool, ctx, tmpfname, false);
      ((Output&)store) << _redirect_policy->policyName() << nreplicas;
      for(Peers::iterator iter = _peers.begin(Peer::REPLICA|Peer::MIRROR);
          iter != _peers.end();
          ++iter)
        store << iter->type() << iter->weight()
	      << iter->location(pool) << iter->secret();
    }
    apr_file_rename(tmpfname.c_str(), fname.c_str(), pool);
    _config_dirty = false;
    _lock.unlock();
  }
}

apr_time_t
OriginHandler::traversedocs(map<string,string>& filelist,
                            const char* location, const char* path,
                            apr_pool_t* ptemp) throw()
{
  apr_time_t newest = 0;
  map<string,string> res;
  apr_status_t status;
  apr_finfo_t finfo;
  apr_dir_t* dirdes;
  if(apr_dir_open(&dirdes, path, ptemp) == APR_SUCCESS) {
    if(location[strlen(location)-1] == '/')
      filelist[location] = "";
    else
      filelist[apr_pstrcat(ptemp, location, "/", NULL)] = "";
    do {
      status = apr_dir_read(&finfo,
                      APR_FINFO_TYPE|APR_FINFO_MTIME|APR_FINFO_CTIME, dirdes);
      if(status == APR_SUCCESS && strncasecmp(finfo.name,".ht",3) &&
         strcmp(finfo.name,".") && strcmp(finfo.name,"..") &&
         strcmp(finfo.name,".htglobule")) {
        const char* docpath;
        if(location && *location)
          if(location[strlen(location)-1] == '/')
            docpath = apr_psprintf(ptemp, "%s%s", location, finfo.name);
          else
            docpath = apr_psprintf(ptemp, "%s/%s", location, finfo.name);
        else
          docpath = finfo.name;
        if(finfo.filetype == APR_DIR) {
          traversedocs(filelist, docpath,
                       apr_psprintf(ptemp, "%s/%s", path, finfo.name), ptemp);
        } else {
          if(finfo.mtime > newest)
            newest = finfo.mtime;
          if(finfo.ctime > newest)
            newest = finfo.ctime;
          filelist[docpath] = apr_psprintf(ptemp, "%s/%s", path, finfo.name);
        }
      }
    } while(status == APR_SUCCESS);
    apr_dir_close(dirdes);
  }
  return newest;
}

void
OriginHandler::evallog(GlobuleEvent& evt) throw()
{
  try {
    apr_size_t size = logsize(evt.pool);
    if(size - _lastevaluationsize > 0) {
      adapt(evt, logname().c_str(), _lastevaluationsize, size);
      _lastevaluationsize = size;
    }
  } catch(globule::netw::HttpException ex) {
    std::cerr << "HttpException: " << ex.getReason() << endl;
  }
}

string
OriginHandler::description() const throw()
{
  return mkstring() << "OriginHandler(" << _uri.host() << ":" << _uri.port()
                    << _uri.path() << ")";
};

void
OriginHandler::switchpolicy(apr_pool_t* p, Context* ctx,
                            const char* path, const char* repltype) throw()
{
  _lock.lock_exclusively();
  Handler* target = gettarget(p, ctx, path, repltype);
  if(target) {
    Lock* lock = target->getlock();
    ap_assert(lock != &_lock);
    lock->lock_exclusively();
    SwitchEvent evt(p, ctx, target, repltype);
    target->handle(evt);
    lock->unlock();
  }
  _lock.unlock();
}
