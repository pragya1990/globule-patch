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
#ifndef _REGISTEREVENT_HPP
#define _REGISTEREVENT_HPP

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <httpd.h>
#include <apr.h>
#include <apr_time.h>
#include "event/GlobuleEvent.hpp"
#include "alloc/Allocator.hpp"
#include "resource/Handler.hpp"

class RegisterEvent : public GlobuleEvent
{
  Handler* _listener;
  apr_time_t _interval;
  std::string _fname;  
public:
  // Frequency for FileMonitor to check for file updates (in seconds)
  static const apr_time_t FILE_MONITOR_PERIOD = (apr_time_t) 10;
public:
  RegisterEvent(apr_pool_t*, Handler*) throw();
  RegisterEvent(apr_pool_t*, Handler*, const char* fname, int seconds=FILE_MONITOR_PERIOD) throw();
  RegisterEvent(apr_pool_t*, Handler*, std::string fname, int seconds=FILE_MONITOR_PERIOD) throw();
#ifndef STANDALONE_APR
  RegisterEvent(apr_pool_t*, Handler*, gstring fname, int seconds=FILE_MONITOR_PERIOD) throw();
#endif
  RegisterEvent(apr_pool_t*, Handler*, int seconds) throw();
  virtual ~RegisterEvent() throw();
  virtual bool asynchronous() throw();
  virtual GlobuleEvent* instantiateEvent() throw();
  inline Handler* destination() const throw() { return _listener; };
  inline std::string filename() const throw() { return _fname; };
  inline const apr_time_t interval() const throw() { return _interval; };
};

#endif /* _REGISTEREVENT_HPP */
