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
#include <http_log.h>
#include <http_core.h>
#include <apr_version.h>
#include "utilities.h"
#include "alloc/Allocator.hpp"
#include "resource/BaseHandler.hpp"
#include "resource/OriginHandler.hpp"
#include "event/ReportEvent.hpp"
#include "event/RegisterEvent.hpp"
#include "documents.hpp"
#include "configuration.hpp"

using namespace std;

map<BaseHandler*,apr_file_t*> BaseHandler::_logfp;

ContainerHandler::ContainerHandler(Handler* parent, apr_pool_t* p, Context* ctx,
                                   const Url& uri)
  throw (SetupException)
  : Handler(parent, p, uri), _lock(5), _path(0),
    _monitor(ctx, rmmmemory::allocate(_uri(p)))
{
}

BaseHandler::BaseHandler(Handler* parent, apr_pool_t* p, Context* ctx,
                         const Url& uri)
  throw (SetupException)
  : ContainerHandler(parent,p,ctx,uri), _initialized(false),
    _accounting(0), _base_uri(uri)
{
  if(uri.port() != 0)
    _servername = apr_psprintf(p, "%s:%d", uri.host(), uri.port());
  else
    _servername = uri.host();
  _servername = rmmmemory::allocate(_servername);
}

ContainerHandler::ContainerHandler(Handler* parent,
                                   apr_pool_t* p, Context* ctx,
                                   const Url& uri, const char* path)
  throw(SetupException)
  : Handler(parent, p, uri), _lock(5), 
    _monitor(ctx, rmmmemory::allocate(_uri(p)))
{
  apr_status_t status;
  apr_finfo_t info;

  status = apr_stat(&info, path, APR_FINFO_TYPE, p);
  if(status != APR_SUCCESS) {
    status = apr_dir_make_recursive(path, APR_OS_DEFAULT, p);
    if(status != APR_SUCCESS && !APR_STATUS_IS_EEXIST(status))
      throw SetupException(status, mkstring()<<"Cannot create directory "<<path);
  } else if(info.filetype != APR_DIR)
    throw SetupException(status, mkstring()<<"Path "<<path<<" is not a directory");
  status = chownToCurrentUser(p, path);
  if(status != APR_SUCCESS)
    throw SetupException(status, mkstring()<<"Cannot change ownership of directory "<<path);
  string cachedir = mkstring::format("%s/.htglobule",path);
  status = apr_dir_make_recursive(cachedir.c_str(), APR_OS_DEFAULT, p);
  if(status != APR_SUCCESS && !APR_STATUS_IS_EEXIST(status)) {
    throw SetupException(status, mkstring()<<"Cannot create directory "<<cachedir);
  } else {
    // When started as root and changing to another user for regular
    // operation make sure the cachedir is writable by that user.
    status = chownToCurrentUser(p, cachedir.c_str());
    if(status != APR_SUCCESS)
      throw SetupException(status, mkstring()<<"Cannot change ownership of directory "<<cachedir);
  }
}

BaseHandler::BaseHandler(Handler* parent, apr_pool_t* p, Context* ctx,
                         const Url& uri, const char* path)
  throw(SetupException)
  : ContainerHandler(parent, p, ctx, uri, path),
    _initialized(false), _accounting(0), _base_uri(uri)
{
  if(uri.port() != 0)
    _servername = apr_psprintf(p, "%s:%d", uri.host(), uri.port());
  else
    _servername = uri.host();
  _servername = rmmmemory::allocate(_servername);
  _path       = rmmmemory::allocate(path);

  if(!ap_exists_config_define("DEBUG")) {
    RegisterEvent ev(p, this, 0);
    if(!GlobuleEvent::submit(ev)) {
      throw "Handler cannot register to HeartBeat";
    }
  } else
    DIAG(ctx,(MONITOR(warning)),("Omitting to send initialization to %s?status\n",_uri(p)));
}

bool
BaseHandler::initialize(apr_pool_t* p, Context* ctx, HandlerConfiguration* cfg)
  throw(SetupException)
{
  if(_initialized)
    return true;
  _initialized = true;
  return false;
}

void
BaseHandler::initialize(apr_pool_t* p, int naliases, const char** aliases)
  throw(SetupException)
{
  for(int i=0; i<naliases; i++)
    if(!strncasecmp(aliases[i],"http://",7) ||
       !strncasecmp(aliases[i],"https://",8))
      {
        Url url(p,aliases[i]);
        _aliases.push_back(url);
        if(i == 0)
          _base_uri = url;
      } else {
        Url url(p,_uri.scheme(),aliases[0],_uri.port(),_uri.pathquery(p));
        _aliases.push_back(url);
        if(i == 0)
          _base_uri = url;
      }
}

