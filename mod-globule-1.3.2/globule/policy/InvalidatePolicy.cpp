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
#include <apr_strings.h>
#include "utilities.h"
#include "monitoring.h"
#include "InvalidatePolicy.hpp"
#include "Constants.hpp"
#include "event/GlobuleEvent.hpp"
#include "event/HttpMetaEvent.hpp"
#include "psodium.h"

InvalidatePolicy::InvalidatePolicy()
  throw()
  : ReplPolicy("InvalidatePolicy", MONITOR(policyinvalidate))
{
}

InvalidatePolicy::~InvalidatePolicy() throw()
{
}

ReplPolicy*
InvalidatePolicy::instantiatePolicy() throw()
{
  return new(rmmmemory::shm()) InvalidatePolicy();
}

Input&
InvalidatePolicy::operator>>(Input& in) throw()
{
  ReplPolicy::operator>>(in);
  apr_int32_t sz;
  in >> InputWrappedPersistent<apr_int32_t>("nreplicas",sz);
  _replicas.clear();
  for(long i=0; i<sz; i++) {
    Peer* tmp = 0;
    in >> (Persistent*&) tmp;
    if(tmp->enabled >= -1)
      _replicas.insert(tmp);
  }
  _loaded = false;
  return in;
}

Output&
InvalidatePolicy::operator<<(Output& out) const throw()
{
  ReplPolicy::operator<<(out);
  apr_int32_t sz = _replicas.size();
  out << OutputWrappedPersistent<apr_int32_t>("nreplicas",sz);
  for(ReplSet::const_iterator iter = _replicas.begin();
      iter != _replicas.end();
      ++iter)
    out << (*iter);
  return out;
}

ReplAction
InvalidatePolicy::onUpdate(Element* doc) throw()
{
  _loaded = false;

#ifdef PSODIUM
// Setting expiry time to something finite and communicating
// that to pSodium is done in document.cpp: Element::invalidate()
#endif
#ifdef NOTDEFINED
  if(!((FileMonitorEvent&)e).lastmod()) {
    /* file modification time 0 signals deletion of document */
    return ReplAction(ReplAction::INVALIDATE_DOCS|ReplAction::UNREG_FILE_UPDATE&_replicas);
  }
#endif
  return ReplAction(ReplAction::INVALIDATE_DOCS|ReplAction::UNREG_FILE_UPDATE, &_replicas);
}

ReplAction
InvalidatePolicy::onSignal(Element* doc) throw()
{
  //    hme = (const HttpMetaEvent *) &e;
  //    if (hme->reqtype()=="invalidate") {
  _loaded = false;
  //    }
  return ReplAction();
}

ReplAction
InvalidatePolicy::onAccess(Element* doc, Peer* from) throw()
{
  if(!isslave(doc)) {
    if(from) {
      _replicas.insert(from);
    }

#ifdef PSODIUM
    DOUT(DEBUG5, "G2P: Invalidate policy setting *infinite* expiry time for pSodium security to use\n");
#endif
    apr_time_t infinite_time = INFINITE_TIME;
    if(!_loaded) {
      _loaded = true;
      return ReplAction(ReplAction::FROM_APACHE | ReplAction::DELIVER_DISCARD | ReplAction::REG_FILE_UPDATE | ReplAction::PSODIUM_EXPIRE, infinite_time);
    } else
      return ReplAction(ReplAction::FROM_APACHE | ReplAction::DELIVER_DISCARD | ReplAction::PSODIUM_EXPIRE, infinite_time);
  } else
    if (_loaded)
      return ReplAction(ReplAction::FROM_DISK | ReplAction::DELIVER_DISCARD);
    else
      return ReplAction(ReplAction::FROM_GET  | ReplAction::DELIVER_SAVE);
}

void
InvalidatePolicy::onLoaded(Element* doc, long sz, long lastmod) throw()
{
  _loaded = true;
}

ReplAction
InvalidatePolicy::onPurge(Element* doc) throw()
{
  if(ismaster(doc) && !_replicas.empty()) {
    _loaded = false;
    return ReplAction(ReplAction::INVALIDATE_DOCS | ReplAction::UNREG_FILE_UPDATE, &_replicas);
  }
  return ReplAction();
}

void
InvalidatePolicy::report(ReportEvent& evt) throw()
{
  int i = 0;
  evt.setProperty("nreplicas", mkstring()<<_replicas.size());
  for(ReplSet::iterator iter = _replicas.begin();
      iter != _replicas.end();
      ++iter,++i)
    evt.setProperty(mkstring()<<"replica"<<i, (*iter)->uri()(evt.pool));
}
