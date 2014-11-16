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
#ifndef _FILEMONITOR_HPP
#define _FILEMONITOR_HPP

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "resource/Handler.hpp"
#include "event/EventMgr.hpp"
#include "alloc/Allocator.hpp"
#include "locking.hpp"

class FileDesc {
  friend class FileMonitor;
private:
  gstring _fname;
  gset<const Handler*> _listeners;
  apr_time_t  _lastmod;
  apr_ssize_t _filesiz;
public:
  FileDesc(apr_pool_t* p, const gstring& fname) throw()
    : _fname(fname)
  {
    apr_finfo_t buf;
    if(apr_stat(&buf,_fname.c_str(),APR_FINFO_SIZE|APR_FINFO_MTIME,p)
       != APR_SUCCESS) {
      _lastmod = 0;
      _filesiz = 0;
    } else {
      _lastmod = buf.mtime;
      _filesiz = buf.size;
    }
  };
  inline void addListener(const Handler* listener) throw() {
    _listeners.insert(listener);
  };
  inline void removeListener(const Handler* listener) throw() {
    _listeners.erase(listener);
  };
  inline gset<const Handler*>::const_iterator listeners() const throw() {
    return _listeners.begin();
  };
  inline gset<const Handler*>::const_iterator end() const throw() {
    return _listeners.end();
  };
  inline bool updated(apr_pool_t* p) throw() {
    apr_finfo_t buf;
    if(apr_stat(&buf,_fname.c_str(),APR_FINFO_SIZE|APR_FINFO_MTIME,p)
       != APR_SUCCESS) {
      _lastmod = 0;
      _filesiz = 0;
      return true;
    } else if(_lastmod != buf.mtime || _filesiz != buf.size) {
      _lastmod = buf.mtime;
      _filesiz = buf.size;
      return true;
    } else
      return false;
  };
  inline bool deleted() const throw() {
    return _lastmod == 0 && _filesiz == 0;
  };
};

class FileMonitor : public Handler {
private:
  Lock _lock;
  gmap<const gstring,FileDesc> _filemap;
  void check_files(apr_pool_t*, Context*) throw();
public:
  FileMonitor(Handler*, apr_pool_t*, const Url& selfcontact) throw();
  virtual ~FileMonitor() throw() { };
  virtual void flush(apr_pool_t* p) throw();
  bool handle(GlobuleEvent&) throw();
  virtual std::string description() const throw();
};

#endif /* _FILEMONITOR_HPP */
