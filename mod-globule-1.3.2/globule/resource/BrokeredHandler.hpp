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
#ifndef _BROKEREDHANDLER_HPP
#define _BROKEREDHANDLER_HPP

#include "resource/ImportHandler.hpp"
#include "alloc/Allocator.hpp"
#include "locking.hpp"

class BrokeredHandler : public BaseHandler
{
private:
  gstring           _path;
  Url               _broker_location;
  gstring           _broker_password;
  int               _reevaluateinterval;
  gvector<Handler*> _handlers;
  ResourceAccounting* _accounting;
  bool createSubHandler(apr_pool_t* pool, Context* ctx, const Url& location,
                        std::string upstream, HttpMetaEvent* evt) throw();
public:
  BrokeredHandler(Handler*, apr_pool_t* p, Context*,
                  const Url& uri, const char* path,
                  const char* broker, const char* password) throw();
  virtual ~BrokeredHandler() throw();
  virtual bool initialize(apr_pool_t*, Context*, HandlerConfiguration*) throw(SetupException);
  virtual void flush(apr_pool_t* p) throw();
  virtual Lock* getlock() throw();
  virtual bool handle(GlobuleEvent& evt, const std::string& remainder) throw();
  virtual std::string description() const throw();
  virtual void accounting(ResourceAccounting*) throw();
  virtual ResourceAccounting* accounting() throw();
  virtual bool ismaster() const throw();
  virtual bool isslave() const throw();
};

#endif /* _BROKEREDHANDLER_HPP */
