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
#include "documents.hpp"
#include <http_core.h>
#include <http_protocol.h>
#include <http_request.h>
#include "resource/BaseHandler.hpp"
#include "resource/ConfigHandler.hpp"
#include "event/FileMonitorEvent.hpp"
#include "event/PostRequestEvent.hpp"
#include "event/ReportEvent.hpp"
#include "event/LoadEvent.hpp"
#include "event/RegisterEvent.hpp"
#include "psodium.h"

using std::string;

Element::Element(ContainerHandler* parent, apr_pool_t* pool, const Url& url,
                 Context* ctx)
  throw()
  : Handler(parent, pool, url), ResourcesUser(),
    _lock(1), _parent(parent), _policy(0),
    _monitor(ctx, rmmmemory::allocate(url(pool)))
{
  _rsrcs = new(rmmmemory::shm()) ResourcesRecord(_parent->accounting(), this);
}

Element::Element(ContainerHandler* parent, apr_pool_t* pool, const Url& url,
                 Context* ctx,
                 const char* path, const char* policy)
  throw()
  : Handler(parent, pool, url), ResourcesUser(),
    _lock(1), _parent(parent), _path(path),
    _monitor(ctx, rmmmemory::allocate(url(pool)))
{
  _rsrcs = new(rmmmemory::shm()) ResourcesRecord(_parent->accounting(), this);
  _policy = ReplPolicy::lookupPolicy(policy);
}

Element::~Element() throw()
{
  if(_rsrcs) {
    rmmmemory::destroy(_rsrcs);
    _rsrcs = 0;
  }
  if(_policy)
    rmmmemory::destroy(_policy);
  rmmmemory::deallocate(const_cast<char*>(_monitor._description));
}

void
Element::flush(apr_pool_t* pool) throw()
{
}

void
Element::purge(Context* ctx, apr_pool_t* pool) throw()
{
  if(_policy) {
    ReplAction action = _policy->onPurge(this);
    /* We should only unregister for policy updates if indicated so by
     * the policy:
     *    if(action.options()&ReplAction::UNREG_FILE_UPDATE) {
     *       ....
     *    }
     * however, this is not possible, because at this time all policies
     * should register for fileupdates in order to do a correct policy
     * evaluated.  However they do not all unregister for file updates.
     * to make sure all documents unregster, we do this always.
     * This may cause warnings because the file isn't registered for file
     * updates, but that results in no harm.
     */
    action = ReplAction(action.options()|ReplAction::UNREG_FILE_UPDATE,
                        action.inv(), action.expiry());
    execute(ctx, pool, action);
  }
  if(_rsrcs) {
    rmmmemory::destroy(_rsrcs);
    _rsrcs = 0;
  }
}

EvictionEvent
Element::evict(Context* ctx, apr_pool_t* pool)
{
  apr_uri_t *uri = (apr_uri_t*) apr_palloc(pool, sizeof(apr_uri_t));
  // FIXME: It is ugly to unparse and parse just to get a copy of the string
  // FIXME: currently the _uri is without path, this is WRONG (monitor should
  // have with it.  However this needs checking the other usages of the uri/url
  apr_uri_parse(pool,apr_pstrcat(pool,apr_uri_unparse(pool,_uri.uri(),0),_path.c_str(),NULL),uri);
  return EvictionEvent(pool, ctx, uri);
}

Lock*
Element::getlock() throw()
{
  /* The call to the _parent is probably too inefficient, as the lock
   * could also be cached at the creation of the Document object.
   */
  return &_lock;
}

void
Element::log(apr_pool_t* pool, string a, const string b) throw()
{
  _parent->log(pool, a, b);
}

bool
Element::handle(GlobuleEvent& evt, const std::string& remainder) throw()
{
  return handle(evt);
}

Output&
Element::operator<<(Output& out) const throw()
{
  Serializable::operator<<(out);
  out << _path;
  out << OutputWrappedPersistent<Persistent*>("resourcerecord", _rsrcs);
  out << OutputWrappedPersistent<Persistent*>("policy", _policy);
  return out;
}

Input&
Element::operator>>(Input& in) throw(WrappedError)
{
  string path;
  
  Serializable::operator>>(in);
  if(_policy) {
    rmmmemory::destroy(_policy);
    _policy = 0;
  }
  in >> _path;
  in >> InputWrappedPersistent<Persistent*>("resourcerecord", (Persistent*&) _rsrcs);
  in >> InputWrappedPersistent<Persistent*>("policy", (Persistent*&) _policy);
  if(!_policy) {
    DIAG(in.context(), (MONITOR(internal)), ("Missing policy on read persistent object, reverting to build in default"));
    _policy = ReplPolicy::lookupPolicy("PureProxy");
  }
  _rsrcs->update(this, _parent->accounting());
  return in;
}