void
BaseHandler::initialize(apr_pool_t* p, Context* ctx,
			const std::vector<Contact>* contacts)
  throw(SetupException)
{
  if(contacts)
    for(vector<Contact>::const_iterator iter = contacts->begin();
        iter != contacts->end();
        ++iter)
      _peers.add(ctx, p, *iter);
}

ContainerHandler::~ContainerHandler() throw()
{
}

BaseHandler::~BaseHandler() throw()
{
  /* Cannot close the files in _logfp anymore */
}

Input&
ContainerHandler::operator>>(Input& in) throw(FileError,WrappedError)
{
  string path;
  in >> path;

  apr_size_t ndocuments;
  in >> ndocuments;
  for(apr_size_t i=0; i<ndocuments; i++) {
    string s;
    in >> s;
    in.push();
    Url location(in.pool(), _uri, s.c_str());
    Element* elt = new(rmmmemory::shm()) Document(this, in.pool(), location, in.context());
    in >> (Serializable&)*elt;
    in.pop();
    _documents[s.c_str()] = elt;
  }

  return in;
}

Input&
BaseHandler::operator>>(Input& in) throw(FileError,WrappedError)
{
  apr_int32_t version;
  string servername;
  string path;
  in >> version;
  if(version != FILEVERSION)
    throw WrappedError(FILEVERSION, version);
  in >> servername >> _peers;
  return ContainerHandler::operator>>(in);
}

Output&
ContainerHandler::operator<<(Output& out) const throw(FileError)
{
  out << _path;

  apr_size_t ndocuments = _documents.size();
  out << ndocuments;
  for(gmap<const gstring,Element*>::const_iterator iter=_documents.begin();
      iter != _documents.end();
      ++iter) {
    out << iter->first;
    out.push();
    out << (Serializable&)*(iter->second);
    out.pop();
  }
  return out;
}

Output&
BaseHandler::operator<<(Output& out) const throw(FileError)
{
  apr_int32_t version = FILEVERSION;
  out << version << _servername << _peers;
  return ContainerHandler::operator<<(out);
}

void
ContainerHandler::flush(apr_pool_t* pool) throw()
{
  for(gmap<const gstring,Element*>::iterator iter = _documents.begin();
      iter != _documents.end();
      ++iter)
    iter->second->flush(pool);
}

void
BaseHandler::flush(apr_pool_t* p) throw()
{
  string fname = mkstring() << _path << "/" << ".htglobule/state";
  try {
    FileStore store(p, 0, fname, false);
    (Output&)store << *this;
  } catch(WrappedError ex) {
    DIAG(0,(MONITOR(error)),("Internal problem; %s",ex.getMessage().c_str()));
    setserial(apr_time_now());
  } catch(FileError ex) {
    DIAG(0,(MONITOR(error)),("Unable to open file %s for writing",fname.c_str()));
    setserial(apr_time_now());
  }
}

void
BaseHandler::restore(apr_pool_t* p, Context* ctx) throw()
{
  bool configUpdated = false;
  string fname = mkstring() << _path << "/" << ".htglobule/state";
  try {
    FileStore store(p, ctx, fname, true);
    (Input&)store >> *this;
    for(;;) {
      Peers::iterator iter=_peers.begin();
      while(iter != _peers.end())
        if(iter->enabled == -2) {
          DIAG(ctx,(MONITOR(notice)),("Peer %s was removed",iter->uri()(p)));
          _peers.erase(iter);
          configUpdated = true;
          break;
        } else
          ++iter;
      if(iter == _peers.end())
        break;
    }
    if(configUpdated)
      setserial(apr_time_now());
  } catch(WrappedError ex) {
    string message = mkstring() << "t=" << apr_time_now();
    log(p, "F", message.c_str());
    DIAG(ctx,(MONITOR(error)),("File version error; %s",ex.getMessage().c_str()));
    setserial(apr_time_now());
  } catch(FileError ex) {
    string message = mkstring() << "t=" << apr_time_now();
    log(p, "F", message.c_str());
    DIAG(ctx,(MONITOR(warning)),("File %s not present or unreadable: %s",fname.c_str(),ex.getMessage().c_str()));
    setserial(apr_time_now());
  }
}

void
BaseHandler::setserial(apr_time_t serial) throw()
{
  for(Peers::iterator iter=_peers.begin(); iter != _peers.end(); ++iter)
    iter->setserial(serial);
}

ResourceAccounting*
ContainerHandler::accounting() throw()
{
  return 0;
}

void
BaseHandler::accounting(ResourceAccounting* acct) throw()
{
  _accounting = acct;
}

ResourceAccounting*
BaseHandler::accounting() throw()
{
  return _accounting;
}

