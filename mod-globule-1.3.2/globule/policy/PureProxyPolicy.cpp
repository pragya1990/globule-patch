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
#ifdef PSODIUM
#include <apr.h>
#include <apr_time.h>
#include "psodium.h"
#endif
#include "utilities.h"
#include "monitoring.h"
#include "PureProxyPolicy.hpp"

PureProxyPolicy::PureProxyPolicy()
  throw()
  : ReplPolicy("PureProxyPolicy", MONITOR(policyproxy))
{
}

PureProxyPolicy::~PureProxyPolicy() throw()
{
}

ReplPolicy*
PureProxyPolicy::instantiatePolicy() throw()
{
  return new(rmmmemory::shm()) PureProxyPolicy();
}

Input&
PureProxyPolicy::operator>>(Input& in) throw()
{
  ReplPolicy::operator>>(in);
  in >> InputWrappedPersistent<bool>("loaded",_loaded);
  return in;
}

Output&
PureProxyPolicy::operator<<(Output& out) const throw()
{
  ReplPolicy::operator<<(out);
  out << OutputWrappedPersistent<bool>("loaded",_loaded);
  return out;
}

ReplAction
PureProxyPolicy::onUpdate(Element* doc) throw()
{
  return ReplAction();
}

ReplAction
PureProxyPolicy::onSignal(Element* doc) throw()
{
  return ReplAction();
}

ReplAction
PureProxyPolicy::onAccess(Element* doc, Peer* from) throw()
{
  if(ismaster(doc)) {
    apr_time_t expiry_time = 0;
#ifdef PSODIUM
    /*
     * We must record the digest of the document we returned to the slave
     * long enough such that:
     * 1. it can be transferred from master to slave
     * 2. it can be transferred from slave to client (who got this request
     *    started in the first place)
     * 3. the client can send a getDigest request to the master (us) to double
     *    check the reply from the master.
     * Latter 2 times already taken into account by code that answers 
     * getDigests. (Internal processing time is ignored)
     */
    apr_time_t now = apr_time_now(); 
    apr_time_t ms_secs = _size / (AVG_MS_THROUGHPUT*1024);
    apr_time_t max_ms_content_prop = pso_max( MIN_MAX_MS_CONTENT_PROP, apr_time_from_sec( ms_secs ));
    expiry_time = now + max_ms_content_prop;

    DOUT( DEBUG3, "G2P: PureProxy policy setting expiry time " << expiry_time << " for pSodium security to use\n" );
#endif
    
    return ReplAction(ReplAction::FROM_APACHE|ReplAction::DELIVER_DISCARD|ReplAction::PSODIUM_EXPIRE, expiry_time );
  } else
    return ReplAction(ReplAction::FROM_GET|ReplAction::DELIVER_DISCARD);
}

void
PureProxyPolicy::onLoaded(Element* doc, long sz, long lastmod) throw()
{
  _loaded = true;
}

ReplAction
PureProxyPolicy::onPurge(Element* doc) throw()
{
  return ReplAction();
}
