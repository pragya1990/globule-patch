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
#include "utilities.h"
#include "BrokeredHandler.hpp"
#include "event/RegisterEvent.hpp"
#include "configuration.hpp"

using std::string;

BrokeredHandler::BrokeredHandler(Handler* parent, apr_pool_t* p, Context* ctx,
                                 const Url& uri, const char* path,
                                 const char* broker, const char* password)
  throw()
  : BaseHandler(parent, p, ctx, uri, path), _path(path),
    _broker_location(p, broker), _broker_password(password)
{
}

BrokeredHandler::~BrokeredHandler() throw()
{
}

bool
BrokeredHandler::initialize(apr_pool_t* p, Context* ctx,
			    HandlerConfiguration* cfg)
  throw(SetupException)
{
  if(BaseHandler::initialize(p, ctx, cfg))
    return true;
  DIAG(ctx,(MONITOR(info)),("Initializing brokered section url=%s\n",_uri(p)));
  _reevaluateinterval = cfg->get_reevaluateinterval();
  if(_reevaluateinterval) {
    RegisterEvent ev(p, this, _reevaluateinterval);
    if(!GlobuleEvent::submit(ev)) {
      throw "ResourcePool cannot register to HeartBeat";
    }
  }
  return false;
}

void
BrokeredHandler::flush(apr_pool_t* p) throw()
{
}

Lock*
BrokeredHandler::getlock() throw()
{
  return &_lock;
}

bool
BrokeredHandler::createSubHandler(apr_pool_t* pool, Context* ctx,
                                  const Url& location, string upstream,
                                  HttpMetaEvent* evt) throw()
{
  const char* s = _path.c_str();
  mkstring directory;
  directory << s;
  if(s[strlen(s)-1] != '/')
    directory << "/";
  for(s = upstream.c_str(); *s; s++)
    switch(*s) {
    case '/': directory << '_'; break;
    default:  directory << *s;
    }
  DIAG(0,(MONITOR(info)),("creating new section at %s in directory %s for %s",location(pool), directory.str().c_str(),upstream.c_str()));
  ImportHandler* newhandler = new(rmmmemory::shm()) ImportHandler(this,
                         evt->pool, ctx, location, directory.str().c_str(),
                         upstream.c_str(), _broker_password.c_str());

  HandlerConfiguration cfg(0, "", "", _reevaluateinterval, 0, 0, 0,
			   0 /* don't know of any keepers */);
  newhandler->initialize(pool, ctx, &cfg);
  if(((Handler*)newhandler)->handle(*(GlobuleEvent*)evt)) {
    _handlers.push_back(newhandler);
    mkstring alternatePath;
    for(s = location.pathquery(pool); *s; ++s)
      if(*s!='/' || s[1]!='/')
        alternatePath << *s;
    s = Url(pool, location.scheme(), location.host(),
        location.port(), alternatePath.str().c_str())(pool);
    newhandler->BaseHandler::initialize(pool, 1, &s);
    return true;
  } else {
    rmmmemory::destroy(newhandler); // operator delete(newhandler, rmmmemory::shm());
    return false;
  }
}

bool
BrokeredHandler::handle(GlobuleEvent& evt, const string& remainder) throw()
{
  switch(evt.type()) {
  case GlobuleEvent::INVALID_EVT: {
    HttpMetaEvent& ev = (HttpMetaEvent&) evt;
    if(remainder == "" && ev.getRequestArgument() == "status") {
      ev.setStatus(HTTP_NO_CONTENT);
      return true;
    }
    for(gvector<Handler*>::iterator iter = _handlers.begin();
        iter != _handlers.end();
        ++iter)
      if((*iter)->handle(evt))
        return true;
    if(ev.getRequestArgument() == "status") {
      Url location(evt.pool,_uri,remainder.c_str());
      return createSubHandler(evt.pool, evt.context, location, remainder, &ev);
    } else
      return false;
  }
  default: {
    for(gvector<Handler*>::iterator iter = _handlers.begin();
        iter != _handlers.end();
        ++iter)
      if((*iter)->handle(evt))
        return true;
    return false;
  }
  }
  return false;
}

string
BrokeredHandler::description() const throw()
{
  return mkstring() << "BrokeredHandler(" << _uri.host() << ":" << _uri.port()
                    << _uri.path()<<")";
}

void
BrokeredHandler::accounting(ResourceAccounting* acct) throw()
{
  _accounting = acct;
}

ResourceAccounting*
BrokeredHandler::accounting() throw()
{
  return _accounting;
}

bool
BrokeredHandler::ismaster() const throw()
{
  return false;
}

bool
BrokeredHandler::isslave() const throw()
{
  return true;
}
