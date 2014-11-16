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
#ifndef _PEER_HPP
#define _PEER_HPP

#include "monitoring.h"
#include "netw/Url.hpp"

class SourceHandler; // forward declaration
class BaseHandler;
class Peer;
class Peers;
class gContact;

class Contact : public Serializable
{
  friend class Peer;
  friend class Peers;
  friend class gContact;
private:
  apr_uint16_t _type;
  apr_uint16_t _weight;
  std::string  _location;
  std::string  _password;
  bool         _disabled;
public:
  Contact() throw();
  Contact(const Contact& org) throw();
  Contact(apr_pool_t*, apr_uint16_t type, const char* location, const char* password, bool disabled=false) throw();
  Contact(apr_pool_t*, apr_uint16_t type, const char* location, const char* password, apr_uint16_t weight) throw();
  virtual ~Contact() throw();
  virtual Input& operator>>(Input&) throw();
  virtual Output& operator<<(Output&) const throw();
};

class gContact : public Serializable
{
  friend class Peer;
  friend class Peers;
private:
  apr_uint16_t _type;
  apr_uint16_t _weight;
  gstring      _location;
  gstring      _password;
  bool         _disabled;
public:
  gContact() throw();
  gContact(const Contact& org) throw();
  ~gContact() throw();
  virtual Input& operator>>(Input&) throw();
  virtual Output& operator<<(Output&) const throw();
};

class Peer : public Persistent
{
  friend class OriginHandler; // FIXME: should have some re-initialize function
  friend class BaseHandler; // FIXME: should have some re-initialize function
  friend class Peers;
  friend Input& operator>>(Input& in, Peers& peers) throw(FileError,WrappedError);
public:
  static const int NONE       = 0x00;
  static const int REDIRECTOR = 0x01;
  static const int KEEPER     = 0x02;
  static const int REPLICA    = 0x04;
  static const int MIRROR     = 0x08;
  static const int ORIGIN     = 0x10;
  static const int MANAGER    = 0x20;
  static const int ANY        = 0xff;
private:
  gContact      _configured;
  apr_uint16_t  _type;
  apr_uint16_t  _weight;
  gUrl          _uri;
  gstring       _secret;
  apr_uint32_t  _addr;     // FIXME: should be a string to make it IPv6
  bool          _master;
  apr_time_t    _serial;
public:
  /* The enabled field is used to indicate whether this replica is enabled.
   * However the exact value indicates how it is enabled.
   *  >1  means it should be kept enabled for N more iterations of avail chking
   *   1  means that it is enabled until the next availability check
   *   0  means it is currently disabled because availability check report
   *  -1  means it is permanently disabled (e.g. because of pSodium)
   *  -2  means it is disabled and should be removed
   */
  int      enabled;
  Monitor* _monitor;
private:
  void initialize(Context* ctx, apr_pool_t*) throw();
  Peer() throw();
public:
  Peer(const Peer&) throw();
  Peer(Context*, apr_pool_t*, Monitor*, const Contact&, bool fast = false) throw();
  Peer(Context*, apr_pool_t*, Monitor*, SourceHandler*, apr_uint16_t weight) throw();
  ~Peer() throw();
  void merge(Context*, apr_pool_t*, const Peer*) throw();
  virtual Persistent* instantiateClass() throw();
  virtual Input& operator>>(Input&) throw(FileError,WrappedError);
  virtual Output& operator<<(Output&) const throw(WrappedError,FileError);
  inline const apr_uint16_t type() const throw() { return _type; };
  inline const gUrl& uri() const throw() { return _uri; };
  const apr_time_t serial() const throw() { return _serial; };
  void setserial(apr_time_t serial) throw();
  inline const char* location(apr_pool_t* p) const throw() { return _uri(p); };
  inline const char* secret() const throw() { return _secret.c_str(); };
  inline apr_uint16_t weight() const throw() { return _weight; };
  const bool authenticate(apr_uint32_t, apr_port_t, const char* secret) throw();
  const bool authenticate(const char*, apr_port_t, const char* secret) throw();
  apr_uint32_t address(Context* ctx, apr_pool_t* p) throw();
  bool operator==(const BaseHandler&) const throw();
  inline bool operator!=(const BaseHandler& other) throw() { return!operator==(other); };
};

