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
#ifndef _MONITORING_H
#define _MONITORING_H

#include <httpd.h>
#include <apr.h>
#include <apr_time.h>
#include <apr_global_mutex.h>
#ifdef __cplusplus
#include <vector>
#include <map>
#include <string>
#include "Storage.hpp"
#endif

/****************************************************************************/

class ConfigHandler;
class Context;
class Monitor;
class Event;

class Event : public Persistent
{
  friend class LogFilter;
public:
  apr_time_t            _timestamp;
  apr_uint64_t          _value;
  std::vector<Monitor*> _tokens;
private:
  std::string               _message;
  const char*               _file;
  int                       _line;
protected:
  Event() throw();
public:
  Event(const Event&) throw();
  Event(apr_int64_t) throw();
  Event(apr_int64_t,apr_int64_t) throw();
  Event(Monitor*) throw();
  Event(Monitor*, Monitor*) throw();
  Event(Monitor*, Monitor*, Monitor*) throw();
  Event(Monitor*, Monitor*, Monitor*, Monitor*) throw();
  Event(Monitor*, Monitor*, Monitor*, Monitor*, Monitor*) throw();
  Event(Monitor*, Monitor*, Monitor*, Monitor*, Monitor*, Monitor*) throw();
  Event(Monitor*, apr_int64_t) throw();
  Event(Monitor*, Monitor*, apr_int64_t) throw();
  Event(Monitor*, Monitor*, Monitor*, apr_int64_t) throw();
  Event(Monitor*, Monitor*, Monitor*, Monitor*, apr_int64_t) throw();
  Event(Monitor*, Monitor*, Monitor*, Monitor*, Monitor*, apr_int64_t) throw();
  Event(Monitor*, Monitor*, Monitor*, Monitor*, Monitor*, Monitor*, apr_int64_t) throw();
  Event(const Event&, std::vector<Monitor*>, apr_time_t timestamp, apr_int64_t v) throw();
  ~Event() throw();
  inline void location(const char* file, int line) throw() {
    _file = file;
    _line = line;
  };
  Event& message(const char *fmt, ...) throw()
    __attribute__ ((__format__ (__printf__, 2, 3)));
  std::string format(Context* ctx) const throw();
  std::string logformat(Context* ctx) const throw();
  int operator<(const Event& other) const throw();
  virtual Persistent* instantiateClass() throw();
  virtual Input& operator>>(Input&) throw();
  virtual Output& operator<<(Output&) const throw();
};

class Monitor {
  friend class Context;
  friend class Event;
private:
  static Monitor root;
  Monitor* _nextRegistered;
  Monitor();
public:
  apr_int32_t _id;
  char* _name;
  const char* _description;
  Monitor(const Monitor&) throw();
  Monitor(const char* name, const char *description = 0) throw();
  Monitor(Context* ctx, const char* description) throw();
protected:
  Monitor(Context* ctx, const char* name, const char* description) throw();
};

class Filter : public MergePersistent
{
  friend class Context;
private:
  static std::map<std::string,Filter*> _filters;
public:
  static std::string nextString(const char** arg) throw();
  static apr_int32_t nextInteger(const char** arg) throw();
public:
  Filter(const char* name) throw();
  virtual void initialize(Context* ctx, const char* arg) throw() = 0;
  virtual Filter* instantiateFilter() throw() = 0;
  virtual Persistent* instantiateClass() throw() { return instantiateFilter(); };
  virtual Output& operator<<(Output& out) const throw();
  virtual Input& operator>>(Input& in) throw();
  virtual bool sync(SharedStorage& store) throw();
  static void registerFilter(const char* name, Filter* filter) throw();
  static Filter* lookupFilter(Context* ctx, const char* name) throw();
  virtual void push(Context* ctx, const Event& evt) throw() = 0;
  virtual void pull(Context* ctx, Filter* callback) throw() = 0;
  virtual std::vector<std::string> description() throw() = 0;
};

class Context : public MergePersistent
{
  friend class Event;
  friend class Filter;
  friend class Monitor;
private:
  SharedStorageSpace*  _storage;
#ifndef STANDALONE_APR
  apr_global_mutex_t*  _mutex;
#endif
  apr_time_t           _cfgtimestamp;
  unsigned int         _nfilters;
  int                  _filteridx;
  Filter**             _filters;
  /* The offset must reside in shared memory, the mutex is used to obtain
   * exclusive access to this object, and any lower data (ShmFilter).
   */
  request_rec*         _request;
protected:
  apr_pool_t*          _pool;
private:
  char *               _arguments;
  static std::map<apr_int32_t,Monitor*> _monitors;
public:
  static void initialize() throw();
#ifndef STANDALONE_APR
  Context(SharedStorageSpace* storage, apr_global_mutex_t* mutex) throw();
#else
  Context() throw();
#endif
  virtual Persistent* instantiateClass() throw();
  Output& operator<<(Output& out) const throw();
  Input& operator>>(Input& in) throw();
  virtual bool sync(SharedStorage& store) throw();
  void lock() throw();
  void unlock() throw();
  bool update(bool retainlock=false) throw();
  static void* emptystorage() throw();
  inline request_rec* request() throw() { return _request; };
  inline void request(request_rec* r) throw() { _request = r; };
  inline apr_pool_t* pool() {return _pool?_pool:(_request?_request->pool:0);};
  inline void pool(apr_pool_t* p) throw() { _pool = p; };
  apr_int32_t monitorid(const char* monitorname) throw();
  inline Filter* filter(apr_uint64_t filterid) throw() {
    return filterid < _nfilters ? _filters[filterid] : 0;
  };
  inline Monitor* monitor(apr_int32_t monitorid) throw() {
    std::map<apr_int32_t,Monitor*>::iterator iter = _monitors.find(monitorid);
    if(iter != _monitors.end())
      return iter->second;
    else
      return 0;
  };
  unsigned int nfilters() throw() { return _nfilters; };
  inline Filter* filter(Monitor* m,...) throw() {
    return _filters[m->_id];
  }
  void filter(const char* monitorname, Filter* f) throw();
  virtual Filter* filter(const char* monitorname) throw();
  void push(const Event&) throw();
};

class OverlayContext : public Context
{
private:
  Context*     _parent;
  unsigned int _nMoreFilters;
  std::map<std::string, Filter*> _moreFilters;
  char *       _arguments;
public:
  OverlayContext(Context* ctx) throw();
  ~OverlayContext() throw();
  void filter(const char* monitorname, Filter* f) throw();
  virtual Filter* filter(const char* name) throw();
};

/****************************************************************************/
#endif
