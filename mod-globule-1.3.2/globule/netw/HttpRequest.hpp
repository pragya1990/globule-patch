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
#ifndef _HTTPREQUEST_HPP
#define _HTTPREQUEST_HPP

#include <string>
#include <vector>
#include <map>
#include <list>
#include <iostream>
#include <apr_date.h>
#include <apr_poll.h>
#include "netw/Url.hpp"

namespace globule { namespace netw {

class HttpRequest;
class HttpResponse;
class HttpConnection;
class HttpInputStream;

class HttpException
{
private:
  std::string _msg;
  int _err;
public:
  HttpException(apr_status_t status=APR_SUCCESS) throw();
  HttpException(std::string message, apr_status_t status=APR_SUCCESS) throw();
  inline std::string getMessage() throw() { return _msg; };
  std::string getReason() throw();
};

class HttpConnectionPool
{
private:
  static std::map<apr_pool_t*,HttpConnectionPool> _buckets;
  std::multimap<std::string,apr_socket_t*> _cache;
  HttpConnectionPool(apr_pool_t*);
public:
  ~HttpConnectionPool();
  static apr_socket_t* lookup(apr_pool_t*, const Url&, HttpConnectionPool*&);
  void insert(const Url&, apr_socket_t*);
  static void poolcleanup(apr_pool_t*);
};

class HttpConnection;
class HttpNonBlockingConnection;
class HttpNonBlockingManager
{
private:
  apr_pool_t* _pool;
  apr_pollset_t* _pollset;
  std::list<HttpNonBlockingConnection*> _connections;
  std::vector<apr_pollfd_t> _pollfds; // must be a vector
public:
  HttpNonBlockingManager(apr_pool_t*) throw();
  ~HttpNonBlockingManager() throw();
  void registerConnection(apr_pool_t*, HttpNonBlockingConnection*) throw();
  void unregisterConnection(HttpNonBlockingConnection*) throw();
  std::list<HttpResponse*> poll(apr_interval_time_t timeout) throw();
  bool empty() throw();
};

class HttpRequest
{
  friend class HttpConnection;
  friend class HttpResponse;
private:
  Url url;
  std::string method;
  std::string username, password;
  enum { prepare, connected, closed } state;
  bool _keepalive;
  HttpConnection *con;
  int timeout;
  std::vector<std::string> headers;
  void initialize() throw();
  HttpRequest operator=(HttpRequest& orig) throw();
protected:
public:
  HttpRequest(apr_pool_t* pool, const Url& url) throw();
  HttpRequest(apr_pool_t *pool, const std::string& urlstring) throw();
  HttpRequest(apr_pool_t *pool, const char *urlstring) throw();
  ~HttpRequest() throw();
  void setHeader(const std::string hdr) throw();
  void setHeader(std::string key, std::string val) throw();
  void setHeader(std::string key, apr_int32_t val) throw();
  void setHeader(std::string key, apr_time_t t) throw();
  void setMethod(std::string method) throw();
  void setAuthorization(std::string& username, std::string& password) throw();
  void setAuthorization(const char* username, const char* password) throw();
  void setConnectTimeout(int seconds) throw();
  void setKeepAlive(bool keepalive = true) throw();
  void setKeepAlive(apr_size_t contentlength) throw();
  void setIfModifiedSince(apr_time_t) throw();
  inline std::string getMethod() throw() { return method; };
  HttpConnection *connect(apr_pool_t*) throw(HttpException);
  HttpConnection *connect(apr_pool_t*, HttpNonBlockingManager*,
                          apr_size_t maxbytesresponse=4096, bool fully=true)
    throw(HttpException);
  inline bool isConnected() throw() { return state != prepare; };
  HttpResponse *response(apr_pool_t*) throw(HttpException);
  int getStatus(apr_pool_t*) throw(HttpException);
};

class HttpConnection
{
  friend class HttpRequest;
  friend class HttpResponse;
  friend class HttpInputStream;
  friend class HttpNonBlockingManager;
protected:
  HttpRequest *req;
  HttpResponse *res;
private:
  HttpConnectionPool* _conpool;
protected:
  apr_socket_t* _socket;
private:
  bool eoi;
  std::filebuf *osb;
  std::ostream *ost;
  apr_ssize_t buffill, bufavail, bufindx;
  bool chunked;
  apr_ssize_t chunkedleft;
  char buffer[10240];
protected:
  HttpConnection(apr_pool_t*, HttpRequest *req,
		 HttpConnectionPool* conpool = 0, apr_socket_t* sock = 0)
    throw(HttpException);
  virtual ~HttpConnection() throw();
  virtual void doOpen(apr_pool_t*, Url &url, apr_socket_t* sock,
                      std::string request, int timeout=0) throw(HttpException);
  virtual void doFlush() throw(HttpException);
  virtual void doClose() throw();
  virtual void doWrite(const char *buf, apr_size_t size) throw(HttpException);
  virtual apr_status_t doRead(char*, apr_size_t*) throw(HttpException);
  bool doRead(bool fully = true) throw(HttpException);
  void doChunking() throw();
  const char* getline() throw(HttpException);
  const char* getline(std::string& line) throw(HttpException);
public:
  virtual HttpResponse *response() throw(HttpException);
  HttpRequest* request() throw();
  void write(apr_file_t* fdes) throw(HttpException);
  void write(apr_file_t* fdes, apr_size_t size) throw(HttpException);
  void rputs(const char*) throw(HttpException);
  void rprintf(const char* fmt,...) throw(HttpException)
    __attribute__ ((__format__ (__printf__, 2, 3)));
};

class HttpResponseCodes
{
public:
  static const int HTTPRES_FAILURE                       = -1;
  static const int HTTPRES_CONTINUE                      = 100;
  static const int HTTPRES_SWITCHING_PROTOCOLS           = 101;
  static const int HTTPRES_PROCESSING                    = 102;
  static const int HTTPRES_OK                            = 200;
  static const int HTTPRES_CREATED                       = 201;
  static const int HTTPRES_ACCEPTED                      = 202;
  static const int HTTPRES_NON_AUTHORITATIVE             = 203;
  static const int HTTPRES_NO_CONTENT                    = 204;
  static const int HTTPRES_RESET_CONTENT                 = 205;
  static const int HTTPRES_PARTIAL_CONTENT               = 206;
  static const int HTTPRES_MULTI_STATUS                  = 207;
  static const int HTTPRES_MULTIPLE_CHOICES              = 300;
  static const int HTTPRES_MOVED_PERMANENTLY             = 301;
  static const int HTTPRES_MOVED_TEMPORARILY             = 302;
  static const int HTTPRES_SEE_OTHER                     = 303;
  static const int HTTPRES_NOT_MODIFIED                  = 304;
  static const int HTTPRES_USE_PROXY                     = 305;
  static const int HTTPRES_TEMPORARY_REDIRECT            = 307;
  static const int HTTPRES_BAD_REQUEST                   = 400;
  static const int HTTPRES_UNAUTHORIZED                  = 401;
  static const int HTTPRES_PAYMENT_REQUIRED              = 402;
  static const int HTTPRES_FORBIDDEN                     = 403;
  static const int HTTPRES_NOT_FOUND                     = 404;
  static const int HTTPRES_METHOD_NOT_ALLOWED            = 405;
  static const int HTTPRES_NOT_ACCEPTABLE                = 406;
  static const int HTTPRES_PROXY_AUTHENTICATION_REQUIRED = 407;
  static const int HTTPRES_REQUEST_TIME_OUT              = 408;
  static const int HTTPRES_CONFLICT                      = 409;
  static const int HTTPRES_GONE                          = 410;
  static const int HTTPRES_LENGTH_REQUIRED               = 411;
  static const int HTTPRES_PRECONDITION_FAILED           = 412;
  static const int HTTPRES_REQUEST_ENTITY_TOO_LARGE      = 413;
  static const int HTTPRES_REQUEST_URI_TOO_LARGE         = 414;
  static const int HTTPRES_UNSUPPORTED_MEDIA_TYPE        = 415;
  static const int HTTPRES_RANGE_NOT_SATISFIABLE         = 416;
  static const int HTTPRES_EXPECTATION_FAILED            = 417;
  static const int HTTPRES_UNPROCESSABLE_ENTITY          = 422;
  static const int HTTPRES_LOCKED                        = 423;
  static const int HTTPRES_FAILED_DEPENDENCY             = 424;
  static const int HTTPRES_UPGRADE_REQUIRED              = 426;
  static const int HTTPRES_INTERNAL_SERVER_ERROR         = 500;
  static const int HTTPRES_NOT_IMPLEMENTED               = 501;
  static const int HTTPRES_BAD_GATEWAY                   = 502;
  static const int HTTPRES_SERVICE_UNAVAILABLE           = 503;
  static const int HTTPRES_GATEWAY_TIME_OUT              = 504;
  static const int HTTPRES_VERSION_NOT_SUPPORTED         = 505;
  static const int HTTPRES_VARIANT_ALSO_VARIES           = 506;
  static const int HTTPRES_INSUFFICIENT_STORAGE          = 507;
  static const int HTTPRES_NOT_EXTENDED                  = 510;
};

class HttpResponse : public HttpResponseCodes
{
  friend class HttpConnection;
  friend class HttpNonBlockingConnection;
private:
  HttpConnection *con;
  HttpInputStream *isb;
  std::istream *ist;
  std::map<std::string,std::string> headers;
  int status;
  std::string message;
  HttpResponse(HttpConnection *con) throw(HttpException);
  ~HttpResponse() throw();
public:
  const std::string getMessage() throw() { return message; };
  int getStatus() throw() { return status; };
  apr_ssize_t getContentLength() throw(); // <0 for not indicated
  const std::string getHeader(const std::string &key) throw();
  void setHeader(const char* key, const char* val) throw();
  void setHeader(const std::string& key, const std::string& val) throw();
  void setHeader(const char* key, const std::string& val) throw();
  const apr_time_t getDateHeader(const std::string &key) throw() {
    return apr_date_parse_http(getHeader(key).c_str());
  };
  std::istream *getInputStream() throw();
  void read(apr_file_t* fd) throw(HttpException);
  apr_ssize_t read(char** buf) throw(HttpException);
  void getHeaders(apr_table_t* table) throw();
  HttpRequest* request() throw();
};

class HttpInputStream : public std::streambuf {
  friend class HttpResponse;
private:
  HttpConnection *conn;
  HttpInputStream(HttpConnection *c) throw();
  std::streamsize xsgetn(char *dest, std::streamsize n) throw(HttpException);
};

}; }; // namespace globule::netw

#endif /* _HTTPREQUEST_HPP */
