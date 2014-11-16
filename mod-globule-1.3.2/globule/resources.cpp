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
#include "monitoring.h"
#include "resources.hpp"

using std::map;

const char*
ResourceDeclaration::name(apr_uint16_t rsrc)
{
  switch(rsrc) {
  case QUOTA_DISKSPACE:  return "diskspace";
  case QUOTA_NUMDOCS:    return "numdocs";
  default:               return "unknown";
  }
}

ResourcesRecord::ResourcesRecord()
  throw()
  : Persistent("ResourcesRecord"), _acct(0), _user(0),
    _lastuse(apr_time_now())
{
}

ResourcesRecord::ResourcesRecord(ResourceAccounting* acct, ResourcesUser* user)
  throw()
  : Persistent("ResourcesRecord"), _acct(acct), _user(user),
    _lastuse(apr_time_now())
{
  if(_acct)
    _acct->add(this);
}

ResourcesRecord::~ResourcesRecord() throw()
{
  if(_acct->del(this)) {
    ResourceDeclaration decl;
    for(gmap<const apr_uint16_t,apr_int64_t>::iterator iter = _usage.begin();
        iter != _usage.end();
        ++iter)
      {
        decl.set(iter->first, -(iter->second));
        iter->second = 0;
      }
    _acct->consume(decl);
  }
}

Persistent*
ResourcesRecord::instantiateClass() throw()
{
  return new(rmmmemory::shm()) ResourcesRecord();
}

Input&
ResourcesRecord::operator>>(Input& in) throw()
{
  ResourceDeclaration decl;
  apr_uint16_t nrsrcsused;
  in >> _lastuse >> nrsrcsused;
  // _usage.resize(nrsrcsused);
  for(int i=0; i<nrsrcsused; i++) {
    apr_uint16_t rsrc;
    apr_int64_t num;
    in >> rsrc >> num;
    _usage[rsrc] = num;
    decl.set(rsrc, num);
  }
  if(_acct)
    _acct->consume(decl);
  return in;
}

Output&
ResourcesRecord::operator<<(Output& out) const throw()
{
  apr_uint16_t nrsrcsused = _usage.size();
  out << _lastuse << nrsrcsused;
  for(gmap<const apr_uint16_t,apr_int64_t>::const_iterator iter = _usage.begin();
      iter != _usage.end();
      ++iter)
    out << iter->first << iter->second;
  return out;
}

void
ResourcesRecord::consume(ResourceDeclaration decl) throw()
{
  for(map<apr_uint16_t,apr_int64_t>::iterator iter=decl._resources.begin();
      iter != decl._resources.end();
      ++iter)
    {
      apr_int64_t usage = iter->second;
      iter->second -= _usage[iter->first];
      _usage[iter->first] = usage;
    }
  if(_acct)
    _acct->consume(decl);
}

void
ResourcesRecord::update(ResourcesUser* newuser, ResourceAccounting* newacct)
{
  if(_acct) {
    if(_acct->del(this)) {
      ResourceDeclaration decl;
      for(gmap<const apr_uint16_t,apr_int64_t>::iterator iter = _usage.begin();
          iter != _usage.end();
          ++iter)
        {
          decl.set(iter->first, -(iter->second));
          iter->second = 0;
        }
      _acct->consume(decl);
    }
  }
  _user = newuser;
  if((_acct = newacct)) {
    _acct->add(this);
    ResourceDeclaration decl;
    for(gmap<const apr_uint16_t,apr_int64_t>::const_iterator iter =
          _usage.begin(); iter != _usage.end(); ++iter)
      decl.set(iter->first, iter->second);
    _acct->consume(decl);
  }
}

ResourceAccounting::ResourceAccounting() throw()
  : _lock(1) // no concurrency allowed
{
}

ResourceAccounting::ResourceAccounting(const ResourceDeclaration decl) throw()
{
  for(map<apr_uint16_t,apr_int64_t>::const_iterator iter=decl._resources.begin();
      iter != decl._resources.end();
      ++iter)
    {
      _total[iter->first]     = iter->second;
      _available[iter->first] = iter->second;
    }
}

