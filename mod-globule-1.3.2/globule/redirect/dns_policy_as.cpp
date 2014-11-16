/* 
 * NetAirt: a Redirection System for the Apache HTTP server.
 * Copyright (c) 2002-2003, Michal Szymaniak
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 *  * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *  * Neither the name of this software nor the names of its contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 * 
 * This product includes software developed by the Apache Software Foundation
 * <http://www.apache.org/>.
 * 
 * Please see <http://www.globule.org/netairt/> for additional information,
 * including recent versions and documentation.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <httpd.h>
#include <apr.h>
#include <apr_time.h>
#include <apr_network_io.h>
#include <apr_file_io.h>
#include <apr_errno.h>
#include <http_log.h>
#ifndef WIN32
#include <arpa/inet.h>
#endif /* ! WIN32 */
#ifdef HAVE_ZLIB_H
#include <zlib.h>
#endif
#include "dns_policy_as.hpp"
#include "locking.hpp"

#define LINE_SIZE 512
#define PREFIX_SIZE 19

#define AS_COUNT 0x10000
#define CLASS_A_COUNT 0x100

#define NET_OFFSET 3
#define PATH_OFFSET 61
#define OREGON_ASN 0

void **bgp_data;
Lock* bgp_lock;

/* auxiliary structs, used while parsing the BGP data file*/

struct net_node {
    apr_uint32_t mask;
    char msize;
    apr_uint16_t origin;
    apr_uint32_t index;
    struct net_node *self;
    struct net_node *next;
    struct net_node *all_next;
    struct net_node *child[2];
};

struct asn_list {
    apr_uint16_t number;
    struct asn_list *next;
};

struct as {
    apr_uint32_t link_count;
    apr_uint32_t net_count;
    struct asn_list *links;
    struct net_node *nets;
};

/* exported structs */

struct as_graph {
    apr_uint32_t node_count;
    apr_uint32_t edge_count;
    apr_uint32_t entry_count;
    apr_uint32_t size;
    apr_uint32_t offset[AS_COUNT];
    apr_uint16_t peers[1];
};

struct prefix {
    apr_uint32_t mask;
    char msize;
    apr_uint16_t origin;
    apr_uint32_t idxchild[2];
};

struct as_lookup_table {
    apr_uint32_t net_count;
    apr_uint32_t incmp_count;
    apr_uint32_t entry_count;
    apr_uint32_t size;
    apr_uint32_t offset[CLASS_A_COUNT];
    struct prefix prefixes[1];
};

struct as_data {
  apr_uint32_t timestamp; // is 0 when data invalid
  struct as_graph *as_graph;
  struct as_lookup_table *as_lookup;
};

/* end of structure definitions */

/* compare two networks (prefixes) (dictionary-like order) */
static int netcmp(const void * v1, const void * v2)
{
    /* sort the prefix table like a bitword dictionary */
    struct net_node *p1 = (struct net_node *)v1;
    struct net_node *p2 = (struct net_node *)v2;
    int b1=(32-p1->msize),b2=(32-p2->msize);
    apr_uint32_t a1=(p1->mask >> b1) << b1;
    apr_uint32_t a2=(p2->mask >> b2) << b2;
    
    if (a1 > a2) return 1;
    if (a1 < a2) return -1;
    if (p1->msize < p2->msize) return -1;
    if (p1->msize > p2->msize) return 1;
    return 0;
}

/* proxy to the above, for sorting the table of pointers */
static int pnetcmp(const void * p1, const void * p2)
{
    return netcmp(*(struct net_node **)p1,*(struct net_node **)p2);
}

/* find first node in 'nntab' that is equal or greater than 'target';
   NULL if there is no such node */
static struct net_node ** find_prefix(struct net_node ** nntab, int nntabsize, struct net_node * target)
{
    int l=0,r=nntabsize-1,c,v;

