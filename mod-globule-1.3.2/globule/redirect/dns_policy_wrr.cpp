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
#include <httpd.h>
#include <apr.h>
#include <apr_network_io.h>
#include "dns_policy_wrr.hpp"

using std::vector;

WeightedRoundRobinRedirectPolicy::WeightedRoundRobinRedirectPolicy(apr_uint32_t ttl)
  throw()
  : RedirectPolicy("WeightedRoundRobinRedirectPolicy",ttl), _cyclecount(0)
{
}

WeightedRoundRobinRedirectPolicy::~WeightedRoundRobinRedirectPolicy() throw()
{
}

RedirectPolicy*
WeightedRoundRobinRedirectPolicy::instantiate() throw()
{
  WeightedRoundRobinRedirectPolicy* policy;
  policy = new(rmmmemory::shm()) WeightedRoundRobinRedirectPolicy(_ttl);
  policy->_policyname = _policyname;
  return policy;
}

void
WeightedRoundRobinRedirectPolicy::release() throw()
{
  rmmmemory::destroy(this); // operator delete(this, rmmmemory::shm());
}

char*
WeightedRoundRobinRedirectPolicy::initialize(dns_config* cfg, apr_pool_t* p)
  throw()
{
  return 0;
}

void
WeightedRoundRobinRedirectPolicy::sync(const vector<Peer*>& current,
                                       const vector<Peer*>& cleared) throw()
{
  _cyclecount = 0;
}

void
WeightedRoundRobinRedirectPolicy::run(apr_uint32_t from, int rtcount,
                                      vector<Peer*>& list,
                                      dns_config* cfg, apr_pool_t* p) throw()
{
  int currweight;
  vector<Peer*> rtnlist;
  if(rtcount > (signed) list.size())
    rtcount = list.size();
  if(rtcount > 0) {
    vector<Peer*>::iterator iter = list.begin();
    currweight = _cyclecount;
    while(iter != list.end() && currweight > (*iter)->weight()) {
      currweight  -= (*iter)->weight();
      ++iter;
    }
    if(iter != list.end()) {
      rtnlist.push_back(*iter);
      --rtcount;
      ++currweight;
      ++_cyclecount;
      ++iter;
    }
    if(iter == list.end())
      _cyclecount = 0;
    while(rtcount-- > 0) {
      if(iter == list.end())
        iter = list.begin();
      rtnlist.push_back(*iter);
    }
  }
  list = rtnlist;
}

void
WeightedRoundRobinRedirectPolicy::finish(dns_config* cfg, apr_pool_t* p)
  throw()
{
}

void
WeightedRoundRobinRedirectPolicy::destroy() throw()
{
}
