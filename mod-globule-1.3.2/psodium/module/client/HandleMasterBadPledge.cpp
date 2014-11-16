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


static char *pso_client_make_into_master_BADPLEDGE_request( request_rec *r, 
                                                     char *master_id, 
                                                     char *slave_id );
static void pso_client_reset_master_BADPLEDGE_request( request_rec *r );




int pso_client_badpledge_request_iff( psodium_conf *conf, 
                                 request_rec *r,
                                 char *master_id,
                                 char *slave_id,
                                 char *url, 
                                 const char *proxyname, 
                                 apr_port_t proxyport,
                                 apr_bool_t *sent_out )
{
    const char *sender_command=NULL;
    int access_status=OK;
    
    *sent_out = FALSE;
    
    sender_command = apr_table_get( r->notes, SENDER_COMMAND_NOTE );
    if (sender_command != NULL && !strcmp( sender_command, SEND_BADPLEDGE_COMMAND ))
    {
        // The pledge the slave sent was bad and traceable to the slave
        DPRINTF( DEBUG1, ("psodium: slave: Reporting bad slave %s to master %s via BADPLEDGE request\n", slave_id, master_id ));
        char *badp_url = pso_client_make_into_master_BADPLEDGE_request( r, master_id, slave_id );
        access_status = renamed_ap_proxy_http_handler( r, conf, badp_url, proxyname, proxyport );
        DPRINTF( DEBUG1, ("psodium: client: BADPLEDGE to master returned %d\n", access_status ));
        pso_client_reset_master_BADPLEDGE_request( r );
        if (access_status != 0)
        {
            pso_error_send_connection_failed( r, access_status, "master" );
            return OK;
        }

        *sent_out = TRUE;
        
        // Forward response from master, not an error
    }
    return OK;
}        




pso_response_action_t pso_client_badpledge_response( psodium_conf *conf, 
                                  request_rec *r, 
                                  request_rec *rp, 
                                  const char *content_length_str,
                                  apr_size_t content_idx, 
                                  apr_size_t content_length,
                                  apr_size_t pledge_length,
                                  apr_bucket_brigade *bb,
                                  apr_off_t brigade_len, 
                                  apr_bucket_brigade **newbb_out,
                                  apr_file_t **temp_file_out,
                                  EVP_MD_CTX *md_ctxp,
                                  digest_t **digest_array_out, 
                                  apr_size_t *digest_array_length_out )
{
    return RESPONSE_ACTION_SEND;
}


static char *pso_client_make_into_master_BADPLEDGE_request( request_rec *r, char *master_id, char *slave_id )
{
    char *query = NULL, *encoded_query = NULL, *digest_url = NULL;
    const char *pledge_base64=NULL;
    apr_size_t encoded_len, encoded_len2=0;

    apr_table_set( r->notes, RECEIVER_COMMAND_NOTE, FORWARD_COMMAND );

    pledge_base64 = retrieve_pledge_encoded( r );
    
    query = apr_pstrcat( r->pool, slave_id, "#", pledge_base64, NULL );
    encoded_len = apr_base64_encode_len( strlen( query ) );
    encoded_query = (char *)apr_palloc( r->pool, encoded_len );
    encoded_len2 = apr_base64_encode( encoded_query, query, strlen( query ) );
    ap_assert( encoded_len == encoded_len2 );

    digest_url = apr_pstrcat( r->pool, "http://", master_id, 
                              PSO_BADPLEDGE_URI_PREFIX, "?", encoded_query, 
                              NULL );
    return digest_url;
}


static void pso_client_reset_master_BADPLEDGE_request( request_rec *r )
{
    apr_table_unset( r->notes, RECEIVER_COMMAND_NOTE );
}

