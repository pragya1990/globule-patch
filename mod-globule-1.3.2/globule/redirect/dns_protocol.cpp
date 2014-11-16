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
#include <apr_network_io.h>
#include <vector>
#ifndef WIN32
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#ifdef DARWIN
#include <arpa/nameser_compat.h>
#endif
#include <resolv.h>
#include <netdb.h>
#endif /* !WIN32 */
#include "dns_protocol.h"
#include "dns_policy.hpp"
#include "event/RedirectEvent.hpp"

using std::vector;

#define MAX_NAME_COUNT 32

static unsigned char *
dumptable(const char* name, const vector<DNSRecord>& table,
          int ipcount, int ttl,
          unsigned char **cpp, unsigned char *eob, apr_byte_t **comp,
          apr_byte_t **lastcomp)
{
  int i, namelen, bufleft;
  apr_uint16_t type;
  unsigned char *rrlenp;

  for(i=0; i<ipcount; i++) {
    type = table[i].type();

    /* put rr header */
    bufleft = eob - (*cpp);
    if((namelen = dn_comp(name, *cpp, bufleft, comp, lastcomp)) < 0)
      return NULL;
    *cpp += namelen;
    if((bufleft -= namelen) < 10)
      return NULL;
    NS_PUT16(type,*cpp);
    NS_PUT16(ns_c_in,*cpp);
    NS_PUT32(ttl,*cpp);
    /* rrdatalen unknown for a while, put 0 instead */
    rrlenp = *cpp;
    NS_PUT16(0, *cpp);

    /* put rr data */
    switch(type) {
    case ns_t_a: {
      if((eob-(*cpp)) < (signed)sizeof(apr_uint32_t))
        return NULL;
      apr_uint32_t addr = table[i].addr();
      memcpy(*cpp, &addr, sizeof(apr_uint32_t));
      (*cpp) += sizeof(apr_uint32_t);
      NS_PUT16(*cpp-rrlenp-2, rrlenp);
      break;
    }
    case ns_t_cname: {
      namelen = dn_comp(table[i].cname(), *cpp, eob - (*cpp), comp, lastcomp);
      if(namelen < 0)
        return NULL;
      *cpp += namelen;
      NS_PUT16(*cpp-rrlenp-2, rrlenp);
      break;
    }
    default:
      return NULL;
    }
  }

  return *cpp;
}

struct callback_data {
  const char *lookupname;
  int *ipcountptr;
  unsigned char **cpp;
  unsigned char *eob;
  apr_byte_t **comp;
  apr_byte_t **lastcomp;
};

static void
callback(void *ptr, int count, const vector <DNSRecord>& replicas,
         RedirectPolicy* policy)
{
  struct callback_data *data = (struct callback_data*) ptr;
  if(count > 0) {
    int ttl = policy->ttl();
    if(!dumptable(data->lookupname, replicas, count, ttl,
                  data->cpp, data->eob, data->comp, data->lastcomp))
      count = -1;
  }
  *(data->ipcountptr) = count;
}

/* process a simple DNS query; call the policy module to obtain the
 * IP addresses that should be included in the response.
 */