    if (netcmp(nntab[0],target) != -1) return &nntab[0];
    if (netcmp(nntab[nntabsize-1],target) == -1) return NULL;
    /* now we know that we have to return a non-null pointer */
    
    while (r>l) {
        c=(l+r)/2;
        v=netcmp(nntab[c],target);
        if (v == -1) {l=c+1;continue;}
        else if (v == 1) {r=c;continue;}
        return &nntab[c];
    }
    
    return &nntab[l];
}

/* build a tree of prefixes from the 'nntab' table; treat the 'mask' of size 'msize'
   as a split point (put it in root); use the pool 'p' for any data allocation */
static struct net_node * build_tree(struct net_node ** nntab, int nntabsize,
                                apr_uint32_t mask, char msize, apr_pool_t * p)
{
    apr_uint32_t mask1=(mask|(1<<(31-msize))),medcount;
    char nextsize=msize+1,shift=0;
    struct net_node mpref,**med;
    struct net_node *node,*c0=NULL,*c1=NULL;
    apr_uint16_t asnc0,asnc1;

    if (!nntab || !(*nntab) || nntabsize<1 || msize>32) return NULL;

    /* if we have a mask fitting exactly the split point, we preserve it,
       so that not to lose the 'origin' value inside */
    mpref.mask=mask;
    mpref.msize=msize;
    if (!netcmp(&mpref,*nntab)) shift=1;

    /* try to split the table */
    if (nntabsize>1) {
        mpref.mask=mask1;
        mpref.msize=nextsize;
        med=find_prefix(nntab,nntabsize,&mpref);
        /* if entire table goes to one of the children - simply lengthen the mask,
           do not allocate new node */
        if (!shift && (!med || med==nntab))
                return build_tree(nntab,nntabsize,(med?mask1:mask),nextsize,p);
        /* split point found - build child trees first */
        if (med) medcount=med-nntab; else medcount=nntabsize;
        c0=build_tree(nntab+shift,medcount-shift,mask,nextsize,p);
        c1=build_tree(med,nntabsize-medcount,mask1,nextsize,p);
    }

    if (shift || (!c0 && !c1)) node=*nntab;
    else {
        /* allocate the root unless there are no children or the node preservation
           has been forced */
        if (!(node=(struct net_node *)apr_pcalloc(p,sizeof(struct net_node)))) return NULL; 
        node->mask=mask;node->msize=msize;
        }

    node->child[0]=node->child[1]=NULL;
    if (!c0 && !c1) return node;
    
    /* check the origins of the children */
    asnc0=(c0?c0->origin:0);
    asnc1=(c1?c1->origin:0);

    if (node->origin) {
        /* forget about children that are consistent with our origin */
        if (asnc0 != node->origin) node->child[0]=c0;
        if (asnc1 != node->origin) node->child[1]=c1;
    } else 
        /* if children have identical origins, put one of them in the root and
           forget the children */
        if (asnc0 == asnc1 && asnc0) node->origin=asnc0;
    else
        /* children have different origins / one of them is a leaf - do not
           cut them */
        { node->child[0]=c0;node->child[1]=c1; }
    
    return node;
}

/* index the prefix tree for further dumping */
static void index_tree(struct net_node * node, apr_uint32_t * index)
{
    if (!node) return;
    node->index=(*index)++;
    index_tree(node->child[0],index);
    index_tree(node->child[1],index);
}

/* dump the prefix tree into a table */
static apr_uint32_t dump_tree(struct net_node * node, struct prefix * table)
{
    apr_uint32_t idx;
    if (!node) return 0;
    idx=node->index;
    table[idx].mask=node->mask;
    table[idx].msize=node->msize;
    table[idx].origin=node->origin;
    table[idx].idxchild[0]=dump_tree(node->child[0],table);
    table[idx].idxchild[1]=dump_tree(node->child[1],table);
    return idx;
}

