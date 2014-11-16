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
#ifndef _NAMEBINDINGHANDLER_HPP
#define _NAMEBINDINGHANDLER_HPP
                                                                               
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <string>
#include <map>
#include <apr.h>
#include "resource/BaseHandler.hpp"
#include "event/RedirectEvent.hpp"
#include "alloc/Allocator.hpp"

class NameBindingHandler : public Handler
{
private:
  RedirectPolicy* _redirect_policy;
public:
  static gvector<gDNSRecord>* dnsdb; // pointer to prevent deallocation
public:
  NameBindingHandler(Handler*, apr_pool_t*, const Url& dummy,
                     const std::vector<DNSRecord>&) throw();
  virtual ~NameBindingHandler() throw();
  virtual void flush(apr_pool_t* p) throw();
  virtual bool handle(GlobuleEvent& evt) throw();
  virtual bool handle(GlobuleEvent& evt, const std::string& remainder) throw();
  virtual std::string description() const throw();
};

#endif /* _NAMEBINDINGHANDLER_HPP */