void
Element::invalidate(Context* ctx, apr_pool_t* pool, ReplSet *repl) throw()
{
  ReplSet::const_iterator iter;
  ReplSet failedrepl;
    
  if(!repl)
    return;

#ifdef PSODIUM
  apr_time_t expiry_t = apr_time_now()+MAX_WRITE_PROP;
  // Should use type safe printf here, really
  char *time_str = apr_psprintf( pool, "%lld", expiry_t );
  DOUT( DEBUG3, "G2P: Invalidate policy setting expiry time " << time_str << " (something finite) for pSodium security to use\n");
  char *hostinfo = apr_psprintf( pool, "%s:%hu", _uri.host(), _uri.port() );
    
  const char *userid = intraserverauthpwd;
  const char *passwd = intraserverauthpwd;
  DOUT( DEBUG3, "G2P: Authenticating UPDATEEXPIRY message with (userid=" << userid << ",passwd=" << passwd << ") \n" );
#endif

  for(iter=repl->begin(); iter!=repl->end(); iter++) {
    Url replica_url(pool, (*iter)->uri(), _path.c_str());
    DIAG(ctx, (MONITOR(info),&_monitor), ("Sending invalidation to %s",replica_url(pool)));
    replica_url.normalize(pool); // just to be safe

#ifdef PSODIUM    
    // QUICK: send UPDATEEXPIRY for each slave instead of for master URL    
    /*
     * This must be done before the Invalidates are sent, otherwise we get
     * concurrency between us updating the VERSIONS datastructure via
     * this HTTP request and the slave's retrieving new versions from the
     * master. There is no concurrency between us and new slaves as they
     * are not yet in VERSIONS for this URL.
     */
     if (intraserverauthpwd != NULL)
     {
        // replica_url(pool) is cast to (char *) hidden in C++ syntax
        char *query = apr_pstrcat( pool, time_str, replica_url(pool), NULL );
        char *psodium_url_str = apr_pstrcat( pool, "http://", 
                                                hostinfo,
                                                PSO_UPDATEEXPIRY_URI_PREFIX, "?",
                                                query,
                                                NULL );
        Url psodium_url( pool, psodium_url_str ); 
        psodium_url.normalize(pool); // just to be safe

        DOUT( DEBUG3, "G2P: Sending UPDATEEXPIRY " << psodium_url( pool ) << "(query=" << query << ") \n" );

        globule::netw::HttpRequest req( psodium_url(pool), pool );
        req.setAuthorization(userid, passwd);
        if (req.getStatus(pool) != HTTP_OK
            && req.getStatus(pool) != HTTP_NO_CONTENT)
        {
          DOUT( DEBUG0, "G2P: pSodium master did not respond correctly (HTTP status "
                << req.getStatus(pool) << ") to our UPDATEEXPIRY." );
        }
     }
#endif

    try {
      globule::netw::HttpRequest req(pool, replica_url);
      req.setMethod("SIGNAL");
      req.setAuthorization((*iter)->secret(), (*iter)->secret());
      req.setHeader("X-Globule-Protocol", BaseHandler::PROTVERSION);
      req.setHeader("X-Globule-Serial", (*iter)->serial());
      int status = req.getStatus(pool);
      if(status != HTTP_OK && status != HTTP_NO_CONTENT &&
         status != HTTP_NOT_FOUND)
        {
          if(status != HTTP_LOCKED) {
            DIAG(ctx, (MONITOR(error),&_monitor), ("Replica server gave improper response %d to invalidation signal",req.getStatus(pool)));
            failedrepl.insert(*iter);
          } else {
            ;
            // FIXME: later on, we should insert these cases into the
            // failedrepl list also, but currently all requests are locked
            // because the global mutex does not implement a trylock.
            // this means that we can never deliver the event, so we don't
            // try to, but in the DocumentHandler of the slave, a flag is
            // set to invalidate the Document upon the next request being
            // made.  Setting this flag does not require access to the
            // Document, only the DocResource so it can be done safely.
          }
        }
    } catch(globule::netw::HttpException ex) {
      DIAG(ctx, (MONITOR(error),&_monitor), ("Could not communicate with replica server"));
      failedrepl.insert(*iter);
    }
  }

  repl->clear();
  for(iter=failedrepl.begin(); iter!=failedrepl.end(); iter++) {
    repl->insert(*iter);
    (*iter)->setserial(apr_time_now()); // force total update
  }
}

