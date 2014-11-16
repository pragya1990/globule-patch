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
#include "dns_policy_wrand.hpp"

using std::vector;

#if (APR_MAJOR_VERSION > 0)
apr_random_t* WeightedRandomRedirectPolicy::_prng = 0;
#endif

WeightedRandomRedirectPolicy::WeightedRandomRedirectPolicy(apr_uint32_t ttl)
  throw()
  : RedirectPolicy("WeightedRandomRedirectPolicy",ttl)
{
}

WeightedRandomRedirectPolicy::~WeightedRandomRedirectPolicy() throw()
{
}

RedirectPolicy*
WeightedRandomRedirectPolicy::instantiate() throw()
{
  WeightedRandomRedirectPolicy* policy;
  policy = new(rmmmemory::shm()) WeightedRandomRedirectPolicy(_ttl);
  policy->_policyname = _policyname;
  return policy;
}

void
WeightedRandomRedirectPolicy::release() throw()
{
  rmmmemory::destroy(this); // operator delete(this, rmmmemory::shm());
}

char*
WeightedRandomRedirectPolicy::initialize(dns_config* cfg, apr_pool_t* p)
  throw()
{
#if (APR_MAJOR_VERSION > 0)
  _prng = apr_random_standard_new(p);
#endif
  return 0;
}

void
WeightedRandomRedirectPolicy::sync(const vector<Peer*>& current,
                                   const vector<Peer*>& affected) throw()
{
}

void
WeightedRandomRedirectPolicy::run(apr_uint32_t from, int rtcount,
                                      vector<Peer*>& list,
                                      dns_config* cfg, apr_pool_t* p) throw()
{
  int chosenweight, totalweight = 0;
  std::list<Peer*> rtnlist;
  static unsigned int randomnumber = 0;

  for(vector<Peer*>::iterator iter=list.begin(); iter!=list.end(); ++iter)
    totalweight += (*iter)->weight();

  do {
#if (APR_MAJOR_VERSION > 0)
    apr_random_insecure_bytes(_prng, &randomnumber, sizeof(randomnumber));
#else
    apr_generate_random_bytes((unsigned char*)&randomnumber,
                              sizeof(randomnumber));
#endif
    chosenweight = randomnumber % totalweight;
    for(vector<Peer*>::iterator iter=list.begin(); iter!=list.end(); ++iter)
      if((*iter)->weight() < chosenweight) {
        rtnlist.push_back(*iter);
        list.erase(iter);
        --rtcount;
        totalweight -= (*iter)->weight();
        break;
      } else
        chosenweight -= (*iter)->weight();
  } while(rtcount > 0 && list.size() > 0);

  list.clear();
  for(std::list<Peer*>::iterator iter=rtnlist.begin();
      iter!=rtnlist.end();
      ++iter)
    list.push_back(*iter);
}

void
WeightedRandomRedirectPolicy::finish(dns_config* cfg, apr_pool_t* p)
  throw()
{
}

void
WeightedRandomRedirectPolicy::destroy() throw()
{
}
