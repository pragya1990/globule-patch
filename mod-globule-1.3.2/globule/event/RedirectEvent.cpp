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
#include "utilities.h"
#include "RedirectEvent.hpp"
/* Required for inet_addr, and ns_t_* not always properly included
 * by APR libraries.
 */
#ifndef WIN32
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "compat/Win32/arpa/nameser.h"
#else
#include <arpa/nameser.h>
#endif

using std::vector;

const int DNSRecord::TYPE_A = ns_t_a;
const int DNSRecord::TYPE_CNAME = ns_t_cname;
const int gDNSRecord::TYPE_A = ns_t_a;
const int gDNSRecord::TYPE_CNAME = ns_t_cname;

gDNSRecord::gDNSRecord(const DNSRecord* r) throw()
  : _name(r->_name), _type(r->_type), _addr(r->_addr),
    _cname(r->_cname?r->_cname:"")
{
}

gDNSRecord::gDNSRecord(const DNSRecord& r) throw()
  : _name(r._name), _type(r._type), _addr(r._addr),
    _cname(r._cname?r._cname:"")
{
}

gDNSRecord::gDNSRecord(const char* name, apr_uint32_t address) throw()
  : _name(name), _type(TYPE_A), _addr(address)
{
}

gDNSRecord::gDNSRecord(const char* name, const char* cname) throw()
  : _name(name), _type(TYPE_CNAME), _addr(0), _cname(cname)
{
}

DNSRecord::DNSRecord(Context* ctx, apr_pool_t* pool, Peer* r) throw()
  : _name(r->uri().host()), _type(TYPE_A), _addr(r->address(ctx, pool)),
    _cname(0)
{
}

DNSRecord::DNSRecord(apr_pool_t* p, const gDNSRecord* r) throw()
  : _name(0), _type(r->_type), _addr(r->_addr), _cname(0)
{
  _name = apr_pstrdup(p, r->_name.c_str());
  if(_type==TYPE_CNAME)
    _cname = apr_pstrdup(p, r->_cname.c_str());
}

DNSRecord::DNSRecord(apr_pool_t* p, const gDNSRecord& r) throw()
  : _name(0), _type(r._type), _addr(r._addr), _cname(0)
{
  _name = apr_pstrdup(p, r._name.c_str());
  if(_type==TYPE_CNAME)
    _cname = apr_pstrdup(p, r._cname.c_str());
}

DNSRecord::DNSRecord(apr_pool_t* p, const char* name, apr_uint32_t address) throw()
  : _name(apr_pstrdup(p,name)), _type(TYPE_A), _addr(address), _cname(0)
{
}

DNSRecord::DNSRecord(apr_pool_t* p, const char* name, const char* cname) throw()
  : _name(apr_pstrdup(p,name)), _type(TYPE_CNAME), _addr(0),
    _cname(apr_pstrdup(p,cname))
{
}

bool
RedirectEvent::asynchronous() throw()
{
  return false;
}

apr_uint32_t
RedirectEvent::remoteAddress() throw()
{
  apr_uint32_t client_ipaddr = _remoteAddress;
  if(_request) {
      // Compensate for Apache bug on Solaris 8?
    if(_request->connection->remote_addr->sa.sin.sin_addr.s_addr == 0L) {
      /* THERE IS NO SUPPORT FOR IPV6 so use normal IPv4 functions
       * int = inet_pton(AF_INET, r->connection->remote_ip, &client_ipaddr);
       */
      client_ipaddr = apr_inet_addr(_request->connection->remote_ip);
      if(client_ipaddr == APR_INADDR_NONE) {
        /* Errr.. It's either IPv6 (not supported by NetAirt) or a bad IPv4,
         * let's not redirect and handle this request ourselves.
         */
        client_ipaddr = 0;
      }
    } else
      memcpy(&client_ipaddr,
             &_request->connection->remote_addr->sa.sin.sin_addr,
             sizeof(apr_uint32_t));
  }
  return client_ipaddr;
}

void
RedirectEvent::callback(Context* ctx, apr_pool_t* pool, int count,
                        vector<Peer*>& replicas, RedirectPolicy* policy) throw()
{
  if(_callback) {
    vector<DNSRecord> records;
    for(vector<Peer*>::iterator iter = replicas.begin();
        iter != replicas.end();
        ++iter)
      records.push_back(DNSRecord(ctx, pool, *iter));
    _callback(_callback_data, count, records, policy);
  }
}

void
RedirectEvent::callback(int count, const DNSRecord answer,
                        RedirectPolicy* policy) throw()
{
  if(_callback) {
    vector<DNSRecord> records;
    records.push_back(answer);
    _callback(_callback_data, count, records, policy);
  }
}