void
Element::switchpolicy(Context* ctx, apr_pool_t* pool, const char* policyname, bool sticky) throw()
{
  ReplPolicy *newpolicy, *aux;

  DIAG(ctx, (MONITOR(info),&_monitor), ("Document switching policy from %s to %s", _policy->policyName(), policyname));
  
  if(!strcmp(policyname, _policy->policyName()))
    return;

  if(!(newpolicy = ReplPolicy::lookupPolicy(policyname, (sticky?-1:0)))) {
    DIAG(ctx, (MONITOR(error),&_monitor),("Could not apply new policy %s",policyname));
    return;
  }

  if(!strcmp(_policy->policyName(),"MirrorNoCons")) {
    mkstring message;
    message << "t=" << apr_time_now()
            << " old;" << _policy->policyName()
            << " new;" << policyname
            << " path:" << _path;
    log(pool, "A", message);
  }

  ReplAction action = _policy->onPurge(this);
  execute(ctx, pool, action);
  aux = _policy;
  _policy = newpolicy;
  rmmmemory::destroy(aux);
}

Document::Document(const Document& doc)
  throw()
  : Element(doc),
    _master_fname(doc._master_fname), _content_type(doc._content_type),
    _lastmod(doc._lastmod), _docsize(doc._docsize), _http_status(HTTP_OK)
{
}

Document::Document(ContainerHandler* parent, apr_pool_t* pool, const Url& url,
                   Context* ctx)
  throw()
  : Element(parent, pool, url, ctx),
    _http_status(HTTP_OK)
{
}

Document::Document(ContainerHandler* parent, apr_pool_t* pool, const Url& url,
                   Context* ctx, const char* path, const char* policy)
  throw()
  : Element(parent, pool, url, ctx, path, policy),
    _http_status(HTTP_OK)
{
}

Document::~Document() throw()
{
}

void
Document::execute(Context* ctx, apr_pool_t* pool, ReplAction action) throw()
{
  if(action.options()&ReplAction::INVALIDATE_DOCS) {
    DIAG(ctx, (MONITOR(info),&_monitor), ("Invalidating replica documents before switching away from policy"));
    invalidate(ctx, pool, action.inv());
  }
  if(action.options()&ReplAction::UNREG_FILE_UPDATE) {
    RegisterEvent ev(pool, this, _master_fname, 0); // unregisters because is 0
    GlobuleEvent::submit(ev);
  }
}