/* insert link to 'peerno' AS into the 'asn's' list of peers */
static int insert_link(struct as * ases, int asn, int peerno, apr_pool_t * p)
{
    struct asn_list *prev,*curr,*fresh;

    fresh = (struct asn_list *)apr_pcalloc(p,sizeof(struct asn_list));
    if (!fresh) return -1;
    fresh->number = peerno;
    ases[asn].link_count++;

    if (!ases[asn].links || ases[asn].links->number > peerno) {
        fresh->next = ases[asn].links;
        ases[asn].links = fresh;
        return 0;
    }
    
    prev=ases[asn].links;curr=prev->next;
    while (curr && curr->number < peerno) {prev=curr;curr=prev->next;}
    
    fresh->next = curr;
    prev->next = fresh;
    return 0;
}

/* put links (a->b and b->a) between 'asn1' and 'asn2' */
static int put_link(struct as * ases, int asn1, int asn2, apr_pool_t * p)
{
    struct asn_list *step;
    int target;
    
    if (!asn1 || !asn2 || asn1 == asn2) return 0;

    /* search the shorter list of peers to check whether the link has been
       put before */
    if (ases[asn1].link_count<ases[asn2].link_count) {step=ases[asn1].links;target=asn2;}
    else {step=ases[asn2].links;target=asn1;}
    
    for (;step;step=step->next) if (step->number == target) return 0;

    if (insert_link(ases,asn1,asn2,p) || insert_link(ases,asn2,asn1,p)) return 0;
    return 1;
}

/* assign prefix 'prefix' to AS 'asn', determine its length if not provided */
static struct net_node * put_net(struct as * ases, int asn, char * prefix, apr_pool_t * p)
{
    char *sep,*pad;
    struct net_node temp,*net;
    apr_uint32_t in;
    
    sep=strchr(prefix,'/');
    if (sep) 
    {
      *sep='\0';temp.msize=atoi(sep+1);
    }
    else 
    {
      sep = prefix + strlen(prefix);
      temp.msize = 0;
    }

    in = apr_inet_addr(prefix);
    if (in == APR_INADDR_NONE)
    {
      return NULL;
    }
    temp.mask = ntohl(in);
    
    if (!temp.msize)
        for (pad=sep-1,temp.msize=32;pad>=prefix && (*pad=='0' || *pad=='.');pad--)
            if (*pad=='.') temp.msize-=8;
    if (!temp.msize) return NULL;
    
    if (!(net=(struct net_node *)apr_pcalloc(p,sizeof(struct net_node)))) return NULL;
    memcpy(net,&temp,sizeof(struct net_node));
    net->origin=asn;
    net->self=net;
    net->next=ases[asn].nets;
    ases[asn].nets=net;
    ases[asn].net_count++;
    return net;
}

/* parse aggregated node in AS-path */
static int parse_aggregated(struct as * ases, char * as_set, int prevasn, apr_uint32_t * linkcount, apr_pool_t * p)
{
    char *eos=as_set+strlen(as_set),*nextas=as_set+1,*nextsep;
    int asn;

    while (nextas < eos) {
        if (!(nextsep=strpbrk(nextas,",}"))) nextsep=eos;
        *nextsep='\0';
        if ((asn=atoi(nextas))) {(*linkcount)+=put_link(ases,prevasn,asn,p);prevasn=asn;}
        nextas=nextsep+1;
    }
    
    return prevasn;
}

