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
#include <math.h>

/*
 * Prototypes
 */
static pso_response_action_t handle_error_while_extracting( request_rec *r, 
                                                     apr_status_t status,
                                                     char *error_str,
                                                     apr_bucket_brigade **newbb_out );
static apr_status_t pso_extract_pledge_from_reply( psodium_conf *conf, 
                                  request_rec *r, 
                                  request_rec *rp, 
                                  const char *content_length_str,
                                  apr_size_t content_idx, 
                                  apr_size_t content_length,
                                  apr_size_t pledge_total_length,
                                  apr_bucket_brigade *bb,
                                  apr_off_t brigade_len, 
                                  apr_bucket_brigade **newbb_out,
                                  EVP_MD_CTX *md_ctxp,
                                  char **pledge_data_out,
                                  apr_size_t *pledge_curr_length_out );
static apr_status_t extract_pledge_bytes( request_rec *r, 
                                   apr_size_t pledge_total_length,
                                   apr_bucket_brigade *bb, 
                                   apr_size_t start, 
                                   apr_size_t brigade_len, 
                                   apr_bucket_brigade **newbb_out,
                                   EVP_MD_CTX *md_ctxp,
                                  char **pledge_data_out,
                                  apr_size_t *pledge_curr_length_out );
static void pso_client_make_into_slave_GET_request( request_rec *r, 
                                             apr_bool_t doublecheck, 
                                             const char *slave_id );
static void pso_client_reset_slave_GET_request( request_rec *r );
static void pso_client_reset_slave_GET_request_headers( request_rec *r );



static int table_func_save_headers( void *rec, const char *key, const char *val );
static apr_status_t save_headers_till_after_doublecheck( request_rec *r );
static int table_func_restore_headers( void *rec, const char *key, const char *val );


/*=================================== Implementation =======================*/

int pso_client_content_request( psodium_conf *conf, 
                                request_rec *r, 
                                const char *server_id,
                                char *url, 
                                const char *proxyname, 
                                apr_port_t proxyport)
{
    apr_status_t status=APR_SUCCESS;
    int access_status = OK;
    char *master_id=NULL;
    
    // Contacting slave.
    apr_bool_t doublecheck=TRUE; 
    const char *putpledge_url=NULL, *sender_command=NULL;
    long random=0;
    apr_bool_t badslave;

    DPRINTF( DEBUG1, ("pso: client: ==> PHASE 3: Retrieving pledged content from slave.\n" )); 
    
    // Make doublecheck probabilistic
    status = pso_create_random_number( &random );
    if (status == APR_SUCCESS)
    {
        double maxlong=pow( 256.0, sizeof( long ) );
        DPRINTF( DEBUG1, ("pso: client: Double check debug maxlong=%f\n", maxlong ));
        float f = fabs( (double)random / maxlong );
        DPRINTF( DEBUG1, ("pso: client: Double check if %f > %f\n", conf->doublecheckprob, f ));
        if (conf->doublecheckprob > f)
            doublecheck=TRUE;
        else
            doublecheck=FALSE;
    }

    status = pso_find_master_id( conf, server_id, &master_id, r->pool );
    if (status != APR_SUCCESS || master_id == NULL)
    {
        return HTTP_INTERNAL_SERVER_ERROR;
    }

    pso_client_make_into_slave_GET_request( r, doublecheck, server_id );
    // Will get reply+pledge from slave. If doublecheck it will check that
    // the reply and the digest in the pledge match and if so, store both.
    // If no doublecheck it will check, and forward the reply and only 
    // store the pledge.
    DPRINTF( DEBUG1, ("psodium: client: Retrieving slave document %s\n", url ));
    access_status = renamed_ap_proxy_http_handler( r, conf, url, proxyname, proxyport );
    DPRINTF( DEBUG1, ("psodium: client: GET from slave returned %d\n", access_status ));
    pso_client_reset_slave_GET_request( r );
    if (access_status != 0)
    {
        pso_error_send_connection_failed( r, access_status, "slave" );
        return OK;
    }
    
    // See if the pledge is good, if not, send bad pledge to master
    access_status = pso_client_badpledge_request_iff( conf, r, master_id, (char *)server_id, url, proxyname, proxyport, &badslave );
    if (badslave)
    {
        pso_client_mark_bad_slave( conf, r->pool, server_id );
        return OK;
    }

    if (doublecheck==TRUE)
    {
        access_status = pso_client_getdigests_request( conf, r, master_id, (char *)server_id, url, proxyname, proxyport, &badslave );
        if (badslave)
        {
            pso_client_mark_bad_slave( conf, r->pool, server_id );
        }
        return access_status;
    }

    // Send pledge to auditor (only proper ones that where not double checked)
    access_status = pso_client_auditor_request( conf, r, server_id, url, proxyname, proxyport );
    
    return OK; // Don't care what putDigest got
}


