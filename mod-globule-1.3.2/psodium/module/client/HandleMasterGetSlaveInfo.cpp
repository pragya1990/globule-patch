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
static apr_status_t pso_extract_info_from_getslaveinfo( psodium_conf *conf, 
                                             request_rec *r,
                                             apr_off_t brigade_len,
                                             apr_bucket_brigade *bb );
static apr_status_t parse_getslaveinfo( request_rec *r, 
                             char *reply, 
                             apr_size_t reply_len, 
                             psodium_conf *conf,
                             char **error_str_out );

static char * pso_client_make_into_master_GETslaveinfo_request( request_rec *r,
                                                                char *master_id,
                                                                char *slave_id );
static void pso_client_reset_master_GETslaveinfo_request( request_rec *r );
static void pso_client_reset_master_GETslaveinfo_request_headers( request_rec *r );




int pso_client_getslaveinfo_request( psodium_conf *conf, 
                                 request_rec *r,
                                 const char *server_id,
                                 char *url, 
                                 const char *proxyname, 
                                 apr_port_t proxyport )
{
    int access_status=OK;
    
    DPRINTF( DEBUG1, ("pso: client: ==> PHASE 2: Contacting master to obtain slave's key and auditor\n" )); 

    const char *location = apr_table_get( r->headers_out, "Location" );
    if (location == NULL)
    {
        DPRINTF( DEBUG1, ("pso: client: Get slave info: Slave location unknown!\n" )); 
        return HTTP_INTERNAL_SERVER_ERROR;
    }
    apr_uri_t loc_uri;
    apr_status_t status = apr_uri_parse( r->pool, location, &loc_uri );
    if (status != APR_SUCCESS)
    {
        DPRINTF( DEBUG1, ("pso: client: Get slave info: Slave location not valid URI!\n" )); 
        return HTTP_BAD_GATEWAY;
    }
    char *slave_id = loc_uri.hostinfo;
    char *master_id = (char *)server_id;
    
    char *slaveinfo_url_str = pso_client_make_into_master_GETslaveinfo_request( r, master_id, slave_id );
    
    access_status = renamed_ap_proxy_http_handler( r, conf, slaveinfo_url_str, proxyname, proxyport );
    DPRINTF( DEBUG1, ("psodium: client: GETSLAVEINFO from master returned %d\n", access_status ));
    pso_client_reset_master_GETslaveinfo_request( r );

    if (access_status != 0)
    {
        pso_error_send_connection_failed( r, access_status, "master" );
        return OK;
    }
    
    return access_status;
}


pso_response_action_t pso_client_getslaveinfo_response( psodium_conf *conf, 
                                 request_rec *r,
                                  apr_bucket_brigade *bb,
                                  apr_off_t brigade_len,
                                  apr_bucket_brigade **newbb_out )
{        
    pso_client_reset_master_GETslaveinfo_request_headers( r ); // to allow 1 server to be master/slave/client

    if (r->status != HTTP_OK)
    {
        char *error_str = "Could not get slave's public key from master!";
        DOUT( DEBUG1, "psodium: client: " << error_str ); 
        pso_error_in_html_to_bb( r, error_str, newbb_out );
        return RESPONSE_ACTION_SEND_NEWBB;
    }

    DPRINTF( DEBUG1, ("psodium: client: Handling getSlaveInfo Reply: Recording returned slave info.\n" )); 
    
    // We're a client. We try to extract slave_id and the slave's
    // public key and the auditor address for this site.

    (void)pso_extract_info_from_getslaveinfo( conf, r, brigade_len, bb );
    
    // Client will have already gotten HTTP 302 redirect from us.
    return RESPONSE_ACTION_NOSEND;
}
        
        


/* 
 * Read extra info from getSlaveInfo command from master.
 * 
 * This method is called for each bucket brigade! 
 *
 * BUG: TODO: this code assumes this bucket brigade contains all info!
 *
 */
