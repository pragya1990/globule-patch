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

#include <stdlib.h>

#include <httpd.h>
#include <http_log.h>
extern "C" {
#include <ap_listen.h>
}
#include <apr.h>
#include <apr_version.h>
#include <apr_network_io.h>
#include <apr_buckets.h>
#include <apr_strings.h>

#include <util_filter.h>

#ifdef WIN32
#include <Winsock2.h>     // Special WSAIOCTL needed
#include <apr_portable.h> // Direct access to the SOCKET type
#endif /* WIN32 */

#include "mod_netairt.h"
#include "dns_comm.h"
#include "dns_config.h"
#include "dns_protocol.h"

/* Stuff needed for storing DNS UDP datagrams */
/* This stores one series of UDP datagrams (dns_request) */
/* read with one accept_func call */
typedef struct dns_udp_list_t {
  struct dns_udp_list_t *next;
  dns_request *dns_requests;
} dns_udp_list_t;

/* Main storage structure, storing a list of lists of dns_requests */
typedef struct dns_udp_datagrams_t {
  dns_udp_list_t *dghead;
  dns_udp_list_t *dgtail;
  apr_thread_mutex_t *dgmutex;
  int counter;
} dns_udp_datagrams_t;

#ifdef DNS_REDIRECTION
/* This is a global accessible structure */
dns_udp_datagrams_t dns_udp_datagrams = {NULL, NULL, NULL, 0};
#endif /* DNS_REDIRECTION */

#define MAX_ROOT_COMMAND 512

/* UDP listener structure - used for comparison when trying to reuse LRs */
static ap_listen_rec *udp_lr = NULL;

/* bool - whether we need to configure udp_lr during the next dns_comm_init call */
static char udp_lr_init_needed = 0;


/* communication module initialization */
char * dns_comm_init(dns_config * cfg, apr_pool_t * p)
{
#ifdef DNS_REDIRECTION
#ifdef WIN32
    DWORD dwBytesReturned = 0;
    BOOL bNewBehavior = FALSE;
    DWORD status;
    DWORD dwErr;
    SOCKET nsd;
#endif /* WIN32 */

    apr_status_t stat;

    // Create the mutex if it does not exist yet.
    if (dns_udp_datagrams.dgmutex == NULL) {
      stat = apr_thread_mutex_create(&dns_udp_datagrams.dgmutex,
                                     APR_THREAD_MUTEX_DEFAULT, p);
      if(stat != APR_SUCCESS) {
        return "Error creating DNS UDP Datagrams lock.";
      }
    }

    if (udp_lr_init_needed) {
      stat = apr_socket_opt_set(udp_lr->sd, APR_SO_REUSEADDR, 1);
      if (stat != APR_SUCCESS  && !APR_STATUS_IS_ENOTIMPL(stat))
        return "setsockopt(SO_REUSEADDR) failed for UDP socket";

      stat = apr_socket_timeout_set(udp_lr->sd, 0);
      if (stat != APR_SUCCESS && !APR_STATUS_IS_ENOTIMPL(stat))
        return "setsockopt(SO_TIMEOUT) failed for UDP socket";

      stat = apr_socket_opt_set(udp_lr->sd, APR_SO_NONBLOCK, 0);
      if (stat != APR_SUCCESS && !APR_STATUS_IS_ENOTIMPL(stat))
        return "setsockopt(SO_TIMEOUT) failed for UDP socket";
             
#ifdef WIN32
      /* Windows 2000 (and XP) systems incorrectly cause UDP sockets using WASRecvFrom
       * to not work correctly, returning a WSACONNRESET error when a WSASendTo
       * fails with an "ICMP port unreachable" response and preventing the
       * socket from using the WSARecvFrom in subsequent operations.
       * The function below fixes this, but requires that Windows 2000
       * Service Pack 2 or later be installed on the system.  NT 4.0
       * systems are not affected by this and work correctly.
       * See Microsoft Knowledge Base Article Q263823 for details of this.
      */
      // Get the real SOCKET
      apr_os_sock_get(&nsd, udp_lr->sd);
      status = WSAIoctl(nsd, SIO_UDP_CONNRESET, &bNewBehavior, sizeof(bNewBehavior),
                        NULL, 0, &dwBytesReturned, NULL, NULL);

      if (SOCKET_ERROR == status) {
        dwErr = WSAGetLastError();
        if (WSAEWOULDBLOCK == dwErr) {
              // nothing to do
        } else {
          ap_log_perror(APLOG_MARK, APLOG_CRIT, dwErr, p, "Could not reset windows socket using WSAIoctl(SIO_UDP_CONNRESET)");
        }
      }
#endif  /* WIN32 */

      stat = apr_socket_bind(udp_lr->sd, udp_lr->bind_addr);
      if (stat != APR_SUCCESS)
        return "bind failed for UDP socket";
        
      udp_lr_init_needed=0;
    }
        
#endif /* DNS_REDIRCETION */

    return NULL;
}

