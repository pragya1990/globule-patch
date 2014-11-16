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
#ifndef _DNS_POLICY_HPP
#define _DNS_POLICY_HPP

#include <vector>
#include <string>
#include <httpd.h>
#include <apr.h>
#include <apr_network_io.h>
#include <apr_pools.h>
#include "mod_netairt.h"
#include "dns_config.h"
#include "resource/Peer.hpp"

class RedirectPolicy : public Persistent
{
  static RedirectPolicy* _default_policy;
  static gmap<const gstring,RedirectPolicy*>* _policies;
protected:
  gstring _policyname;
  apr_uint32_t _ttl; // Time-To-Live in seconds
public:
  RedirectPolicy(const char* classname, apr_uint32_t ttl) throw();
  virtual ~RedirectPolicy() throw();
  virtual Persistent* instantiateClass() throw() { return instantiate(); };
  virtual Input& operator>>(Input& i) throw() {
    return Persistent::operator>>(i);
  };
  virtual Output& operator<<(Output& o) const throw() {
    return Persistent::operator<<(o);
  };
  /* Instantiate is called whenever a policy object is needed to work
   * with in an actual environment.  If no state is required, this
   * would normally return the template object.  However if the policy
   * object requires state, it might clone the template object and
   * return a new parameterized version of itself.
   * When 
   */
  virtual RedirectPolicy* instantiate() throw() { return this; };

  /* Release is called to give back a previous retrieved copy as
   * returned by instantiate()
   */
  virtual void release() throw() { };

  /* initialize is called on every template object to do internal
   * initialization before the server has been properly started.
   * After this initialization, no more template policies may be entered
   */
  virtual char* initialize(dns_config* cfg, apr_pool_t* p) throw() = 0;
  void sync(const std::vector<Peer*>& current, Peer*) throw();
  virtual void sync(const std::vector<Peer*>& current,
                    const std::vector<Peer*>& cleared) throw() = 0;

  /* A policy run() requests the policy to select one or more replica
   * peers from a list of available servers.
   */
  virtual void run(apr_uint32_t from, int rtcount,
                   std::vector<Peer*>&,
                   dns_config* cfg, apr_pool_t* p) throw() = 0;

  /* Finish should be called after a policy run() has been dealt with.
   * the finish call might clean things up, or re-read configuration data
   * and may thus take a long time.  Therefor this should be called in the
   * post-request phase (wrapup).
   */
  virtual void finish(dns_config* cfg, apr_pool_t* p) throw() = 0;

  /* Destroy is called to dispose of the template object */
  virtual void destroy() throw() = 0;

  apr_uint32_t ttl() throw() { return _ttl; };
  virtual void ttl(apr_uint32_t ttl) throw() { _ttl = ttl; };
  inline const char* policyName() const throw() { return _policyname.c_str(); };
  static void registerPolicy(std::string name, RedirectPolicy* policy) throw();
  static RedirectPolicy* lookupPolicy(std::string name) throw();
  static RedirectPolicy* defaultPolicy() throw();
  static char* initializeAll(dns_config* cfg, apr_pool_t* p) throw();
  static void finishAll(dns_config* cfg, apr_pool_t* p) throw();
  static void destroyAll(dns_config* cfg, apr_pool_t* p) throw();
};

class StaticRedirectPolicy : public RedirectPolicy
{
public:
  StaticRedirectPolicy(apr_uint32_t ttl) throw();
  virtual char* initialize(dns_config* cfg, apr_pool_t* p) throw();
  virtual void sync(const std::vector<Peer*>&, const std::vector<Peer*>&) throw();
  virtual void run(apr_uint32_t from, int rtcount,
                   std::vector<Peer*>&,
                   dns_config* cfg, apr_pool_t* p) throw();
  virtual void finish(dns_config* cfg, apr_pool_t* p) throw();
  virtual void destroy() throw();
};

#endif /* _DNS_POLICY_HPP */