pso_response_action_t pso_client_content_response( apr_bool_t doublecheck,
                                  psodium_conf *conf, 
                                  request_rec *r, 
                                  request_rec *rp, 
                                  const char *content_length_str,
                                  apr_size_t content_idx, 
                                  apr_size_t content_length,
                                  apr_size_t pledge_total_length,
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
    char *pledge_data=NULL;
    apr_size_t pledge_curr_length=0;

    /*
     * We are processing a reply from the slave that has a pledge appended.
     * If we don't double check with the master, we temporarily store the 
     * reply until we can verify that the pledge is OK, and then forward it.
     * If we DO double check, we store the reply and only return it after we
     * got the list of valid digests from the master and matched one with
     * the pledge.
     */
    DPRINTF( DEBUG1, ("psodium: client: Handling slave's pledged reply.\n" )); 

    pso_client_reset_slave_GET_request_headers( r );  // to allow 1 server to be master/slave/client

    retrieve_pledge( r, &pledge_data, &pledge_curr_length );

    status = pso_extract_pledge_from_reply( conf, r, rp, 
                                            content_length_str, content_idx, 
                                            content_length, pledge_total_length,
                                            bb, brigade_len, 
                                            newbb_out, 
                                            md_ctxp, &pledge_data, 
                                            &pledge_curr_length );
    if (status != APR_SUCCESS)
    {
        return handle_error_while_extracting( r, status, "Cannot extract pledge from replica's reply!", newbb_out );
    }

    store_pledge( r, pledge_data, pledge_curr_length );

    // Save bb's until we read the end and can verify digests
    status = pso_bb_to_temp_storage( conf, r, temp_file_out, *newbb_out, !doublecheck );
    if (status != APR_SUCCESS)
    {
        return handle_error_while_extracting( r, status, "Cannot temporarily store replica's reply on disk!", newbb_out );
    }

    // The end!
    if (APR_BUCKET_IS_EOS( APR_BRIGADE_LAST( *newbb_out )))
    {
        digest_t md = NULL; // could alloc the mem on stack, but would need to define digest elem type
        digest_len_t md_len;
        const char *slave_id=NULL;
        md = (digest_t)apr_palloc( r->pool, EVP_MAX_MD_SIZE );
        int err=0;

        slave_id = apr_table_get( r->notes, TEMP_SLAVEID_NOTE );

        // OPENSSL
        EVP_DigestFinal( md_ctxp, md, &md_len );

        if (pledge_data != NULL)
        {
            status = pso_client_check_pledge( conf, r, md, md_len, (char *)slave_id, 
                                              pledge_data, pledge_curr_length, 
                                              &match, &error_str, r->pool );
            if (status == APR_SUCCESS)
            {
                if (match)
                {
                    DPRINTF( DEBUG1, ("psodium: client: pledge CORRECT.\n" )); 
                    if (doublecheck)
                    {
                        // OK, the browser's not getting the reply now
                        save_headers_till_after_doublecheck( r );

                        apr_file_close( *temp_file_out );
                        *temp_file_out = NULL;

                        // Only now allow getDigest to master
                        apr_table_set( r->notes, PLEDGE_CORRECT_NOTE, TRUE_STR );

                        return RESPONSE_ACTION_NOSEND;
                    }
                }
                else
                {
                    DPRINTF( DEBUG1, ("psodium: client: pledge does not match, INCORRECT.\n" )); 
                    error_str = apr_psprintf( r->pool, "The result returned by slave didn't match slave's pledge (%s). Hence, pSodium does not return it.", error_str );

                    // Set command so we'll send a putBadPledge command
                    apr_table_set( r->notes, SENDER_COMMAND_NOTE, SEND_BADPLEDGE_COMMAND );

                    return RESPONSE_ACTION_NOSEND;
                }
            }
            else
                DPRINTF( DEBUG1, ("psodium: client: pledge unable to check correctness of pledge: %s\n", error_str )); 
        }
        else
        {
            error_str = "Pledge not received or parts not properly recorded";
            DPRINTF( DEBUG1, ("psodium: client: pledge unable to check correctness of pledge: %s\n", error_str )); 
        }

        if (match)
        {
            /* We're not double checking, return content immediately */
            status = pso_temp_storage_to_filebb( r, *temp_file_out, newbb_out );
            if (status == APR_SUCCESS)
                return RESPONSE_ACTION_SEND_NEWBB;
            else
            {
               char apr_err_msg_storage[ APR_MAX_ERROR_STRLEN ];
               DPRINTF( DEBUG1, ("psodium: client: extract pledge: **** ERROR reading from temp storage: %s\n", apr_strerror( status, apr_err_msg_storage, APR_MAX_ERROR_STRLEN ) )); 
            }
        }

        return handle_error_while_extracting( r, status, error_str, newbb_out );
    }
    else
    {
        // Not last bucket, get some more to save on disk
        /* make sure we always clean up after ourselves */
        DPRINTF( DEBUG4, ("More data still to receive, continuing..." ));
        apr_brigade_cleanup( *newbb_out );

        return RESPONSE_ACTION_GETNEXTBB;
    }
}