static char *
bgp_load(dns_config *cfg, apr_pool_t *ptemp)
{
    char line[LINE_SIZE],*pathoff=line+PATH_OFFSET,*netoff=line+NET_OFFSET;
    apr_uint32_t linecount=0;
    char prefix[PREFIX_SIZE];
    char *eop,*nextas,*nextsp,*eon;
    apr_uint16_t asn,prevasn;
    apr_uint32_t i;

    struct as * ases;
    apr_uint32_t netcount=0,linkcount=0,incmcount=0,ascount=0,index=0;
    apr_uint32_t sections[CLASS_A_COUNT+1],currsect,sect;
    struct net_node *allnets=NULL,**nntab,*nntrees[CLASS_A_COUNT];
    struct net_node *net,**nrd,**nwr,**nlast;
    struct asn_list *link;
    
    struct as_graph * as_graph;
    struct as_lookup_table * as_lookup;
    struct as_data * as_data;
    apr_uint32_t asg_size,asl_size,bgp_size;
    apr_uint16_t *cursor;

#ifdef HAVE_ZLIB_H
    gzFile db;
#else
    apr_file_t * db;
    apr_status_t status;
#endif

    /* allocate the auxiliary table of ases */
    ases=(struct as *)apr_pcalloc(ptemp,sizeof(struct as)*(AS_COUNT));
    if (!ases) return "no memory left for an internal BGP data structure";

    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, 0,"Reading BGP data from '%s'.",cfg->bgp_file_name);

    const char *fname = ap_server_root_relative(ptemp, cfg->bgp_file_name);
#ifdef HAVE_ZLIB_H
    if(!(db = gzopen(fname, "rb")))
      return "cannot open the compressed BGP data file";
#else
    status = apr_file_open(&db, fname, APR_READ|APR_BUFFERED, 0, ptemp);
    if (status != APR_SUCCESS)
        return "cannot open the BGP data file";
#endif

#ifdef HAVE_ZLIB_H
    while(gzgets(db, line, LINE_SIZE) != Z_NULL)
#else
    while((status = apr_file_gets(line,LINE_SIZE, db)),
          status == APR_SUCCESS)
#endif
    {
        /* ignore non-route lines */
        if (strlen(line) < PATH_OFFSET || *line != '*') continue;
        /* cut the route type */
        if (!(eop=strrchr(line,' '))) continue;
        *eop='\0';
        if (!(++linecount%1000000))
            ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, 0, "%d routes analysed.",linecount);
        /* jump to the beginning of AS-path and parse it */
        nextas=pathoff;prevasn=0;
        while (nextas < eop) {
            if (!(nextsp=strchr(nextas,' '))) nextsp=eop;
            *nextsp='\0';
            if (*nextas=='{') asn=parse_aggregated(ases,nextas,prevasn,&linkcount,ptemp);
            else if ((asn=atoi(nextas))) linkcount+=put_link(ases,prevasn,asn,ptemp);
            if (asn) prevasn=asn;
            nextas=nextsp+1;
        }
        /* try to assign the prefix to AS, if ASN is known */
        if (!prevasn) continue;
        /* count incomplete routes - for statistics */
        if (eop[1] == '?') incmcount++;
        /* assign prefix to AS only for best routes, or the last ones, if there
           is no best one */
        if ((eon=strchr(netoff,' ')) != netoff && eon && eon-netoff < PREFIX_SIZE)
            {*eon='\0';strcpy(prefix,netoff);}
        if (line[1] != '>') continue;
        if (!(net=put_net(ases,prevasn,prefix,ptemp))) continue;
        /* put all prefixes on a global list */
        net->all_next=allnets;
        allnets=net;
        netcount++;
    }
    
#ifdef HAVE_ZLIB_H
    gzclose(db);
#else
    apr_file_close(db);
#endif

    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, 0, "BGP data file loaded (%d routes), compressing data..", linecount);
    
    /* count ASes that have any links - only these are relevant */
    for (i=0;i<AS_COUNT;i++) if (ases[i].link_count) ascount++;
    
    /* copy the list of all prefixes to a table */
    if (!(nntab=(struct net_node **)apr_pcalloc(ptemp,netcount*sizeof(struct net_node *))))
            return "no memory left for an internal BGP data structure";

    for (i=0, net=allnets; i<netcount && net; i++, net=net->all_next)
    {
      nntab[i]=net;
    }
    
    /* sort the prefixes like a bitword dictionary */
    qsort(nntab,netcount,sizeof(struct net_node *),pnetcmp);
     
    /* remove duplicate prefixes */
    nrd=nwr=nntab+1;nlast=nntab+netcount;
    while (nrd < nlast) {
        if (!pnetcmp(nrd-1,nrd)) {nrd++;netcount--;continue;}
        if (nrd != nwr) (*nwr)=(*nrd);
        nrd++;nwr++;
    }

    /* split the table into sections, each concerning one class-A network */
    /* in cell i we have the offset where the description of A-class
       network j.0.0.0 starts; j is the least number greater or equal to i
       and having non-zero number of prefixes in the prefixes table */
    sections[0]=0;currsect=0;
    for (i=0;i<netcount;i++) {
        sect=nntab[i]->mask >> 24;
        while (currsect != sect) sections[++currsect]=i;
    }
    while (currsect != CLASS_A_COUNT) sections[++currsect]=netcount;
    
    /* build a prefix tree for each class-a network */
