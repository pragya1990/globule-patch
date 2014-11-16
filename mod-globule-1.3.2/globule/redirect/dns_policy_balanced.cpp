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
#include <list>
#include "dns_policy_balanced.hpp"

using std::vector;

BalancedRedirectPolicy::BalancedRedirectPolicy(apr_uint32_t ttl)
  throw()
  : AutonomousSystemRedirectPolicy(ttl, "BalancedRedirectPolicy"),
    _totalload(0), _totalweight(0), _initload(0.50), _maxload(1.10)
{
}

BalancedRedirectPolicy::~BalancedRedirectPolicy() throw()
{
}

RedirectPolicy*
BalancedRedirectPolicy::instantiate() throw()
{
  BalancedRedirectPolicy* policy;
  policy = new(rmmmemory::shm()) BalancedRedirectPolicy(_ttl);
  policy->_policyname = _policyname;
  return policy;
}

void
BalancedRedirectPolicy::release() throw()
{
  rmmmemory::destroy(this); // operator delete(this, rmmmemory::shm());
}

void
BalancedRedirectPolicy::sync(const vector<Peer*>& current,
                             const vector<Peer*>& affected) throw()
{
  for(vector<Peer*>::const_iterator iter = affected.begin();
      iter != affected.end();
      ++iter)
    {
      gmap<const Peer* const,int>::iterator i = _load.find(*iter);
      if(i != _load.end())
        _load.erase(i);
    }
  _totalweight = 0;
}

void
BalancedRedirectPolicy::run(apr_uint32_t from, int rtcount,
                            vector<Peer*>& list,
                            dns_config* cfg, apr_pool_t* p) throw()
{
  std::list<Peer*> rtnlist;

  if(!_totalweight)
    for(vector<Peer*>::iterator iter=list.begin(); iter!=list.end(); ++iter)
      _totalweight += (*iter)->weight();

  AutonomousSystemRedirectPolicy::run(from,
		      (rtcount<_minrequest?_minrequest:rtcount),
		      list, cfg, p);

  for(vector<Peer*>::iterator first=list.begin(); first!=list.end(); ++first) {
    gmap<const Peer* const,int>::iterator iter = _load.find(*first);
    if(iter == _load.end()) {
      _load[*first] = (int)(_initload*(*first)->weight() / (_totalweight?_totalweight:1) + 0.5);
      fprintf(stderr,"BAS selected %s (weight=%uh,total=%d) with initial load %d \n",(*first)->location(p),(*first)->weight(),_totalweight,_load[*first]);fflush(stderr);
      _totalload  += _load[*first];
      break;
    } else if(_load[*first] * _totalweight <= _maxload * (*first)->weight() * _totalload ) {
       /* If condition is: _load[*first] / _totalload <= _maxload * (*first)->weight() / _totalweight */
      fprintf(stderr,"BAS selected %s (weight=%uh,total=%d) with normal load %d \n",(*first)->location(p),(*first)->weight(),_totalweight,_load[*first]);fflush(stderr);
      _load[*first] += 1;
      _totalload    += 1;
      rtnlist.push_front(*first);
      while(++first != list.end())
	rtnlist.push_back(*first);
      break;
    } else {
      fprintf(stderr,"BAS skipping %s (weight=%uh,total=%d) with load %d \n",(*first)->location(p),(*first)->weight(),_totalweight,_load[*first]);fflush(stderr);
    }
  }

  list.clear();
  for(std::list<Peer*>::iterator iter=rtnlist.begin();
      iter!=rtnlist.end() && rtcount > 0;
      ++iter,--rtcount)
    list.push_back(*iter);

  if(_totalload > (INT_MAX>>2))
    for(gmap<const Peer* const,int>::iterator iter = _load.begin();
	iter != _load.end();
	++iter)
      iter->second >>= 2;
}
