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
#ifndef _DNS_PROTOCOL_H
#define _DNS_PROTOCOL_H

#include <httpd.h>
#include <apr.h>
#include <apr_network_io.h>
#include <apr_pools.h>

#include "dns_config.h"

#ifdef WIN32
#include "compat/Win32/arpa/nameser.h"
#else
#include <arpa/nameser.h>
#endif /* WIN32 */


#define TCP_PACKETSZ (NS_PACKETSZ*5)

/* transport-layer-independent structure of a DNS request */
typedef struct dns_request dns_request;
struct dns_request {
    dns_request *next;
    apr_sockaddr_t from; /* make it so it is allocated with malloc, only sa is needed */
    apr_size_t size;
    apr_size_t maxsize;
    char data[NS_PACKETSZ];
};

/* extended version of the above, for requests coming via TCP; allows to
   send more data than via UDP, so the buffer is larger as well */
struct dns_request_tcp {
    dns_request header;
    apr_byte_t data[TCP_PACKETSZ-NS_PACKETSZ];
};

/* main protocol function - verify and process the DNS request 'p' using
   the configuration 'cfg' and temporary data pool 'pool'; store the
   response in the same buffer that is used by the query (p); return
   the length of the response (0 means that no response has been generated
   and the datagram/connection should be dropped */
int dns_protocol_run(dns_request * p, dns_config * cfg, apr_pool_t * pool);

/* auxiliary function, called at the end of each socket processing to
   trigger any Redirector self-updates */
void dns_protocol_finish(dns_config * cfg, apr_pool_t * pool);

#endif /* _DNS_PROTOCOL_H */
