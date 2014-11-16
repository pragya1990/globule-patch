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
#include <http_protocol.h>
#include <http_log.h>
#ifdef HAVE_LIBMYSQLCLIENT
#include <mysql/mysql.h>
#endif
#include "resource/DatabaseHandler.hpp"
#include "event/RedirectEvent.hpp"
#include "event/HttpMetaEvent.hpp"

using std::string;
using std::stringbuf;
using std::vector;

static std::string
encode(int col, int ncols, const char* buf, int nbytes = -1)
{
  mkstring rt;
  if(col != 1)
    rt << '\t';
  if(buf && nbytes != 0) {
    while((nbytes < 0 && *buf) || (nbytes >= 0 && nbytes-- > 0)) {
      switch(*buf) {
      case '\n':  rt << "\\n";   break;
      case '\t':  rt << "\\t";   break;
      case '\\':  rt << "\\\\";  break;
      default:
        rt << *buf;
      }
      ++buf;
    }
  } else
    rt << "\\N";
  if(col == ncols)
    rt << '\n';
  return rt;
}

class QueryResult : public Element
{
  bool _valid;
  gstring _qresults;
  gstring _qargument;
  gmap<const gstring,gstring> _headers;
protected:
  virtual Input& operator>>(Input& in) throw() {
    Element::operator>>(in);
    in >> _qresults;
    return in;
  };
  virtual Output& operator<<(Output& out) const throw() {
    Element::operator<<(out);
    out << _qresults;
    return out;
  };
  virtual void execute(Context* ctx, apr_pool_t* pool, ReplAction action) throw() {
    if(action.options()&ReplAction::INVALIDATE_DOCS)
      _valid = false;
  };
private:
  QueryResult() throw();
public:
  QueryResult(const QueryResult& doc) throw()
    : Element(doc), _valid(false)
  {
  };
  QueryResult(ContainerHandler* parent, apr_pool_t* pool,
              const Url& url, Context* ctx) throw()
    : Element(parent, pool, url, ctx), _valid(false)
  {
  };
  QueryResult(ContainerHandler* parent, apr_pool_t* pool,
              const Url& url, Context* ctx,
              const char* path, const char* policy) throw()
    : Element(parent, pool, url, ctx, path, policy), _valid(false)
  {
  };
  virtual ~QueryResult() throw() {
  };
  virtual bool handle(GlobuleEvent& evt) throw() {
    return handle(evt, "");
  }
  virtual bool handle(GlobuleEvent& evt, const std::string& remainder) throw() {
    switch(evt.type()) {
    case GlobuleEvent::REQUEST_EVT: {
      HttpReqEvent& ev = (HttpReqEvent&) evt;
      request_rec* req = ev.getRequest();
      char* buffer;
      apr_ssize_t nbytes;
      if(!_valid) {
        _qargument = remainder.c_str();
        Fetch* ftch = _parent->fetch(evt.pool, _path.c_str(), location(),
                                     0, _qargument.c_str(),
                                     req->method_number==M_POST?"POST":"GET");
        if(!ftch->getStatus()) {
          ev.setStatus(HTTP_OK);
          mkstring qresults;
          if(req) {
            ftch->getHeaders(req->headers_out);
            _headers["X-Globule-ColumnCount"] = ftch->getHeader("X-Globule-ColumnCount").c_str();
            _headers["X-Globule-RowCount"]    = ftch->getHeader("X-Globule-RowCount").c_str();
            ap_set_content_type(req, "text/x-globule-sql");
            req->chunked = 1;
            while((nbytes = ftch->read(&buffer)) > 0) {
              string outbuffer = ftch->encode(buffer, nbytes);
              qresults << outbuffer;
              ap_rwrite(outbuffer.c_str(), outbuffer.length(), req);
            }
          } else
            while((nbytes = ftch->read(&buffer)) > 0) {
              qresults << ftch->encode(buffer,nbytes);
            }
          string aux(qresults.str());
          _qresults = aux.c_str();
          _valid = true;
        } else {
          ap_log_rerror(APLOG_MARK, APLOG_ERR, APR_EGENERAL, req,
                        ftch->getMessage().c_str());
          ev.setStatus(ftch->getStatus());
        }
        delete ftch;
      } else {
        ev.setStatus(HTTP_OK);
        if(req) {
          for(gmap<const gstring,gstring>::iterator iter = _headers.begin();
              iter != _headers.end();
              ++iter)
            apr_table_set(req->headers_out,iter->first.c_str(),iter->second.c_str());
          ap_set_content_type(req, "text/x-globule-sql");
          ap_rputs(_qresults.c_str(), req);
        }
      }
      if(req) {
        ap_rflush(req);
        //ap_finalize_request_protocol(req);
      }
      return true;
    }
    case GlobuleEvent::INVALID_EVT: {
      HttpMetaEvent& ev = (HttpMetaEvent&) evt;
      ev.setStatus(HTTP_NO_CONTENT);
      _valid = false;
      return true;
    }
    case GlobuleEvent::WRAPUP_EVT: {
      return true;
    }
    default:
      return false;
    }
  };
  virtual std::string description() const throw() {
    return mkstring() << "DataHandler::QueryResult("
                      << _uri.path() << ")";
  };
};

