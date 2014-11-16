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

static apr_status_t pso_extract_digests( psodium_conf *conf, 
                                  request_rec *r, 
                                  request_rec *rp, 
                                  apr_off_t brigade_len,
                                  apr_bucket_brigade *bb, 
                                  char **error_str_out,
                                  digest_t **digest_array_out, 
                                  apr_size_t *digest_array_length_out );
static apr_status_t parse_digests( request_rec *r, 
                            char *reply, 
                            apr_size_t reply_len, 
                            psodium_conf *conf, 
                            char **error_str_out,
                            digest_t **digest_array_out, 
                            apr_size_t *digest_array_length_out );

static char * pso_client_make_into_master_getDigest_request( request_rec *r, 
                                                        char *master_id, 
                                                        char *slave_id );
static void pso_client_reset_master_getDigest_request( request_rec *r );



/*=================================== Implementation =======================*/

int pso_client_getdigests_request( psodium_conf *conf, 
                                 request_rec *r,
                                 char *master_id,
                                 char *slave_id,
                                 char *url, 
                                 const char *proxyname, 
                                 apr_port_t proxyport,
                                 apr_bool_t *badslave_out )
{
    int access_status=OK;
    const char *pledge_correct_str=NULL;
    char *digest_url = NULL;
    
    // Only if the pledge was correct should we doublecheck
    pledge_correct_str = apr_table_get( r->notes, PLEDGE_CORRECT_NOTE );
    if (pledge_correct_str == NULL)
    {
        // Error already sent
        DPRINTF( DEBUG1, ("psodium: client: Not doublechecking, pledge not correct" ));
        return APR_SUCCESS;
    }

    digest_url = pso_client_make_into_master_getDigest_request( r, master_id, slave_id );
    // Will contact master to get digests and check that against
    // digest in slave's pledge. Only if there is a match will it
    // return the stored reply.
    DPRINTF( DEBUG1, ("pso: client: ==> PHASE 4: Doublechecking content with master.\n" )); 
    access_status = renamed_ap_proxy_http_handler( r, conf, digest_url, proxyname, proxyport );
    DPRINTF( DEBUG1, ("psodium: client: GETDIGESTS from master returned %d\n", access_status ));
    pso_client_reset_master_getDigest_request( r );

    if (access_status != 0)
    {
        pso_error_send_connection_failed( r, access_status, "master" );
        return OK;
    }
    else
        return pso_client_badpledge_request_iff( conf, r, master_id, slave_id, url, proxyname, proxyport, badslave_out );
}



pso_response_action_t pso_client_getdigests_response( psodium_conf *conf, 
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
    apr_status_t status=APR_SUCCESS;
    char *error_str=NULL;
    apr_bool_t  match=FALSE;

    /*
     * We are processing a reply from the master containing the list of 
     * valid digests
     */
    DPRINTF( DEBUG1, ("psodium: client: Doing double check: Receiving digests from master\n" )); 

    status = pso_extract_digests( conf, r, rp, brigade_len, bb, &error_str, digest_array_out, digest_array_length_out );
    if (status == APR_SUCCESS)
    {
        char *pledge_data=NULL;
        apr_size_t pledge_curr_length=0;

        retrieve_pledge( r, &pledge_data, &pledge_curr_length );

        status = pso_client_check_pledge_against_digests( *digest_array_out, *digest_array_length_out, pledge_data, pledge_curr_length, &match, &error_str, r->pool );
        if (status == APR_SUCCESS)
        {
            if (match)
            {
                DPRINTF( DEBUG1, ("psodium: client: Doing double check: CORRECT!\n" )); 
                // TODO: restore headers on original slave reply
                pso_client_restore_slave_reply_headers( r );
                status = pso_reopen_temp_storage( conf, r, temp_file_out );
                if (status == APR_SUCCESS)
                {
                    // Return original reply
                    status = pso_temp_storage_to_filebb( r, *temp_file_out, newbb_out );
                    if (status == APR_SUCCESS)
                        return RESPONSE_ACTION_SEND_NEWBB;
                }
                else
                    error_str = "Replica's reply was OK, but cannot reopen temporary storage holding it!";
            }
            else
            {
                DPRINTF( DEBUG1, ("psodium: client: Doing double check: INCORRECT, according to the master the slave returned an invalid reply!\n" )); 
                error_str = "The digest in the slave's pledge didn't match any master digests!";

                // Set command so we'll send a putBadPledge command
                apr_table_set( r->notes, SENDER_COMMAND_NOTE, SEND_BADPLEDGE_COMMAND );

                return RESPONSE_ACTION_NOSEND;
            }
        }
    }
    else
    {
        error_str = apr_pstrcat( r->pool, "Error double checking slave's reply with master: ", error_str, NULL );
    }
    pso_error_in_html_to_bb( r, error_str, newbb_out );
    return RESPONSE_ACTION_SEND_NEWBB;
}