static apr_status_t pso_extract_info_from_getslaveinfo( psodium_conf *conf, 
                                             request_rec *r,
                                             apr_off_t brigade_len,
                                             apr_bucket_brigade *bb )
{
    apr_status_t    status=APR_SUCCESS;
    char *reply=NULL;
    char *error_str="error_str not set.";
    apr_size_t reply_idx=0;
    apr_bucket *e=NULL;

    if (brigade_len > 1*1024*1024L)
    {
        DPRINTF( DEBUG0, ("psodium: client: Parsing getslaveinfo from master: Response too big: %ld bytes", brigade_len ));
        return APR_EINVAL;
    }
    else if (brigade_len == 0)
    {
        DPRINTF( DEBUG0, ("psodium: client: Parsing getslaveinfo from master: no data to parse.\n" )); 
        return APR_SUCCESS;
    }

    DPRINTF( DEBUG1, ("psodium: client: Parsing getslaveinfo from master:  Extracting slave info from reply of %d bytes\n", brigade_len )); 

    reply = (char *)apr_palloc( r->pool, brigade_len );
    if (reply == NULL)
        return APR_ENOMEM;

    /*
     * Read master's reply into buffer.
     */

    for (e = APR_BRIGADE_FIRST(bb); e != APR_BRIGADE_SENTINEL(bb); e = APR_BUCKET_NEXT(e)) 
    {
        const char *block;
        apr_size_t block_len;
        apr_size_t  i=0;

        if (APR_BUCKET_IS_EOS(e)) 
        {
            DPRINTF( DEBUG5, ("psodium: master digest reply totals %d", reply_idx ));
            break;
        }

        if (APR_BUCKET_IS_FLUSH(e)) 
        {
            /* Arno: can we have flush buckets here? */
            continue;
        }

        DPRINTF( DEBUG5, ("psodium: client: Parsing getslaveinfo from master: extracting slave info: before read\n" )); 

        /* read */
        apr_bucket_read(e, &block, &block_len, APR_BLOCK_READ);

        ap_assert( reply_idx+block_len <= brigade_len );

        DPRINTF( DEBUG5, ("psodium: client: Parsing getslaveinfo from master: extracting slave info: before memcpy\n" )); 

        memcpy( reply+reply_idx, block, block_len );
        reply_idx += block_len;

        DPRINTF( DEBUG5, ("psodium: client: Parsing getslaveinfo from master: reading %d bytes from bucket", block_len ));

        DPRINTF( DEBUG5, ("psodium: client: Parsing getslaveinfo from master: : read <%s>", block ));
    }

    DPRINTF( DEBUG5, ("psodium: client: Parsing getslaveinfo from master: extracting slave info: before parse_getslaveinfo\n" )); 

    status = parse_getslaveinfo( r, reply, reply_idx, conf, &error_str );
    if (status != APR_SUCCESS)
    {
        DPRINTF( DEBUG0, ("psodium: client: Parsing getslaveinfo from master: Error:  %s\n", error_str )); 
    }
    return status;
}