bool
Document::handle(GlobuleEvent& evt) throw()
{
  apr_status_t status;
  ap_assert(_policy != 0);
  switch(evt.type()) {
  case GlobuleEvent::UPDATE_EVT: {
    FileMonitorEvent& ev = (FileMonitorEvent&) evt;
    ReplAction action = _policy->onUpdate(this);
    mkstring message;
    message << "t=" << apr_time_now() << " lastmod=" << ev.lastmod()
            << " docsize=" << ev.docsize() << " path:" << _path.c_str();
    log(evt.pool, string("U"), message);
    if(action.options()&ReplAction::INVALIDATE_DOCS)
      invalidate(evt.context, evt.pool, action.inv());
    /* FIXME: workaround, see also FileMonitor.cpp:check_files
     * because documents need to unregister their file monitor when the
     * file is deleted they normally would send an FileUnregEvent.
     * However this is not possible because this object is locked (as
     * the file monitor is traversing its list at this time.
     * The file monitor will, as a cludge AUTOmatically remove the
     * file motoring.  This is however not the way to go as he should
     * not make assumptions about what the document wants to do.
     *
     *      if (action.options()&ReplAction::UNREG_FILE_UPDATE)
     *        unregister_from_events(evt.pool);
     */
    break;
  }
  case GlobuleEvent::LOAD_EVT:
  case GlobuleEvent::REQUEST_EVT: {
    request_rec* r = 0;
    if(evt.type() == GlobuleEvent::REQUEST_EVT)
      r = ((HttpReqEvent&)evt).getRequest();
    DIAG(evt.context,(MONITOR(policy),(_policy->mon)),(0));
    if(_rsrcs)
      _rsrcs->update();

    if(r && ((HttpReqEvent&)evt).getPeer() && _parent->ismaster()) {
      HttpReqEvent& ev = ((HttpReqEvent&)evt);
      if(strcmp(r->uri, r->parsed_uri.path)) {
        apr_table_setn(r->headers_out, "Location", ap_construct_url(evt.pool,
          (r->args ? apr_pstrcat(evt.pool,
                          ap_escape_uri(evt.pool, r->uri), "?", r->args, NULL):
                          ap_escape_uri(evt.pool, r->uri)), r));
        ev.setStatus(HTTP_MOVED_PERMANENTLY);
        // FIXME: does one does not count in the resource accounting
        return true;
      }
    }

    // See what replication policy wants us to do
    ReplAction action;
    if(evt.type() == GlobuleEvent::REQUEST_EVT) {
      HttpReqEvent& ev = ((HttpReqEvent&)evt);
      action = _policy->onAccess(this, ev.getPeer());
    } else
      action = _policy->onAccess(this, 0);

#ifdef PSODIUM    
    if(action.options()&ReplAction::PSODIUM_EXPIRE)
      apr_table_set(r->notes, PSODIUM_RECORD_DIGEST_EXPIRYTIME_NOTE,
                    apr_psprintf(evt.pool, "%" APR_TIME_T_FMT,
                                 action.expiry()));
#endif                                 
    if(action.options()&ReplAction::REG_FILE_UPDATE) {
      if(r && r->filename) {
        RegisterEvent ev(evt.pool, this, r->filename);
        GlobuleEvent::submit(ev);
      } else {
        DIAG(evt.context,(MONITOR(error)),("filename of requested document unknown"));
        if(evt.type() == GlobuleEvent::REQUEST_EVT) {
          HttpReqEvent& ev = ((HttpReqEvent&)evt);
          ev.setStatus(HTTP_INTERNAL_SERVER_ERROR);
        }
      }
    }
    switch(action.from()) {
    case ReplAction::FROM_GET_IMS:
      DIAG(evt.context, (MONITOR(warning)),("Policy instructed unimplemented If-Modified-Since retrieval of document, falling back to plain GET"));
    case ReplAction::FROM_GET:
      DIAG(evt.context, (MONITOR(replmethod),MONITOR(dofetch)),(0));
      break;
    case ReplAction::FROM_DISK:
      DIAG(evt.context, (MONITOR(replmethod),MONITOR(docache)),(0));
      break;
    case ReplAction::FROM_APACHE:
      DIAG(evt.context, (MONITOR(replmethod),MONITOR(donative)),(0));
      break;
    default:
      DIAG(evt.context, (MONITOR(replmethod),MONITOR(doerror)),(0));
      break;
    }

    //
    // Arno: CAREFUL: Must be done before do_get(), as that's where we
    // directly pass on data from the master to the client, and therefore
    // all headers must be set before that time!
    //
    /* Duplicate the X-Globule-Trace into a cookie for client-site javascript
     * display.  Should only be used if the document/server will not generate
     * any cookies by itself.  because this will overwrite any other cookies.
     * The data of the cookie needs to be stored in an escaped form (avoiding
     * "() chars and spaces) and with a expiration date set slightly in the
     * future.  If it isn't set (cookie remains valid for browser session), or
     * too far in the future, the info will still be there for the javascript,
     * even if a non-globule-replicated page is requested.
     */
    if(r && !apr_table_get(r->headers_out, "Set-Cookie") &&
       !(apr_table_get(r->headers_in, "User-Agent") &&
         !(strncmp(apr_table_get(r->headers_in, "User-Agent"),
                   "Flood/", strlen("Flood/"))))) {
      char expiration[APR_RFC822_DATE_LEN];
      apr_rfc822_date(expiration, apr_time_now()+apr_time_from_sec(60));
      /* replace second and third space into hyphen, coz fcking netscape
       * uses it's own format and not RFC822.  The only change is that
       * the date is not "weekday, day month year" but "weekday, day-month-"
       */
      *strchr(strchr(expiration,' ')+1,' ') = '-';
      *strchr(strchr(expiration,' ')+1,' ') = '-';
      const char *s = apr_table_get(r->headers_out, "X-Globule-Trace");
      mkstring cookie;
      cookie << "GLOBULETRACE=\"";
      while(*s) {
        switch(*s) {
        // The Netscape standard requires ";, and space to be escaped
        case '\"': cookie << "%22"; break;
        case ';':  cookie << "%3B"; break;
        case ',':  cookie << "%2C"; break;
        case ' ':  cookie << '+';   break;
        // But some are not satisfied with that and requires () escaped too
        case '(':  cookie << "%28"; break;
        case ')':  cookie << "%29"; break;
        default:
          cookie << *s;
        }
        ++s;
      }
      cookie << "\"; expires=" << expiration;
      apr_table_set(r->headers_out, "Set-Cookie", cookie.str().c_str());
    }

    ResourceDeclaration rsrcusage;
    rsrcusage.set(ResourceDeclaration::QUOTA_NUMDOCS, 1);
    unsigned int fromwhere = action.from();
    if(fromwhere == ReplAction::FROM_GET_IMS ||
       fromwhere == ReplAction::FROM_GET) {
      if(evt.type() == GlobuleEvent::REQUEST_EVT) {
        HttpReqEvent& ev = (HttpReqEvent&)evt;
        Url replica_url(evt.pool,ap_construct_url(evt.pool,r->unparsed_uri,r));
        int rtnstatus = fetch(evt.context, evt.pool, replica_url, r, action,
                              rsrcusage);
        if(rtnstatus == DECLINED)
          fromwhere = ReplAction::FROM_APACHE;
        else
          ev.setStatus(rtnstatus);
      } else if(evt.type() == GlobuleEvent::LOAD_EVT) {
        LoadEvent& ev = (LoadEvent&)evt;
        fetch(evt.context, evt.pool, ev.location(), (request_rec*)0, action,
              rsrcusage);
      }
    }
    if(r) {
      for(gmap<const gstring,gstring>::iterator iter = _http_headers.begin();
          iter != _http_headers.end();
          ++iter)
        apr_table_set(r->headers_out,iter->first.c_str(),iter->second.c_str());
      if(_http_status != HTTP_OK)
        ((HttpReqEvent&)evt).setStatus(_http_status);
    }
    if(fromwhere == ReplAction::FROM_APACHE ||
       fromwhere == ReplAction::FROM_DISK) {
      if(_parent->ismaster())
        apr_table_set(r->headers_out, "X-Globule-Policy",
                      _policy->policyName());
      if(!_parent->isslave()) {
        if(r->canonical_filename == NULL && r->filename == NULL) {
          DIAG(evt.context, (MONITOR(error)),("internal error locating file"));
          if(evt.type() == GlobuleEvent::REQUEST_EVT) {
            HttpReqEvent& ev = ((HttpReqEvent&)evt);
            ev.setStatus(HTTP_NOT_FOUND);
          }
        }
        _master_fname = r->canonical_filename ? r->canonical_filename
                                              : r->filename;
        if(fromwhere == ReplAction::FROM_APACHE && r) {
          _docsize      = r->finfo.size;
          _lastmod      = r->finfo.mtime;
          _content_type = (r->content_type?r->content_type:"");
        }
      } else if(r) {
        if(_http_status == HTTP_OK)
          r->filename = r->canonical_filename = apr_pstrdup(evt.pool, _parent->docfilename(_path.c_str()).c_str());
        r->mtime = _lastmod;
        ap_set_last_modified(r);
        ap_set_content_type(r, apr_pstrdup(r->pool, _content_type.c_str()));

        if(r->path_info != NULL)
          r->path_info[0] = 0;

        DIAG(evt.context,(MONITOR(detail),&_monitor),("Apache will send file %s to client",r->canonical_filename));
        if((status = ap_directory_walk(r))) {
          DIAG(evt.context,(MONITOR(internal),&_monitor),("Error gathering information for file %s (walking through directories)",r->canonical_filename));
        }
        if((status = ap_file_walk(r))) {
          DIAG(evt.context ,(MONITOR(internal),&_monitor),("Error gathering information for file %s (walking to file)",r->canonical_filename));
        }
        status = apr_stat(&r->finfo, r->filename, APR_FINFO_MIN, evt.pool);
        if(status == APR_SUCCESS) {
          if(r->finfo.filetype != APR_REG) {
            DIAG(evt.context,(MONITOR(error),&_monitor),("Cannot send requested file %s, since it is not a regular file",r->canonical_filename));
            if(evt.type() == GlobuleEvent::REQUEST_EVT) {
              HttpReqEvent& ev = ((HttpReqEvent&)evt);
              ev.setStatus(HTTP_INTERNAL_SERVER_ERROR);
            }
          } else {
            if(!strcmp(r->handler,DIR_MAGIC_TYPE))
              r->handler = apr_pstrdup(evt.pool, _content_type.c_str());
            /* Do not set status through ev.setStatus().  This will result
             * in a "return DECLINED" from the handler, which means that
             * Apache will deliver the doc based on above settings.
             */
          }
        } else {
          DIAG(evt.context,(MONITOR(error),&_monitor),("Cannot send requested file %s, cannot get local file information",r->canonical_filename));
          if(evt.type() == GlobuleEvent::REQUEST_EVT) {
            HttpReqEvent& ev = ((HttpReqEvent&)evt);
            ev.setStatus(HTTP_INTERNAL_SERVER_ERROR);
          }
        }
      }
    }
    if(r && r->content_type &&
       !strcmp(r->content_type,"application/x-httpd-php")) {
      /* We can either put environment variables into r->subprocess_env
       * using a apr_table_addn, which will be accessible to PHP scripts
       * in the _SERVER global variable, or we can use putenv to set the
       * environment variables which will be available as a _ENV script.
       */
      string phpscript = mkstring() << "require_once \""
                                    << ConfigHandler::path
                                    << "/globule.php\";\n";
      apr_table_addn(r->subprocess_env,"GLOBULE_PHPSCRIPT",
                     apr_pstrdup(evt.pool, phpscript.c_str()));
      apr_table_addn(r->subprocess_env,"GLOBULE_SECTION",
                     _parent->location(evt.pool));

      /* FIXME: This is very weird; if we use a normal ImportHandler
       * instantiated in the parent process, the PHP filter is present in
       * the output filter chain when we request the page on a replica server.
       * However when using a BrokerHandler, the PHP filter is missing.
       * As a patch, we insert the PHP handler forcefully outself if
       * the content-type is application/x-httpd-php and the php filter
       * is not already present.
       */
      ap_filter_t* f;
      for(f=r->output_filters; f; f=f->next)
        if(f->frec && f->frec->name && !strcasecmp(f->frec->name,"php"))
          break;
      if(!f)
        ap_add_output_filter("php", NULL, r, r->connection);
    }
    _rsrcs->consume(rsrcusage);
    break;
  }
  case GlobuleEvent::SWITCH_EVT: {
    if(_policy->getStickyness() != -1)
      switchpolicy(evt.context, evt.pool, ((SwitchEvent&)evt).policytype());
    break;
  }
  case GlobuleEvent::WRAPUP_EVT: {
    PostRequestEvent& ev = (PostRequestEvent&)evt;
    request_rec* r = ev.getRequest();
    if(ev.getRequestStatusPurging()) {
      purge(evt.context, evt.pool);
    } else
      if(r) {
        if(r->method_number == M_GET || r->method_number == M_POST) {
          DIAG(ev.context,(MONITOR(retrieval),&_monitor,(apr_int64_t)r->bytes_sent),(0));
          if(!apr_table_get(r->headers_in,"X-From-Replica")) {
            mkstring message;
            message << "t=" << r->request_time
                    << " client;" << r->connection->remote_ip
                    << " elapsed=" << (apr_time_now() - r->request_time);
            if(r->sent_bodyct && r->bytes_sent)
              message << " sndsize=" << r->bytes_sent;

	    if(apr_table_get(r->headers_in,"User-Agent"))
	      message << " browser;" << mkstring::urlencode(apr_table_get(r->headers_in,"User-Agent"));
	    if(apr_table_get(r->headers_in,"Referer"))
	      message << " referer;" << mkstring::urlencode(apr_table_get(r->headers_in,"Referer"));
            message << " path:" << _path.c_str();
            log(evt.pool, "R", message);
          }
        }
      }
    break;
  }
  case GlobuleEvent::EVICTION_EVT: {
    DIAG(evt.context,(MONITOR(info)),("Evicting element %s from cache",_path.c_str()));
    string message = mkstring() << "t=" << apr_time_now() << " path:" << _path;
    log(evt.pool, "E", message.c_str());
    DIAG(evt.context,(MONITOR(rsrcevict),&_monitor),(0));
    if(_parent->isslave()) {
      string filename = _parent->docfilename(_path.c_str());
      apr_file_remove(filename.c_str(), evt.pool);
    }
    break;
  }
  case GlobuleEvent::INVALID_EVT: {
    HttpMetaEvent& ev = (HttpMetaEvent&)evt;
    string message = mkstring() << "t=" << apr_time_now() << " path:" << _path;
    log(evt.pool, "I", message.c_str());
    _policy->onSignal(this);
    ev.setStatus(HTTP_NO_CONTENT);
    break;
  }
  case GlobuleEvent::REPORT_EVT: {
    ReportEvent& ev = (ReportEvent&)evt;
    ReportEvent subevt("doc", &ev);
    subevt.setProperty("path",        _path);
    subevt.setProperty("policy",      _policy->policyName());
    subevt.setProperty("contenttype", _content_type);
    subevt.setProperty("lastmod",     mkstring()<<apr_time_sec(_lastmod));
    subevt.setProperty("docsize",     mkstring()<<_docsize);
    ReportEvent polsubevt("policy", &ev);
    _policy->report(polsubevt);
    break;
  }
  default:
    ap_assert(!"programming error");
  };
  return true;
}

