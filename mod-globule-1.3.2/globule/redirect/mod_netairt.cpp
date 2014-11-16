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
#include <stddef.h>
#include <vector>
#include <apr.h>
#include <apr_pools.h>
#include <apr_errno.h>
#include <apr_strings.h>
#include <apr_network_io.h>
#include <httpd.h>
#include <http_config.h>
#include <http_connection.h>
#include <http_log.h>
#include <ap_config.h>
#include <ap_listen.h>
#include <ap_mmn.h>
/* Required for inet_addr, not always properly included by APR libraries. */
#ifndef WIN32
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif /* !WIN32 */
#include "utilities.h"
#include "mod_netairt.h"
#include "dns_comm.h"
#include "dns_policy.hpp"
#include "dns_config.h"

using std::vector;

apr_status_t
dns_process_connection(conn_rec *c)
{
  if(!globule_dns_config || !globule_dns_config->dns_redirection)
    return DECLINED;
    
#ifdef DNS_REDIRECTION
  if(c->local_addr->port == globule_dns_config->port ||
     globule_dns_config->port == 0)
    return dns_comm_process_tcp(c, globule_dns_config);
#endif /* DNS_REDIRECTION */

  return DECLINED;
}

int
dns_post_config(apr_pool_t *pconf, apr_pool_t *plog,
                apr_pool_t *ptemp, server_rec *s)
{
  const char *errmsg;
  if(globule_dns_config->dns_redirection)
    if((errmsg = dns_comm_init(globule_dns_config, pconf))) {
      ap_log_error(APLOG_MARK, APLOG_CRIT, OK, s,
                   "Globule redirector failed to destroy communication: %s",
                   errmsg);
      return DONE;
    }
  if(ap_strchr(s->server_hostname,':'))
    globule_dns_config->myhostname = apr_pstrndup(pconf, s->server_hostname,
                        ap_strchr(s->server_hostname,':') - s->server_hostname);
  else
    globule_dns_config->myhostname = apr_pstrdup(pconf, s->server_hostname);
  return OK;
}

int
dns_http_cleanup(request_rec *r)
{
  if (r->status == HTTP_MOVED_TEMPORARILY)
    RedirectPolicy::finishAll(globule_dns_config, r->pool);
  return DECLINED;
}
