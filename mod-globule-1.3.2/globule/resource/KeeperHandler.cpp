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
#include "utilities.h"
#include "resource/KeeperHandler.hpp"
#include "event/LoadEvent.hpp"
#include "Storage.hpp"
#include "redirect/dns_policy.hpp"
#include "event/ReportEvent.hpp"
#include "event/RegisterEvent.hpp"
#include "configuration.hpp"

using std::string;
using std::set;
using std::map;

KeeperHandler::KeeperHandler(Handler* parent, apr_pool_t* p, Context* ctx,
                             const Url& uri, const char* path,
                             const char* upstream, const char* password)
  throw()
  : ImportHandler(parent,p,ctx, uri, path, upstream, password)
{
  Url upstream_location(p,upstream);
  _reevaluateadaptskip = _reevaluateadaptskipcnt = 20; // FIXME fixed
}

KeeperHandler::~KeeperHandler() throw()
{
}

bool
KeeperHandler::initialize(apr_pool_t* p, Context* ctx,
			  HandlerConfiguration* cfg)
  throw(SetupException)
{
  if(ImportHandler::initialize(p, ctx, cfg))
    return true;
  DIAG(ctx,(MONITOR(info)),("Initializing keeper section url=%s\n",_uri(p)));
  _replpolicy = cfg->get_replication_policy();
  fetchcfg(p, ctx,
	   _peers.origin()->uri(), _peers.origin()->secret(),
	   "redirection");
  reload(p, ctx);
  return false;
}

bool
KeeperHandler::handle(GlobuleEvent& evt, const string& remainder) throw()
{
  bool rtnvalue;
  switch(evt.type()) {
  case GlobuleEvent::UPDATE_EVT: {
    if(evt.target() == this) {
      reload(evt.pool, evt.context);
      return true;
    }
    break;
  };
  case GlobuleEvent::HEARTBEAT_EVT: {
    if(_reevaluateadaptskipcnt > 0) {
      if(--_reevaluateadaptskipcnt == 0) {
        if(fetchcfg(evt.pool, evt.context,
		    _peers.origin()->uri(), _peers.origin()->secret(),
		    "redirection"))
          reload(evt.pool, evt.context);
        _reevaluateadaptskipcnt = _reevaluateadaptskip;
      }
    }
    break;
  };
  case GlobuleEvent::INVALID_EVT: {
    if(remainder == ".htglobule/redirection") {
      fetchcfg(evt.pool, evt.context,
	       _peers.origin()->uri(), _peers.origin()->secret(),
	       "redirection");
      return true;
    }
    break;
  };
  case GlobuleEvent::REQUEST_EVT: {
    HttpReqEvent& ev = (HttpReqEvent&) evt;
    rtnvalue = ImportHandler::handle(evt, remainder);
    if(ev.getRequest())
      /* reset replication policy, should not be Invalidate policy, while
       * the origin is suppost to use Invalidate policy.
       */
      apr_table_set(ev.getRequest()->headers_out, "X-Globule-Policy",
                    _replpolicy.c_str());
    return rtnvalue;
  };
  case GlobuleEvent::REPORT_EVT: {
    DEBUGACTION(KeeperHandler,report,begin);
    _lock.unlock();
    ReportEvent& ev = (ReportEvent&)evt;
    ReportEvent subevt("section", &ev);
    subevt.setProperty("uri",_uri(evt.pool));
    subevt.setProperty("site",siteaddress(evt.pool));
    subevt.setProperty("type","keeper");
    subevt.setProperty("path",_path);
    subevt.setProperty("servername",_servername);
    BaseHandler::handle(subevt, remainder);
    DEBUGACTION(KeeperHandler,report,end);
    return false;
  }
  default:
    ; // skip
  }
  return ImportHandler::handle(evt, remainder);
}

void
KeeperHandler::reload(apr_pool_t* ptemp, Context* ctx) throw()
{
  apr_int32_t ndocs;
  set<string> docs;
  string doc, dummy, fname;
  int i;

  BaseHandler::reload(ptemp, ctx, _peers.origin()->uri(), _peers.origin()->secret(), dummy);

  fname = mkstring() << _path << "/" << ".htglobule/files";
  try {
    FileStore store(ptemp, ctx, fname, true);
    store >> ndocs;
    for(i=0; i<ndocs; i++) {
      store >> doc;
      docs.insert(doc);
    }
  } catch(FileError) {
    return;
  }

  DIAG(ctx,(MONITOR(info)),("Preloading documents from origin server has started"));
  for(set<string>::iterator iter = docs.begin(); iter != docs.end(); ++iter) {
    Url doc(ptemp, _uri, iter->c_str());
    LoadEvent ev(ptemp, ctx, doc);
    Handler::handle((GlobuleEvent&)ev);
  }
  DIAG(ctx,(MONITOR(info)),("Preloading documents from origin server has been completed"));
}

ResourceAccounting*
KeeperHandler::accounting() throw()
{
  return 0;
}

std::string
KeeperHandler::description() const throw()
{
  return mkstring() << "KeeperHandler(" << _uri.host() << ":" << _uri.port()
                    << _uri.path() << ")";
}