#define sectsize(i) (sections[i+1]-sections[i])
    nntrees[0]=NULL;
    for (index=1,i=1;i<CLASS_A_COUNT;i++) {
        nntrees[i]=build_tree(&nntab[sections[i]],sectsize(i),i<<24,8,ptemp);
        index_tree(nntrees[i],&index);
    }
#undef sectsize

    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, 0, "BGP data compressed, dumping into shared memory..");

    /* time to dump everything to the shm */
    
    asg_size = sizeof(struct as_graph)+(2*linkcount+ascount)*sizeof(apr_uint16_t);
    if (asg_size&7) asg_size+=8-(asg_size&7); /* pad the size to 64bitword */
    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, 0,"AS map size: %d",asg_size);
    asl_size = sizeof(struct as_lookup_table)+(index-1)*sizeof(struct prefix);
    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, 0,"AS lookup table size: %d",asl_size);
    bgp_size = sizeof(struct as_data) + asl_size + asg_size;
    rmmmemory::deallocate(*bgp_data);
    *bgp_data = rmmmemory::allocate(bgp_size);
    memset(*bgp_data,0,bgp_size);
    as_data = (struct as_data *)*bgp_data;
    as_data->timestamp = apr_time_sec(apr_time_now());

    as_graph = as_data->as_graph =
        (struct as_graph *)(((char*)as_data)+sizeof(struct as_data));
    as_lookup = as_data->as_lookup =
        (struct as_lookup_table *)(((char*)as_graph)+asg_size);

    as_graph->size=asg_size;
    as_graph->node_count=ascount;
    as_graph->edge_count=linkcount;
    as_graph->entry_count=2*linkcount+ascount;
    cursor=&(as_graph->peers[1]);
    for(i=0; i<AS_COUNT; i++) {
      if(!ases[i].links)
        continue; /* points to 0 anyway */
      as_graph->offset[i] = cursor - as_graph->peers;
      for(link=ases[i].links; link; link=link->next)
        *(cursor++) = link->number;
      cursor++; /* leave ending null */
    }

    as_lookup->size=asl_size;
    as_lookup->net_count=netcount;
    as_lookup->incmp_count=incmcount;
    as_lookup->entry_count=index-1;
    for (i=0;i<CLASS_A_COUNT;i++)
        as_lookup->offset[i]=dump_tree(nntrees[i],as_lookup->prefixes);

    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, 0,"BGP data loaded and made available.");

    return NULL;
}

/* determine asn for given IP, basing on the lookup table asl (simply
   go down through the binary search tree encoded in the table) */
static apr_uint16_t find_asn(struct as_lookup_table * asl, apr_uint32_t in)
{
    apr_uint32_t mask, idx;
    struct prefix *p;
    apr_uint16_t origin=0;
    apr_byte_t bshift;
    
    mask = ntohl(in);
    idx = asl->offset[mask >> 24];
    
    while (idx) {
        p=&asl->prefixes[idx];
        bshift=32-p->msize;
        if (((mask>>bshift)<<bshift) != p->mask) break;
        idx=p->idxchild[(mask>>(bshift-1))&1];
        if (p->origin) origin=p->origin;
    }
    
    return origin;
}

