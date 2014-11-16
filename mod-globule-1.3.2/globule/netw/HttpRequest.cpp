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
#include <httpd.h>

#include <apr.h>
#include <apr_lib.h>
#include <apr_base64.h>
#include <apr_strings.h>
#include <apr_version.h>

#include <string>
#include <vector>
#include <iostream>
#include <sstream>
#include <fstream>
#include <string.h>
#include "utilities.h"
#include "netw/Url.hpp"
#include "netw/HttpRequest.hpp"
#include "Constants.hpp"

using namespace std;

namespace globule { namespace netw {

/****************************************************************************/

class HttpNonBlockingConnection : public HttpConnection
{
  friend class HttpRequest;
  friend class HttpNonBlockingManager;
  struct data {
    apr_size_t size;
    apr_size_t offset;
    char* buffer;
  };
  HttpNonBlockingManager* _manager;
  apr_pool_t*             _pool;
  bool                    _fully;
  apr_size_t              _maxbytes;
  list<struct data>       _outputlist;
  list<struct data>       _inputlist;
protected:
  HttpNonBlockingConnection(apr_pool_t*, HttpRequest*, HttpNonBlockingManager*,
                            apr_size_t maxbytesresponse, bool fully)
    throw(HttpException);
  virtual ~HttpNonBlockingConnection() throw();
  virtual void doOpen(apr_pool_t*, Url &url, apr_socket_t* sock,
                      std::string request, int timeout=0) throw(HttpException);
  virtual void doFlush() throw(HttpException);
  virtual void doWrite(const char *, apr_size_t) throw(HttpException);
  virtual apr_status_t doRead(char *, apr_size_t*) throw(HttpException);
  virtual void doClose() throw();
  virtual HttpResponse *response() throw(HttpException);
};

HttpNonBlockingConnection::HttpNonBlockingConnection(apr_pool_t* pool,
                                HttpRequest *req, HttpNonBlockingManager *mngr,
                                apr_size_t maxbytesresponse, bool fully)
  throw(HttpException)
  : HttpConnection(pool, req, 0, 0), _manager(mngr), _pool(pool),
    _fully(fully), _maxbytes(maxbytesresponse)
{
  apr_pool_create(&_pool, pool);
  req->setKeepAlive(false);
  _manager->registerConnection(pool, this);
}

HttpNonBlockingConnection::~HttpNonBlockingConnection()
  throw()
{
  _manager->unregisterConnection(this);
  apr_pool_destroy(_pool);
}

void
HttpNonBlockingConnection::doOpen(apr_pool_t* pool, Url &url,
                                  apr_socket_t* sock, string request,
                                  int timeout)
  throw(HttpException)
{
  apr_status_t status;
  apr_sockaddr_t *addr;

  status = url.address(pool, &addr);
  if(status != APR_SUCCESS)
    throw HttpException("Host not found");

  status = apr_socket_create(&_socket, APR_INET, SOCK_STREAM,
#if (APR_MAJOR_VERSION > 0)
                             APR_PROTO_TCP,
#endif
                             pool);
  if(status != APR_SUCCESS)
    throw HttpException("Cannot create socket");

  if(timeout > 0) {
    status = apr_socket_timeout_set(_socket, timeout * (apr_time_t)1000000);
    if(status != APR_SUCCESS)
      throw HttpException("error setting timeout");
  }

  status = apr_socket_connect(_socket, addr);
  if(status != APR_SUCCESS)
    throw HttpException("cannot connect");

  doWrite(request.c_str(), request.size());
}

void
HttpNonBlockingConnection::doWrite(const char* buf, apr_size_t size)
  throw(HttpException)
{
  struct data store;
  store.size   = size;
  store.offset = 0;
  store.buffer = (char*) apr_palloc(_pool, size);
  memcpy(store.buffer, buf, size);
  _outputlist.push_back(store);
}

apr_status_t
HttpNonBlockingConnection::doRead(char* buf, apr_size_t* size)
  throw(HttpException)
{
  list<struct data>::iterator d = _inputlist.begin();
  if(d == _inputlist.end())
    return HttpConnection::doRead(buf, size);
  if(d->size > 0) {
    if(*size > d->size - d->offset)
      *size = d->size;
    memcpy(buf, &d->buffer[d->offset], *size);
    d->offset += *size;
    if(d->size - d->offset == 0) {
      _inputlist.erase(d);
      d = _inputlist.begin();
    }
  }
  if(d->size == 0)
    return APR_EOF;
  else
    return APR_SUCCESS;
}

void
HttpNonBlockingConnection::doFlush() throw(HttpException)
{
  struct data store;
  store.size   = 0;
  store.offset = 0;
  store.buffer = NULL;
  _outputlist.push_back(store);
}

void
HttpNonBlockingConnection::doClose() throw()
{
  struct data store;
  doFlush();
  store.size   = 0;
  store.offset = 0;
  store.buffer = NULL;
  _inputlist.push_back(store);
}

HttpResponse *
HttpNonBlockingConnection::response() throw(HttpException)
{
  if(!res) {
    int lastch = 0; /* last character (except cr are skipped) or \0 if non */
    apr_size_t bytessofar = 0;
    /* This implementation could be a little more efficient, by remembering
     * the last crlf seen so far and restarting the procedure looking for 
     * a double crlf and/or recording the bytes so far and only incrementing
     * it with the last read blocks.  However C++ STL make it hard to
     * preserve state of there we where in the list.
     */
      for(list<struct data>::const_iterator iter=_inputlist.begin();
          iter != _inputlist.end();
          ++iter)
        if(iter->size > 0) {
          if(_fully) {
            for(unsigned int i=0; i<iter->size; i++)
              if(iter->buffer[i] == '\n' && lastch == '\n')
                return (res = new HttpResponse(this));
              else if(iter->buffer[i] != '\r')
                lastch = iter->buffer[i];
          }
          bytessofar += iter->size;
        } else
          return (res = new HttpResponse(this));
      if(bytessofar >= _maxbytes) {
        doClose();
        return (res = new HttpResponse(this));
      }
      return 0;
  } else
    return res;
}

/****************************************************************************/

HttpRequest::HttpRequest(apr_pool_t* pool, const Url& u)
  throw()
  : url(pool, u), method("GET"), state(prepare), _keepalive(false), con(NULL),
    timeout(0)
{
  initialize();
}
HttpRequest::HttpRequest(apr_pool_t *pool, const string& u)
  throw()
  : url(pool, u), method("GET"), state(prepare), _keepalive(false), con(NULL),
    timeout(0)
{
  initialize();
}
HttpRequest::HttpRequest(apr_pool_t *pool, const char *u)
  throw()
  : url(pool, u), method("GET"), state(prepare), _keepalive(false), con(NULL),
    timeout(0)
{
  initialize();
}

void
HttpRequest::initialize() throw()
{
  if(url.host())
    if(url.port() > 0) {
      ostringstream ost;
      ost << "Host: " << url.host() << ":" << url.port();
      setHeader(ost.str());
    } else
      setHeader("Host", url.host());
  string useragent = mkstring() << PACKAGE << "/" << VERSION;
  setHeader("User-Agent", useragent);
}

HttpRequest::~HttpRequest() throw()
{
  if(con)
    delete con;
}

void
HttpRequest::setMethod(string methd) throw()
{
  method = methd;
}

void
HttpRequest::setAuthorization(string& user, string& pass) throw()
{
  username = user;
  password = pass;
}

void
HttpRequest::setAuthorization(const char* user, const char* pass) throw()
{
  username = (user ? user : "");
  password = (pass ? pass : "");
}

void
HttpRequest::setKeepAlive(bool keepalive) throw()
{
  _keepalive = keepalive;
}

void
HttpRequest::setKeepAlive(apr_size_t contentlength) throw()
{
  _keepalive = true;
  setHeader("Content-Length", (apr_int32_t)contentlength);
}

void
HttpRequest::setIfModifiedSince(apr_time_t ims) throw()
{
  setHeader("If-Modified-Since", ims);
}

void
HttpRequest::setConnectTimeout(int seconds) throw()
{
  timeout = seconds;
}

void
HttpRequest::setHeader(const string hdr) throw()
{
  headers.push_back(hdr);
}

void
HttpRequest::setHeader(string key, string val) throw()
{
  setHeader(key + ": " + val);
}

void
HttpRequest::setHeader(string key, apr_int32_t val) throw()
{
  ostringstream ost;
  ost << key << ": " << val;
  setHeader(ost.str());
}

void
HttpRequest::setHeader(string key, apr_time_t t) throw()
{
  char timestring[APR_RFC822_DATE_LEN];
  apr_rfc822_date(timestring, t);
  setHeader(key, timestring);
}

HttpConnection *
HttpRequest::connect(apr_pool_t* pool) throw(HttpException)
{
  switch(state) {
  case prepare:
    state = connected;
    if(_keepalive) {
      HttpConnectionPool* conpool;
      apr_socket_t* sock = HttpConnectionPool::lookup(pool, url, conpool);
      if(sock)
        return (con = new HttpConnection(pool, this, conpool, sock));
      else
        return (con = new HttpConnection(pool, this, conpool));
    }
    return (con = new HttpConnection(pool, this));
  case connected:
    return con;
  case closed:
    return 0;
  }
  return 0;
}

HttpConnection *
HttpRequest::connect(apr_pool_t* pool, HttpNonBlockingManager* manager,
                     apr_size_t maxbytesresponse, bool fully)
  throw(HttpException)
{
  switch(state) {
  case prepare:
    state = connected;
    setKeepAlive(false);
    return (con = new HttpNonBlockingConnection(pool, this, manager,
                                                maxbytesresponse, fully));
  case connected:
    return con;
  case closed:
    return 0;
  }
  return 0;
}

HttpResponse*
HttpRequest::response(apr_pool_t* pool) throw(HttpException)
{
  HttpConnection* con = connect(pool);
  return (con ? con->response() : 0);
}

HttpRequest*
HttpConnection::request() throw()
{
  return req;
}

int
HttpRequest::getStatus(apr_pool_t* pool) throw(HttpException)
{
  HttpResponse* res = response(pool);
  return (res ? res->getStatus() : HttpResponse::HTTPRES_FAILURE);
}

/****************************************************************************/

map<apr_pool_t*,HttpConnectionPool> HttpConnectionPool::_buckets;

extern "C" {
static apr_status_t cleanupfunc(void* pool) {
  HttpConnectionPool::poolcleanup((apr_pool_t*)pool);
  return APR_SUCCESS;
}
}

void
HttpConnectionPool::poolcleanup(apr_pool_t* gonepool)
{
  map<apr_pool_t*,HttpConnectionPool>::iterator iter = _buckets.find(gonepool);
  if(iter != _buckets.end())
    _buckets.erase(iter);
}

HttpConnectionPool::HttpConnectionPool(apr_pool_t* pool)
{
  apr_pool_cleanup_register(pool, pool, cleanupfunc, apr_pool_cleanup_null);
}

HttpConnectionPool::~HttpConnectionPool()
{
  for(multimap<string,apr_socket_t*>::iterator iter=_cache.begin();
      iter != _cache.end();
      ++iter)
    {
      apr_socket_shutdown(iter->second, APR_SHUTDOWN_READWRITE);
      apr_socket_close(iter->second);
    }
}

apr_socket_t*
HttpConnectionPool::lookup(apr_pool_t* pool, const Url& u,
                           HttpConnectionPool*& conpoolref)
{
  string search = mkstring()<<u.scheme()<<"://"<<u.host()<<u.port()<<"/";
  apr_socket_t* sock = 0;
  map<apr_pool_t*,HttpConnectionPool>::iterator iter1 = _buckets.find(pool);
  if(iter1 == _buckets.end()) {
    _buckets.insert(make_pair(pool, HttpConnectionPool(pool)));
    iter1 = _buckets.find(pool);
  }
  conpoolref = &iter1->second;
  multimap<string,apr_socket_t*>::iterator iter2
                                    = conpoolref->_cache.find(search);
  if(iter2 != conpoolref->_cache.end()) {
    sock = iter2->second;
    conpoolref->_cache.erase(iter2);
  }
  return sock;
}

void
HttpConnectionPool::insert(const Url& u, apr_socket_t* sock)
{
  string search = mkstring()<<u.scheme()<<"://"<<u.host()<<u.port()<<"/";
  _cache.insert(make_pair(search, sock));
}

/****************************************************************************/

HttpConnection::HttpConnection(apr_pool_t* pool, HttpRequest *r,
                               HttpConnectionPool* conpool, apr_socket_t* sock)
  throw(HttpException)
  : req(r), res(NULL), _conpool(conpool), _socket(0), eoi(false),
    osb(NULL), ost(NULL), buffill(0), bufavail(0), bufindx(0),
    chunked(false), chunkedleft(0)
{
  ostringstream o;
  o << req->getMethod() << " " << req->url.pathquery(pool) << " HTTP/1.1"
    << CRLF;
  if(r->_keepalive)
    o << "Connection: keep-alive" << CRLF;
  else
    o << "Connection: close" << CRLF;
  if(r->username != "") {
    string s = mkstring() << r->username << ":" << r->password;
    apr_size_t encoded_len = apr_base64_encode_len(s.length());
    {
      char *userpass = (char *) apr_pcalloc(pool, encoded_len);
      apr_base64_encode(userpass, s.c_str(), s.length());
      o << "Authorization: Basic " << userpass << CRLF;
    }
  }
  char currenttime[APR_RFC822_DATE_LEN];
  apr_rfc822_date(currenttime, apr_time_now());
  o << "Date: " << currenttime << CRLF;
  for(vector<string>::iterator iter=req->headers.begin();
      iter!=req->headers.end();
      iter++)
  {
    o << *iter << CRLF;
  }
  o << CRLF;
  try {
    if(r->timeout > 0)
      doOpen(pool, req->url, sock, o.str(), r->timeout);
    else
      doOpen(pool, req->url, sock, o.str());
#ifdef NOTDEFINED
    // Disable the Nagle algorithm
    int just_say_no = 1;
    err = setsockopt(_fd, IPPROTO_TCP, TCP_NODELAY,
                          (char *) &just_say_no, sizeof(int));
#endif
  } catch(HttpException e) {
    doClose();
    throw e;
  }
}

HttpConnection::~HttpConnection() throw()
{
  req->state = HttpRequest::closed;
  if(res)
    delete res;
  if(req->_keepalive)
    _conpool->insert(req->url, _socket);
  else
    doClose();
  if(ost) {
    delete ost;
    delete osb;
  }
}

HttpResponse *
HttpConnection::response() throw(HttpException)
{
  if(!res) {
    /* Currently this implementation assumes a pure request-response
     * scenario, where the whole input is send first, before any input can
     * be received.  Therefor, the doFlush call is called here.  This doFlush
     * method does not do a regular flush() but shuts down the socket for
     * output, consequently also performing a flush().  This makes sure the
     * pipe on the other end gets a EOF.  For our usage, this is proper
     * behaviour and makes life a lot simpler to program, but it is probably
     * not generic enough for wide spread deployment.
     */
    doFlush();
    return (res = new HttpResponse(this));
  } else
    return res;
}

void
HttpConnection::doOpen(apr_pool_t* pool, Url &url, apr_socket_t* sock,
                       string request, int timeout) throw(HttpException)
{
  apr_status_t status;
  apr_sockaddr_t *addr;

  status = url.address(pool, &addr);
  if(status != APR_SUCCESS)
    throw HttpException("Host not found");
  
  if(!(_socket = sock)) {
    status = apr_socket_create(&_socket, APR_INET, SOCK_STREAM, 
#if (APR_MAJOR_VERSION > 0)
                               APR_PROTO_TCP,
#endif
                               pool);
    if(status != APR_SUCCESS)
      throw HttpException("Cannot create socket");
  }

  if(timeout > 0) {
    status = apr_socket_timeout_set(_socket, timeout * (apr_time_t)1000000);
    if(status != APR_SUCCESS)
      throw HttpException("error setting timeout");
  }
  
  if(!sock) {
    status = apr_socket_connect(_socket, addr);
    if(status != APR_SUCCESS) {
      throw HttpException("cannot connect");
    }
  }

  doWrite(request.c_str(), request.size());
}

void
HttpConnection::doWrite(const char *buf, apr_size_t size) throw(HttpException)
{
  apr_status_t status;
  apr_size_t len;
  while (size > 0) {
    len = size;
    status = apr_socket_send(_socket, buf, &len);
    size -= len;
    buf   = &buf[len];
    if(status != APR_SUCCESS)
      throw HttpException("write request failed");
  }
}

void
HttpConnection::write(apr_file_t* fd) throw(HttpException)
{
  char buf[10240];
  apr_size_t len;
  apr_status_t status;
  do {
    len = sizeof(buf);
    status = apr_file_read(fd, buf, &len);
    switch(status) {
    case APR_SUCCESS:
      doWrite(buf, len);
      break;
    case APR_EOF:
      ap_assert(len == 0);
      break;
    default:
      throw HttpException();
    }
  } while(len != 0);
}

void
HttpConnection::write(apr_file_t* fd, apr_size_t size) throw(HttpException)
{
  char buf[10240];
  apr_size_t len;
  apr_status_t status;
  do {
    len = (size > sizeof(buf) ? sizeof(buf) : size);
    status = apr_file_read(fd, buf, &len);
    if(status != APR_SUCCESS && !APR_STATUS_IS_EOF(status)) {
      throw HttpException();
    } else {
      doWrite(buf, len);
      size -= len;
    }
  } while(size > 0 && len != 0);
  if(size != 0)
    throw HttpException("EOF reached");
}

void
HttpConnection::rputs(const char* s) throw(HttpException)
{
  doWrite(s, strlen(s));
}

void
HttpConnection::rprintf(const char* fmt,...) throw(HttpException)
{
  va_list ap;
  va_start(ap, fmt);
  int sz = apr_vsnprintf(NULL, 0, fmt, ap);
  std::vector<char> v(sz+1, '\0');
  apr_vsnprintf(&v[0], sz+1, fmt, ap);
  doWrite(&v[0], sz+1);
  va_end(ap);
}

void
HttpConnection::doFlush() throw(HttpException)
{
  apr_status_t status;
  if(!req->_keepalive) {
    status = apr_socket_shutdown(_socket, APR_SHUTDOWN_WRITE);
    if(status != APR_SUCCESS)
      throw HttpException("Partial socket shutdown failed");
  }
}

void
HttpConnection::doChunking() throw()
{
  chunked = true;
  bufavail    = 0;
  chunkedleft = 0;
}

/* returns whether we have reached end-of-file, even though there may still
 * be data left in the buffer (bufavail > 0).
 */
bool
HttpConnection::doRead(bool fully) throw(HttpException)
{
  int i;
  apr_size_t len;
  apr_ssize_t bufleft = 0;
  apr_status_t status;
  
  if(eoi)
    return true;
  if(!chunked || bufavail==0 || chunkedleft>0) {
    do {
      if(buffill == 0 && bufindx > 0)
        bufindx = 0;
        
      bufleft = sizeof(buffer) - buffill - bufindx;
      if(chunked) {
        if(chunkedleft==0) {
          ap_assert(bufavail==0);
          for(i=0; i<buffill; i++)
            if(buffer[bufindx+i]!='\n' && buffer[bufindx+i]!='\r')
              break;
          bufindx += i;
          buffill -= i;
          for(i=0; i<buffill; i++)
            if(buffer[bufindx+i] >= '0' && buffer[bufindx+i] <= '9') {
              chunkedleft = (chunkedleft<<4) | (buffer[bufindx+i]-'0');
              ap_assert(chunkedleft >= 0);
            } else if(buffer[bufindx+i] >= 'a' && buffer[bufindx+i] <= 'f') {
              chunkedleft = (chunkedleft<<4) | (buffer[bufindx+i]-('a'-0xa));
              ap_assert(chunkedleft >= 0);
            } else if(buffer[bufindx+i] >= 'A' && buffer[bufindx+i] <= 'F') {
              chunkedleft = (chunkedleft<<4) | (buffer[bufindx+i]-('A'-0xa));
              ap_assert(chunkedleft >= 0);
            } else if(buffer[bufindx+i] == '\n')
              break;
            else if(buffer[bufindx+i] != '\r') {
              throw HttpException("Bad character %d in chunk size ");
              //return (eoi = true);
            }
            if(i < buffill) {
              ++i;
              bufindx += i;
              buffill -= i;
              bufavail = buffill;
              if(bufavail > chunkedleft)
                bufavail = chunkedleft;
              if(chunkedleft == 0)
                return (eoi = true);
              chunkedleft -= bufavail;
              if(bufavail > 0)
                return false;
            } else
              chunkedleft = 0;
        } else {
          if(bufleft > chunkedleft)
            bufleft = chunkedleft;
        }
      }
      if((bufindx > 32 && bufleft < 32) || bufleft == 0) {
        if(bufindx > 0) {
          memmove(&buffer[0], &buffer[bufindx], buffill);
          bufleft += bufindx;
          bufindx = 0;
        }
        if(bufleft == 0)
          throw HttpException("No more buffer space");
      }
      len = bufleft;
      status = doRead(&buffer[bufindx+buffill], &len);
      bufleft -= len;
      if(status != APR_SUCCESS && !APR_STATUS_IS_EOF(status)) {
        eoi = true;
        throw HttpException("Read request failed");
      } else if(len == 0) {
        return (eoi = true);
      } else {
        buffill += len;
        if(chunked) {
          if(chunkedleft > 0) {
            bufavail += len;
            chunkedleft -= len;
          }
        } else
          bufavail += len;
      }
    } while(chunked ? bufavail==0 || (fully ? bufleft>0 && chunkedleft>0 
                                      : len <= 0)
            : (fully ? bufleft>0 : len <= 0));
  }
  return false;
}

apr_status_t
HttpConnection::doRead(char *buf, apr_size_t* size) throw(HttpException)
{
  return apr_socket_recv(_socket, buf, size);
}

void
HttpConnection::doClose() throw()
{
  apr_status_t status;
  if(!_socket)
    return;
    
  status = apr_socket_shutdown(_socket, APR_SHUTDOWN_READWRITE);
  status = apr_socket_close(_socket);
  _socket = 0;
}

/****************************************************************************/

void
HttpResponse::read(apr_file_t* fd) throw(HttpException)
{
  apr_size_t l;
  apr_ssize_t len;
  len = getContentLength();
  while(len != 0) {
    if(con->bufavail > 0) {
      do {
        l = (len > con->bufavail || len < 0 ? con->bufavail : len);
        status = apr_file_write(fd, &con->buffer[con->bufindx], &l);
        len          -= l;
        con->buffill  -= l;
        con->bufavail -= l;
      } while(con->bufavail > 0 && len != 0);
    } else
      if(con->doRead())
        len = con->bufavail;
  }
}

apr_ssize_t
HttpResponse::read(char** bufptr) throw(HttpException)
{
  apr_size_t len;
  bool end = false;
  for(;;) {
    if(con->bufavail > 0) {
      len = con->bufavail;
      *bufptr = &con->buffer[con->bufindx];
      con->buffill  -= len;
      con->bufavail -= len;
      con->bufindx  += len;
      return len;
    }
    if(end)
      return -1;
    end = con->doRead();
  }
  return 0;
}

const char*
HttpConnection::getline() throw(HttpException)
{
  int i;
  bool end = false;
  for(;;) {
    for(i=0; i<bufavail; i++)
      if(buffer[bufindx+i]=='\n') {
        ++i;
        buffill  -= i;
        bufavail -= i;
        bufindx  += i;
        buffer[bufindx-1] = '\0';
        if(bufindx>1 && buffer[bufindx-2] == '\r')
          buffer[bufindx-2] = '\0';
        return &buffer[bufindx-i];
      }
    if(end)
      return 0;
    end = doRead(false);
  }
}

const char*
HttpConnection::getline(string& line) throw(HttpException)
{
  int i;
  bool end = false;
  line.clear();
  for(;;) {
    for(i=0; i<bufavail; i++)
      if(buffer[bufindx+i]=='\n') {
        ++i;
        buffill  -= i;
        bufindx  += i;
        bufavail -= i;
        buffer[bufindx-1] = '\0';
        if(bufindx>1 && buffer[bufindx-2] == '\r')
          buffer[bufindx-2] = '\0';
        line += &buffer[bufindx-i];
        return line.c_str();
      }
    if(bufavail > 0) {
      line.append(&buffer[bufindx], bufavail);
      buffill  -= bufavail;
      bufindx  += bufavail;
      bufavail -= bufavail;
    }
    if(end)
      return 0;
    end = doRead(false);
  }
}

/****************************************************************************/

HttpResponse::HttpResponse(HttpConnection *c)
  throw(HttpException)
  : con(c), isb(NULL), ist(NULL)
{
  bool chunked = false;
  int major, minor, pos;
  const char *line = con->getline();
  if(!line || sscanf(line, "HTTP/%d.%d %d %n",
                     &major, &minor, &status, &pos) < 3) {
    status = HTTPRES_FAILURE;
  } else {
    message = &line[pos];
    while((line = con->getline()) && *line != '\0') {
      const char *s1 = line;
      const char *s2 = strchr(s1, ':');
      if(s2 == NULL)
        s2 = &s1[strlen(line)];
      string key = string(line).substr(0, s2-s1);
      if(*s2 == ':')
        ++s2;
      while(*s2 && apr_isspace(*s2))
        ++s2;
      string val = string(line).substr(s2-s1);
      headers[key] = val;
      if(key == "Transfer-Encoding" && val == "chunked")
        chunked = true;
      if(key == "Connection" && val == "close")
        con->req->setKeepAlive(false);
    }
    if(chunked)
      c->doChunking();
  }
}

HttpResponse::~HttpResponse() throw()
{
  if(ist) {
    delete ist;
    delete isb;
  }
}

const string
HttpResponse::getHeader(const string &key) throw()
{
  return headers[key];
}

void
HttpResponse::setHeader(const char* key, const char* val) throw()
{
  headers[key] = val;
}

void
HttpResponse::setHeader(const string& key, const string& val) throw()
{
  headers[key] = val;
}

void
HttpResponse::setHeader(const char* key, const string& val) throw()
{
  headers[key] = val;
}

void
HttpResponse::getHeaders(apr_table_t* table) throw()
{
  for(map<string,string>::iterator iter=headers.begin();
      iter!=headers.end();
      iter++)
    {
      apr_table_set(table, iter->first.c_str(), iter->second.c_str());
    }
}

apr_ssize_t
HttpResponse::getContentLength() throw()
{
  string lengthHeader;
  char *s;
  lengthHeader = getHeader("Content-Length");
  apr_ssize_t len = strtol(lengthHeader.c_str(), &s, 10);

  if(s == lengthHeader)
    return -1;
  else
    return len;
}

istream *
HttpResponse::getInputStream() throw()
{
  if(ist == NULL) {
    isb = new HttpInputStream(con);
    ist = new istream(isb);
  }
  return ist;
}

HttpRequest*
HttpResponse::request() throw()
{
  return con->request();
}

/****************************************************************************/

HttpInputStream::HttpInputStream(HttpConnection *c) throw()
  : conn(c)
{
}

streamsize
HttpInputStream::xsgetn(char *dest, streamsize n) throw(HttpException)
{
  int count = 0, size;
  bool eof = false;
  while(n > 0 && !eof) {
    if(conn->bufavail == 0)
      eof = ! conn->doRead();
    size = (conn->bufavail > n ? n : conn->bufavail);
    memcpy(&dest[count], &(conn->buffer[conn->bufindx]), size);
    conn->bufavail -= size;
    conn->buffill  -= size;
    conn->bufindx  += size;
    n     -= size;
    count += size;
  }
  return count;
}

/****************************************************************************/

HttpException::HttpException(apr_status_t status) throw()
  : _msg(""), _err(status)
{
}

HttpException::HttpException(string message, apr_status_t status) throw()
  : _msg(message), _err(status)
{
}

string
HttpException::getReason() throw()
{
  if(_err != APR_SUCCESS) {
    ostringstream ost;
    char desc[256];
    apr_strerror(_err, desc, sizeof(desc));
    if(*desc)
      ost << _msg << ": " << strerror(_err) << " (" << _err << ")";
    else
      ost << _msg << ": " << _err;
    return ost.str();
  } else
    return _msg;
}

/****************************************************************************/

HttpNonBlockingManager::HttpNonBlockingManager(apr_pool_t* pool)
  throw()
  : _pool(pool), _pollset(0)
{
  // defer creating pollset until we know the number of pollfds.
}

HttpNonBlockingManager::~HttpNonBlockingManager()
  throw()
{
  if(_pollset)
    apr_pollset_destroy(_pollset);
}

void
HttpNonBlockingManager::registerConnection(apr_pool_t* pool,
                                           HttpNonBlockingConnection* con)
  throw()
{
  apr_pollfd_t pollfd;
  _connections.push_back(con);
  pollfd.p           = pool;
  pollfd.desc_type   = APR_POLL_SOCKET;
  pollfd.reqevents   = APR_POLLOUT|APR_POLLERR|APR_POLLHUP|APR_POLLNVAL;
  pollfd.desc.s      = con->_socket;
  pollfd.client_data = 0; // unused
  _pollfds.push_back(pollfd);
  if(_pollset) {
    apr_pollset_destroy(_pollset);
    _pollset = 0;
  }
}

void
HttpNonBlockingManager::unregisterConnection(HttpNonBlockingConnection* con)
  throw()
{
  for(list<HttpNonBlockingConnection*>::iterator found = _connections.begin();
      found != _connections.end();
      ++found)
    if(*found == con) {
      for(vector<apr_pollfd_t>::iterator iter = _pollfds.begin();
          iter != _pollfds.end();
          ++iter)
        {
          if(iter->desc.s == (*found)->_socket) {
            apr_pollset_remove(_pollset, &*iter);
            _pollfds.erase(iter);
            break;
          }
        }
      _connections.erase(found);
      break;
    }
}

list<HttpResponse*>
HttpNonBlockingManager::poll(apr_interval_time_t timeout)
  throw()
{
  apr_status_t status;
  apr_int32_t nready;
  list<HttpResponse*> readyset;
  list<HttpNonBlockingConnection*> removelist;
  if(!_pollset) {
    status = apr_pollset_create(&_pollset, _connections.size(), _pool, 0);
    for(vector<apr_pollfd_t>::iterator iter = _pollfds.begin();
        iter != _pollfds.end();
        ++iter)
      apr_pollset_add(_pollset, &*iter);
  }
  for(vector<apr_pollfd_t>::iterator iter = _pollfds.begin();
      iter != _pollfds.end();
      ++iter)
    iter->rtnevents = 0;
  /* because _pollfds is a vector, we have the guarantee the elements
   * are in a normal array.
   */
  const apr_pollfd_t* polls = &_pollfds[0];
  status = apr_pollset_poll(_pollset, timeout, &nready, &polls);
  for(unsigned int i=0; i<_pollfds.size(); i++) {
    list<HttpNonBlockingConnection*>::iterator found = _connections.begin();
    while(found != _connections.end())
      if((*found)->_socket == polls[i].desc.s)
        break;
    if(found == _connections.end()) {
      // shouldn't happen, but just remove to avoid endless looping, jic.
      apr_pollset_remove(_pollset, &polls[i]);
    } else if(polls[i].rtnevents & APR_POLLNVAL ||
              polls[i].rtnevents & APR_POLLERR ||
              polls[i].rtnevents & APR_POLLHUP) {
      apr_socket_shutdown((*found)->_socket, APR_SHUTDOWN_READWRITE);
      apr_socket_close((*found)->_socket);
      (*found)->_socket = 0;
      (*found)->doClose();
      HttpResponse* res = (*found)->response();
      if(res)
        readyset.push_front(res);
      removelist.push_back(*found);
    } else if(polls[i].rtnevents & APR_POLLIN) {
      char buffer[65536]; // temporary storage for buffer
      struct HttpNonBlockingConnection::data d;
      apr_size_t size = sizeof(buffer);
      status = apr_socket_recv((*found)->_socket, buffer, &size);
      if(size > 0) {
        d.buffer = (char*) apr_palloc((*found)->_pool, size);
        d.size   = size;
        d.offset = 0;
        (*found)->_inputlist.push_back(d);
      }
      HttpResponse* res = (*found)->response();
      if(res)
        readyset.push_front(res);
      removelist.push_back(*found);
    } else if(polls[i].rtnevents & APR_POLLOUT) {
      list<struct HttpNonBlockingConnection::data>::iterator d =
        (*found)->_outputlist.begin();
      /* Note that the buffers aren't deallocated (nor by c++, not explicitly
       * here, they are allocated on the apr_pool, so are deallocated as the
       * pool gets destroyed.
       */
      if(d != (*found)->_outputlist.end()) {
        if(d->size == 0) {
          apr_socket_shutdown((*found)->_socket, APR_SHUTDOWN_WRITE);
          while(d != (*found)->_outputlist.end()) {
            (*found)->_outputlist.erase(d);
            d = (*found)->_outputlist.begin();
          }
        } else {
          apr_size_t len = d->size - d->offset;
          apr_socket_send((*found)->_socket, &d->buffer[d->offset], &len);
          if(len == d->size - d->offset) {
            (*found)->_outputlist.erase(d);
            d = (*found)->_outputlist.begin();
          } else
            d->offset += len;
        }
      }
      if(d == (*found)->_outputlist.end()) {
        // remove APR_POLLOUT from requested events
        for(vector<apr_pollfd_t>::iterator iter = _pollfds.begin();
            iter != _pollfds.end(); ++iter)
          if(iter->desc.s == polls[i].desc.s) {
            apr_pollset_remove(_pollset, &*iter);
            iter->reqevents &= ~APR_POLLOUT;
            apr_pollset_add(_pollset, &*iter);
            break;
          }
      }
    } else {
      abort();
    }
  }
  for(list<HttpNonBlockingConnection*>::iterator iter=removelist.begin();
      iter != removelist.end();
      ++iter)
    unregisterConnection(*iter);
  return readyset;
}

bool
HttpNonBlockingManager::empty()
  throw()
{
  return _connections.empty();
}

/****************************************************************************/

}; }; // namespace globule::netw