class Peers
{
  friend Input& operator>>(Input&, Peers&) throw(FileError,WrappedError);
  friend Output& operator<<(Output&, const Peers&) throw(WrappedError,FileError);
protected:
  gvector<Peer> _peers;

public:

  class iterator
    : protected std::iterator_traits<gvector<Peer>::iterator>
  {
    friend class Peers;
  private:
    apr_uint16_t _type;
    Peers* _cont;
    gvector<Peer>::iterator _curr;
  public:
    friend bool operator==(const iterator& i, const iterator& j) throw() {
      return i._cont == j._cont && i._curr == j._curr;
    }
    inline bool operator!=(const iterator& rhs) const throw() {
      return _curr != rhs._curr;
    };
    inline iterator(Peers& x, int type = Peer::ANY) throw()
      : _type(type), _cont(&x), _curr(x._peers.begin()) {
      while(*this != _cont->end() && (_curr->type() & _type) == 0)
	++_curr;
    };
    inline iterator(Peers& x, const Url& uri) throw()
      : _type(Peer::NONE), _cont(&x), _curr(x._peers.begin()) {
      while(*this != _cont->end() && !(_curr->uri() == uri))
        ++_curr;
    };
    inline iterator(Peers& x, gvector<Peer>::iterator iter) throw()
      : _type(Peer::ANY), _cont(&x), _curr(iter) {
    };
    inline reference operator*() const throw() {
      return *_curr;
    };
    inline pointer operator->() const throw() {
      return &*_curr;
    };
    inline iterator& operator++() throw() {
      do {
          ++_curr;
      } while(*this!=_cont->end() && (_curr->type() & _type) == 0);
      return *this;
    };
    inline iterator operator++(int) throw() {
      iterator tmp = *this;
      ++*this;
      return tmp;
    };
  };

  class const_iterator
    : protected std::iterator_traits<gvector<Peer>::const_iterator>
  {
    friend class Peers;
  private:
    int _type;
    const Peers* _cont;
    gvector<Peer>::const_iterator _curr;
  public:
    friend bool operator==(const const_iterator& i, const const_iterator& j) throw() {
      return i._cont == j._cont && i._curr == j._curr;
    }
    inline bool operator!=(const const_iterator& rhs) const throw() {
      return _curr != rhs._curr;
    };
    inline const_iterator(const Peers& x, int type = Peer::ANY) throw()
      : _type(type), _cont(&x), _curr(x._peers.begin()) {
      while(*this != _cont->end() && (_curr->type() & _type) == 0)
	++_curr;
    };
    inline const_iterator(const Peers& x,gvector<Peer>::const_iterator iter) throw()
      : _type(Peer::ANY), _cont(&x), _curr(iter) {
    };
    inline reference operator*() const throw() {
      return *_curr;
    };
    inline pointer operator->() const throw() {
      return &*_curr;
    };
    inline const_iterator& operator++() throw() {
      do {
        ++_curr;
      } while(*this!=_cont->end() && (_curr->type() & _type) == 0);
      return *this;
    };
    inline const_iterator operator++(int) throw() {
      const_iterator tmp = *this;
      ++*this;
      return tmp;
    };
  };

  Peers() throw();
  ~Peers() throw();
  void erase(Peers::iterator& iter) throw();
  int  nreplicas() const throw();
  iterator        begin(apr_uint16_t type = Peer::ANY) throw();
  const_iterator  begin(apr_uint16_t type = Peer::ANY) const throw();
  iterator        end() throw();
  const_iterator  end() const throw();
  iterator        find(const Url& u) throw();
  Peer*           origin() throw();
  const Peer*     origin() const throw();
  Peer* add(const Peer&) throw();
  Peer* add(Context*, apr_pool_t*, const Contact&) throw();
  Peer* add(Context*, apr_pool_t*, SourceHandler*, apr_uint16_t weight) throw();
};

extern Input& operator>>(Input&, Peers&) throw(FileError,WrappedError);
extern Output& operator<<(Output&, const Peers&) throw(WrappedError,FileError);

#endif