/* 
 * Read digests from getDigest from master.
 * 
 * This method is called for each bucket brigade! 
 *
 * BUG: TODO: this code assumes this bucket brigade contains all info!
 */
static apr_status_t pso_extract_digests( psodium_conf *conf, 
                                  request_rec *r, 
                                  request_rec *rp, 
                                  apr_off_t brigade_len,
                                  apr_bucket_brigade *bb, 
                                  char **error_str_out,
                                  digest_t **digest_array_out, 
                                  apr_size_t *digest_array_length_out )
{
    apr_status_t    status=APR_SUCCESS;
    char *reply=NULL;
    apr_size_t reply_idx=0;
    apr_bucket *e=NULL;

    if (brigade_len > 1*1024*1024L)
    {
        DPRINTF( DEBUG0, ("psodium: client: Doing double check: response from master too big: %ld bytes", brigade_len ));
        return APR_EINVAL;
    }
    else if (brigade_len == 0)
    {
        DPRINTF( DEBUG0, ("psodium: client: Doing double check: extracting digests: no data to extract from.\n" )); 
        return APR_SUCCESS;
    }

    DPRINTF( DEBUG5, ("psodium: client: Doing double check: extracting digests from reply of %d bytes\n", brigade_len )); 

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
            ap_log_error(APLOG_MARK, APLOG_DEBUG|APLOG_NOERRNO, 0,
                 r->server, "psodium: master digest reply totals %d", reply_idx );
            break;
        }

        if (APR_BUCKET_IS_FLUSH(e)) 
        {
            /* Arno: can we have flush buckets here? */
            continue;
        }

        DPRINTF( DEBUG5, ("psodium: client: Doing double check: extracting digests: before read\n" )); 

        /* read */
        apr_bucket_read(e, &block, &block_len, APR_BLOCK_READ);

        ap_assert( reply_idx+block_len <= brigade_len );

        DPRINTF( DEBUG5, ("psodium: client: Doing double check: extracting digests: before memcpy\n" )); 

        memcpy( reply+reply_idx, block, block_len );
        reply_idx += block_len;
    }

    DPRINTF( DEBUG3, ("psodium: client: Doing double check: extracting digests: before parse_digests: %*s\n", reply_idx, reply )); 

    status = parse_digests( r, reply, reply_idx, conf, error_str_out, digest_array_out, digest_array_length_out );
    if (status != APR_SUCCESS)
    {
        DPRINTF( DEBUG1, ("psodium: client: Doing double check: Error: %s\n", *error_str_out )); 
    }
    return status;
}



