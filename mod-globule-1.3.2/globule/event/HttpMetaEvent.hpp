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
#ifndef _HTTPMETAEVENT_HPP
#define _HTTPMETAEVENT_HPP

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <httpd.h>
#include "event/HttpReqEvent.hpp"
#include "GlobuleEvent.hpp"

class HttpMetaEvent : public HttpReqEvent {
public:
  HttpMetaEvent(apr_pool_t* p, Context* ctx, request_rec *req)
    throw()
    : HttpReqEvent(p, ctx, req)
  {
    _type = INVALID_EVT;
  };
  HttpMetaEvent(apr_pool_t* p, Context* ctx, HttpReqEvent& req)
    throw()
    : HttpReqEvent(p, ctx, req.getRequest())
  {
    _type = INVALID_EVT;
  };
  HttpMetaEvent(apr_pool_t* p, Context* ctx, apr_uri_t* uri)
    throw()
    : HttpReqEvent(p, ctx, uri)
  {
    _type = INVALID_EVT;
  };
  HttpMetaEvent(apr_pool_t* p, Context* ctx, Handler* r)
    throw()
    : HttpReqEvent(p, ctx, r)
  {
    _type = INVALID_EVT;
  };
  virtual ~HttpMetaEvent() throw() { };
  virtual bool asynchronous() throw() { return false; };
  virtual GlobuleEvent* instantiateEvent() throw() { return 0; };
};

#endif /* _HTTPMETAEVENT_HPP */