string
Document::description() const throw()
{
  return mkstring() << "Document(" << _uri.host() << ":" << _uri.port()
                    << _uri.path() << ")";
}

Output&
Document::operator<<(Output& out) const throw()
{
  apr_time_t master_mtime = APR_TIME_C(0);
  if(_parent->ismaster()) {
    /*
     * Save modification time from file system such that we can check
     * if the file was changed during time we are going to be offline
     * when we get back up.
     */
    if(_master_fname.compare( "" )) {
      /* Arno: BUG, CONCURRENCY RISK
       * If the file is modified just before we measure the mtime this
       * update will go unnoticed. The FileMonitor thread may or may not
       * detect the update, and even when it does, the event will not
       * be delivered to this Document anymore. So the mtime we store
       * here is equal to the new mtime, which won't be different from
       * the mtime we get when we reboot unless there are new updates made.
       * Hence, no invalidates will be sent at reboot.
       */
      apr_finfo_t finfo;
      apr_status_t status;
      status=apr_stat(&finfo,_master_fname.c_str(),APR_FINFO_MTIME,out.pool());
      if(status == APR_SUCCESS)
        master_mtime = finfo.mtime;
      else
        // Set somewhere in future to force an invalidate when we unmarshall
        master_mtime = apr_time_now() + APR_TIME_C( 0xffffffff );
    } else {
      // Set somewhere in the past, so we won't refresh the slave's copies
      // (shouldn't happen)
      master_mtime = APR_TIME_C(0);
    }
  }
  Element::operator<<(out);
  out << _master_fname << master_mtime << _content_type
      << _lastmod << _docsize << _http_status
      << (apr_int16_t)_http_headers.size();
  for(gmap<const gstring,gstring>::const_iterator iter = _http_headers.begin();
      iter != _http_headers.end();
      ++iter)
    out << iter->first << iter->second;
  return out;
}