apr_file_t*
BaseHandler::logfile(apr_pool_t* pool, bool drop) throw()
{
  apr_file_t* fp = NULL;
  map<BaseHandler*,apr_file_t*>::iterator iter = _logfp.find(this);
  if(iter != _logfp.end()) {
    fp = iter->second;
    if(drop)
      _logfp.erase(iter);
  } else if(!drop) {
    apr_pool_t* aux;
    while((aux = apr_pool_parent_get(pool)))
      pool = aux;
    apr_status_t status;
    string fname = logname();
    status = apr_file_open(&fp, fname.c_str(),
                         APR_APPEND|APR_READ|APR_WRITE|APR_CREATE|APR_XTHREAD,
                         APR_OS_DEFAULT, pool);
    if(status != APR_SUCCESS) {
      ap_log_perror(APLOG_MARK, APLOG_CRIT, status, pool,
                   "Could not open log file %s.",
                   fname.c_str());
    }
    if(fp != NULL)
      _logfp[this] = fp;
  }
  return fp;
}

apr_size_t
BaseHandler::logsize(apr_pool_t* pool) throw()
{
  apr_status_t status;
  apr_finfo_t finfo;
  apr_file_t* fp = logfile(pool);
  if(fp) {
    status = apr_file_info_get(&finfo, APR_FINFO_SIZE, fp);
    if(status == APR_SUCCESS)
      return finfo.size;
  }
  return 0;
}

string
BaseHandler::logname() throw()
{
  if(_path)
    return mkstring() << _path << "/" << ".htglobule/report.log";
  else
    throw string("BaseHandler::logname");
}

void
BaseHandler::log(apr_pool_t* pool, string msg1, const string msg2) throw()
{
  apr_file_t* fp = logfile(pool);
  if(fp) {
    if(msg2 == "")
      apr_file_printf(fp, "%s\n", msg1.c_str());
    else
      apr_file_printf(fp, "%s %s\n", msg1.c_str(), msg2.c_str());
    apr_file_flush(fp); // maybe not necessary
  }
}

const char*
BaseHandler::servername() throw()
{
  return _servername;
}

const Url&
BaseHandler::siteaddress() const throw()
{
  return _base_uri;
}

const char*
BaseHandler::siteaddress(apr_pool_t* p) const throw()
{
  return _base_uri(p);
}

Lock*
ContainerHandler::getlock() throw()
{
  return &_lock;
}

Handler*
ContainerHandler::gettarget(apr_pool_t *p, Context* ctx,
                            const string remainder,
                            const char* policy, const Handler* target) throw()
{
  if(target) {
    return const_cast<Handler*>(target);
  } else {
    gstring docname(remainder.c_str());
    gmap<const gstring,Element*>::iterator iter = _documents.find(docname);
    if(iter == _documents.end()) {
      if(policy) {
        Url url(p, _uri);
        Element* elt;
        elt = newtarget(p, url, ctx, remainder.c_str(), policy);
        _documents[docname] = elt;
        return elt;
      } else
        return 0;
    } else
      return iter->second;
  }
}

void
ContainerHandler::deltarget(Context* ctx, apr_pool_t* pool,
                            const string remainder) throw()
{
  gstring docname(remainder.c_str());
  _lock.lock_exclusively();
  gmap<const gstring,Element*>::iterator iter = _documents.find(docname);
  if(iter != _documents.end()) {
    Element* target = iter->second;
    Lock* lock = target->getlock();
    lock->lock_exclusively();
    _documents.erase(iter);
    _lock.unlock();
    target->purge(ctx, pool);
    lock->unlock();
    rmmmemory::destroy(target);
  } else
    _lock.unlock();
}

void
ContainerHandler::delalltargets(Context* ctx, apr_pool_t* pool) throw()
{
  for(;;) {
    gmap<const gstring,Element*>::iterator iter = _documents.begin();
    if(iter != _documents.end()) {
      Element* target = iter->second;
      Lock* lock = target->getlock();
      ap_assert(lock != &_lock);
      lock->lock_exclusively();
      _documents.erase(iter);
      target->purge(ctx, pool);
      lock->unlock();
      rmmmemory::destroy(target);
    } else
      break;
  }
}

void
ContainerHandler::alltargets(GlobuleEvent& evt) throw()
{
  for(gmap<const gstring,Element*>::iterator iter = _documents.begin();
      iter != _documents.end();
      ++iter)
    iter->second->handle(evt);
}

string
ContainerHandler::docfilename(const char* path) throw()
{
  if(_path) {
    if(_path[strlen(_path)-1] == '/')
      return mkstring::format("%s%s", _path, path);
    else
      return mkstring::format("%s/%s", _path, path);
  } else
    throw string("ContainerHandler::docfilename");
}

string
ContainerHandler::docdirectory(const char* path) throw()
{
  string fname = docfilename(path);
  fname = fname.erase(fname.rfind('/'),string::npos);
  return fname == "" ? "/" : fname;
}

Element*
BaseHandler::newtarget(apr_pool_t* pool, const Url& url, Context* ctx,
                       const char* path, const char* policy) throw()
{
  return new(rmmmemory::shm()) Document(this, pool, url, ctx, path, policy);
}

