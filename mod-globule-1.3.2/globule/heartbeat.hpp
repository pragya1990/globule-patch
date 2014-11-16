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
#ifndef _HEARTBEAT_HPP
#define _HEARTBEAT_HPP

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <apr.h>
#include <apr_time.h>
#include "resource/Handler.hpp"
#include "alloc/Allocator.hpp"
#include "netw/Url.hpp"
#include "locking.hpp"

class HeartBeat : public Handler
{
public:
  // Time resolution of HeartBeat (apr_time_t is in micro-seconds)
  static const apr_time_t HEART_BEAT_RESOLUTION = (apr_time_t)10000000;

  class Thread;
  class Registration {
  private:
    Handler* _listener;
    apr_time_t _interval;
  public:
    inline Registration(apr_time_t periodicity, Handler* callback) throw()
      : _listener(callback), _interval(periodicity)
    { };
    inline Handler* listener() const throw() { return _listener; };
    inline const apr_time_t interval() const throw() { return _interval; };
  };
private:
  Lock _lock;
  gmultimap<const apr_time_t, Registration> _recv;
  Thread* _thread;
  void add(Context*, const apr_time_t, Registration) throw();
public:
  HeartBeat(Handler*, apr_pool_t*, const Url& selfcontact) throw();
  virtual ~HeartBeat() throw();
  virtual void flush(apr_pool_t* p) throw();
  virtual Lock* getlock() throw();
  virtual bool handle(GlobuleEvent&) throw();
  virtual std::string description() const throw();
};

#endif /* _HEARTBEAT_HPP */