Input&
Document::operator>>(Input& in) throw()
{
  apr_time_t master_mtime = APR_TIME_C(0);
  string path;
  apr_int16_t nheaders;
  
  Element::operator>>(in);
  in >> _master_fname >> master_mtime >> _content_type
     >> _lastmod >> _docsize >> _http_status >> nheaders;
  for(int i=0; i<nheaders; i++) {
    string key, val;
    in >> key >> val;
    _http_headers[key.c_str()] = val.c_str();
  }

  if(_parent->ismaster() && _master_fname.compare("")) {
    // Do this before activating the file monitor, to prevent potential
    // concurrency (there shouldn't be any)
    if(master_mtime != APR_TIME_C(0)) {
      apr_status_t status;
      apr_finfo_t finfo;
      status = apr_stat(&finfo, _master_fname.c_str(),
                        APR_FINFO_MTIME|APR_FINFO_SIZE,in.pool());
      if(status != APR_SUCCESS) {
        // Set somewhere in the future to force an invalidate
        finfo.mtime = -1;
        DIAG(in.context(),(MONITOR(error),&_monitor),("Offline updates of %s are not signalled to slaves, cannot get local file information",_master_fname.c_str()));
      } else {
        if(finfo.mtime > master_mtime) {
          DIAG(in.context(),(MONITOR(info),&_monitor),("Document %s was changed while server was offline, notifying replicas",_master_fname.c_str()));
          ReplAction action = _policy->onUpdate(this);
          _lastmod = finfo.mtime;
          _docsize = finfo.size;
          mkstring message;
          message << "t=" << apr_time_now() << " lastmod=" << finfo.mtime
                  << " docsize=" << finfo.size << " path:%s" << _path.c_str();
          log(in.pool(), string("U"), message);
          if(action.options()&ReplAction::INVALIDATE_DOCS)
            invalidate(in.context(), in.pool(), action.inv());
        }
      }
    }
    // Reregister with FileMonitor such that we will again receive update
    // events.
    RegisterEvent ev(in.pool(), this, _master_fname.c_str());
    GlobuleEvent::submit(ev);
  }
  return in;
}

