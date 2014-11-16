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
#include "monitoring.h"
#include "MirrorNoConsPolicy.hpp"

MirrorNoConsPolicy::MirrorNoConsPolicy()
  throw()
  : ReplPolicy("MirrorNoConsPolicy", MONITOR(policymirror))
{
}

MirrorNoConsPolicy::~MirrorNoConsPolicy() throw()
{
}

ReplPolicy*
MirrorNoConsPolicy::instantiatePolicy() throw()
{
  return new(rmmmemory::shm()) MirrorNoConsPolicy();
}

Input&
MirrorNoConsPolicy::operator>>(Input& in) throw()
{
  ReplPolicy::operator>>(in);
  return in;
}

Output&
MirrorNoConsPolicy::operator<<(Output& out) const throw()
{
  ReplPolicy::operator<<(out);
  return out;
}

ReplAction
MirrorNoConsPolicy::onUpdate(Element* doc) throw()
{
  return ReplAction();
}

ReplAction
MirrorNoConsPolicy::onSignal(Element* doc) throw()
{
  return ReplAction();
}

ReplAction
MirrorNoConsPolicy::onAccess(Element* doc, Peer* from) throw()
{
  if(ismaster(doc))
    return ReplAction(ReplAction::FROM_APACHE|ReplAction::DELIVER_DISCARD);
  else
    if(!_loaded)
      return ReplAction(ReplAction::FROM_GET|ReplAction::DELIVER_SAVE);
    else
      return ReplAction(ReplAction::FROM_DISK|ReplAction::DELIVER_DISCARD);
}

void
MirrorNoConsPolicy::onLoaded(Element* doc, long sz, long lastmod) throw()
{
  _loaded = true;
}

ReplAction
MirrorNoConsPolicy::onPurge(Element* doc) throw()
{
  return ReplAction();
}
