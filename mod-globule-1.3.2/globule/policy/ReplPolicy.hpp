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
#ifndef _REPLPOLICY_HPP
#define _REPLPOLICY_HPP

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <httpd.h>
#include "utilities.h"
#include "alloc/Allocator.hpp"
#include "Storage.hpp"
#include "monitoring.h"
#include "event/GlobuleEvent.hpp"
#include "event/ReportEvent.hpp"
#include "resource/Peer.hpp"

typedef gset<Peer*> ReplSet;

class ReplAction
{
private:
  static const apr_uint32_t FROM_MASK    = 0x000000FF;
  static const apr_uint32_t DELIVER_MASK = 0x0000FF00;
  static const apr_uint32_t OPTION_MASK  = 0x00FF0000;
public:
  static const apr_uint32_t FROM_GET     = 0x00000001;
  static const apr_uint32_t FROM_GET_IMS = 0x00000002;
  static const apr_uint32_t FROM_DISK    = 0x00000004;
  static const apr_uint32_t FROM_APACHE  = 0x00000008;
  static const apr_uint32_t DELIVER_SAVE    = 0x00000100;
  static const apr_uint32_t DELIVER_DISCARD = 0x00000200;
  static const apr_uint32_t REG_FILE_UPDATE   = 0x00010000;
  static const apr_uint32_t UNREG_FILE_UPDATE = 0x00020000;
  static const apr_uint32_t INVALIDATE_DOCS   = 0x00040000;
  static const apr_uint32_t PSODIUM_EXPIRE    = 0x00080000;
private:
  apr_int32_t _flags;
  apr_time_t  _ims;   // If-Modified-Since predicate time
  ReplSet*    _inv;
  apr_time_t  _psodium_expire;
public:
  inline ReplAction(apr_int32_t flags=0, ReplSet* inv=0, apr_time_t expire=0) throw()
    : _flags(flags), _ims(0), _inv(inv), _psodium_expire(expire) { };
  inline ReplAction(apr_int32_t flags, apr_time_t expire) throw()
    : _flags(flags), _ims(0), _inv(0), _psodium_expire(expire) { };
  inline apr_uint32_t from()    const throw() { return _flags&FROM_MASK; };
  inline apr_uint32_t deliver() const throw() { return _flags&DELIVER_MASK; };
  inline apr_uint32_t options() const throw() { return _flags&OPTION_MASK; };
  inline apr_time_t   ims()     const throw() { return _ims; };
  inline ReplSet *    inv()     const throw() { return _inv; };
  inline apr_time_t   expiry()  const throw() { return _psodium_expire; };
};

class Element;

class ReplPolicy : public Persistent {
  static std::map<std::string,ReplPolicy*> policies;
protected:
  bool    _loaded;
  int     _stickyness;
  gstring _policyname;
public:
  Monitor* mon;

  inline ReplPolicy(const char* clazzname, Monitor* m) throw()
    : Persistent(clazzname), _loaded(false), mon(m) { }
  virtual ~ReplPolicy() throw() { };

  virtual ReplAction onUpdate(Element*) throw() = 0;
  virtual ReplAction onSignal(Element*) throw() = 0;
  virtual ReplAction onAccess(Element*, Peer*) throw() = 0;
  virtual void onLoaded(Element*, long sz, long lastmod) throw() = 0;
  virtual ReplAction onPurge(Element*) throw() = 0;
  virtual void report(ReportEvent&) throw();

  virtual ReplPolicy* instantiatePolicy() throw() = 0;
  virtual Persistent* instantiateClass() throw();
  Output& operator<<(Output& out) const throw();
  Input& operator>>(Input& in) throw();
  static void registerPolicy(const char* name, ReplPolicy* policy) throw();
  static ReplPolicy* lookupPolicy(std::string name, int stickyness=0) throw();
  bool ismaster(Element*) const throw();
  bool isslave(Element*) const throw();
  inline const char* policyName() const throw() { return _policyname.c_str(); };
  inline int getStickyness() const throw() { return _stickyness; };
};

#endif /* _REPLPOLICY_HPP */
