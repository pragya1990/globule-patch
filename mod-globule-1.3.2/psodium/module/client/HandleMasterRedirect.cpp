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
// APL

#include "mod_psodium.h"

/*
 * Prototypes
 */
static apr_status_t pso_extract_info_from_redirect( psodium_conf *conf, 
                                             request_rec *r,
                                             apr_off_t brigade_len,
                                             apr_bucket_brigade *bb );
static apr_status_t parse_redirect( request_rec *r, 
                             char *reply, 
                             apr_size_t reply_len, 
                             psodium_conf *conf,
                             char **error_str_out );

static void pso_client_make_into_master_GETredir_request( psodium_conf *conf, 
                                                   request_rec *r );
static void pso_client_reset_master_GETredir_request( request_rec *r );
static void pso_client_reset_master_GETredir_request_headers( request_rec *r );




int pso_client_redirect_request( psodium_conf *conf, 
                                 request_rec *r,
                                 const char *server_id,
                                 char *url, 
                                 const char *proxyname, 
                                 apr_port_t proxyport )
{
    int access_status=OK;

    DPRINTF( DEBUG1, ("pso: client: ==> PHASE 1: Contacting master so it can redirect us to a slave\n" )); 
    
    // Contacting what we assume is the master
    pso_client_make_into_master_GETredir_request( conf, r );
    access_status = renamed_ap_proxy_http_handler( r, conf, url, proxyname, proxyport );
    DPRINTF( DEBUG1, ("psodium: client: Redirect solicitation at master returned %d\n", access_status ));
    pso_client_reset_master_GETredir_request( r );
    
    if (access_status != 0)
    {
        pso_error_send_connection_failed( r, access_status, "master" );
        return OK;
    }

    return access_status;
}


pso_response_action_t pso_client_redirect_response( psodium_conf *conf, 
                                 request_rec *r,
                                  apr_bucket_brigade *bb,
                                  apr_off_t brigade_len )                                 
{        
    pso_client_reset_master_GETredir_request_headers( r ); // to allow 1 server to be master/slave/client

    return RESPONSE_ACTION_SEND;
}



static void pso_client_make_into_master_GETredir_request( psodium_conf *conf, 
                                                   request_rec *r )
{
    char *list_str = NULL;
    apr_table_set( r->headers_in, WANT_SLAVE_HEADER, TRUE_STR );
    apr_table_set( r->notes, RECEIVER_COMMAND_NOTE, IGNORE_REDIRECT_REPLY_COMMAND );
    
#ifdef LATER
    // Add list of bad slaves which we (client) don't want to talk to
    // anymore
    list_str = pso_client_badslaves_to_header( conf, r->pool );
    if (list_str != NULL)
    {
        apr_table_set( r->headers_in, BADSLAVES_HEADER, list_str );
    }
#endif
}

static void pso_client_reset_master_GETredir_request( request_rec *r )
{
    apr_table_unset( r->notes, RECEIVER_COMMAND_NOTE );
    pso_client_reset_master_GETredir_request_headers( r );
}

static void pso_client_reset_master_GETredir_request_headers( request_rec *r )
{
    apr_table_unset( r->headers_in, WANT_SLAVE_HEADER );
#ifdef LATER
    apr_table_unset( r->headers_in, BADSLAVES_HEADER );
#endif
}