ResourceAccounting::~ResourceAccounting() throw()
{
  for(gset<ResourcesRecord*>::iterator iter=_users.begin();
      iter != _users.end(); ++iter)
    (*iter)->_acct = 0;
}

void
ResourceAccounting::add(ResourcesRecord* user) throw()
{
  DEBUGACTION(ResourceAccounting,add,begin);
  _lock.lock();
  _users.insert(user);
  _lock.unlock();
  DEBUGACTION(ResourceAccounting,add,end);
}

bool
ResourceAccounting::del(ResourcesRecord* user) throw()
{
  DEBUGACTION(ResourceAccounting,del,begin);
  _lock.lock();
  int count = _users.erase(user);
  _lock.unlock();
  DEBUGACTION(ResourceAccounting,del,end);
  return (count > 0);
}

void
ResourceAccounting::consume(ResourceDeclaration diff) throw()
{
  DEBUGACTION(ResourceAccounting,consume,begin);
  _lock.lock();
  for(map<apr_uint16_t,apr_int64_t>::iterator iter=diff._resources.begin();
      iter != diff._resources.end();
      ++iter)
    _available[iter->first] -= iter->second;
  _lock.unlock();
  DEBUGACTION(ResourceAccounting,consume,end);
}

void
ResourceAccounting::info(ResourceDeclaration& total,
                         ResourceDeclaration& available) throw()
{
  _lock.lock();
  for(gmap<const apr_uint16_t,apr_int64_t>::iterator iter =
      _total.begin(); iter != _total.end(); ++iter)
    total.set(iter->first,iter->second);
  for(gmap<const apr_uint16_t,apr_int64_t>::iterator iter =
      _available.begin(); iter != _available.end(); ++iter)
    available.set(iter->first,iter->second);
  _lock.unlock();
}

void
ResourceAccounting::conform(Context* ctx, apr_pool_t* pool) throw()
{
  DEBUGACTION(ResourceAccounting,conform,begin);
  for(;;) {
    _lock.lock();
    gmap<const apr_uint16_t,apr_int64_t>::iterator rsrciter = _available.begin();
    while(rsrciter != _available.end())
      if(rsrciter->second < 0)
        break;
      else
        ++rsrciter;
    if(rsrciter == _available.end()) {
      _lock.unlock();
      DEBUGACTION(ResourceAccounting,conform,end);
      return;
    }
    DIAG(ctx,(MONITOR(info)),("Out of resource %s by %" APR_UINT64_T_FMT
      " units", ResourceDeclaration::name(rsrciter->first),-rsrciter->second));
    ResourcesRecord *lru = 0;
    for(gset<ResourcesRecord*>::iterator useriter = _users.begin();
        useriter != _users.end();
        ++useriter)
      if(!lru || lru->_lastuse > (*useriter)->_lastuse) {
#ifdef NOTDEFINED
        /* Unfortunately; we cannot at this time check whether freeing up
         * this document would actually free up the resources we need,
         * because we cannot lock the ResourceRecord in any way.
         */
        gmap<const apr_uint16_t,apr_int64_t>::iterator finditer =
          (*useriter)->_usage.find(rsrciter->first);
        if(finditer != (*useriter)->_usage.end() && finditer->second > 0)
#endif
          lru = *useriter;
      }
    if(!lru) {
      DIAG(ctx,(MONITOR(error)),("Resources exhausted with no documents in "
                               "cache"));
      _lock.unlock();
      DEBUGACTION(ResourceAccounting,conform,end);
      return;
    } else {
      _users.erase(lru);
      for(gmap<const apr_uint16_t,apr_int64_t>::iterator iter = lru->_usage.begin();
          iter != lru->_usage.end();
          ++iter)
        _available[iter->first] += iter->second;
      EvictionEvent ev = lru->_user->evict(ctx, pool);
      _lock.unlock();
      GlobuleEvent::submit(ev);
    }
  }
}