/* bits */
#define AS_VISITED 1
#define AS_REPLICA 2
#define ASQ_SIZE (1<<14) /* has to be a power of 2 */
#define AS_LAST 0xffff

/* execute shortest-paths algorithm for given AS graph, client IP
   and IPs of replicas; return k addresses of replicas that are the
   closest (in terms of as-hops) to the client */
static int select_replicas(struct as_data * asd, apr_uint32_t clip, std::vector<Peer*>& reptable, int repcount, std::vector<Peer*>& outtable, int limit, apr_pool_t * p)
{
    apr_byte_t ases[AS_COUNT]; /* bits of ASes */
    apr_uint16_t ases_idx[AS_COUNT]; /* auxiliary table with indexes */
    apr_uint16_t asqueue[ASQ_SIZE]; /* a node queue for use during the search */
    int asqstart=0,asqend=0;
    apr_uint16_t client,asn,*asnoff,listwalk;
    apr_uint32_t *offset=asd->as_graph->offset;
    apr_uint16_t *peers=asd->as_graph->peers;
    int i,count=0;
    
    apr_int16_t *ases_list = (apr_int16_t *) apr_pcalloc(p, (repcount * sizeof(apr_int16_t)));

     /* give up if we cannot determine the asn of client */ 
    if (!(client=find_asn(asd->as_lookup,clip))) return 0;
    memset(ases,0,AS_COUNT*sizeof(apr_byte_t));

    /* determine IPs of replicas - for these from the client's ASN put them
       into the response immediately */
    for (i=0;i<repcount && count < limit;i++) {
        asn=find_asn( asd->as_lookup, reptable[i]->address(0,p)); // FIXME context to address() call should not be 0.
        /* create lists of replicas coming from the same AS
           inside the ases_list table */
        if (ases[asn]&AS_REPLICA) ases_list[i]=ases_idx[asn];
        else {ases_list[i]=AS_LAST;ases[asn]|=AS_REPLICA;}
        ases_idx[asn]=i;
        if (asn!=client) continue;
        ases[asn]|=AS_VISITED;
        outtable.push_back(reptable[i]);
        count++;
    }

    /* go through the graph until we have enough results, or there are no
       unvisited nodes; visit each node at most one time */
    asqueue[asqend++]=client;
    while (asqend != asqstart && count < limit) {
        asn=asqueue[asqstart++];asqstart&=ASQ_SIZE-1;
        /* decomposite the node */
        for (asnoff=peers+offset[asn];(asn=*asnoff) && count < limit;asnoff++) {
            /* do not put previously visited nodes into the queue */
            if (ases[asn]&AS_VISITED) continue;
            /* mark as visited yet when putting into queue; none of the
               nodes can be thus put more than once there */
            ases[asn]|=AS_VISITED;
            asqueue[asqend++]=asn;asqend&=ASQ_SIZE-1;
            /* check if the peer contains any replicas just after putting
               it into the queue instead of when taking it out */
            if (!(ases[asn]&AS_REPLICA)) continue;
            /* put the replicas located in the peer to the table of results */
            for (listwalk=ases_idx[asn];
                (listwalk != AS_LAST) && (count < limit);
                listwalk=ases_list[listwalk])
            {
                outtable.push_back(reptable[listwalk]);
                count++;
            }
        } /* for */
    } /* while */

    return count;
}

AutonomousSystemRedirectPolicy::AutonomousSystemRedirectPolicy(apr_uint32_t ttl, const char* clazzname)
  throw()
  : RedirectPolicy(clazzname, ttl)
{
}

AutonomousSystemRedirectPolicy::~AutonomousSystemRedirectPolicy() throw()
{
}

RedirectPolicy*
AutonomousSystemRedirectPolicy::instantiate() throw()
{
  AutonomousSystemRedirectPolicy* policy;
  policy = new(rmmmemory::shm()) AutonomousSystemRedirectPolicy(_ttl);
  policy->_policyname = _policyname;
  return policy;
}