class QueryClass : public Element, public ContainerHandler
{
private:
  gstring _qstatement;
  gvector<gstring> _qrelations;
private:
  static apr_status_t getdata(request_rec* r, string& data) throw() {
    const unsigned int maxdata = 10240;
    const int maxbuffer = 10240;
    char* content;
    apr_status_t status;
    apr_off_t actual;
    apr_size_t total;
    apr_bucket_brigade *bb;
    bb = apr_brigade_create(r->pool, r->connection->bucket_alloc);
    status = ap_get_brigade(r->input_filters, bb, AP_MODE_READBYTES,
                            APR_BLOCK_READ, maxbuffer);
    apr_brigade_length(bb, 1, &actual);
    total = (apr_size_t)actual;
    if(total > maxdata)
      return HTTP_BAD_REQUEST; // max'ed out
    content = (char*) apr_palloc(r->pool, total + 1);
    status = apr_brigade_flatten(bb, content, &total);
    content[total] = '\0';
    if(status != APR_SUCCESS)
      return status;
    data = content;
    return APR_SUCCESS;
  };
protected:
  virtual Input& operator>>(Input& in) throw() {
    ContainerHandler::operator>>(in);
    Element::operator>>(in);
    in >> _qstatement;
    return in;
  };
  virtual Output& operator<<(Output& out) const throw() {
    ContainerHandler::operator<<(out);
    Element::operator<<(out);
    out << _qstatement;
    return out;
  };
  virtual void execute(Context* ctx, apr_pool_t* pool, ReplAction action) throw() { };
private:
  QueryClass() throw();
  QueryClass(QueryClass&) throw();
public:
  QueryClass(BaseHandler* parent, apr_pool_t* pool, Context* ctx,
              const Url& url, const char* path) throw()
    : Element(parent, pool, url, ctx),
      ContainerHandler(parent, pool, ctx, url)
  {
  };
  QueryClass(BaseHandler* parent, apr_pool_t* pool, Context* ctx,
              const Url& url, const char* path, const char* policy)
    throw (SetupException)
    : Element(parent, pool, url, ctx, path, policy),
      ContainerHandler(parent, pool, ctx, url, parent->docfilename(path).c_str())
  {
  };
  virtual ~QueryClass() throw() {
  };
  virtual Element* newtarget(apr_pool_t* pool, const Url& url, Context* ctx,
                             const char* path, const char* policy) throw()
  {
    return new(rmmmemory::shm()) QueryResult(this,pool,url,ctx,path,policy);
  }
  virtual bool handle(GlobuleEvent& evt) throw() {
    apr_status_t status;
    switch(evt.type()) {
    case GlobuleEvent::REQUEST_EVT: {
      HttpReqEvent& ev = (HttpReqEvent&)evt;
      request_rec* req = ev.getRequest();
      string qargument;
      status = getdata(req, qargument);
      if(status != APR_SUCCESS) {
        ap_log_rerror(APLOG_MARK, APLOG_ERR, status, req,
                      "Cannot read query arguments from request");
        ev.setStatus(status);
        return true;
      }
      switch(req->method_number) {
      case M_PUT: {
        string::size_type pos;
        if((pos = qargument.find("\n")) != string::npos) {
          _qstatement = qargument.substr(0,pos).c_str();
          qargument.erase(0, pos+1);
          while((pos = qargument.find("\n")) != string::npos) {
            _qrelations.push_back(qargument.substr(0,pos).c_str());
            qargument.erase(0, pos+1);
          }
          if(qargument.length() > 0)
            _qrelations.push_back(qargument.c_str());
        } else
          _qstatement = qargument.c_str();
        ev.setStatus(HTTP_CREATED);
        break;
      }
      case M_POST: {
        Handler* elt = gettarget(evt.pool,evt.context,qargument,"GlobeCB"); // FIXME do not create
        elt->handle(evt, qargument);
        for(gvector<gstring>::iterator iter = _qrelations.begin();
            iter != _qrelations.end();
            ++iter)
          {
            Url url(evt.pool, ContainerHandler::baselocation(), iter->c_str());
            HttpMetaEvent invalidevt(evt.pool, evt.context,
                                     const_cast<apr_uri_t*>(url.uri()));
            GlobuleEvent::submit(invalidevt);
          }
        deltarget(evt.context, evt.pool, qargument); // FIXME first create then delete
        break;
      }
      case M_GET:
        Handler* elt = gettarget(evt.pool,evt.context,qargument,"GlobeCB"); // FIXME do not create
        elt->handle(evt, qargument);
        break;
      }
      return true;
    }
    case GlobuleEvent::INVALID_EVT: {
      string remainder;
      // FIXME next line from Handler::handle
      if(evt.target()==(ContainerHandler*)this || evt.match(ContainerHandler::_uri,remainder)) {
        alltargets(evt);
        return true;
      } else {
        Handler* elt = gettarget(evt.pool, evt.context, remainder);
        if(elt)
          elt->handle(evt);
        return true;
      }
    }
    case GlobuleEvent::WRAPUP_EVT: {
      return true;
    };
    case GlobuleEvent::REPORT_EVT: {
      int i;
      ReportEvent& ev = (ReportEvent&)evt;
      ev.setProperty("name",        Element::_path.c_str()+1);
      ev.setProperty("statement",   _qstatement.c_str());
      i = 0;
      ev.setProperty("nrelations","0");
      for(gvector<gstring>::iterator iter = _qrelations.begin();
          iter != _qrelations.end();
          ++iter,++i)
        ev.setProperty(mkstring()<<"relation"<<i,*iter);
      ContainerHandler::handle(evt, "");
      return false;
      break;
    };
    default:
      return false;
    }
  };
  virtual bool ismaster() const throw() { return Element::_parent->ismaster(); };
  virtual bool isslave() const throw() { return Element::_parent->isslave(); };
  virtual std::string description() const throw() { return "FIXME"; };
  virtual Fetch* fetch(apr_pool_t* pool, const char* path, const Url& replica,
                       apr_time_t ims, const char* arguments = 0,
		       const char* method = 0)
    throw()
  {
    if(ismaster()) {
      const char* s;
      string query = _qstatement.c_str();
      string::size_type qargidx;
      while((qargidx = query.find("?")) != string::npos) {
        if(arguments && *arguments) {
          s = arguments;
          while(*s && *s != '\n' && *s != '\t' && *s != '&')
            ++s;
          query.replace(qargidx,1,arguments,s-arguments);
          while(*s && (*s == '\n' || *s == '\t' || *s == '&'))
            ++s;
          arguments = s;
        } else {
          return new ExceptionFetch(HTTP_BAD_REQUEST,
                                    "Missing argument to query");
        }
      }
      if(arguments && *arguments) {
        return new ExceptionFetch(HTTP_BAD_REQUEST,
                                  "Too many arguments to query");
      }
      return Element::_parent->fetch(pool, path, replica, ims, query.c_str());
    } else {
      return Element::_parent->fetch(pool, Element::_path.c_str(), replica,
                                     ims, arguments, method);
    }
  };
};