static apr_status_t parse_getslaveinfo( request_rec *r, 
                             char *reply, 
                             apr_size_t reply_len, 
                             psodium_conf *conf, 
                             char **error_str_out )
{
    const char *master_id = NULL, *slave_id = NULL, *auditor_addr = NULL;
    const char *slave_pubkey_pem=NULL;
    const char *slave_uri_prefix=NULL;
    apr_uri_t slave_uri_record, *slave_uri=&slave_uri_record;
    apr_status_t status=APR_SUCCESS;
    apr_size_t soffset=0,eoffset=0;

    /*
     * Host: header in original request contains master_id
     * TODO: master portno??????????
     */ 
    // TODO: HTTP 1.0
    master_id = apr_table_get( r->headers_in, "Host" );

    slave_id = apr_table_get( r->notes, STORED_SLAVEID_NOTE );
    DPRINTF( DEBUG1, ("psodium: client: Parsing getslaveinfo from master %s: slave_id is %s\n", master_id, slave_id )); 
    DPRINTF( DEBUG5, ("psodium: client: Parsing getslaveinfo from master: reply is %p reply_len is %d\n", reply, reply_len )); 
    /*
     * Now sniff slave's public key, uri prefix and auditor address from reply 
     */
     //
     // SLAVEURIPREFIX <uri>CRLF
     //
    soffset = pso_find_keyword( reply, 0, reply_len, SLAVE_URI_PREFIX_KEYWORD );
    if (soffset == -1)
    {
        *error_str_out = "Cannot find SLAVE_URI_PREFIX_KEYWORD keyword in response body.";
        return APR_EINVAL;
    }

    DPRINTF( DEBUG5, ("psodium: client: Parsing getslaveinfo from master: after find SLAVE_URI_PREFIX_KEYWORD\n" )); 

    soffset = pso_skip_whitespace( reply, soffset+strlen( SLAVE_URI_PREFIX_KEYWORD ), reply_len );
    if (soffset == -1)
    {
        *error_str_out = "Cannot find value after SLAVE_URI_PREFIX_KEYWORD in response body.";
        return APR_EINVAL;
    }

    DPRINTF( DEBUG5, ("psodium: client: Parsing getslaveinfo from master: After skip white space PREFIX\n" )); 

    eoffset = pso_find_keyword( reply, soffset, reply_len, CRLF );
    if (eoffset == -1)
    {
        *error_str_out = "Cannot find CRLF after SLAVE_URI_PREFIX_KEYWORD keyword in response body.";
        return APR_EINVAL;
    }

    DPRINTF( DEBUG5, ("psodium: client: Parsing getslaveinfo from master: After find CRLF PREFIX\n" )); 

    slave_uri_prefix = pso_copy_tostring( reply, soffset, eoffset, r->pool );
     
    DPRINTF( DEBUG1, ("psodium: client: Parsing getslaveinfo from master: URI prefix for this slave is %s\n", slave_uri_prefix )); 


    soffset=0;
     //
     // AUDITORADDR <hostname>:<port>CRLF
     //
    soffset = pso_find_keyword( reply, soffset, reply_len, AUDITOR_ADDR_KEYWORD );
    if (soffset == -1)
    {
        *error_str_out = "Cannot find AUDITORADDR keyword in response body.";
        return APR_EINVAL;
    }

    DPRINTF( DEBUG5, ("psodium: client: Parsing getslaveinfo from master: after find AUDITORADDR\n" )); 

    soffset = pso_skip_whitespace( reply, soffset+strlen( AUDITOR_ADDR_KEYWORD ), reply_len );
    if (soffset == -1)
    {
        *error_str_out = "Cannot find value after AUDITORADDR in response body.";
        return APR_EINVAL;
    }

    DPRINTF( DEBUG5, ("psodium: client: Parsing getslaveinfo from master: After skip white space ADDR\n" )); 

    eoffset = pso_find_keyword( reply, soffset, reply_len, CRLF );
    if (eoffset == -1)
    {
        *error_str_out = "Cannot find CRLF after AUDITORADDR keyword in response body.";
        return APR_EINVAL;
    }

    DPRINTF( DEBUG5, ("psodium: client: Parsing getslaveinfo from master: After find CRLF ADDR\n" )); 

    auditor_addr = pso_copy_tostring( reply, soffset, eoffset, r->pool );
     
    DPRINTF( DEBUG1, ("psodium: client: Parsing getslaveinfo from master: Auditor for this master is %s\n", auditor_addr )); 


    //
    // pub key
    //
    soffset=0;
    soffset = pso_find_keyword( reply, soffset, reply_len, KEY_START_KEYWORD );
    if (soffset == -1)
    {
        *error_str_out = "Cannot find slave's public key certificate in response body, BEGIN missing";
        return APR_EINVAL;
    }

    DPRINTF( DEBUG5, ("psodium: client: Parsing getslaveinfo from master: got start cert >\n" ));

    eoffset = pso_find_keyword( reply, soffset, reply_len, KEY_END_KEYWORD );
    if (eoffset == -1)
    {
        *error_str_out = "BEGIN CERTIFICATE not followed by END in response body";
        return APR_EINVAL;
    }

    DPRINTF( DEBUG5, ("psodium: client: Parsing getslaveinfo from master: got end cert >\n" ));

    slave_pubkey_pem = pso_copy_tostring( reply, soffset, eoffset+strlen( KEY_END_KEYWORD), r->pool );

    DPRINTF( DEBUG1, ("psodium: client: Parsing getslaveinfo from master: Registering as known slave for master [%s]: slave [%s], with auditor [%s], pubkey [%s]", 
                              master_id, slave_id, auditor_addr, slave_pubkey_pem )); 

    // Add to shared memory database                              
    pso_add_slave_record( conf,  slave_id, master_id, slave_pubkey_pem, auditor_addr, slave_uri_prefix );

    return APR_SUCCESS;                              
}   





static char * pso_client_make_into_master_GETslaveinfo_request( request_rec *r,
                                                                char *master_id,
                                                                char *slave_id )
{
    char *slaveinfo_url_str = NULL;
    char *query = NULL, *encoded_query = NULL, *digest_url = NULL;
    apr_size_t encoded_len, encoded_len2=0;

    apr_table_set( r->notes, RECEIVER_COMMAND_NOTE, RECORD_SLAVEINFO_COMMAND );
    apr_table_set( r->notes, STORED_SLAVEID_NOTE, slave_id );

    
    // Encode Slave URI so we can safely pass it in a
    // getSlaveInfo? request.
    query = apr_pstrcat( r->pool, slave_id, NULL );
    encoded_len = apr_base64_encode_len( strlen( query ) );
    encoded_query = (char *)apr_palloc( r->pool, encoded_len );
    encoded_len2 = apr_base64_encode( encoded_query, query, strlen( query ) );
    ap_assert( encoded_len == encoded_len2 );

    slaveinfo_url_str = apr_pstrcat( r->pool, "http://", master_id, 
                              PSO_GETSLAVEINFO_URI_PREFIX, "?", encoded_query, 
                              NULL );
    return slaveinfo_url_str;
}


static void pso_client_reset_master_GETslaveinfo_request( request_rec *r )
{
    apr_table_unset( r->notes, RECEIVER_COMMAND_NOTE );
    apr_table_unset( r->notes, STORED_SLAVEID_NOTE );
    pso_client_reset_master_GETslaveinfo_request_headers( r );
}

static void pso_client_reset_master_GETslaveinfo_request_headers( request_rec *r )
{
}