Peer*
BaseHandler::addReplicaFast(Context* ctx, apr_pool_t* p,
                            const char* location, const char* secret) throw()
{
  Monitor* m = new(rmmmemory::shm()) Monitor(ctx, rmmmemory::allocate(location));
  Contact contact(p, (secret?Peer::REPLICA:Peer::MIRROR), location, (secret?secret:""));
  Peer peer(ctx, p, m, contact, true);
  return _peers.add(peer);
}

void
BaseHandler::changeReplica(Peer* p) throw()
{
}

bool
BaseHandler::handle(GlobuleEvent& evt, const string& remainder) throw()
{
  switch(evt.type()) {
  case GlobuleEvent::REPORT_EVT: {
    int i;
    ReportEvent& ev = (ReportEvent&)evt;
    ev.setProperty("ndocs","0");
    alltargets(evt);
    ev.setProperty("npeers","0");
    i = 0;
    for(Peers::iterator iter = _peers.begin();
        iter != _peers.end();
        ++iter,++i)
      {
        ev.setProperty(mkstring()<<"peer"<<i, iter->uri()(evt.pool));
        ReportEvent subevt("peer", &ev);
        switch(iter->type()) {
        case Peer::NONE:
          subevt.setProperty("type", "none");
          break;
        case Peer::ANY:
          subevt.setProperty("type", "any");
          break;
        case Peer::REDIRECTOR:
          subevt.setProperty("type", "redirector");
          break;
        case Peer::KEEPER:
          subevt.setProperty("type", "keeper");
          break;
        case Peer::REPLICA:
          subevt.setProperty("type", "replica");
          break;
        case Peer::MIRROR:
          subevt.setProperty("type", "mirror");
          break;
        case Peer::ORIGIN:
          subevt.setProperty("type", "origin");
          break;
        default:
          subevt.setProperty("type", "unknown");
        }
        subevt.setProperty("uri", iter->uri()(evt.pool));
        subevt.setProperty("secret", iter->secret());
        subevt.setProperty("serial", mkstring()<<iter->serial());
        subevt.setProperty("weight", mkstring()<<iter->weight());
        subevt.setProperty("enabled", mkstring()<<iter->enabled);
        subevt.setProperty("available", (iter->enabled>0 ? "true" : "false") );
      }
    if(accounting()) {
      ResourceDeclaration rsrcavailable, rsrctotal;
      accounting()->info(rsrctotal, rsrcavailable);
      ReportEvent subevt("resource", &ev);
      subevt.setProperty("disk.total",
          mkstring()<<rsrctotal.get(ResourceDeclaration::QUOTA_DISKSPACE));
      subevt.setProperty("disk.available",
          mkstring()<<rsrcavailable.get(ResourceDeclaration::QUOTA_DISKSPACE));
      subevt.setProperty("numdocs.total",
          mkstring()<<rsrctotal.get(ResourceDeclaration::QUOTA_NUMDOCS));
      subevt.setProperty("numdocs.available",
          mkstring()<<rsrcavailable.get(ResourceDeclaration::QUOTA_NUMDOCS));
    }
    return false;
  }
  default:
    ;
  }
  return false;
}

void
BaseHandler::shiplog(Context* ctx, apr_pool_t* p, apr_size_t& offset)
  throw()
{
  using namespace globule::netw;
  apr_file_t* fd = 0;
  try {
    apr_off_t size = (apr_off_t) logsize(p);
    if((apr_size_t)size < offset) {
      /* file was truncated since last time.  There is no guarentee we
       * will detect this properly, because in the meanwhile the file may
       * have grown larger than the original case.  But in normal usage,
       * for instance when using a rotating log, the truncated file will
       * be smaller.
       */
      offset = 0;
    }
    if(size - offset >= 0 || (apr_size_t)size < offset) {
      if(apr_file_open(&fd, logname().c_str(), APR_READ, 0, p))
        throw HttpException("open failed");
      {
	apr_off_t aux = offset;
	if(apr_file_seek(fd, APR_SET, &aux))
	  throw HttpException("seek failed");
      }
      if(_peers.origin()) {
        HttpRequest req(p, _peers.origin()->uri());
        req.setMethod("REPORT");
        req.setHeader("X-Globule-Protocol", PROTVERSION);
        req.setHeader("X-Globule-Serial", _peers.origin()->serial());
        req.setHeader("X-From-Replica", _servername);
        req.setHeader("Content-Length",(apr_int32_t)(size - offset));
        HttpConnection *con = req.connect(p);
        con->write(fd, size - offset);
        apr_file_close(fd);
        fd = 0;
        HttpResponse *res = con->response();
        if(res->getStatus() != HttpResponse::HTTPRES_OK &&
          res->getStatus() != HttpResponse::HTTPRES_NO_CONTENT) {
          DIAG(ctx,(MONITOR(warning)),("Failed to transmit log to %s: %s (%d)",_peers.origin()->uri()(p),res->getMessage().c_str(),res->getStatus()));
        } else
          offset = size;
      } else
        ; // FIXME: should have an origin() F*CK
    }
  } catch(HttpException ex) {
    DIAG(ctx,(MONITOR(warning)),("Failed to transmit log to %s: http exception %s",_peers.origin()->uri()(p),ex.getReason().c_str()));
    if(fd)
      apr_file_close(fd);
  }
}

