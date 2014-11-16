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
#include <apr_version.h>
#include <http_protocol.h>
#include <string>
#include "utilities.h"
#include "resource/SourceHandler.hpp"
#include "netw/HttpRequest.hpp"
#include "event/RedirectEvent.hpp"
#include "event/PostRequestEvent.hpp"
#include "event/ReportEvent.hpp"
#include "event/HttpLogEvent.hpp"
#include "configuration.hpp"

using namespace std;
using namespace globule::netw;

SourceHandler::SourceHandler(Handler* parent, apr_pool_t* p, Context* ctx,
                             const Url& uri, const char* path)
  throw()
  : RedirectorHandler(parent, p, ctx, uri, path)
{
}

SourceHandler::~SourceHandler() throw()
{
}

bool
SourceHandler::initialize(apr_pool_t* p, Context* ctx,
			  HandlerConfiguration* cfg)
  throw(SetupException)
{
  if(RedirectorHandler::initialize(p, ctx, cfg))
    return true;
  _default_policy  = cfg->get_replication_policy();
  _redirect_policy = RedirectPolicy::lookupPolicy(cfg->get_redirect_policy());
  if(!_redirect_policy) {
    DIAG(ctx, (MONITOR(error),&_monitor), ("No redirection policy named %s found, reverting to default",cfg->get_redirect_policy()));
    _redirect_policy = RedirectPolicy::defaultPolicy();
    if(!_redirect_policy) {
      DIAG(ctx, (MONITOR(fatal),&_monitor), ("Could not revert to non-existing default redirection policy"));
      return false;
    }
  }
  _peers.add(ctx, p, this, cfg->get_weight());
  BaseHandler::initialize(p, ctx, cfg->get_contacts());
  return false;
}

bool
SourceHandler::receivelog(request_rec* req) throw()
{
  const char *v = apr_table_get(req->headers_in, "X-From-Replica");
  char *line; 
  apr_size_t len;
  apr_status_t status;
  apr_bucket_brigade *bb;

  bb = apr_brigade_create(req->pool, req->connection->bucket_alloc);
  line = (char*) apr_palloc(req->pool, 1024);
  do {

    /* The ap_rgetline_core function has an issue (#18783) which causes
     * an infinite loop when reading the entire request using ap_rgetline.
     * A patch has been submitted to resolve it.  An initial workaround
     * didn't work as expected, so a replacement function for rgetline
     * based on the patch is now used.  This replacement function
     * is named globule_rgetline and should be removed when the bug is
     * resolved.
     */
    /* The new version of Apache no longer allocates a line when the pointer
     * to the line is NULL.  But ap_rgetline, even though completely
     * rewritten, seems to be broken as ever.
     */
    status = globule_rgetline(&line, 1024, &len, req, 0, bb);

    if(status == APR_SUCCESS && len > 0) {
      /* FIXME: check that line contains server= field matching the one we
       * expect and the log line is proper.
       */
      BaseHandler::log(req->pool, line, "");
    } else if(status != APR_ENOSPC) // skip lines that are too long
      line = 0;
  } while(line);
  apr_brigade_destroy(bb);

  /* For the slave replica site for which we have just received a log from,
   * we will now mark the replica site as enabled.  This will prevent/skip
   * checking the site for the next heartbeat cycle if it was already enabled.
   */
  int port=80, hostnamelen;
  const char *s;
  /* The parsing of a URL should not be done again and again inline in the
   * code.  Also the X-From-Replica does not include a path, making it
   * impossible to import two sections from the same server (not only because
   * of this piece of code).
   */
  if((s = strchr(v,':'))) {
    hostnamelen = s++ - v;
    if(isdigit(*s))
      port = atoi(s);
  } else
    hostnamelen = strlen(v);
  for(Peers::iterator iter = _peers.begin(Peer::REPLICA);
      iter != _peers.end();
      ++iter)
    {
      // The following check isn't precise enough, the path isn't compared.
      if(!strncasecmp(iter->uri().uri()->hostname,v,hostnamelen) &&
         iter->uri().uri()->port == port) {
        /* The reason to allow, in principle other values than 0 (currently
         * disabled replica), 1 (currently enabled), 2 (enabled, log received,
         * skip on next check) is that -1 could indicate a permanent disabled
         * server and >>2 a server which is very stable.
         */
        if(iter->enabled >= 0 && iter->enabled <= 1)
          iter->enabled = 2;
      }
    }

  return true;
}

Peer*
SourceHandler::authorized(Context* ctx, request_rec* r) throw()
{
  char *remote_ipaddr = 0, *secret = "";
  apr_port_t remote_port;
  if(!getCredentials(ctx, r, &remote_ipaddr, &remote_port, &secret))
    return false;
  for(Peers::iterator iter = _peers.begin();
      iter != _peers.end();
      ++iter)
  {
      
#ifdef PSODIUM
    Url known_peer_url = iter->uri();
#endif
    if(iter->authenticate(remote_ipaddr, remote_port, secret)
#ifdef DEBUG
       || (secret && !strcmp(secret,"backdoor"))
#endif
       )
     {
#ifdef PSODIUM
        // IMPORTANT: This note should be set only when the slave is
        // properly authenticated! 
        const char *from_str = apr_table_get(r->headers_in, "X-From-Replica");
        if (from_str)
        {
          Url remote_peer_url( r->pool, from_str );
          remote_peer_url.normalize( r->pool );        // just to be sure
          if (strncmp( remote_peer_url( r->pool ), known_peer_url( r->pool), strlen( known_peer_url( r->pool) ) ))
          {
             DOUT( DEBUG0, "Prefix of X-From-Replica header is not equal to specified GlobuleReplicaIs URI!" << from_str << "!=" << known_peer_url( r->pool) );
          }
          else           
          {        
            char *authority = apr_psprintf( r->pool, "%s:%hu", remote_peer_url.host(), remote_peer_url.port() );
            DOUT( DEBUG3, "G2P: Setting SLAVEID r->notes to " << authority );
            apr_table_set( r->notes, PSODIUM_RECORD_DIGEST_AUTH_SLAVEID_NOTE, authority );
            // QUICK
            apr_table_set( r->notes, PSODIUM_RECORD_DIGEST_URI_NOTE, "" );
          }
        }
        else
        {
          // Could be redirector or something, so don't cry wolf.
        }
#endif    
      return &*iter;
     }
  }
  DIAG(ctx,(MONITOR(error)),("Replica server used incorrect password"));
  return 0;
}

void
SourceHandler::log(apr_pool_t* pool, string msg1, const string msg2) throw()
{
  msg1 += mkstring::format(" server;%s",_servername);
  BaseHandler::log(pool, msg1, msg2);
}
