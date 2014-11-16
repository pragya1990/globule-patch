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
#include <string>
#include <apr.h>
#include <httpd.h>
#include <http_core.h>
#include <http_protocol.h>
#include <http_request.h>
#include <util_filter.h>
#include <apr_lib.h>
#include "utilities.h"
#include "resource/NameBindingHandler.hpp"
#include "event/HttpReqEvent.hpp"
#include "event/HttpMetaEvent.hpp"
#include "event/RedirectEvent.hpp"
#include "event/ReportEvent.hpp"

using std::string;
using std::vector;

gvector<gDNSRecord>* NameBindingHandler::dnsdb;

NameBindingHandler::NameBindingHandler(Handler* parent, apr_pool_t* p,
                                       const Url& dummy,
                                       const vector<DNSRecord>& initial)
  throw()
  : Handler(parent, p, dummy)
{
  _redirect_policy = RedirectPolicy::lookupPolicy("static");
  for(vector<DNSRecord>::const_iterator iter=initial.begin();
      iter != initial.end();
      ++iter)
    (*dnsdb).push_back(*iter);
}

NameBindingHandler::~NameBindingHandler() throw()
{
}

void
NameBindingHandler::flush(apr_pool_t* p) throw()
{
}

bool
NameBindingHandler::handle(GlobuleEvent& evt) throw()
{
  if(evt.type() == GlobuleEvent::REDIRECT_EVT) {
    RedirectEvent& ev = (RedirectEvent&)evt;
    if(!ev.getRequest()) { // Never redirect on HTTP requests
      apr_uri_t uri;
      uri.scheme   = 0;
      uri.path     = "/";
      uri.port     = 0;
      string remainder;
      gvector<gDNSRecord>::iterator iter = (*dnsdb).begin();
      while(iter != (*dnsdb).end()) {
        uri.hostname = const_cast<char*>(iter->name()); // safe cast
        if(ev.match(uri, remainder))
          break;
        else
          ++iter;
      }
      if(iter != (*dnsdb).end()) {
        ev.callback(1, DNSRecord(evt.pool,*iter), _redirect_policy);
        return true;
      }
    }
  }
  return false;
}

bool
NameBindingHandler::handle(GlobuleEvent& evt, const string& remainder) throw()
{
  return false;
}

std::string
NameBindingHandler::description() const throw()
{
  return "NameBindingHandler()";
}
