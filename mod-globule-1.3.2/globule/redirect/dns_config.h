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
#ifndef _DNS_CONFIG_H
#define _DNS_CONFIG_H

#include <httpd.h>
#include <apr.h>
#include <apr_network_io.h>

#include "mod_netairt.h"

#define DNS_TTL_MINUTE 60
#define DNS_TTL_HOUR 3600
#define DNS_TTL_DAY 86400
#define DNS_TTL_WEEK 604800

#define CFG_MODE_OFF    0x00
#define CFG_MODE_DNS    0x01
#define CFG_MODE_HTTP   0x02
#define CFG_MODE_BOTH   (CFG_MODE_DNS|CFG_MODE_HTTP)

struct dns_config 
{
  char host_set;
  char * host;                          /* local IP for DNS ports */
  char port_set;
  apr_uint16_t port;                    /* DNS port number */
  apr_uint32_t max_sites;               /* maximum number of sites */
  apr_uint32_t max_replicas;            /* maximum number of replicas */
  apr_uint32_t ttl_none;
  apr_uint32_t ttl_rr;
  apr_uint32_t ttl_as;
  apr_uint32_t ns_ttl;                  /* TTL for redirects to 2-lvl DNS */
  apr_uint32_t ipcount;                 /* number of IPs returned */
  apr_byte_t ror;                       /* destroy data on restart? */
  apr_uint32_t bgp_refresh;             /* how long is BGP data valid? */
  const char * bgp_file_name;           /* BGP data file name (with path) */
  int dns_redirection;                  /* boolean whether used */
  char * myhostname;
};
extern dns_config *globule_dns_config;

#endif /* _DNS_CONFIG_H */