static apr_status_t parse_digests( request_rec *r, 
                            char *reply, 
                            apr_size_t reply_len, 
                            psodium_conf *conf, 
                            char **error_str_out,
                            digest_t **digest_array_out, 
                            apr_size_t *digest_array_length_out )
{
    apr_status_t status=APR_SUCCESS;
    apr_size_t soffset=0,eoffset=0,foffset=0;
    char *num_str=NULL;
    apr_size_t i=0;

    /*
     * Now sniff digests from the reply
     */
    soffset=0;
    soffset = pso_find_keyword( reply, soffset, reply_len, DIGESTS_START_KEYWORD );
    if (soffset == -1)
    {
        *error_str_out = "Cannot find digests in master's response, BEGIN missing";
        return APR_EINVAL;
    }

    DPRINTF( DEBUG5, ("psodium: client: Doing double check: Got start digests\n" ));
    soffset += strlen( DIGESTS_START_KEYWORD );
    soffset = pso_find_keyword( reply, soffset, reply_len, DIGESTS_COUNT_KEYWORD );
    if (soffset == -1)
    {
        *error_str_out = "Cannot find digests count in master's response, keyword missing";
        return APR_EINVAL;
    }
    soffset += strlen( DIGESTS_COUNT_KEYWORD );

    eoffset = pso_find_keyword( reply, soffset, reply_len, CRLF );
    if (eoffset == -1)
    {
        *error_str_out = "No CRLF after BEGIN DIGESTS in master's response";
        return APR_EINVAL;
    }

    num_str = pso_copy_tostring( reply, soffset, eoffset, r->pool );
    if (num_str == NULL)
    {
        *error_str_out = "Cannot read count after BEGIN DIGESTS in master's response";
        return APR_EINVAL;
    }
    else
    {
        int ret=0;

        ret = pso_atoN( num_str, digest_array_length_out, sizeof( *digest_array_length_out ) );
        if (ret == -1)
        {
            DPRINTF( DEBUG1, ("psodium: client: Doing double check: count not number ^%s$\n", num_str )); 
            *error_str_out = "Digest count not a number in master's response";
            return APR_EINVAL;
        }
    }

    DPRINTF( DEBUG1, ("psodium: client: Doing double check: got count %d digests\n", *digest_array_length_out )); 
   
    if (*digest_array_length_out == 0)
    {
        *error_str_out = "The master returned 0 digests for the contacted slave!";    
        return APR_EINVAL;
    }
  

    foffset = pso_find_keyword( reply, soffset, reply_len, DIGESTS_END_KEYWORD );
    if (foffset == -1)
    {
        *error_str_out = "BEGIN DIGESTS not followed by END in the master's response";
        return APR_EINVAL;
    }

    *digest_array_out = (digest_t *)apr_palloc( r->pool, *digest_array_length_out*sizeof( digest_t ) );
    soffset = eoffset + strlen( CRLF );
    for (i=0; i<*digest_array_length_out; i++)
    {
        apr_size_t decoded_len=0, decoded_len2;
        digest_t decoded = NULL;

        eoffset = pso_find_keyword( reply, soffset, reply_len, CRLF );
        if (eoffset == -1)
        {
            *error_str_out = "No CRLF after digest in master's response";
            return APR_EINVAL;
        }

        num_str = pso_copy_tostring( reply, soffset, eoffset, r->pool );
        DPRINTF( DEBUG5, ("psodium: client: Doing double check: got digest string %s\n", num_str )); 
        
        decoded_len = apr_base64_decode_len( num_str ); 
        decoded = (digest_t)apr_palloc( r->pool, decoded_len );
        decoded_len2 = apr_base64_decode( (char *)decoded, num_str );

        (*digest_array_out)[i] = decoded;

        soffset = eoffset+strlen( CRLF );
    }

    return APR_SUCCESS;                              
}   



static char * pso_client_make_into_master_getDigest_request( request_rec *r, 
                                                            char *master_id, 
                                                            char *slave_id )
{
    char *query = NULL, *encoded_query = NULL, *digest_url = NULL;
    apr_size_t encoded_len, encoded_len2=0;

    apr_table_set( r->notes, RECEIVER_COMMAND_NOTE, DOUBLECHECK_COMMAND );

    // Encode slave_id and original URI so we can safely pass them in a
    // getDigest? request.
    query = apr_pstrcat( r->pool, slave_id, "#", r->parsed_uri.path, NULL );
    encoded_len = apr_base64_encode_len( strlen( query ) );
    encoded_query = (char *)apr_palloc( r->pool, encoded_len );
    encoded_len2 = apr_base64_encode( encoded_query, query, strlen( query ) );
    ap_assert( encoded_len == encoded_len2 );

    digest_url = apr_pstrcat( r->pool, "http://", master_id, 
                              PSO_GETDIGEST_URI_PREFIX, "?", encoded_query, 
                              NULL );
    return digest_url;
}


static void pso_client_reset_master_getDigest_request( request_rec *r )
{
    apr_table_unset( r->notes, RECEIVER_COMMAND_NOTE );
}

