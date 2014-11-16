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
#include <httpd.h>
#ifndef WIN32
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif
#include "utilities.h"
#include "alloc/Allocator.hpp"
#include "Peer.hpp"
#include "Handler.hpp"
#include "BaseHandler.hpp"
#include "SourceHandler.hpp"

Contact::Contact(const Contact& org)
  throw()
  : _type(org._type), _weight(org._weight),
    _location(org._location), _password(org._password),
    _disabled(org._disabled)
{
}

Contact::Contact(apr_pool_t* p, apr_uint16_t type,
                 const char* location, const char* password,
                 bool disabled)
  throw()
  : _type(type), _weight(disabled?0:1),
    _location(location), _password(password),
    _disabled(disabled)
{
}

Contact::Contact(apr_pool_t* p, apr_uint16_t type,
                 const char* location, const char* password,
                 apr_uint16_t weight)
  throw()
  : _type(type), _weight(weight),
    _location(location), _password(password),
    _disabled(false)
{
}

Contact::~Contact() throw()
{
}

Input&
Contact::operator>>(Input& in) throw()
{
  return in >> _type >> _weight >> _location >> _password >> _disabled;
}

Output&
Contact::operator<<(Output& out) const throw()
{
  return out << _type << _weight << _location << _password << _disabled;
}

gContact::gContact()
  throw()
  : _type(Peer::NONE), _weight(1), _location(""), _password(""),
    _disabled(false)
{
}

gContact::gContact(const Contact& org)
  throw()
  : _type(org._type), _weight(org._weight),
    _location(org._location.c_str()), _password(org._password.c_str()),
    _disabled(org._disabled)
{
}

gContact::~gContact() throw()
{
}

Input&
gContact::operator>>(Input& in) throw()
{
  return in >> _type >> _weight >> _location >> _password >> _disabled;
}

Output&
gContact::operator<<(Output& out) const throw()
{
  return out << _type << _weight << _location << _password << _disabled;
}

void
Peer::initialize(Context* ctx, apr_pool_t* p) throw()
{
  apr_status_t status;
  apr_sockaddr_t *sa = NULL;
  status = apr_sockaddr_info_get(&sa, _uri.host(), AF_UNSPEC, 0, 0, p);
  if(status != APR_SUCCESS) {
    DIAG(ctx,(MONITOR(error)),("Cannot resolve IP address of peer hostname %s\n",_uri.host()));
    _addr = APR_INADDR_NONE;
  } else
    _addr = sa->sa.sin.sin_addr.s_addr;
}

void
Peer::merge(Context* ctx, apr_pool_t* p, const Peer* r) throw()
{
  if(r->_configured._password == _secret && _secret != r->_secret)
    _secret = r->_secret;
  _serial = r->_serial;
  if(r->_configured._weight == _configured._weight)
    _weight = r->_weight;
  if(r->_configured._disabled == _configured._disabled)
    enabled = r->enabled;
  initialize(ctx, p);
}

Peer::Peer(const Peer& r)
  throw()
  : Persistent("Peer"), _configured(r._configured), _type(r._type), _weight(r._weight),
    _uri(r._uri), _secret(r._secret), _addr(r._addr), _master(r._master),
    _serial(apr_time_now()),
    enabled(r.enabled), _monitor(r._monitor)
{
}

Peer::Peer()
  throw()
  : Persistent("Peer"), _type(NONE), _weight(1), _uri(), _master(false),
    _serial(apr_time_now()), enabled(0), _monitor(0)
{
}

Peer::Peer(Context* ctx, apr_pool_t* p, Monitor* mon, const Contact& def,
           bool fast)
  throw()
  : Persistent("Peer"), _configured(def), _type(def._type), _weight(def._weight),
    _uri(p,def._location), _addr(APR_INADDR_NONE), _master(false),
    _serial(apr_time_now()),
    enabled(0), _monitor(mon)
{
  _secret = def._password.c_str();
  if(def._disabled)
    enabled = -1;
  if(!fast)
    initialize(ctx, p);
}

Peer::Peer(Context* ctx, apr_pool_t* p, Monitor* monitor,
                 SourceHandler* self, apr_uint16_t weight)
  throw()
  : Persistent("Peer"),
    _configured(Contact(p, Peer::ORIGIN, self->location(p), "", weight)),
    _type(ORIGIN), _weight(weight), _uri(self->location()), _master(true),
    _serial(apr_time_now()), enabled(1), _monitor(monitor)
{
  _secret = "";
  initialize(ctx, p);
}

Peer::~Peer() throw()
{
}

Persistent*
Peer::instantiateClass() throw()
{
  return new(rmmmemory::shm()) Peer();
}

