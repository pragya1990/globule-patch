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
#include <apr.h>
#include <apr_time.h>
#include "utilities.h"
#include "monitoring.h"
#include "TtlPolicy.hpp"
#include "psodium.h"

TtlPolicy::TtlPolicy(long ttl) throw()
  : ReplPolicy("TtlPolicy", MONITOR(policyttl))
{
  // An apr_time_t is a number of microseconds
  // since 00:00:00 january 1, 1970 UTC
#ifdef PSODIUM
  DOUT(DEBUG3, "G2P: TTL policy TTL in sec is " << ttl );
#endif
  _ttl      = apr_time_from_sec(ttl);
  _loaddate = 0;
}

TtlPolicy::~TtlPolicy() throw()
{
}

ReplPolicy*
TtlPolicy::instantiatePolicy() throw()
{
  return new(rmmmemory::shm()) TtlPolicy(apr_time_sec(_ttl));
}

Input&
TtlPolicy::operator>>(Input& in) throw()
{
  ReplPolicy::operator>>(in);
  in >> InputWrappedPersistent<bool>("loaded",_loaded)
     >> _loaddate
     >> _ttl;
  return in;
}

Output&
TtlPolicy::operator<<(Output& out) const throw()
{
  ReplPolicy::operator<<(out);
  out << OutputWrappedPersistent<bool>("loaded",_loaded)
      << _loaddate
      << _ttl;
  return out;
}

ReplAction
TtlPolicy::onUpdate(Element* doc) throw()
{
  return ReplAction();
}

ReplAction
TtlPolicy::onSignal(Element* doc) throw()
{
  return ReplAction();
}

ReplAction
TtlPolicy::onAccess(Element* doc, Peer* from) throw()
{
  if(ismaster(doc)) {
    // PSODIUM
    apr_time_t expiretime = apr_time_now() + _ttl;
#ifdef PSODIUM
    DOUT(DEBUG3, "G2P: TTL policy setting expiry time " << expiretime << " for pSodium security to use, TTL in msec is " << _ttl  );
#endif
    if(_loaded)
      return ReplAction(ReplAction::FROM_APACHE|ReplAction::DELIVER_DISCARD|ReplAction::PSODIUM_EXPIRE, expiretime);
    else
      return ReplAction(ReplAction::FROM_APACHE|ReplAction::DELIVER_DISCARD|ReplAction::PSODIUM_EXPIRE, expiretime);
  } else
    if((_loaddate)&&(apr_time_now()-_loaddate<_ttl))
      return ReplAction(ReplAction::FROM_DISK|ReplAction::DELIVER_DISCARD);
    else
      return ReplAction(ReplAction::FROM_GET|ReplAction::DELIVER_SAVE);
}

void
TtlPolicy::onLoaded(Element* doc, long sz, long lastmod) throw()
{
  _loaded = true;
  _loaddate = apr_time_now();
}

ReplAction
TtlPolicy::onPurge(Element* doc) throw()
{
  return ReplAction();
}
