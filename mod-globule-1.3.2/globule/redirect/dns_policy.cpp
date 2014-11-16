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
#include <vector>
#include <map>
#include <httpd.h>
#include <apr.h>
#include <apr_network_io.h>
#include <apr_strings.h>
#include "dns_policy.hpp"

using std::string;
using std::map;
using std::vector;

gmap<const gstring,RedirectPolicy*>* RedirectPolicy::_policies;
RedirectPolicy* RedirectPolicy::_default_policy = 0;

RedirectPolicy::RedirectPolicy(const char* classname, apr_uint32_t ttl)
  throw()
  : Persistent(classname), _ttl(ttl)
{ }

RedirectPolicy::~RedirectPolicy() throw()
{
}

void
RedirectPolicy::registerPolicy(string name, RedirectPolicy* policy) throw()
{
  if(_policies == 0)
    _policies = new(rmmmemory::shm()) gmap<const gstring,RedirectPolicy*>;
  (*_policies)[name.c_str()] = policy;
  policy->_policyname = name.c_str();
  if(!_default_policy)
    _default_policy = policy;
}

RedirectPolicy*
RedirectPolicy::lookupPolicy(string name) throw()
{
  gmap<const gstring,RedirectPolicy*>::iterator iter=_policies->find(name.c_str());
  if(iter != _policies->end())
    return iter->second->instantiate();
  return 0;
}

RedirectPolicy*
RedirectPolicy::defaultPolicy() throw()
{
  return _default_policy->instantiate();
}

char*
RedirectPolicy::initializeAll(dns_config* cfg, apr_pool_t* p) throw()
{
  char* msg;
  char* rtnvalue = 0;
  for(gmap<const gstring,RedirectPolicy*>::iterator iter=_policies->begin();
      iter != _policies->end();
      ++iter)
    if((msg = iter->second->initialize(cfg, p)))
      rtnvalue = msg;
  return rtnvalue;
}

void
RedirectPolicy::finishAll(dns_config* cfg, apr_pool_t* p) throw()
{
  for(gmap<const gstring,RedirectPolicy*>::iterator iter=_policies->begin();
      iter != _policies->end();
      ++iter)
    iter->second->finish(cfg, p);
}


void
RedirectPolicy::sync(const std::vector<Peer*>& current, Peer* peer) throw()
{
  vector<Peer*> affected;
  affected.push_back(peer);
  sync(current, affected);
}

StaticRedirectPolicy::StaticRedirectPolicy(apr_uint32_t ttl)
  throw()
  : RedirectPolicy("StaticRedirectPolicy", ttl)
{
}

char*
StaticRedirectPolicy::initialize(dns_config* cfg, apr_pool_t* p) throw()
{
  return 0;
}

void
StaticRedirectPolicy::sync(const vector<Peer*>& current,
                           const vector<Peer*>& affected) throw()
{
}

void
StaticRedirectPolicy::run(apr_uint32_t from, int rtcount,
                          vector<Peer*>& list,
                          dns_config* cfg, apr_pool_t* p) throw()
{
  vector<Peer*> rtnlist;
  for(vector<Peer*>::const_iterator iter=list.begin();
      iter != list.end() && rtcount > 0;
      ++iter, --rtcount)
    rtnlist.push_back(*iter);
  list = rtnlist;
}

void
StaticRedirectPolicy::finish(dns_config* cfg, apr_pool_t* p) throw()
{
}

void
StaticRedirectPolicy::destroy() throw()
{
}