Input&
Peer::operator>>(Input& in) throw(FileError,WrappedError)
{
  in >> _type >> _weight >> _uri >> _secret >> _master >> _serial
     >> enabled >> _configured;
  initialize(in.context(), in.pool());
  return in;
}

Output&
Peer::operator<<(Output& out) const throw(WrappedError,FileError)
{
  out << _type << _weight << _uri << _secret << _master << _serial
      << enabled << _configured;
  return out;
}

bool
Peer::operator==(const BaseHandler& other) const throw()
{
  if(other.location() == _uri)
    return _master;
  else
    return false;
}

const bool
Peer::authenticate(apr_uint32_t addr, apr_port_t port, const char* secret) throw()
{
  if(addr == APR_INADDR_NONE)
    return false;
  return addr == _addr && !strcmp(secret,_secret.c_str());
}

const bool
Peer::authenticate(const char* remote_ipaddr, apr_port_t remote_port,
                      const char* secret) throw()
{
  return authenticate(apr_inet_addr(remote_ipaddr), remote_port, secret);
}

apr_uint32_t
Peer::address(Context* ctx, apr_pool_t* p) throw()
{
  if(_addr == APR_INADDR_NONE)
    initialize(ctx, p);
  return _addr;
}

void
Peer::setserial(apr_time_t serial) throw()
{
  _serial = serial;
}

Peers::Peers() throw()
{
}

Peers::~Peers() throw()
{
}

void
Peers::erase(Peers::iterator& iter) throw()
{
  _peers.erase(iter._curr);
}

Peer*
Peers::add(Context* ctx, apr_pool_t* p, const Contact& contact) throw()
{
  Monitor* m;
  m = new(rmmmemory::shm()) Monitor(ctx, rmmmemory::allocate(contact._location.c_str()));
  _peers.push_back(Peer(ctx, p, m, contact));
  return &_peers.back();
}

Peer*
Peers::add(Context* ctx, apr_pool_t* p, SourceHandler* self, apr_uint16_t weight) throw()
{
  Monitor* m = new(rmmmemory::shm()) Monitor(ctx, rmmmemory::allocate(((Handler*)self)->location(p)));
  _peers.push_back(Peer(ctx, p, m, self, weight));
  return &_peers.back();
}

Peer*
Peers::add(const Peer& peer) throw()
{
  _peers.push_back(peer);
  return &_peers.back();
}

Peers::iterator
Peers::begin(apr_uint16_t type) throw()
{
  return iterator(*this, type);
}

Peers::const_iterator
Peers::begin(apr_uint16_t type) const throw()
{
  return const_iterator(*this, type);
}

Peers::iterator
Peers::end() throw()
{
  return iterator(*this,_peers.end());
}

Peers::const_iterator
Peers::end() const throw()
{
  return const_iterator(*this,_peers.end());
}

Peer*
Peers::origin() throw()
{
  Peers::iterator iter = iterator(*this,Peer::ORIGIN);
  return iter != end() ? &*iter : 0;
}

const Peer*
Peers::origin() const throw()
{
  Peers::const_iterator iter = const_iterator(*this,Peer::ORIGIN);
  return iter != end() ? &*iter : 0;
}

Peers::iterator
Peers::find(const Url& u) throw()
{
  return iterator(*this, u);
}

int
Peers::nreplicas() const throw()
{
  int count = 0;
  for(gvector<Peer>::const_iterator iter = _peers.begin();
      iter != _peers.end();
      ++iter)
    if((iter->_type & (Peer::REPLICA|Peer::MIRROR|Peer::ORIGIN)) != 0)
      ++count;
  return count;
}


Input&
operator>>(Input& in, Peers& peers) throw(FileError,WrappedError)
{
  apr_size_t npeers;
  in >> npeers;
  for(apr_size_t i=0; i<npeers; i++) {
    Peer replicard;
    Peer* replicais;
    in >> replicard;
    Peers::iterator iter = peers.find(replicard.uri());
    if(iter != peers.end() && iter->type() == replicard.type()) {
      replicais = &*iter;
      replicais->merge(in.context(), in.pool(), &replicard);
    } else {
      replicais = peers.add(replicard);
      replicais->enabled = -2; // permanently disable and mark for deletion
    }
    in.overrideObject(replicais);
  }
  return in;
}

Output&
operator<<(Output& out, const Peers& peers) throw(WrappedError,FileError)
{
  apr_size_t npeers = peers._peers.size();
  out << npeers;
  for(Peers::const_iterator iter=peers.begin();
      iter!=peers.end();
      ++iter)
    out << *iter;
  return out;
}
