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
#ifndef _REDIRECTORHANDLER_HPP
#define _REDIRECTORHANDLER_HPP
                                                                               
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <string>
#include <set>
#include <vector>
#include <map>
#include <apr.h>
#include "resource/BaseHandler.hpp"
#include "alloc/Allocator.hpp"
#include "event/RedirectEvent.hpp"
#include "redirect/dns_policy.hpp"
#include "resource/BaseHandler.hpp"
#include "netw/Url.hpp"

class RedirectorHandler : public BaseHandler
{
private:
  gUrl            _upstream_location;
  char*           _upstream_password;
  gvector<gUrl>   _aliases;
protected:
  int             _redirect_mode;
  RedirectPolicy* _redirect_policy;
  apr_size_t      _lastshipmentsize;
  int             _reevaluateinterval;
  virtual Peer* addReplicaFast(Context* ctx, apr_pool_t* p, const char* location, const char* secret) throw();
  virtual void changeReplica(Peer* p) throw();
  void checkreplicas(apr_pool_t* pool, Context* ctx) throw();
  void redirect(RedirectEvent&, const std::string& remainder) throw();
  void reload(apr_pool_t* ptemp, Context*) throw();
protected:
  RedirectorHandler(Handler*, apr_pool_t* p, Context* ctx,
                    const Url& uri, const char *path) throw();
public:
  RedirectorHandler(Handler*, apr_pool_t* p, Context* ctx,
                    const Url& uri, const char *path,
                    const char* upstream, const char* password) throw();
  virtual ~RedirectorHandler() throw();
  virtual void initialize(apr_pool_t*, int naliases, const char** aliases) throw(SetupException);
  virtual bool initialize(apr_pool_t*, Context*, HandlerConfiguration*) throw(SetupException);
  virtual bool handle(GlobuleEvent& evt) throw();
  virtual bool handle(GlobuleEvent& evt, const std::string& remainder) throw();
  virtual std::string description() const throw();
  inline virtual bool ismaster() const throw() { return false; };
  inline virtual bool isslave() const throw() { return false; };
  virtual void setAuthorization(globule::netw::HttpRequest* req,
                                const char* uri, apr_pool_t* pool) throw();
};

#endif /* _REDIRECTORHANDLER_HPP */