pso_response_action_t handle_error_while_extracting( request_rec *r, 
                                                     apr_status_t status,
                                                     char *error_str,
                                                     apr_bucket_brigade **newbb_out )
{
   char *apr_err_msg_storage, *apr_err_msg=NULL, *full=NULL;
   apr_err_msg_storage = (char *)apr_palloc( r->pool, APR_MAX_ERROR_STRLEN );
   apr_err_msg = apr_strerror( status, apr_err_msg_storage, APR_MAX_ERROR_STRLEN );
   full = "Error extracting pledge from replica's reply: ";
   if (error_str != NULL)
       full = apr_pstrcat( r->pool, full, error_str, NULL );
   if (status != APR_SUCCESS)       
       full = apr_pstrcat( r->pool, full, ": ", apr_err_msg, NULL );
   
   DPRINTF( DEBUG1, ("psodium: client: Error extracting pledge: %s, returning bad content image\n", full )); 
   const char *slave_id = apr_table_get( r->notes, TEMP_SLAVEID_NOTE );   

   // Cleanup of temp storage is handled in temp_storage.c
   
   //pso_error_in_html_to_bb( r, full, newbb_out );
   (void)pso_create_badcontent_reply_bb( r, newbb_out, (char *)slave_id, NULL, 0, full );

   
   return RESPONSE_ACTION_SEND_NEWBB;
}



/* 
 * This method is called for each bucket brigade! 
 *
 */
apr_status_t pso_extract_pledge_from_reply( psodium_conf *conf, 
                                  request_rec *r, 
                                  request_rec *rp, 
                                  const char *content_length_str,
                                  apr_size_t content_idx, 
                                  apr_size_t content_length,
                                  apr_size_t pledge_total_length,
                                  apr_bucket_brigade *bb,
                                  apr_off_t brigade_len, 
                                  apr_bucket_brigade **newbb_out,
                                  EVP_MD_CTX *md_ctxp,
                                  char **pledge_data_out,
                                  apr_size_t *pledge_curr_length_out )
{
    const char *updated_cl=NULL;
    char *new_content_len_str=NULL;
    
    //DPRINTF( DEBUG1, ("psodium: client: extract_pledge: conf %p, rec %p, rp %p, cls %p, cidx %d, class %d, pl %d, bl %d, bb %p new %p ctx %p.\n", 
    //    r, rp, conf, content_length_str, content_idx, content_length, pledge_total_length, brigade_len, bb, *newbb_out, md_ctxp )); 
    
    if (content_length_str == NULL)
    {
        DPRINTF( DEBUG1, ("psodium: client: extract_pledge: pSodium doesn't work on content with unknown length.\n" )); 
        return APR_SUCCESS;
    }

    /*
     * We are receiving a response with a pledge attached at the end.
     * We'll edit the bucket brigade such that the pledge is removed.
     */
    updated_cl = apr_table_get( r->notes, TEMP_UPDATED_CONTENT_LENGTH_NOTE );
    if (updated_cl == NULL)
    {
        /* First brigade in request: */
        apr_size_t newlen = content_length - pledge_total_length;
        int ret=pso_Ntoa( &newlen, sizeof( newlen ), &new_content_len_str, r->pool );
        apr_table_set( r->headers_out, "Content-Length", new_content_len_str );
        
        apr_table_unset( r->headers_out, PLEDGE_LENGTH_HEADER );
        
        apr_table_set( r->notes, TEMP_UPDATED_CONTENT_LENGTH_NOTE, TRUE_STR );
    }

    
    if ((content_idx+brigade_len) > (content_length - pledge_total_length))
    {
        // We have some pledge bytes in this brigade
        apr_int32_t start = brigade_len - ((content_idx+brigade_len)-(content_length-pledge_total_length));
        if (start < 0) // whole brigade is pledge
            start = 0;

        DPRINTF( DEBUG5, ("psodium: client: c_idx=%ld bl=%ld, cl=%ld ps=%ld, start=%ld\n", content_idx, brigade_len, content_length, pledge_total_length, start )); 
        return extract_pledge_bytes( r, pledge_total_length, bb, start, brigade_len, newbb_out, md_ctxp, pledge_data_out, pledge_curr_length_out );
    }
    else
    {
        // No pledge here, send unmodified brigade, after calculating digest
        apr_bucket *e=NULL;
        for (e = APR_BRIGADE_FIRST(bb); e != APR_BRIGADE_SENTINEL(bb); e = APR_BUCKET_NEXT(e)) 
        {
            const char *block;
            apr_size_t block_len=0;

            if (APR_BUCKET_IS_FLUSH(e)) 
            {
                continue;
            }

            /* read */
            apr_bucket_read(e, &block, &block_len, APR_BLOCK_READ);

            // OPENSSL
            if (block_len != 0)
                EVP_DigestUpdate( md_ctxp, block, block_len );
        }
        
        *newbb_out = bb;
        return APR_SUCCESS;
    }
}