/* communication module destruction */
void dns_comm_destroy()
{
#ifdef DNS_REDIRECTION
  apr_thread_mutex_destroy(dns_udp_datagrams.dgmutex);
#endif
}


#ifdef DNS_REDIRECTION
static void add_datagrams(dns_request *dns_requests)
{
  dns_udp_list_t *new_dglist;

  if (dns_requests == NULL) {
    return;
  }

  new_dglist = (dns_udp_list_t *) malloc(sizeof(dns_udp_list_t));
  if (new_dglist == NULL) {
    DIAG(0,(MONITOR(fatal)),("Ouch!  Out of memory in add_datagrams()!"));
    return;
  }
  
  new_dglist->next = NULL;
  new_dglist->dns_requests = dns_requests;

  // Acquire lock
  apr_thread_mutex_lock(dns_udp_datagrams.dgmutex);

  // If there is a tail, append this behind it
  if (dns_udp_datagrams.dgtail != NULL) {
          dns_udp_datagrams.dgtail->next = new_dglist;
  }            
          
        // We are the new tail
  dns_udp_datagrams.dgtail = new_dglist;
    
  // If there is no head, we are the new head
  if (!dns_udp_datagrams.dghead) {
          dns_udp_datagrams.dghead = new_dglist;
        }

  dns_udp_datagrams.counter++;
  
  // Release the lock
  apr_thread_mutex_unlock(dns_udp_datagrams.dgmutex);
}

static dns_request *remove_datagrams(void)
{
  dns_udp_list_t *dgl = NULL;
  dns_request *dns_requests = NULL;

  // Acquire lock
  apr_thread_mutex_lock(dns_udp_datagrams.dgmutex);

  // Check if there are any datagrams ready
  if (!dns_udp_datagrams.dghead) {
    apr_thread_mutex_unlock(dns_udp_datagrams.dgmutex);
          return NULL;
  }
  
  // Take the first packet
  dgl = dns_udp_datagrams.dghead;
  
  // Update the head
  dns_udp_datagrams.dghead = dgl->next;
    
  // Check if this was the last list
  if (dns_udp_datagrams.dghead == NULL) {
          dns_udp_datagrams.dgtail = NULL;
  }
  
  dns_udp_datagrams.counter--;
  
  // Release the lock  
  apr_thread_mutex_unlock(dns_udp_datagrams.dgmutex);
  
  dns_requests = dgl->dns_requests;
  free(dgl);

  return dns_requests;
}
#endif // DNS_REDIRECTION


/* create a new listener structure and insert it into the global list;
   reuse previously allocated listeners, if possible */
