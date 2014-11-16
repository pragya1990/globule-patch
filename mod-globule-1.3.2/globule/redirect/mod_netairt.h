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
#ifndef _MOD_NETAIRT_H
#define _MOD_NETAIRT_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "utilities.h"

#include "apr_errno.h"
#ifndef WIN32
#include <netinet/in.h>
#endif

/* maximum length of a replica DNS name */
#define DNS_MAX_NAME_LENGTH 128     // Arno: careful: includes '\0' :-(

/* Arno: maximum length of the path part of a URI, as used 
  in site definitions. Officially there is no such limit, but
  the NetAirt SHM code currently forces this. */
#define DNS_MAX_URI_PATH_LENGTH    256 // Arno: careful: includes '\0' :-(

/* Arno: rough estimation of the maximum length of a URI, as derived 
  from site definitions. Officially there is no such limit, but
  the NetAirt SHM code currently forces this. */
#define DNS_MAX_URI_LENGTH         (10+DNS_MAX_NAME_LENGTH+6+DNS_MAX_URI_PATH_LENGTH)

typedef struct dns_config dns_config;
typedef struct dns_context dns_context;

/*
 * Globule/NetAirt integration:
 */
#include <httpd.h>
#include <http_config.h>

/* module config initialization */
void *create_dns_server_config(apr_pool_t *p, server_rec *s);
void *merge_dns_server_config( apr_pool_t *p, void *base_conf, void *override_conf );
apr_status_t dns_process_connection(conn_rec * c);
const char * dns_config_ttl_as(cmd_parms * cmd, void * mconfig, const char * param);
const char * dns_config_ttl_rr(cmd_parms * cmd, void * mconfig, const char * param);
const char * dns_config_ipcount(cmd_parms * cmd, void * mconfig, const char * param);
const char * dns_config_bgpslot(cmd_parms * cmd, void * mconfig, const char * param);
const char * dns_config_bgprefresh(cmd_parms * cmd, void * mconfig, const char * param);
const char * dns_config_bgpfile(cmd_parms * cmd, void * mconfig, const char * param);
const char * dns_config_password(cmd_parms * cmd, void * mconfig, const char * param);
const char * dns_config_ror(cmd_parms * cmd, void * mconfig, int reset);
const char * dns_config_policy(cmd_parms * cmd, void * mconfig, const char * param);

/* creates/destroys the replica database, communication module and policy module  */
int         dns_post_config( apr_pool_t *pconf, apr_pool_t *plog, 
                             apr_pool_t *ptemp, server_rec *s);
int dns_http_cleanup(request_rec *r);

#endif /* _MOD_NETAIRT_H */
