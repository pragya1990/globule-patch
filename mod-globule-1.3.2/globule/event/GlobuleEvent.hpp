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
#ifndef _GLOBULEEVENT_HPP
#define _GLOBULEEVENT_HPP

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <string>
#include <httpd.h>
#include <apr_uri.h>
#include "monitoring.h"
#include "netw/Url.hpp"


class Handler;
class BaseHandler;
class GlobuleEvent
{
public:
  enum Type {
    REQUEST_EVT=0, LOGGING_EVT, INVALID_EVT, UPDATE_EVT, FILE_MONITOR_REG,
    FILE_MONITOR_UNREG, HEARTBEAT_EVT, HEART_BEAT_REG, SWITCH_EVT,
    WRAPUP_EVT, REDIRECT_EVT, LOAD_EVT, REPORT_EVT, REGISTER_EVT,
    EVICTION_EVT
  };
private:
  static const char* descriptions[];
  static const char* desc(enum Type) throw();
  const char* desc() const throw();
private:
  static Handler*   _main;
protected:
  enum Type _type;
  const Handler*    _target;
  const apr_uri_t*  _uri;
public:
  Context* context;
  apr_pool_t* pool;
protected:
  apr_uri_t* matchall(apr_pool_t* p) throw();
public:
  GlobuleEvent(apr_pool_t*, Context*, Type, const Handler* target) throw();
  GlobuleEvent(apr_pool_t*, Context*, Type, request_rec* r) throw();
  GlobuleEvent(apr_pool_t*, Context*, Type, const apr_uri_t* uri) throw();
  inline const Type type() const throw() { return _type; };
  bool match(const apr_uri_t& partial, std::string& remainder) throw();
  bool match(const Url& partial, std::string& remainder) throw();
  const Handler* target() const throw() { return _target; };
  void target(const Handler* target) throw();
  virtual bool asynchronous() throw() = 0;
  static Handler* hbreceiver;
  static Handler* fmreceiver;
  bool dispatch(Handler* target = 0) throw();
  virtual GlobuleEvent* instantiateEvent() throw() = 0;
  static bool submit(GlobuleEvent&, Handler* target = 0) throw();
  static void registerMainHandler(Handler* handler) throw();
  static void disposeMainHandler(apr_pool_t*) throw();
};

#endif /* _GLOBULEEVENT_HPP */
