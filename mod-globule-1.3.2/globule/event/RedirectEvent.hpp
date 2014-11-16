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
#ifndef _REDIRECTEVENT_HPP
#define _REDIRECTEVENT_HPP

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <httpd.h>
#include <vector>
#include "event/HttpReqEvent.hpp"
#include "GlobuleEvent.hpp"
#include "redirect/dns_policy.hpp"

class DNSRecord;
class gDNSRecord
{
  friend class DNSRecord;
private:
  gstring _name;
  int _type;
  apr_uint32_t _addr;
  gstring _cname;
public:
  static const int TYPE_A;
  static const int TYPE_CNAME;
  gDNSRecord(const DNSRecord* r) throw();
  gDNSRecord(const DNSRecord& r) throw();
  gDNSRecord(const char* name, apr_uint32_t address) throw(); // creates a A record
  gDNSRecord(const char* name, const char* cname) throw(); // creates a CNAME record
  inline const char* name() const throw() { return _name.c_str(); };
  inline int type() const throw() { return _type; };
  inline apr_uint32_t addr() const throw() { return _addr; };
  inline const char* cname() const throw() { return _cname.c_str(); };
};

class DNSRecord
{
  friend class gDNSRecord;
private:
  const char* _name;
  int _type;
  apr_uint32_t _addr;
  const char* _cname;
public:
  static const int TYPE_A;
  static const int TYPE_CNAME;
  DNSRecord(Context* ctx, apr_pool_t*, Peer* r) throw(); // creates a A record
  DNSRecord(apr_pool_t*, const gDNSRecord* r) throw();
  DNSRecord(apr_pool_t*, const gDNSRecord& r) throw();
  DNSRecord(apr_pool_t*, const char* name, apr_uint32_t address) throw();
  DNSRecord(apr_pool_t*, const char* name, const char* cname) throw();
  inline const char* name() const throw() { return _name; };
  inline int type() const throw() { return _type; };
  inline apr_uint32_t addr() const throw() { return _addr; };
  inline const char* cname() const throw() { return _cname; };
};

class RedirectEvent : public HttpReqEvent
{
private:
  apr_uint32_t _remoteAddress;
  int _count;
  void (*_callback)(void*, int, const std::vector<DNSRecord>&,
                    RedirectPolicy*) throw();
  void* _callback_data;
public:
  RedirectEvent(apr_pool_t* p, Context* ctx,
                request_rec *req)
    throw()
    : HttpReqEvent(p, ctx, req), _remoteAddress(0), _count(1),
      _callback(0), _callback_data(0)
  {
    _type = REDIRECT_EVT;
  };
  RedirectEvent(apr_pool_t* p, Context* ctx,
                const HttpReqEvent& req)
    throw()
    : HttpReqEvent(p, ctx, getRequest()), _remoteAddress(0), _count(1),
      _callback(0), _callback_data(0)
  {
    _type = REDIRECT_EVT;
  };
  RedirectEvent(apr_pool_t* p, Context* ctx,
                apr_uri_t *u, apr_uint32_t clientip,
                int numrequested=0, void (*callback)(void*,int,
                   const std::vector<DNSRecord>&, RedirectPolicy*)=0,
                   void*data=0)
    throw()
    : HttpReqEvent(p, ctx, u), _remoteAddress(clientip), _count(numrequested),
      _callback(callback), _callback_data(data)
  {
    _type = REDIRECT_EVT;
  };
  virtual ~RedirectEvent() throw() { };
  virtual bool asynchronous() throw();
  virtual GlobuleEvent* instantiateEvent() throw() { return 0; };
  apr_uint32_t remoteAddress() throw();
  int requestedCount() throw() { return _count; };
  void callback(Context* ctx, apr_pool_t* pool,
                int count, std::vector<Peer*>& replicas,
                RedirectPolicy* policy) throw();
  void callback(int count, const DNSRecord answer, RedirectPolicy* policy) throw();
};

#endif /* _REDIRECTEVENT_HPP */