int
Document::fetch(Context* ctx, apr_pool_t* pool, const Url& url,
                request_rec *r, ReplAction &action,
                ResourceDeclaration& rsrcusage)
  throw()
{
  apr_status_t status;
  char* buffer;

  if(!_parent->isslave()) {
    DIAG(0, (MONITOR(error)),("Origin server was instructed by replication "
                              "policy to retrieve document from itself"));
    return HTTP_INTERNAL_SERVER_ERROR;
  }

  Url replica_url(pool, url);
  replica_url.normalize(pool);
  try {
    // FIXME: the request from _res->fetch() may never be freed correctly
    Fetch* fetch = _parent->fetch(pool, _path.c_str(), replica_url,
                   (action.from() == ReplAction::FROM_GET_IMS?action.ims():0));
    if(fetch->getHeader("Location") != "")
      _http_headers["Location"] = fetch->getHeader("Location").c_str();
    if(fetch->getStatus()) {
      _http_status = fetch->getStatus();
      if(_http_status == Fetch::HTTPRES_MOVED_PERMANENTLY ||
         _http_status == Fetch::HTTPRES_NOT_FOUND) {
        // Pass 'Location' header on
        if(_http_status == Fetch::HTTPRES_MOVED_PERMANENTLY && r)
          apr_table_set(r->headers_out, "Location",
                        _http_headers["Location"].c_str());
        _lastmod = apr_time_now();
        switchpolicy(ctx, pool, "Special");
        _policy->onLoaded(this, 0, _lastmod);
        delete fetch;
        return _http_status;
      } else {
        DIAG(0, (MONITOR(error)),("Origin server did not return content but HTTP code %d",fetch->getStatus()));
        delete fetch;
        return Fetch::HTTPRES_INTERNAL_SERVER_ERROR;
      }
    } else
      _http_status = globule::netw::HttpResponse::HTTPRES_OK;

    string respondedpolicy = fetch->getHeader("X-Globule-Policy");
    
    if(respondedpolicy != "") {
      if(respondedpolicy != _policy->policyName())
        switchpolicy(ctx, pool, respondedpolicy.c_str());
    } else {
      DIAG(ctx, (MONITOR(error),&_monitor), ("While trying to fetch a copy of a web page, the upstream server did not seem to be Globule enabled.  Reverting to pure proxy policy for this page"));
      switchpolicy(ctx, pool, "PureProxy", true);
    }
  
    // Open a file to save the document
    apr_file_t *file = 0;
    if(action.deliver() == ReplAction::DELIVER_SAVE) {
      string filename = _parent->docfilename(_path.c_str());
      string directory = _parent->docdirectory(_path.c_str());
      status = apr_dir_make_recursive(directory.c_str(), APR_OS_DEFAULT, pool);
      if(status != APR_SUCCESS) {
        DIAG(0, (MONITOR(error)),("Cannot create directory hierarchy %s",directory.c_str()));
        delete fetch;
        return HTTP_INTERNAL_SERVER_ERROR;
      }
      status = apr_file_open(&file, filename.c_str(),
                             APR_WRITE|APR_CREATE|APR_TRUNCATE|APR_BUFFERED,
                             APR_OS_DEFAULT, pool);
      if(status != APR_SUCCESS) {
        DIAG(0, (MONITOR(warning)),("Cannot open file %s",filename.c_str()));
        delete fetch;
        return HTTP_INTERNAL_SERVER_ERROR;
      }
    }
    
    if(r) {
      apr_table_t* tmp_headers_out = apr_table_make(pool, 0);
      fetch->getHeaders(tmp_headers_out);
      apr_table_overlap(tmp_headers_out,r->headers_out,APR_OVERLAP_TABLES_SET);
      r->headers_out = tmp_headers_out;
    }
    const char* contenttype = apr_pstrdup(pool,
                                     fetch->getHeader("Content-Type").c_str());
    bool phpscript = true;
    if(contenttype) {
      if(!strcasecmp(contenttype, "application/x-httpd-php-raw")) {
        phpscript = true;
        contenttype = "application/x-httpd-php";
      } else
        contenttype = apr_pstrdup(pool, contenttype);
      if(r)
        ap_set_content_type(r, contenttype);
    } else
      contenttype = "text/html";
    apr_time_t mtime = fetch->getDateHeader("Last-Modified");
    if(r) {
      if(mtime != 0)
        r->mtime = mtime;
      ap_set_last_modified(r);
      if(fetch->getContentLength() > 0)
        ap_set_content_length(r, fetch->getContentLength());
    }
    
    // Stream the document to the client and/or file
    apr_ssize_t nbytes_read = 0, sz = 0;
    apr_size_t nbytes_written = 0;
    apr_status_t status = APR_SUCCESS;
    while((nbytes_read = fetch->read(&buffer)) > 0) {

#ifdef PSODIUMTESTSLAVEBAD
      // Corrupt slave's output once in a while
      unsigned char will = random() % 256;
      DOUT( DEBUG3, "Corrupting my version of the document if " << will << " > 200" );
      if (will > 200 && sz == 0)
      {
            DOUT( DEBUG3, "Corrupting my version of the document" );
            buffer[0]='A';
      }
#endif

      if(action.deliver() == ReplAction::DELIVER_SAVE) {
        nbytes_written = nbytes_read;
        status = apr_file_write( file, buffer, &nbytes_written );
        if(status!=APR_SUCCESS || nbytes_read!=(apr_ssize_t)nbytes_written) {
          DIAG(ctx,(MONITOR(error),&_monitor),("Could not save imported document"));
          return HTTP_INTERNAL_SERVER_ERROR;
        }
      }
      sz += nbytes_read;
      if(r && !phpscript)
        ap_rwrite(buffer, nbytes_read, r);
    }

    if(action.deliver() == ReplAction::DELIVER_SAVE) {
      apr_file_close(file);
      rsrcusage.set(ResourceDeclaration::QUOTA_DISKSPACE, sz);
    }

    if(sz != fetch->getContentLength()) {
      DIAG(ctx,(MONITOR(warning),&_monitor),("Upstream server returned content larger or smaller than it specified earlier (indicated %d, received %d)",fetch->getContentLength(),sz));
    }

    _docsize = sz;
    _lastmod = mtime;
    _content_type = contenttype;
    _policy->onLoaded(this, _docsize, _lastmod);
    delete fetch;
    if(phpscript)
      return DECLINED;
    else
      return HTTP_OK;
  } catch(globule::netw::HttpException ex) {
    DIAG(0, (MONITOR(error)),("Cannot retrieve document from origin server: %s",ex.getReason().c_str()));
    return HTTP_BAD_GATEWAY;
  }
}