apr_status_t extract_pledge_bytes( request_rec *r, 
                                   apr_size_t pledge_total_length,
                                   apr_bucket_brigade *bb, 
                                   apr_size_t start, 
                                   apr_size_t brigade_len, 
                                   apr_bucket_brigade **newbb_out,
                                   EVP_MD_CTX *md_ctxp,
                                  char **pledge_data_out,
                                  apr_size_t *pledge_curr_length_out )
{
    apr_bucket *e, *copyb;
    apr_size_t brigade_idx=0;
    const char *pledge_data=NULL;
    char *temp_digest_str=NULL;
    apr_size_t i=0;
    apr_status_t status=APR_SUCCESS;

    // We remove the pledge from the old bucket brigade(s), this is the
    // new one that will be sent to the browser.
    //
    apr_pool_t *p = r->connection->pool;
    conn_rec *c = r->connection;
    *newbb_out = apr_brigade_create(p, c->bucket_alloc);

    if (*newbb_out == NULL)
    {
        DPRINTF( DEBUG1, ("psodium: client: extracting pledge bytes: cannot alloc new brigade\n" )); 
    }

    // As the pledge comes from the slave we cannot trust the input. 
    ap_assert( *pledge_curr_length_out <= pledge_total_length );
    //DPRINTF( DEBUG3, ("*pledge_curr_length %ld <= pledge_total_length %ld\n", *pledge_curr_length_out, pledge_total_length ));

    for (e = APR_BRIGADE_FIRST(bb); e != APR_BRIGADE_SENTINEL(bb); e = APR_BUCKET_NEXT(e)) 
    {
        const char *block;
        apr_size_t block_len=0;
        int err=0;

        if (APR_BUCKET_IS_FLUSH(e)) /* Arno: can we have flush buckets here? */
        {
            // Add unmodified buckets to new brigade 
            // (via zero-copy copy(), as a bucket brigade is a peculiar 
            // data struct)
            status = apr_bucket_copy( e, &copyb );
            if (status != APR_SUCCESS)
                return status;
            
            APR_BRIGADE_INSERT_TAIL( (*newbb_out), copyb );
            continue;
        }

        /* read */
        apr_bucket_read(e, &block, &block_len, APR_BLOCK_READ);

        if ((brigade_idx+block_len) >= start && block_len > 0)
        {
            char *part = NULL;

            apr_size_t block_off = block_len - ((brigade_idx+block_len)-start);
            if (block_off < 0) // whole block is pledge
                block_off = 0;

            DPRINTF( DEBUG5, ("psodium: client: b_idx=%ld zl=%ld, bl=%ld ps=%ld, block_off=%ld\n", brigade_idx, block_len, brigade_len, pledge_total_length, block_off )); 

            // Digest excludes pledge bytes! 
            EVP_DigestUpdate( md_ctxp, block, block_off );

            // Extract bytes
            char *new_pledge_data = (char *)apr_palloc( r->pool, (*pledge_curr_length_out)+(block_len-block_off) );
            if (*pledge_curr_length_out > 0)
                memcpy( new_pledge_data, *pledge_data_out, *pledge_curr_length_out );
            memcpy( new_pledge_data+*pledge_curr_length_out, block+block_off, (block_len-block_off) );
            *pledge_data_out = new_pledge_data;
            *pledge_curr_length_out = *pledge_curr_length_out + (block_len-block_off);

            // DEBUG
            part = pso_copy_tostring( (char *)block, block_off, block_len, r->pool );
            DPRINTF( DEBUG5, ("psodium: client: extract pledge: part $%s$\n", part )); 

            if (block_off > 0)
            {
                // Part of the current block is not pledge
                apr_bucket *tail = apr_bucket_pool_create( block, block_off, p, c->bucket_alloc);

                // If old bucket was EOS, so is new
                if (APR_BUCKET_IS_EOS( e ))
                {
                    apr_bucket_eos_make( tail );
                }
                APR_BRIGADE_INSERT_TAIL( (*newbb_out), tail );
            }
        }
        else
        {
            // Add unmodified buckets to new brigade
            // (via zero-copy copy(), as a bucket brigade is a peculiar 
            // data struct)
            DPRINTF( DEBUG5, ("psodium: client: extract pledge: adding non-pledge block to new_bb\n" )); 

            // OPENSSL
            if (block_len > 0)
                EVP_DigestUpdate( md_ctxp, block, block_len );

            status = apr_bucket_copy( e, &copyb );
            if (status != APR_SUCCESS)
                return status;

            APR_BRIGADE_INSERT_TAIL( (*newbb_out), copyb );
        }

        if (APR_BUCKET_IS_EOS(e)) 
        {
            break;
        }

        brigade_idx += block_len;
    }

    DPRINTF( DEBUG4, ("psodium: client: Extract pledge: Done.\n" )); 

    return APR_SUCCESS;
}



