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
#ifndef _RESOURCES_HPP
#define _RESOURCES_HPP
#include <map>
#include <apr.h>
#include <apr_time.h>
#include "alloc/Allocator.hpp"
#include "locking.hpp"
#include "Storage.hpp"
#include "event/EvictionEvent.hpp"

typedef apr_uint16_t apr_uint16_t;
class ResourceAccounting;

class ResourceDeclaration
{
  friend class ResourceAccounting;
  friend class ResourcesRecord;
private:
  std::map<apr_uint16_t,apr_int64_t> _resources;
public:
  static const int QUOTA_DISKSPACE = 1;
  static const int QUOTA_NUMDOCS   = 2;
  static const char* name(apr_uint16_t rsrc);
  inline void set(apr_uint16_t rsrc, apr_int64_t num) throw() {
    _resources[rsrc] = num;
  };
  inline bool get(apr_uint16_t rsrc, apr_int64_t& value) throw() {
    std::map<apr_uint16_t,apr_int64_t>::iterator iter = _resources.find(rsrc);
    if(iter != _resources.end()) {
      value = iter->second;
      return true;
    } else
      return false;
  };
  inline apr_int64_t get(apr_uint16_t rsrc) {
    std::map<apr_uint16_t,apr_int64_t>::iterator iter = _resources.find(rsrc);
    if(iter != _resources.end())
      return iter->second;
    else
      return 0;
  };
};

class ResourcesUser
{
  friend class ResourceAccounting;
private:
  virtual EvictionEvent evict(Context* ctx, apr_pool_t*) = 0;
};

class ResourcesRecord : public Persistent
{
  friend class ResourceAccounting;
private:
  ResourceAccounting* _acct;
  ResourcesUser*      _user;
  apr_time_t _lastuse;
  gmap<const apr_uint16_t,apr_int64_t> _usage;
public:
  ResourcesRecord() throw(); // should not be used directly
  ResourcesRecord(ResourceAccounting* acct, ResourcesUser* user) throw();
  virtual ~ResourcesRecord() throw();
  Input& operator>>(Input& in) throw();
  Output& operator<<(Output& out) const throw();
  virtual Persistent* instantiateClass() throw();
  void consume(ResourceDeclaration decl) throw();
  inline void update() { _lastuse = apr_time_now(); };
  void update(ResourcesUser* newuser, ResourceAccounting* newacct);
};

class ResourceAccounting
{
  friend class ResourcesRecord;
private:
  Lock _lock;
  gmap<const apr_uint16_t,apr_int64_t> _total;
  gmap<const apr_uint16_t,apr_int64_t> _available;
  gset<ResourcesRecord*> _users;
  void consume(ResourceDeclaration diff) throw();
  void add(ResourcesRecord* user) throw();
  bool del(ResourcesRecord* user) throw();
public:
  ResourceAccounting() throw();
  ResourceAccounting(const ResourceDeclaration decl) throw();
  ~ResourceAccounting() throw();
  void info(ResourceDeclaration& total, ResourceDeclaration& available)
    throw();
  void conform(Context* ctx, apr_pool_t*) throw();
};

#endif