ap_listen_rec * dns_comm_alloc_listener(apr_pool_t * p, char * host, apr_port_t port, int type)
{
#ifdef DNS_REDIRECTION
    apr_status_t stat = 0;
    ap_listen_rec *lr = 0;
    char *err = 0;

    ap_listen_rec **walk;
    apr_sockaddr_t *sa;

    for (walk = &ap_old_listeners; *walk; walk = &(*walk)->next) {
        sa = (*walk)->bind_addr;
        if (!sa)
          continue;
          
        if (*walk == udp_lr && type != SOCK_DGRAM)
          continue;
          
        if (type == SOCK_STREAM && (*walk)->process_func)
          continue;
        
        if (strcasecmp(sa->hostname, host) || sa->port != port)
          continue;
        
        lr = *walk;
        *walk = lr->next;
        lr->next = ap_listeners;
        ap_listeners = lr;
        
        return lr;
    }

    lr = (ap_listen_rec*) apr_palloc(p, sizeof(ap_listen_rec));
    lr->active = 0;
    lr->accept_func = NULL;
    lr->process_func = NULL;
    stat = apr_sockaddr_info_get(&lr->bind_addr, host, APR_INET, port, 0, p);
    if (stat != APR_SUCCESS) 
    { 
      err = "sockaddr_get failed";
      goto alloc_listener_failed;
    }

    stat = apr_socket_create(&lr->sd, APR_INET, type,
#if (APR_MAJOR_VERSION > 0)
                             (type == SOCK_DGRAM ? APR_PROTO_UDP
                                                 : APR_PROTO_TCP),
#endif
                             p);
    if (stat != APR_SUCCESS) { 
      err = "socket_create failed";
      goto alloc_listener_failed;
    }

    lr->next = ap_listeners;
    ap_listeners = lr;

    if (type == SOCK_DGRAM) {
        /* mark UDP sockets as active so that Apache did not touch them */
        lr->active = 1;
        udp_lr = lr;
        udp_lr_init_needed = 1;
    }

    return lr;

alloc_listener_failed:
    ap_log_error(APLOG_MARK, APLOG_CRIT, stat, 0,"dns_comm_alloc_listener: %s", err);
#endif
    return NULL;
}

#ifdef DNS_REDIRECTION
/* accept function for the UDP DNS socket, NOTE: the POOL will be NULL
 * as we don't need it but we have to adhere to the accept_func prototype.
 */
apr_status_t dns_comm_accept_udp(void **csd, ap_listen_rec *lr, apr_pool_t *p)
{
    dns_request *request = NULL;
    dns_request *list = NULL;
    apr_status_t stat;
    apr_sockaddr_t *temp;

    // Windows doesn't like it when directly specifying the request->from
    temp = (apr_sockaddr_t *) malloc(sizeof(apr_sockaddr_t));
    if (temp == NULL) {
      DIAG(0,(MONITOR(fatal)),("dns_comm_accept_udp: RAN OUT OF MEMORY !"));
      return APR_EGENERAL;
    }

    /* prefork MPM needs csd to be set */
    *csd = lr->sd;

    /* retrieve all datagrams from the socket buffer */
    for (;;) {
        request = (dns_request *) calloc(1, sizeof(dns_request));
        if (!request)
        {
          DIAG(0,(MONITOR(fatal)),("dns_comm_accept_udp: Could not allocate memory"));
          free(temp);
          return APR_EGENERAL;
        }

        request->size = NS_PACKETSZ;
        request->maxsize = NS_PACKETSZ;
        
        // Temp mem can't be all 0s, windows does some weird mem check
        memset(temp, 1, sizeof(apr_sockaddr_t));
   
        // Receive the UDP datagram
        stat = apr_socket_recvfrom(temp, lr->sd, 0, request->data, &(request->size));
        memcpy(&request->from, temp, sizeof(apr_sockaddr_t));

        if (stat != APR_SUCCESS) {
          // no success will indicate that the recvfrom would block
          free(request);
          free(temp);
          break;
        } 

        request->next = list;
        list = request;
    }

    // Add the dns requests
    add_datagrams(list);
    
    return APR_SUCCESS;
}