#include <apr_file_io.h>
#ifndef STANDALONE_APR
#ifndef WIN32
#include <unixd.h> // for chown
#endif /* !WIN32 */
#endif
#include <apr_errno.h>
#include "Constants.hpp"
#ifndef WIN32
#include <unistd.h>
#endif /* !WIN32 */
#include <sys/types.h>

apr_status_t
ContainerHandler::chownToCurrentUser(apr_pool_t* pool, const char* fname) throw()
{
#ifndef STANDALONE_APR
#ifndef WIN32
  if(getuid() == 0) { // Only when started as root
    if(chown(fname, unixd_config.user_id, unixd_config.group_id) < 0)
      return APR_FROM_OS_ERROR(errno);
  }
#endif /* !WIN32 */
  return APR_SUCCESS;
#else
  return APR_SUCCESS;
#endif
}

Fetch*
BaseHandler::fetch(apr_pool_t* p, const char* path,
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
  char* secret = apr_pstrdup(p, peer->secret());
  Peers::iterator iter = _peers.begin(Peer::KEEPER);
  HttpRequest* req = 0;
  for(;;) {
    try {
      Url source(p, peer->uri(), path);
      req = new HttpRequest(p, source);
      if(method)
        req->setMethod(method);
      req->setAuthorization(secret, secret);
      req->setHeader("X-Globule-Protocol", PROTVERSION);
      req->setHeader("X-Globule-Serial", peer->serial());
      req->setHeader("X-From-Replica", replicaUrl(p));
      if(arguments)
        req->setHeader("Content-Length", mkstring()<<strlen(arguments));
      req->setConnectTimeout(3);
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
          const char* fetchlocation = peer->location(p);
          if(!strncmp(recvdlocation.c_str(),fetchlocation,strlen(fetchlocation)))
            req->response(p)->setHeader("Location", apr_pstrcat(p,siteaddress(p),
                          &recvdlocation.c_str()[strlen(fetchlocation)], NULL));
        }
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
    if(iter != _peers.end()) {
      peer = &*iter;
      ++iter;
    } else
      break;
  }
  return new ExceptionFetch(Fetch::HTTPRES_SERVICE_UNAVAILABLE,
                            "No stable servers available");
}

bool
BaseHandler::fetchcfg(apr_pool_t* pool, Context* ctx,
		      const Url& location, const char* authorization,
		      const char* metadata)
  throw()
{
  apr_status_t status;
  string uripath = mkstring() << "/.htglobule/" << metadata;
  Url url(pool, location, uripath.c_str());
  DIAG(ctx,(MONITOR(detail)),("Fetching meta configuration data %s from %s\n",
			      metadata,url(pool)));
  try {
    globule::netw::HttpRequest req(pool, url);
    req.setAuthorization(authorization, authorization);
    req.setHeader("X-Globule-Protocol", PROTVERSION);
    if(_peers.origin())
      req.setHeader("X-Globule-Serial", authorization);
    req.setHeader("X-From-Replica", _uri(pool));
    req.setConnectTimeout(10);
    globule::netw::HttpResponse* res = req.response(pool);
    if(res->getStatus() == globule::netw::HttpResponse::HTTPRES_OK) {
      apr_file_t* fp;
      string fname = mkstring() << _path << "/" << ".htglobule/" << metadata;
      DIAG(ctx,(MONITOR(detail)),("Storing meta configuration data in file "
				  "%s\n",fname.c_str()));
      RegisterEvent ev(pool, this, gstring(fname.c_str()));
      GlobuleEvent::submit(ev);
      status = apr_file_open(&fp, fname.c_str(), APR_WRITE|APR_CREATE,
                             APR_OS_DEFAULT, pool);
      if(status == APR_SUCCESS) {
        res->read(fp);
        apr_file_close(fp);
        RegisterEvent ev(pool, this, gstring(fname.c_str()));
        GlobuleEvent::submit(ev);
        return true;
      }
      DIAG(ctx,(MONITOR(error)),("Error retrieving meta configuration data"));
    }
  } catch(globule::netw::HttpException ex) {
    DIAG(ctx,(MONITOR(notice)),("Could not fetch meta configuration data %s "
				"from %s",metadata,url(pool)));
  }
  return false;
}

