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
#ifndef _DNS_POLICY_BALANCED_HPP
#define _DNS_POLICY_BALANCED_HPP

#include "dns_policy_as.hpp"

class BalancedRedirectPolicy : public AutonomousSystemRedirectPolicy
{
private:
  gmap<const Peer* const,int> _load;
  int _totalload;
  int _totalweight;

  double _initload; /* valid range 0.00 <= x <= 1.00 */
  double _maxload;  /* valid range 1.00 <  x (normally maximal 2.00) */
  static const int _minrequest = 5;
public:
  BalancedRedirectPolicy(apr_uint32_t ttl) throw();
  virtual ~BalancedRedirectPolicy() throw();
  virtual RedirectPolicy* instantiate() throw();
  virtual void release() throw();
  virtual void sync(const std::vector<Peer*>&, const std::vector<Peer*>&) throw();
  virtual void run(apr_uint32_t from, int rtcount,
                   std::vector<Peer*>&,
                   dns_config* cfg, apr_pool_t* p) throw();
};

#endif /* _DNS_POLICY_BALANCED_HPP */