DatabaseHandler::DatabaseHandler(Handler* parent, apr_pool_t* p, Context* ctx,
                                 const Url& uri, const char* path,
                                 const char* upstream, const char *password)
  throw()
  : BaseHandler(parent,p,ctx,uri,path), _upstream(p, upstream)
{
  if(!strcmp(_upstream.scheme(),"http") ||
     !strcmp(_upstream.scheme(),"https"))
    _peers.add(ctx, p, Contact(p, Peer::ORIGIN, upstream, password));
}

DatabaseHandler::~DatabaseHandler() throw()
{
}

bool
DatabaseHandler::initialize(apr_pool_t* p, Context* ctx,
			    HandlerConfiguration* cfg)
  throw(SetupException)
{
  if(BaseHandler::initialize(p, ctx, cfg))
    return true;
  DIAG(ctx,(MONITOR(info)),("Initializing database section url=%s\n",_uri(p)));
  return false;
}

Element*
DatabaseHandler::newtarget(apr_pool_t* pool, const Url& url, Context* ctx,
                           const char* path, const char* policy) throw()
{
  Url targeturl(pool, url,path); 
  return new(rmmmemory::shm()) QueryClass(this, pool, ctx, targeturl, path,
                                          policy);
}

bool
DatabaseHandler::handle(GlobuleEvent& evt, const std::string& remainder) throw()
{
  _lock.lock_shared();
  Handler* target = gettarget(evt.pool, evt.context, remainder, evt.target());
  switch(evt.type()) {
  case GlobuleEvent::REDIRECT_EVT: {
    RedirectEvent& ev = (RedirectEvent&) evt;
    _lock.unlock();
    ev.setStatus(HTTP_OK);
    ev.getRequest()->filename = "Boe";  // Humor ap_directory_walk()
    return true;
  }
  case GlobuleEvent::INVALID_EVT: {
    HttpMetaEvent& ev = (HttpMetaEvent&) evt;
    if(target) {
      Lock* lock = target->getlock();
      if(ev.getRequest()) {
        lock->lock_exclusively();
        _lock.unlock();
        target->handle(evt);
        lock->unlock();
      } else {
        _lock.unlock();
        target->handle(evt);
      }
    } else {
      _lock.unlock();
      ev.setStatus(HTTP_GONE);
    }
    return true;
  }
  case GlobuleEvent::REQUEST_EVT: {
    Lock* lock = (target ? target->getlock() : 0);
    if(!target) {
      _lock.unlock();
      _lock.lock_exclusively();
      target = gettarget(evt.pool, evt.context, remainder, "GlobeCBPolicy");
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
  case GlobuleEvent::WRAPUP_EVT: {
    _lock.unlock();
    return true;
  }
  case GlobuleEvent::REPORT_EVT: {
    _lock.unlock();
    ReportEvent& ev = (ReportEvent&)evt;
    ReportEvent subevt("section", &ev);
    subevt.setProperty("uri",_uri(evt.pool));
    subevt.setProperty("site",siteaddress(evt.pool));
    subevt.setProperty("type","database");
    subevt.setProperty("path",_path);
    subevt.setProperty("servername",_servername);
    BaseHandler::handle(subevt, remainder);
  }
  default:
    _lock.unlock();
    return false;
  }
}

string
DatabaseHandler::description() const throw()
{
  return mkstring() << "DatabaseHandler(" << _uri.host() << ":" << _uri.port()
                    << _uri.path() << ")";
}

#ifdef HAVE_LIBMYSQLCLIENT
class DatabaseFetch
  : public Fetch
{
  friend class DatastreamFetch;
private:
  char* _errmessage;
  MYSQL*     _mysql;
  MYSQL_RES* _mysql_result;
  MYSQL_ROW  _mysql_row;
  int _nrows, _ncols, _rowidx, _colidx;
public:
  DatabaseFetch(apr_pool_t* pool, const char* host,
                const char* user, const char* pass, const char* dbase,
                const char* query)
    throw()
    : _errmessage(0), _mysql_result(0)
  {
    if(!user) user = "";
    if(!pass) pass = "";
    if(!host) host = "localhost";
    if(!dbase) {
      _errmessage = "No database specified";
      return;
    }
    if((_mysql = mysql_init(0))) {
#ifdef CLIENT_MULTI_STATEMENTS
      if(mysql_real_connect(_mysql,host,user,pass,dbase,0,0,
                                                   CLIENT_MULTI_STATEMENTS))
#else
      if(mysql_real_connect(_mysql,host,user,pass,dbase,0,0,0))
#endif
      {
        if(!mysql_query(_mysql, query)) {
          if((_mysql_result = mysql_store_result(_mysql))) {
            _nrows = mysql_num_rows(_mysql_result);
            _ncols = mysql_num_fields(_mysql_result);
          } else {
            _nrows = _ncols = 0;
          }
          _rowidx = -1;
          _colidx = _ncols;
        } else
          _errmessage = "Query failed";
      } else
        _errmessage = "Cannot connect to globecbc database";
    } else
      _errmessage = "Cannot initialize MySQL interface";
  };
  virtual ~DatabaseFetch() throw() {
    if(_mysql_result)
      mysql_free_result(_mysql_result);
    if(_mysql)
      mysql_close(_mysql);
  };
  virtual const std::string getMessage() throw() {
    if(_errmessage) {
      if(mysql_error(_mysql))
        return mkstring::format("%s: %s (%d)", _errmessage,
                                mysql_error(_mysql), mysql_errno(_mysql));
      else
        return _errmessage;
    } else
      return "";
  };
  virtual int getStatus() throw() {
    if(_errmessage)
      return globule::netw::HttpResponse::HTTPRES_INTERNAL_SERVER_ERROR;
    return 0;
  };
  virtual apr_ssize_t getContentLength() throw() {
    return -1;
  };
  virtual const std::string getHeader(const std::string &key) throw() {
    if(key == "X-Globule-ColumnCount")
      return mkstring() << _ncols;
    else if(key == "X-Globule-RowCount")
      return mkstring() << _nrows;
    else
      return "";
  };
  virtual void getHeaders(apr_table_t* table) throw() {
    const char* headers[] = { "X-Globule-ColumnCount","X-Globule-RowCount",0 };
    for(int i=0; headers[i]; i++)
      apr_table_set(table, headers[i], getHeader(headers[i]).c_str());
  };
  virtual apr_ssize_t read(char** buf) throw() {
    if(_colidx >= _ncols) {
      if(_rowidx+1 >= _nrows)
        return -1;
      ++_rowidx;
      _colidx = 0;
      _mysql_row = mysql_fetch_row(_mysql_result);
    }
    if((*buf = _mysql_row[_colidx++])) {
      return strlen(*buf);
    } else {
      return 0;
    }
  };
  virtual std::string encode(const char* buf, int nbytes = -1) throw() {
    return ::encode(_colidx, _ncols, buf, nbytes);
  };
};
#endif

class DatastreamFetch
  : public Fetch
{
private:
  Fetch* _fetch;
  int _rowidx, _colidx, _nrows, _ncols;
  vector<string> _fields;
public:
  DatastreamFetch(apr_pool_t* pool, Fetch* ftch)
    throw()
    : _fetch(ftch), _rowidx(0), _colidx(0)
  {
    char* buf;
    apr_ssize_t nbytes;
    int i, col=0, row=0;
    stringbuf field;
    if(sscanf(_fetch->getHeader("X-Globule-RowCount").c_str(),"%d",
                                                             &_nrows) == 1 &&
       sscanf(_fetch->getHeader("X-Globule-ColumnCount").c_str(),"%d",
                                                             &_ncols) == 1) {
      _fields.resize(_ncols * _nrows);
      while((nbytes = _fetch->read(&buf)) >= 0) {
        for(i=0; i<nbytes; i++)
          switch(buf[i]) {
          case '\t':
          case '\n':
            ap_assert(col + row * _ncols < _ncols * _nrows);
            _fields[col + row * _ncols] = field.str();
            field.str("");
            if(++col == _ncols) {
              col = 0;
              ++row;
            }
            break;
          default:
            field.sputc(buf[i]);
          }
      }
    } else {
      delete _fetch;
      _fetch = 0;
    }
  };
  virtual ~DatastreamFetch() throw() {
    if(_fetch)
      delete _fetch;
  };
  virtual const std::string getMessage() throw() {
    if(_fetch)
      return _fetch->getMessage();
    else
      return "globule protocol error";
  };
  virtual int getStatus() throw() {
    if(_fetch)
      return _fetch->getStatus();
    else
      return HTTPRES_BAD_GATEWAY;
  };
  virtual apr_ssize_t getContentLength() throw() {
    if(_fetch)
      return _fetch->getContentLength();
    else
      return -1;
  };
  virtual const std::string getHeader(const std::string &key) throw() {
    if(key == "X-Globule-ColumnCount")
      return mkstring() << _ncols;
    else if(key == "X-Globule-RowCount")
      return mkstring() << _nrows;
    else if(_fetch)
      return _fetch->getHeader(key);
    else
      return "";
  };
  virtual void getHeaders(apr_table_t* table) throw() {
    const char* headers[] = { "X-Globule-ColumnCount","X-Globule-RowCount",0 };
    for(int i=0; headers[i]; i++)
      apr_table_set(table, headers[i], getHeader(headers[i]).c_str());
  };
  virtual apr_ssize_t read(char** buf) throw() {
    if(_colidx >= _ncols) {
      ++_rowidx;
      _colidx = 0;
    }
    if(_rowidx < _nrows) {
      // FIXME const_cast unknown to be safe
      *buf = const_cast<char*>(_fields[_colidx + _rowidx * _ncols].c_str());
      ++_colidx;
      return strlen(*buf);
    } else
      return -1;
  };
  virtual std::string encode(const char* buf, int nbytes = -1) throw() {
    return ::encode(_colidx, _ncols, buf, nbytes);
  };
};

bool
DatabaseHandler::ismaster() const throw()
{
  return !isslave();
}

bool
DatabaseHandler::isslave() const throw()
{
  return !strcmp(_upstream.scheme(),"http") ||
         !strcmp(_upstream.scheme(),"https");
}

Fetch*
DatabaseHandler::fetch(apr_pool_t* p, const char* path,
                       const Url& replicaUrl, apr_time_t ims,
                       const char *arguments, const char* method)
  throw()
{
  Fetch* f;
  if(isslave()) {
    f = BaseHandler::fetch(p,path,replicaUrl,ims,arguments,method);
    if(f->getStatus())
      return f;
    return new DatastreamFetch(p, f);
#ifdef HAVE_LIBMYSQLCLIENT
  } else if(!strcmp(_upstream.scheme(),"mysql")) {
    const char* dbname = _upstream.path();
    while(*dbname && *dbname == '/')
      ++dbname;
    return new DatabaseFetch(p, _upstream.host(), _upstream.user(),
                             _upstream.pass(), dbname, arguments);
  } else {
#endif
    return new ExceptionFetch(Fetch::HTTPRES_INTERNAL_SERVER_ERROR,
                   mkstring::format("unsupported database connector scheme %s",
                                    _upstream.scheme()));
  }
}