/* processing function for the UDP DNS socket */
apr_status_t dns_comm_process_udp(ap_listen_rec *lr, server_rec *s, apr_pool_t *p)
{
    dns_request *request = NULL, *temp = NULL;
    apr_status_t stat;
    apr_size_t msglen;
    int failed = 0;

    /* Get the datagrams list */
    temp = remove_datagrams();

    /* process all the UDP datagrams one by one */
    request = temp;
    while (request != NULL) {
        
      /* call the DNS protocol module */
      msglen = dns_protocol_run(request, globule_dns_config, p);
      if(msglen > 0) {
        
        /* send a DNS response, if available */
        stat = apr_socket_sendto(lr->sd, &request->from, 0, request->data, &msglen);

        /* count send failures for debugging purposes */
        if (stat != APR_SUCCESS)
          failed++;
      }

        temp = request;
        request = request->next;
        free(temp);
    }
    
    if (failed)
      DIAG(0,(MONITOR(fatal)),("dns_comm_process_udp: sendto calls failed"));

    /* call 'protocol-finish' routine to enable any additional operations
       (like BGP data file load) */
    dns_protocol_finish(globule_dns_config, p);

    return APR_SUCCESS;
}
#endif // DNS_REDIRECTION

/* receive and translate data from the Apache-native connection
   abstraction to raw content */
static int tcp_read(conn_rec * c, char * buf, int size, ap_input_mode_t mode)
{
    apr_bucket_brigade *bb = apr_brigade_create(c->pool,c->bucket_alloc);
    apr_off_t off = size;
    apr_status_t stat;

    stat = ap_get_brigade(c->input_filters, bb, mode, APR_BLOCK_READ, size);
    if (stat != APR_SUCCESS || APR_BRIGADE_EMPTY(bb))
      goto tcpread_failed;
    
    stat = apr_brigade_flatten(bb, buf, (apr_size_t*) &off);    
    if (stat == APR_SUCCESS)
      return (int) off;
                
tcpread_failed:
    apr_brigade_destroy(bb);
    return -1;
}

/* translate and send data using the Apache-native connection
   abstraction */
static int tcp_write(conn_rec * c, char * buf, int size, int mode)
{
    apr_bucket_brigade *bb = apr_brigade_create(c->pool,c->bucket_alloc);
    apr_bucket *b;
    apr_status_t stat;

    if (mode == AP_MODE_GETLINE)
        stat = apr_brigade_printf(bb,NULL,NULL,"%s",buf);
    else 
        stat = apr_brigade_write(bb,NULL,NULL,buf,size);
        
    if (stat != APR_SUCCESS)
      goto tcpwrite_failed;
    
    b = apr_bucket_flush_create(c->bucket_alloc); 
    APR_BRIGADE_INSERT_TAIL(bb, b);
    ap_pass_brigade(c->output_filters, bb);
    return 0;

tcpwrite_failed:
    apr_brigade_destroy(bb);
    return -1;
}

/* processing function for TCP DNS socket */
apr_status_t dns_comm_process_tcp(conn_rec * c, dns_config * cfg)
{
    struct dns_request_tcp tcpreq;
    dns_request *request = (dns_request*) &tcpreq;
    apr_uint16_t msglen;

    /* receive two-bytes message size field */
    if (tcp_read(c, (char*)&msglen, NS_INT16SZ, AP_MODE_READBYTES) == -1)
        /* we return 'OK' each time so that Apache did not pass this
           connection to the HTTP handlers */
        return OK;
    msglen = ntohs(msglen);
    
    /* receive the actual message */
    if (tcp_read(c, request->data, msglen, AP_MODE_READBYTES) == -1)
        return OK;

    request->from = *c->remote_addr;
    request->size = msglen;
    request->maxsize = TCP_PACKETSZ;

    /* call the DNS protcol module */    
    msglen = dns_protocol_run(request,cfg,c->pool);
    if (!msglen)
      return OK;
    
    /* send two-byte response size field */
    msglen = htons(msglen);
    if (tcp_write(c, (char*) &msglen, NS_INT16SZ, AP_MODE_READBYTES) == -1)
        return OK;
    msglen = ntohs(msglen);

    /* send the actual response */
    tcp_write(c, request->data, msglen, AP_MODE_READBYTES);

    /* call 'protocol-finish' routine, just like in the UDP version */
    dns_protocol_finish(cfg,c->pool);
    
    return OK;    
}