void
AutonomousSystemRedirectPolicy::release() throw()
{
  rmmmemory::destroy(this); // operator delete(this, rmmmemory::shm());
}

/* AS policy initialization; create empty slots for BGP data and the switch */
char*
AutonomousSystemRedirectPolicy::initialize(dns_config* cfg, apr_pool_t* p) throw()
{
  struct as_data *data;
  bgp_data = (void**)rmmmemory::allocate(sizeof(void*));
  *bgp_data = rmmmemory::allocate(sizeof(struct as_data));
  data = (struct as_data *)*bgp_data;
  data->timestamp = 0;
  data->as_graph  = 0;
  data->as_lookup = 0;
  bgp_lock = new(rmmmemory::shm()) Lock(5);
  return NULL;
}

void
AutonomousSystemRedirectPolicy::sync(const std::vector<Peer*>& current,
                                     const std::vector<Peer*>& cleared) throw()
{
}

void
AutonomousSystemRedirectPolicy::run(apr_uint32_t from, int rtcount,
                                    std::vector<Peer*>& list,
                                    dns_config* cfg, apr_pool_t* p) throw()
{
    int rv=0;
    std::vector<Peer*> outtable;

    /* if BGP data is available - call the actual searching function */
    bgp_lock->lock_shared();
    if(*bgp_data && ((struct as_data *)*bgp_data)->timestamp) {
      rv=select_replicas((struct as_data *)*bgp_data,from,list,list.size(),outtable,rtcount,p);
    }
    bgp_lock->unlock();
    if(!rv) {
      /* fall back to static results in case BGP data is not
       * available / does not help
       */ 
      rv=(list.size()<(unsigned)rtcount?list.size():rtcount);
      for(int i=0; i<rv; i++)
        outtable.push_back(list[i]);
    }
    list = outtable;
}

/* as policy process-finishing function - reload BGP data if expired or if none
   is available so far */
void
AutonomousSystemRedirectPolicy::finish(dns_config* cfg, apr_pool_t* p) throw()
{
    apr_status_t status;
    apr_pool_t * ptemp;
    apr_allocator_t * alloctemp;
    apr_uint32_t ctime = apr_time_sec(apr_time_now());
    apr_uint32_t minstamp = ctime - cfg->bgp_refresh;
    char * err;

    if(((struct as_data *)*bgp_data)->timestamp != 0 &&
       ((struct as_data *)*bgp_data)->timestamp >= minstamp)
      return;
    bgp_lock->lock_exclusively();
    if(((struct as_data *)*bgp_data)->timestamp != 0 &&
       ((struct as_data *)*bgp_data)->timestamp >= minstamp) {
      bgp_lock->unlock();
      return;
    }

    /* use a brand new pool for BGP data loading, as there are lots of
       temporary structures created */

    status = apr_allocator_create(&alloctemp);
    if (status != APR_SUCCESS)
        err="cannot create a memory allocator";
    else {
        status = apr_pool_create_ex(&ptemp,p,NULL,alloctemp);
        if (status != APR_SUCCESS)
            err="cannot create a resource sub-pool";
        else {
          // Arno: I don't think registering cleanups as an extra
          // safety is really necessary, as we're creating a subpool
          // here. Anyway:
          apr_pool_cleanup_register( ptemp, ptemp, apr_pool_cleanup_null, apr_pool_cleanup_null );
            err = bgp_load(cfg, ptemp);
            apr_pool_destroy(ptemp);
        }
        apr_allocator_destroy(alloctemp);
    }
    ((struct as_data *)*bgp_data)->timestamp = apr_time_sec(apr_time_now());
    bgp_lock->unlock();

    if (err) ap_log_error(APLOG_MARK, APLOG_CRIT, 0, 0,"BGP load error: %s",err);
}

void
AutonomousSystemRedirectPolicy::destroy() throw()
{
}