static int
handle_dnsquery(HEADER *hp, unsigned char **cpp, unsigned char *eom,
                unsigned char *eob, apr_uint32_t from, dns_config *cfg,
                apr_pool_t *pool)
{
  char name[NS_MAXDNAME];
  int namelen, ipcount = cfg->ipcount;
  apr_uint16_t type, clazz;
  apr_byte_t *comp[MAX_NAME_COUNT], **lastcomp = comp + MAX_NAME_COUNT - 1;

  /* valid queries have one question only, and no answers */
  if(ntohs(hp->qdcount) != 1 || ntohs(hp->ancount))
    return ns_r_formerr;

  /* in our case - also nscount and arcount should be zero */
  /* there are queries that use NS/AR records, but we do not support them */
  if(ntohs(hp->nscount) || ntohs(hp->arcount))
    return ns_r_notimpl;

  /* let's see what is the query about */
  namelen = dn_expand((unsigned char*) hp, eom, *cpp, name, sizeof(name));
  if(namelen < 0)
    return ns_r_formerr;
  /* store some pointers for further name compression */
  comp[0] = (apr_byte_t*)hp;
  comp[1] = *cpp;
  comp[2] = NULL;
  *cpp += namelen;

  /* did we get also class and type? */
  if(*cpp + 2*INT16SZ > eom)
    return ns_r_formerr;

  /* macros from /usr/include/arpa/nameser.h */
  NS_GET16(type, *cpp);
  NS_GET16(clazz, *cpp);

  /* 'class' must be ``internet'' */
  if(clazz != ns_c_in)
    return ns_r_notimpl;

  /* *cpp is the end of the query part now -- simply
   * discard the trailing octets, if any */

  switch(type) {
  case ns_t_aaaa:
  case ns_t_mx:
    return ns_r_noerror;
  case ns_t_any:
  case ns_t_a:
    apr_uri_t uri;
    uri.scheme   = 0;
    uri.hostname = name;
    uri.path     = "/";
    uri.port     = 0;
    struct callback_data data = { name, &ipcount, cpp, eob, comp, lastcomp };
    RedirectEvent ev(pool, 0, &uri, from, ipcount, &callback, &data);
    struct workerstate_struct* state = diag_message(NULL);
    state->ctx->request(0);
    if(!GlobuleEvent::submit(ev))
      return ns_r_nxdomain;
    if(ipcount < 0)
      return ns_r_servfail;
    else if(ipcount == 0)
      return ns_r_nxdomain;
    hp->ancount = htons(ipcount);

    int bufleft, ttl=0;
    unsigned char *rrlenp;    

    hp->nscount = htons(1);
    /* put NS RR */
    bufleft = eob - (*cpp);
    if((namelen = dn_comp(name, *cpp, bufleft, comp, lastcomp)) < 0)
      return ns_r_servfail;
    *cpp += namelen;
    if((bufleft -= namelen) < 10)
      return ns_r_servfail;
    NS_PUT16(ns_t_ns, *cpp);
    NS_PUT16(ns_c_in, *cpp);
    NS_PUT32(ttl,*cpp);
    rrlenp = *cpp;     /* rrdatalen unknown for a while, put 0 instead */
    NS_PUT16(0, *cpp);
    namelen = dn_comp(globule_dns_config->myhostname, *cpp, eob - (*cpp),
                      comp, lastcomp);
    if(namelen < 0)
      return ns_r_servfail;
    *cpp += namelen;
    NS_PUT16(*cpp-rrlenp-2, rrlenp);

    return ns_r_noerror;
  }
  return ns_r_notimpl;
}

/* verify and process a DNS packet; call the above query processing function
 * if necessary.
 */
int
dns_protocol_run(dns_request *p, dns_config *cfg, apr_pool_t *pool)
{
  unsigned char *cp  = (unsigned char*)(p->data) + NS_HFIXEDSZ;
  unsigned char *eom = (unsigned char*)(p->data) + p->size;
  unsigned char *eob = (unsigned char*)(p->data) + p->maxsize;
  HEADER *hp = (HEADER *)p->data;

  if(p->size < NS_HFIXEDSZ)
    return 0;
  /* decide whether the source port number 'port' of a DNS packet is valid,
   * just like Bind does.
   */
  switch(p->from.sa.sin.sin_port) {
  case 0:
  case 7:
  case 13:
  case 19:
  case 37:
  case 464:
    return 0;
  }

  if(hp->opcode != ns_o_query)
    hp->rcode = ns_r_notimpl;
  else
    hp->rcode = handle_dnsquery(hp, &cp, eom, eob,
                                p->from.sa.sin.sin_addr.s_addr, cfg, pool);

  hp->aa = 1; /* we are the only authoritative dns server */
  hp->ra = 0; /* we do not support recursive queries (?) */
  hp->qr = 1; /* query/response indication bit on */

  if(hp->rcode == ns_r_noerror)
    return p->size = (cp - (unsigned char*)hp);

  /* error -> return only header */
  hp->qdcount = htons(0);
  hp->ancount = htons(0);
  hp->nscount = htons(0);
  hp->arcount = htons(0);
  return p->size = NS_HFIXEDSZ;
}

/* process-finishing function; forwarded to the policy module. */
void
dns_protocol_finish(dns_config * cfg, apr_pool_t *pool)
{
  RedirectPolicy::finishAll(cfg, pool);
}
