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
#include "ReplPolicy.hpp"
#include "Constants.hpp"
#include "documents.hpp"
#include "resource/BaseHandler.hpp"

using std::string;

std::map<std::string,ReplPolicy*> ReplPolicy::policies;

Input&
ReplPolicy::operator>>(Input& in) throw()
{
  Persistent::operator>>(in);
  in >> InputWrappedPersistent<bool>("loaded",_loaded)
     >> InputWrappedPersistent<int>("stickyness",_stickyness)
     >> InputWrappedPersistent<gstring>("policyname",_policyname);
  return in;
}

Output&
ReplPolicy::operator<<(Output& out) const throw()
{
  Persistent::operator<<(out);
  out << OutputWrappedPersistent<bool>("loaded",_loaded)
      << OutputWrappedPersistent<int>("stickyness",_stickyness)
      << OutputWrappedPersistent<gstring>("policyname",_policyname);
  return out;
}

void
ReplPolicy::registerPolicy(const char* name, ReplPolicy* policy) throw()
{
  policies[name] = policy;
  policy->_policyname = name;
}

Persistent*
ReplPolicy::instantiateClass() throw()
{
  return instantiatePolicy();
}

ReplPolicy*
ReplPolicy::lookupPolicy(string name, int stickyness) throw()
{
  ReplPolicy* newpolicy;
  std::map<string,ReplPolicy*>::iterator iter=policies.find(name);
  if(iter != policies.end()) {
    newpolicy = iter->second->instantiatePolicy();
  } else {
    newpolicy = policies.find("Ttl")->second->instantiatePolicy();
  }
  newpolicy->_stickyness = stickyness;
  newpolicy->_policyname = name.c_str();
  return newpolicy;
}

void
ReplPolicy::report(ReportEvent& evt) throw()
{
}

bool
ReplPolicy::ismaster(Element* doc) const throw()
{
  return doc->_parent->ismaster();
}

bool
ReplPolicy::isslave(Element* doc) const throw()
{
  return doc->_parent->isslave();
}
