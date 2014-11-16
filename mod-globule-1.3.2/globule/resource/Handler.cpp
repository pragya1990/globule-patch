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
#include <apr_base64.h>
#include "resource/Handler.hpp"
#include "resource/ConfigHandler.hpp"
#include "netw/HttpRequest.hpp"
#include "event/RegisterEvent.hpp"

using namespace std;

Handler::Handler()
  throw()
  : _initialized(false), _parent(0)
{
}

Handler::Handler(Context* ctx, apr_pool_t* pool, const Url& uri)
  throw()
  : _initialized(false), _parent(0), _uri(uri)
{
  RegisterEvent registerevt(pool, this);
  GlobuleEvent::submit(registerevt);
}

Handler::Handler(Handler* parent, apr_pool_t* p, const Url& uri)
  throw()
  : _initialized(false), _parent(parent), _uri(uri)
{
}

Handler::~Handler() throw()
{
}

bool
Handler::handle(GlobuleEvent& evt, const std::string& remainder) throw()
{
  return false;
}

bool
Handler::handle(GlobuleEvent& evt) throw()
{
  string remainder;
  const Handler* target;

  bool ismatch = false;

  for(target=evt.target(); target; target=target->_parent)
    if(target == this) {
      ismatch = true;
      break;
    }
  if(!ismatch && evt.match(_uri,remainder))
    ismatch = true;
  if(!ismatch)
    for(gvector<gUrl>::iterator iter=_aliases.begin();
        iter!=_aliases.end();
        ++iter)
      if(evt.match(*iter,remainder)) {
        ismatch = true;
        break;
      }
  if(ismatch) {
    switch(evt.type()) {
    case GlobuleEvent::HEARTBEAT_EVT:
      if(!_initialized) {
        try {
          Url statusUri(evt.pool, _uri, "?status");
          globule::netw::HttpRequest req(evt.pool, statusUri);
          req.setMethod("SIGNAL");
          req.setAuthorization(ConfigHandler::password, ConfigHandler::password);
          int reqstatus = req.getStatus(evt.pool); 
          if(reqstatus == globule::netw::HttpResponse::HTTPRES_OK ||
             reqstatus == globule::netw::HttpResponse::HTTPRES_NO_CONTENT) {
            DIAG(evt.context,(MONITOR(info)),("Initialized section %s",_uri(evt.pool)));
            _initialized = true;
            return true;
          } else {
            DIAG(evt.context,(MONITOR(error)),("Initializing section %s failed",_uri(evt.pool)));
            return false;
          }
        } catch(globule::netw::HttpException ex) {
          DIAG(evt.context,(MONITOR(error)),("Initializing section %s failed",_uri(evt.pool)));
          return false;
        }
      }
      break;
    case GlobuleEvent::INVALID_EVT: {
      if(remainder == "" || remainder == "/") {
        HttpMetaEvent& ev = (HttpMetaEvent&) evt;
        if(ev.getRequestArgument() != "") {
          ev.target(this);
          char* secret;
          bool hasCredentials = getCredentials(ev.context, ev.getRequest(), NULL, NULL, &secret);
          if(ev.getRequestArgument() == "check") {
            if(!ev.getRequest() || (hasCredentials && !strcmp(secret, ConfigHandler::password))) {
              handle(evt, remainder);
              ev.target(0);
              return false;
            } else {
              ev.setStatus(HTTP_UNAUTHORIZED);
              return true;
            }
          } else if(ev.getRequestArgument() == "status") {
            if(hasCredentials && (!strcmp(secret, ConfigHandler::password)
#ifdef DEBUG
                                  || !strcmp(secret, "backdoor")
#endif
               )) {
              ev.setStatus(HTTP_NO_CONTENT);
              return true;
            }
          }
        }
      }
      break;
    }
    default:
      ;
    }
    if(!_initialized) {
      switch(evt.type()) {
      case GlobuleEvent::HEARTBEAT_EVT:
        break;
      case GlobuleEvent::REQUEST_EVT:
      case GlobuleEvent::LOGGING_EVT:
      case GlobuleEvent::INVALID_EVT:
      case GlobuleEvent::REDIRECT_EVT:
        ((HttpReqEvent&)evt).setStatus(HTTP_SERVICE_UNAVAILABLE);
        // deliberate fall through
      default:
        return true;
      }
    }
    return handle(evt, remainder);
  } else
    return false;
}

Lock*
Handler::getlock() throw()
{
  return 0;
}

bool
Handler::getCredentials(Context* ctx, request_rec* r, char** remote_ipaddr,
                        apr_port_t* remote_port, char** secret) throw()
{
  if(!r)
    return false;
  const char *auth_str = apr_table_get(r->headers_in, "Authorization");
  apr_status_t status;
  if(remote_ipaddr) {
    status = apr_sockaddr_ip_get(remote_ipaddr, r->connection->remote_addr);
    if(status != APR_SUCCESS) {
      DIAG(ctx,(MONITOR(error)),("Could not obtain peer IP address from supplied"));
      return false;
    }
    if(remote_port)
      *remote_port = r->connection->remote_addr->port;
  }
  if(secret) {
    if(!auth_str) {
      DIAG(ctx,(MONITOR(error)),("No authorization given%s%s",(remote_ipaddr?" by ":""),(remote_ipaddr?(*remote_ipaddr ? *remote_ipaddr : "unknown peer"):"")));
      return false;
    }
    /* ap_get_basic_auth_pw() requires all sorts of fields in r to be set,
     * do it ourselves.
     */
    char *base64userpass = (char *)auth_str + strlen("Basic ");
    apr_size_t decoded_len = apr_base64_decode_len(base64userpass); 
    char *decoded = (char *)apr_palloc(r->pool, decoded_len);
    apr_base64_decode(decoded, base64userpass);
    *secret = strchr(decoded, ':');
    if(*secret == NULL) {
      DIAG(ctx,(MONITOR(error)),("Broken authentication header supplied by %s",(remote_ipaddr ? *remote_ipaddr : "unknown peer")));
      return false;
    } else
      *secret += 1;
  }
  return true;
}