void
BaseHandler::reload(apr_pool_t* ptemp, Context* ctx,
		    const Url& upstream, const char* authorization,
		    string& redirect_policy) throw()
{
  map<string,string> replicas;
  int i, nreplicas;
  apr_uint16_t type, weight;
  string location, secret, fname;

  fname = mkstring() << _path << "/" << ".htglobule/redirection";
  try {
    FileStore store(ptemp, ctx, fname, true);
    store >> redirect_policy >> nreplicas;
    for(i=0; i<nreplicas; i++) {
      store >> type >> weight >> location >> secret;
      replicas[location] = secret;
    }
  } catch(FileError) {
    return;
  }

  DIAG(ctx,(MONITOR(info)),("Reloading redirection configuration from %s\n",fname.c_str()));

  for(Peers::iterator iter = _peers.begin(Peer::REPLICA|Peer::MIRROR);
      iter != _peers.end();
      ++iter)
    {
      string location(iter->location(ptemp));
      if(replicas.find(location) == replicas.end()) {
	DIAG(ctx,(MONITOR(info)),("Replica host %s no longer present\n",location.c_str()));
	iter->enabled = -2;
      }
    }
  for(map<string,string>::iterator iter = replicas.begin();
      iter != replicas.end();
      ++iter)
    {
      Url replicaUrl(ptemp, iter->first);
      Peers::iterator replicaIter = _peers.find(replicaUrl);
      if(replicaIter != _peers.end()) {
	replicaIter->_secret = iter->second.c_str();
      } else {
	DIAG(ctx,(MONITOR(info)),("New replica host %s appeared\n",location.c_str()));
	if(iter->second == "" &&
	   !strcmp(iter->first.c_str(),upstream(ptemp))) {
	  addReplicaFast(ctx, ptemp, iter->first.c_str(), authorization);
	} else
	  addReplicaFast(ctx, ptemp, iter->first.c_str(), iter->second.c_str());
      }
    }
}

