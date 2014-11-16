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
#ifndef _URL_HPP
#define _URL_HPP

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <string>
#include <apr.h>
#include <apr_uri.h>
#include <apr_pools.h>
#include <ostream>
#include "Storage.hpp"

namespace globule { namespace netw {
  class HttpRequest;
}};

class UrlException
{
private:
  std::string _message;
public:
  inline UrlException(std::string message) : _message(message) { };
  inline const std::string getMessage() { return _message; };
};

class Url
{
#ifndef STANDALONE_APR
  friend class gUrl;
#endif
private:
  apr_uri_t _uri;
  apr_sockaddr_t* _hostaddr;
  static apr_uri_t construct(apr_pool_t* p, const char* scheme,
                             const char* hostname, apr_port_t port,
                             const char *rest)
                   throw(UrlException);
  static apr_uri_t construct(apr_pool_t* p, const char* uri_str,
                             const char* extension)
                   throw(UrlException);
  static apr_uri_t construct(apr_pool_t* p, const char* uri_str)
                   throw(UrlException);
protected:
  static apr_status_t urlutil_normalize(const char *src_url_str,
                                        apr_uri_t **dest_url_inout, 
                                        char **dest_url_str_out,
                                        apr_pool_t *pool)
                                throw(UrlException);
#ifndef STANDALONE_APR
private:
#else
public:
#endif
  Url() throw();
  Url(Url& c) throw();
  Url(const Url& u) throw();
  Url& operator=(const Url& org) throw();
public:
  Url(apr_pool_t* pool, const apr_uri_t* u) throw();
  Url(apr_pool_t* pool, const Url& u, const char*) throw(UrlException);
  Url(apr_pool_t* pool, const char* url_str) throw(UrlException);
  Url(apr_pool_t* pool, const std::string s) throw(UrlException);
  Url(apr_pool_t* pool, const char* scheme, const char* hostname,
      apr_port_t port, const char* rest) throw(UrlException);
  Url(apr_pool_t* pool, const Url&) throw();
  inline const char* scheme() const throw() { return _uri.scheme; };
  inline const char* host() const throw() { return _uri.hostname; };
  inline const char* path() const throw() { return _uri.path; };
  inline const char* query() const throw() { return _uri.query; };
  inline const char* user() const throw() { return _uri.user; };
  inline const char* pass() const throw() { return _uri.password; };
  char* pathquery(apr_pool_t* pool) const throw();
  apr_status_t address(apr_pool_t*, apr_sockaddr_t**, bool cache = false)
    throw();
  apr_port_t port() const throw() { return _uri.port; };
  void normalize(apr_pool_t* pool) throw();
  char* operator()(apr_pool_t* pool) const throw();
  const apr_uri_t* uri() const throw() { return &_uri; };
  bool operator==(const Url&) const throw();
};

#ifndef STANDALONE_APR

class gUrl : public Url
{
public:
  gUrl() throw(); // should not be used; for Persist only
  ~gUrl() throw();
  gUrl(apr_pool_t* pool, const std::string url_str) throw(UrlException);
  gUrl(const Url& u) throw();
  gUrl(const gUrl& org) throw();
  gUrl& operator=(const gUrl& org) throw();
};

extern Input& operator>>(Input&, gUrl&) throw();
extern Output& operator<<(Output&, const gUrl&) throw();

#else

typedef Url gUrl;

#endif

#endif /* _URL_HPP */
