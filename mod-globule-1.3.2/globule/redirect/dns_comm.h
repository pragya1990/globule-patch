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
#ifndef _DNS_COMM_H
#define _DNS_COMM_H

#include "httpd.h"
#include "http_connection.h"
#include "ap_listen.h"
#include "apr_pools.h"
#include "apr_errno.h"
#include "dns_config.h"

/* communication module initialization - allocates any needed resources
   according to the configuration 'cfg' in the persistent pool 'p';
   returns a textual error description or NULL on success
*/
char * dns_comm_init(dns_config * cfg, apr_pool_t * p);

/* communication module destruction - releases any previously allocated
   resources */
void dns_comm_destroy();

/* allocates a new listener structure of type 'type' for local IP address
   'host', a port number 'port'; uses the persistent pool 'ppool';
   type can be either SOCK_STREAM or SOCK_DGRAM; the new listener is 
   added to the global list of listeners;
   returns the new listener
*/
ap_listen_rec * dns_comm_alloc_listener(apr_pool_t * ppool, char * host,
                apr_port_t port, int type);

/* accept function for the UDP DNS port */
apr_status_t dns_comm_accept_udp(void **csd, ap_listen_rec *lr, apr_pool_t *ptrans);

/* processing function for the UDP DNS port */
apr_status_t dns_comm_process_udp(ap_listen_rec * lr, server_rec * s, apr_pool_t * ptrans);
                
/* processing function for the TCP DNS port */
apr_status_t dns_comm_process_tcp(conn_rec *, dns_config *);

#endif /* _DNS_COMM_H */
