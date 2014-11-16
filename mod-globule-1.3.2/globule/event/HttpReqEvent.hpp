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
#ifndef _HTTPREQEVENT_HPP
#define _HTTPREQEVENT_HPP

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <httpd.h>
#include "GlobuleEvent.hpp"
#include "Constants.hpp"
#include "resource/Peer.hpp"

class HttpReqEvent : public GlobuleEvent
{
protected:
  request_rec* _request;
  bool _completed;
  Peer* _peer;
  HttpReqEvent(apr_pool_t* p, Context* ctx,
               apr_uri_t *u)
    throw()
    : GlobuleEvent(p, ctx, REQUEST_EVT, u),
      _request(0), _completed(false), _peer(0)
  { };
public:
  HttpReqEvent(apr_pool_t* p, Context* ctx,
               request_rec *r)
    throw()
    : GlobuleEvent(p, ctx, REQUEST_EVT, r),
      _request(r), _completed(false), _peer(0)
  { };
  HttpReqEvent(apr_pool_t* p, Context* ctx,
               Handler* r)
    throw()
    : GlobuleEvent(p, ctx, REQUEST_EVT, r),
      _request(0), _completed(false), _peer(0)
  { };
  virtual ~HttpReqEvent() throw() { };
  virtual bool asynchronous() throw() { return false; };
  virtual GlobuleEvent* instantiateEvent() throw() { return 0; };
  virtual void setStatus(int arg) throw() {
    _completed = true;
    if(_request)
      _request->status = arg;
  };
  virtual bool completed() throw() { return _completed; };
  request_rec* getRequest() const throw() { return _request; };
  std::string getRequestArgument() const throw() {
    if(_request && _request->parsed_uri.query)
      return _request->parsed_uri.query;
    else if(_uri && _uri->query)
      return _uri->query;
    else
      return "";
  }
  bool getRequestStatusPurging() throw() {
    if(_request)
      if(_request->status < 200 || _request->status >= 400)
        return true;
    return false;
  };
  void setPeer(Peer* from) throw() { _peer = from; };
  Peer* getPeer() const throw() { return _peer; };
};

#endif /* _HTTPREQEVENT_HPP */