void pso_client_make_into_slave_GET_request( request_rec *r, apr_bool_t doublecheck, const char *slave_id )
{
    apr_table_set( r->headers_in, WANT_PLEDGE_HEADER, TRUE_STR );
    if (doublecheck)
        apr_table_set( r->notes, RECEIVER_COMMAND_NOTE, KEEP_REPLY4DOUBLECHK_COMMAND );
    else
        apr_table_set( r->notes, RECEIVER_COMMAND_NOTE, KEEP_PLEDGE4AUDIT_COMMAND );
    apr_table_set( r->notes, TEMP_SLAVEID_NOTE, slave_id );
}


void pso_client_reset_slave_GET_request( request_rec *r )
{
    apr_table_unset( r->notes, TEMP_SLAVEID_NOTE );
    apr_table_unset( r->notes, RECEIVER_COMMAND_NOTE );
    pso_client_reset_slave_GET_request_headers( r );
}

void pso_client_reset_slave_GET_request_headers( request_rec *r )
{
    apr_table_unset( r->headers_in, WANT_PLEDGE_HEADER );
}


/*
 * Save headers of slave reply in r->notes
 */

static int table_func_save_headers( void *rec, const char *key, const char *val )
{
    request_rec *r = (request_rec *)rec;
    const char *newkey = NULL;

    DPRINTF( DEBUG5, ("psodium: client: Saving slave's reply header %s %s\n", key, val )); 

    newkey = apr_pstrcat( r->pool, SAVED_REPLY_HEADERS_NOTE_PREFIX, key, NULL );
    if (newkey == NULL)
        return 0;
    apr_table_set( r->notes, newkey, val );
    return 1;
}


static apr_status_t save_headers_till_after_doublecheck( request_rec *r )
{
    if (apr_table_do( table_func_save_headers, (void *)r, r->headers_out, NULL ))
        return APR_SUCCESS;
    else
        return APR_ENOMEM;
}



static int table_func_restore_headers( void *rec, const char *key, const char *val )
{
    request_rec *r = (request_rec *)rec;
    const char *oldkey = NULL;
    if (strlen( key ) > strlen( SAVED_REPLY_HEADERS_NOTE_PREFIX ) &&
        !strncmp( key, SAVED_REPLY_HEADERS_NOTE_PREFIX, strlen( SAVED_REPLY_HEADERS_NOTE_PREFIX )))
    {
        DPRINTF( DEBUG5, ("psodium: client: Restoring header from original slave reply %s %s\n", key, val )); 

        oldkey = key+strlen( SAVED_REPLY_HEADERS_NOTE_PREFIX );

        // Exception
        if (!strcmp( oldkey, "Content-Type" ))
        {
            apr_table_unset( r->headers_out, oldkey );
            ap_set_content_type(r, val );
        }
        else
            apr_table_set( r->headers_out, oldkey, val );
    }
    return 1;
}


apr_status_t pso_client_restore_slave_reply_headers( request_rec *r )
{
    if (apr_table_do( table_func_restore_headers, (void *)r, r->notes, NULL ))
        return APR_SUCCESS;
    else
        return APR_ENOMEM;
}