extern "C" {
#include <apr_buckets.h>
#define MIN_LINE_ALLOC 80
apr_status_t
globule_rgetline(char **s, apr_size_t n,
                 apr_size_t *read, request_rec *r,
                 int fold, apr_bucket_brigade *bb)
{
#if (APR_MAJOR_VERSION == 0)
    apr_status_t rv;
    apr_bucket *e;
    apr_size_t bytes_handled = 0, current_alloc = 0;
    char *pos, *last_char = *s;
    int do_alloc = (*s == NULL), saw_eos = 0;

    while (!saw_eos) {
    apr_brigade_cleanup(bb);
    rv = ap_get_brigade(r->input_filters, bb, AP_MODE_GETLINE,
                        APR_BLOCK_READ, 0);

    if (rv != APR_SUCCESS) {
        return rv;
    }

    /* Something horribly wrong happened.  Someone didn't block! */
    if (APR_BRIGADE_EMPTY(bb)) {
        return APR_EGENERAL;
    }

    APR_BRIGADE_FOREACH(e, bb) {
        const char *str;
        apr_size_t len;

        /* If we see an EOS, don't bother doing anything more. */
        if (APR_BUCKET_IS_EOS(e)) {
            saw_eos = 1;
            break;
        }

        rv = apr_bucket_read(e, &str, &len, APR_BLOCK_READ);

        if (rv != APR_SUCCESS) {
            return rv;
        }

        if (len == 0) {
            /* no use attempting a zero-byte alloc (hurts when
             * using --with-efence --enable-pool-debug) or
             * doing any of the other logic either
             */
            continue;
        }

        /* Would this overrun our buffer?  If so, we'll die. */
        if (n < bytes_handled + len) {
            *read = bytes_handled;
            if (*s) {
                /* ensure this string is NUL terminated */
                if (bytes_handled > 0) {
                    (*s)[bytes_handled-1] = '\0';
                }
                else {
                    (*s)[0] = '\0';
                }
            }
            return APR_ENOSPC;
        }

        /* Do we have to handle the allocation ourselves? */
        if (do_alloc) {
            /* We'll assume the common case where one bucket is enough. */
            if (!*s) {
                current_alloc = len;
                if (current_alloc < MIN_LINE_ALLOC) {
                    current_alloc = MIN_LINE_ALLOC;
                }
                *s = (char*) apr_palloc(r->pool, current_alloc);
            }
            else if (bytes_handled + len > current_alloc) {
                /* Increase the buffer size */
                apr_size_t new_size = current_alloc * 2;
                char *new_buffer;

                if (bytes_handled + len > new_size) {
                    new_size = (bytes_handled + len) * 2;
                }

                new_buffer = (char*) apr_palloc(r->pool, new_size);

                /* Copy what we already had. */
                memcpy(new_buffer, *s, bytes_handled);
                current_alloc = new_size;
                *s = new_buffer;
            }
        }

        /* Just copy the rest of the data to the end of the old buffer. */
        pos = *s + bytes_handled;
        memcpy(pos, str, len);
        last_char = pos + len - 1;

        /* We've now processed that new data - update accordingly. */
        bytes_handled += len;
    }

        /* If we got a full line of input, stop reading */
        if (last_char && (*last_char == APR_ASCII_LF)) {
            break;
        }
    }

    /* Now NUL-terminate the string at the end of the line;
     * if the last-but-one character is a CR, terminate there */
    if (last_char > *s && last_char[-1] == APR_ASCII_CR) {
        last_char--;
    }
    *last_char = '\0';
    bytes_handled = last_char - *s;

    /* If we're folding, we have more work to do.
     *
     * Note that if an EOS was seen, we know we can't have another line.
     */
    if (fold && bytes_handled && !saw_eos) {
        for (;;) {
        const char *str;
        apr_size_t len;
        char c;

        /* Clear the temp brigade for this filter read. */
        apr_brigade_cleanup(bb);

        /* We only care about the first byte. */
        rv = ap_get_brigade(r->input_filters, bb, AP_MODE_SPECULATIVE,
                            APR_BLOCK_READ, 1);

        if (rv != APR_SUCCESS) {
            return rv;
        }

        if (APR_BRIGADE_EMPTY(bb)) {
                break;
        }

        e = APR_BRIGADE_FIRST(bb);

        /* If we see an EOS, don't bother doing anything more. */
        if (APR_BUCKET_IS_EOS(e)) {
                break;
        }

        rv = apr_bucket_read(e, &str, &len, APR_BLOCK_READ);

        if (rv != APR_SUCCESS) {
                apr_brigade_cleanup(bb);
            return rv;
        }

        /* Found one, so call ourselves again to get the next line.
         *
         * FIXME: If the folding line is completely blank, should we
         * stop folding?  Does that require also looking at the next
         * char?
         */
            /* When we call destroy, the buckets are deleted, so save that
             * one character we need.  This simplifies our execution paths
             * at the cost of one character read.
             */
            c = *str;
        if (c == APR_ASCII_BLANK || c == APR_ASCII_TAB) {
            /* Do we have enough space? We may be full now. */
                if (bytes_handled >= n) {
                    *read = n;
                    /* ensure this string is terminated */
                    (*s)[n-1] = '\0';
                    return APR_ENOSPC;
                }
                else {
                apr_size_t next_size, next_len;
                char *tmp;

                /* If we're doing the allocations for them, we have to
                 * give ourselves a NULL and copy it on return.
                 */
                if (do_alloc) {
                    tmp = NULL;
                } else {
                    /* We're null terminated. */
                    tmp = last_char;
                }

                next_size = n - bytes_handled;

                    rv = globule_rgetline(&tmp, next_size,
                                          &next_len, r, 0, bb);

                if (rv != APR_SUCCESS) {
                    return rv;
                }

                if (do_alloc && next_len > 0) {
                    char *new_buffer;
                    apr_size_t new_size = bytes_handled + next_len + 1;

                    /* we need to alloc an extra byte for a null */
                    new_buffer = (char*) apr_palloc(r->pool, new_size);

                    /* Copy what we already had. */
                    memcpy(new_buffer, *s, bytes_handled);

                    /* copy the new line, including the trailing null */
                    memcpy(new_buffer + bytes_handled, tmp, next_len + 1);
                    *s = new_buffer;
                }

                    bytes_handled += next_len;
            }
            }
            else { /* next character is not tab or space */
                break;
            }
        }
    }

    *read = bytes_handled;
    return APR_SUCCESS;
#else
    apr_status_t rv;
    apr_bucket *e;
    apr_size_t bytes_handled = 0, current_alloc = 0;
    char *pos, *last_char = *s;
    int do_alloc = (*s == NULL), saw_eos = 0;

    while (!saw_eos) {
        apr_brigade_cleanup(bb);
        rv = ap_get_brigade(r->input_filters, bb, AP_MODE_GETLINE,
                            APR_BLOCK_READ, 0);
        if (rv != APR_SUCCESS) {
            return rv;
        }

        /* Something horribly wrong happened.  Someone didn't block! */
        if (APR_BRIGADE_EMPTY(bb)) {
            return APR_EGENERAL;
        }

        for (e = APR_BRIGADE_FIRST(bb);
             e != APR_BRIGADE_SENTINEL(bb);
             e = APR_BUCKET_NEXT(e))
        {
            const char *str;
            apr_size_t len;

            /* If we see an EOS, don't bother doing anything more. */
            if (APR_BUCKET_IS_EOS(e)) {
                saw_eos = 1;
                break;
            }

            rv = apr_bucket_read(e, &str, &len, APR_BLOCK_READ);
            if (rv != APR_SUCCESS) {
                return rv;
            }

            if (len == 0) {
                /* no use attempting a zero-byte alloc (hurts when
                 * using --with-efence --enable-pool-debug) or
                 * doing any of the other logic either
                 */
                continue;
            }

            /* Would this overrun our buffer?  If so, we'll die. */
            if (n < bytes_handled + len) {
                *read = bytes_handled;
                if (*s) {
                    /* ensure this string is NUL terminated */
                    if (bytes_handled > 0) {
                        (*s)[bytes_handled-1] = '\0';
                    }
                    else {
                        (*s)[0] = '\0';
                    }
                }
                return APR_ENOSPC;
            }

            /* Do we have to handle the allocation ourselves? */
            if (do_alloc) {
                /* We'll assume the common case where one bucket is enough. */
                if (!*s) {
                    current_alloc = len;
                    if (current_alloc < MIN_LINE_ALLOC) {
                        current_alloc = MIN_LINE_ALLOC;
                    }
                    *s = (char*) apr_palloc(r->pool, current_alloc);
                }
                else if (bytes_handled + len > current_alloc) {
                    /* Increase the buffer size */
                    apr_size_t new_size = current_alloc * 2;
                    char *new_buffer;

                    if (bytes_handled + len > new_size) {
                        new_size = (bytes_handled + len) * 2;
                    }

                    new_buffer = (char*) apr_palloc(r->pool, new_size);

                    /* Copy what we already had. */
                    memcpy(new_buffer, *s, bytes_handled);
                    current_alloc = new_size;
                    *s = new_buffer;
                }
            }

            /* Just copy the rest of the data to the end of the old buffer. */
            pos = *s + bytes_handled;
            memcpy(pos, str, len);
            last_char = pos + len - 1;

            /* We've now processed that new data - update accordingly. */
            bytes_handled += len;
        }

        /* If we got a full line of input, stop reading */
        if (last_char && (*last_char == APR_ASCII_LF)) {
            break;
        }
    }

    /* Now NUL-terminate the string at the end of the line;
     * if the last-but-one character is a CR, terminate there */
    if (last_char > *s && last_char[-1] == APR_ASCII_CR) {
        last_char--;
    }
    *last_char = '\0';
    bytes_handled = last_char - *s;

    /* If we're folding, we have more work to do.
     *
     * Note that if an EOS was seen, we know we can't have another line.
     */
    if (fold && bytes_handled && !saw_eos) {
        for (;;) {
            const char *str;
            apr_size_t len;
            char c;

            /* Clear the temp brigade for this filter read. */
            apr_brigade_cleanup(bb);

            /* We only care about the first byte. */
            rv = ap_get_brigade(r->input_filters, bb, AP_MODE_SPECULATIVE,
                                APR_BLOCK_READ, 1);
            if (rv != APR_SUCCESS) {
                return rv;
            }

            if (APR_BRIGADE_EMPTY(bb)) {
                break;
            }

            e = APR_BRIGADE_FIRST(bb);

            /* If we see an EOS, don't bother doing anything more. */
            if (APR_BUCKET_IS_EOS(e)) {
                break;
            }

            rv = apr_bucket_read(e, &str, &len, APR_BLOCK_READ);
            if (rv != APR_SUCCESS) {
                apr_brigade_cleanup(bb);
                return rv;
            }

            /* Found one, so call ourselves again to get the next line.
             *
             * FIXME: If the folding line is completely blank, should we
             * stop folding?  Does that require also looking at the next
             * char?
             */
            /* When we call destroy, the buckets are deleted, so save that
             * one character we need.  This simplifies our execution paths
             * at the cost of one character read.
             */
            c = *str;
            if (c == APR_ASCII_BLANK || c == APR_ASCII_TAB) {
                /* Do we have enough space? We may be full now. */
                if (bytes_handled >= n) {
                    *read = n;
                    /* ensure this string is terminated */
                    (*s)[n-1] = '\0';
                    return APR_ENOSPC;
                }
                else {
                    apr_size_t next_size, next_len;
                    char *tmp;

                    /* If we're doing the allocations for them, we have to
                     * give ourselves a NULL and copy it on return.
                     */
                    if (do_alloc) {
                        tmp = NULL;
                    } else {
                        /* We're null terminated. */
                        tmp = last_char;
                    }

                    next_size = n - bytes_handled;

                    rv = globule_rgetline(&tmp, next_size,
                                          &next_len, r, 0, bb);
                    if (rv != APR_SUCCESS) {
                        return rv;
                    }

                    if (do_alloc && next_len > 0) {
                        char *new_buffer;
                        apr_size_t new_size = bytes_handled + next_len + 1;

                        /* we need to alloc an extra byte for a null */
                        new_buffer = (char*) apr_palloc(r->pool, new_size);

                        /* Copy what we already had. */
                        memcpy(new_buffer, *s, bytes_handled);

                        /* copy the new line, including the trailing null */
                        memcpy(new_buffer + bytes_handled, tmp, next_len + 1);
                        *s = new_buffer;
                    }

                    last_char += next_len;
                    bytes_handled += next_len;
                }
            }
            else { /* next character is not tab or space */
                break;
            }
        }
    }

    *read = bytes_handled;
    return APR_SUCCESS;
#endif
}
}
