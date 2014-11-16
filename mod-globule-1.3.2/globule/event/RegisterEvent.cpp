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
#include <config.h>
#endif
#include "utilities.h"
#include "event/RegisterEvent.hpp"

using std::string;

RegisterEvent::RegisterEvent(apr_pool_t*, Handler* listener)
  throw()
  : GlobuleEvent(pool, 0, REGISTER_EVT, listener->location().uri()),
    _listener(listener), _interval(0), _fname("")
{
}

RegisterEvent::RegisterEvent(apr_pool_t* pool, Handler* listener,
                             const char* fname, int seconds)
  throw()
  : GlobuleEvent(pool, 0, FILE_MONITOR_REG, fmreceiver),
    _listener(listener), _interval(apr_time_from_sec(seconds)), _fname(fname)
{
  switch(seconds) {
  case -1:
    _interval = 0;
    break;
  case 0:
    _type = FILE_MONITOR_UNREG;
    break;
  }
}

RegisterEvent::RegisterEvent(apr_pool_t* pool, Handler* listener,
                             string fname, int seconds)
  throw()
  : GlobuleEvent(pool, 0, FILE_MONITOR_REG, fmreceiver),
    _listener(listener), _interval(apr_time_from_sec(seconds)), _fname(fname)
{
  switch(seconds) {
  case -1:
    _interval = 0;
    break;
  case 0:
    _type = FILE_MONITOR_UNREG;
    break;
  }
}

RegisterEvent::RegisterEvent(apr_pool_t* pool, Handler* listener,
                             gstring fname, int seconds)
  throw()
  : GlobuleEvent(pool, 0, FILE_MONITOR_REG, fmreceiver),
    _listener(listener), _interval(apr_time_from_sec(seconds)),
    _fname(fname.c_str())
{
  switch(seconds) {
  case -1:
    _interval = 0;
    break;
  case 0:
    _type = FILE_MONITOR_UNREG;
    break;
  }
}

RegisterEvent::RegisterEvent(apr_pool_t* pool, Handler* listener, int seconds)
  throw()
  : GlobuleEvent(pool, 0, HEART_BEAT_REG, hbreceiver),
    _listener(listener), _interval(apr_time_from_sec(seconds)), _fname("")
{
  switch(seconds) {
  case -1:
    _interval = 60;
    break;
  }
}

RegisterEvent::~RegisterEvent() throw()
{
}

bool
RegisterEvent::asynchronous() throw()
{
  if(_type == HEART_BEAT_REG || _type == REGISTER_EVT)
    return false;
  else
    return true;
}

GlobuleEvent*
RegisterEvent::instantiateEvent() throw()
{
  // FIXME reusing the pool in instantiateEvent is not a good idea
  switch(_type) {
  case REGISTER_EVT:
    return new RegisterEvent(pool,_listener);
  case FILE_MONITOR_REG:
  case FILE_MONITOR_UNREG:
    return new RegisterEvent(pool,_listener,_fname,apr_time_sec(_interval));
  case HEART_BEAT_REG:
    return new RegisterEvent(pool,_listener,_fname,apr_time_sec(_interval));
  default:
    return 0;
  }
}
