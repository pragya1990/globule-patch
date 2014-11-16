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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "utilities.h"
#include "GlobuleEvent.hpp"
#include "resource/BaseHandler.hpp"

Handler* GlobuleEvent::_main = 0;
Handler* GlobuleEvent::hbreceiver = 0;
Handler* GlobuleEvent::fmreceiver = 0;

const char*
GlobuleEvent::descriptions[] = {
  "request", "logging", "invalid", "update", "fm-register",
  "fm-unregister", "heartbeat", "hb-register", "switch",
  "wrapup", "redirect", "load", "report", "register"
};

const char*
GlobuleEvent::desc(enum Type type) throw()
{
  if(type < 0 || type >= (signed)(sizeof(descriptions)/sizeof(const char*)))
    return "unknown";
  else
    return descriptions[type];
}

const char*
GlobuleEvent::desc() const throw()
{
  return desc(_type);
}

GlobuleEvent::GlobuleEvent(apr_pool_t* p, Context* ctx,
                           Type t, const Handler* target)
  throw()
  : _type(t), _target(target), _uri(0), context(ctx), pool(p)
{
}

GlobuleEvent::GlobuleEvent(apr_pool_t* p, Context* ctx,
                           Type t, request_rec* r)
  throw()
  : _type(t), _target(0), _uri(&r->parsed_uri), context(ctx), pool(p)
{
  r->parsed_uri.hostname = apr_pstrdup(p, r->server->server_hostname);
  r->parsed_uri.port     = r->server->port;
}

GlobuleEvent::GlobuleEvent(apr_pool_t* p, Context* ctx,
                           Type t, const apr_uri_t* u)
  throw()
  : _type(t), _target(0), _uri(u), context(ctx), pool(p)
{
}

void
GlobuleEvent::target(const Handler* target) throw()
{
  _target = target;
}

bool
GlobuleEvent::submit(GlobuleEvent& evt, Handler* target) throw()
{
  DIAG(evt.context,(MONITOR(detail)),
       ("Submit of event type=%s\n",desc(evt._type)));
#ifdef NOTDEFINED
  if(target) {
    // FIXME: hbreceiver should pick up from ConfigHandler and ConfigHandler
    if(hbreceiver->handle(evt))
      return true;
    if(target->handle(evt))
      return true;
  }
#endif
  if(evt.asynchronous()) {
    struct workerstate_struct* state = diag_message(NULL);
    if(target)
      evt.target(target);
    state->queuedEvents.push(evt.instantiateEvent());
    return true;
  } else
    return evt.dispatch(target);
}

bool
GlobuleEvent::dispatch(Handler* target) throw()
{
  DIAG(context,(MONITOR(detail)),
       ("Dispatch of event type=%s\n",desc()));
#ifdef NOTDEFINED
  if(target) {
    // FIXME: hbreceiver should pick up from ConfigHandler and ConfigHandler
    if(hbreceiver->handle(*this))
      return true;
    if(target->handle(*this))
      return true;
  }
#endif
  return _main->handle(*this);
}

void
GlobuleEvent::registerMainHandler(Handler* handler) throw()
{
  _main = handler;
}

void
GlobuleEvent::disposeMainHandler(apr_pool_t* pool) throw()
{
  if(_main) {
    _main->flush(pool);
    rmmmemory::destroy(_main); // operator delete(_main, rmmmemory::shm());
  }
}

apr_uri_t*
GlobuleEvent::matchall(apr_pool_t* p) throw()
{
  apr_uri_t* uri = (apr_uri_t*) apr_pcalloc(p, sizeof(apr_uri_t));
  uri->hostname = 0;
  uri->path = apr_pstrdup(p, "");
  return uri;
}

bool
GlobuleEvent::match(const apr_uri_t& partial, std::string& remainder) throw()
{
  DIAG(context,(MONITOR(detail)),
       ("Matching event type=%s path=%s:%d/%s against %s:%d/%s\n", desc(),
        (_uri&&_uri->hostname?_uri->hostname:"(null)"),
        (_uri?_uri->port:0), (_uri?_uri->path:"(null)"),
        (partial.hostname?partial.hostname:"(null)"),
        partial.port, (partial.path?partial.path:"(null)")));
  if(_uri && _uri->path) {
    if((partial.hostname && _uri->hostname &&
        strcmp(partial.hostname,_uri->hostname)) ||
       partial.port && _uri->port && partial.port != _uri->port)
      return false;
    if(partial.path && !strncmp(_uri->path, partial.path, strlen(partial.path))) {
      remainder = &_uri->path[strlen(partial.path)];
      return true;
    }
    if(!strcmp(_uri->path,"")) {
      remainder = "";
      return true;
    }
  }
  return false;
}

bool
GlobuleEvent::match(const Url& partial, std::string& remainder) throw()
{
  apr_uri_t uri;
  uri.path     = const_cast<char *>(partial.path());     // safe casts
  uri.hostname = const_cast<char *>(partial.host());
  uri.port     = partial.port();
  return match(uri, remainder);
}
