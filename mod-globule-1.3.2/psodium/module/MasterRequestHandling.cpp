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
#include <apr_base64.h>
#include "utillib/LyingClientsVector.hpp"
#include "utillib/BadSlavesVector.hpp"
#include <string.h>

extern "C" 
{
    
static apr_status_t pso_master_send_redirect( request_rec *r );
static apr_status_t pso_master_send_slaveinfo( psodium_conf *conf, 
                                           request_rec *r,
                                           char *slave_id );
static apr_status_t pso_master_send_digests( request_rec *r, 
                                      digest_t *digest_array, 
                                      apr_size_t digest_array_length );
static apr_status_t pso_master_handle_auditor_feedback( request_rec *r, 
                                                        IPAddrPortVector *vec );

static apr_status_t pso_master_handle_clientfeedback( psodium_conf *conf,
                                                      request_rec *r, 
                                                      char *slave_id, 
                                                      char *pledge_data,
                                                      apr_size_t pledge_curr_length,
                                                      char **error_str_out,
                                                      apr_bool_t *clientIsLying_out,
                                                      apr_bool_t *slaveIsBad_out );
static apr_status_t pso_master_reply2clientfeedback( request_rec *r, 
                                                     apr_status_t old_status, 
                                                     char *error_str,
                                                     apr_bool_t clientIsLying,
                                                     apr_bool_t slaveIsBad,
                                                     char *slave_id, 
                                                     char *pledge_data, 
                                                     apr_size_t pledge_curr_length );
static apr_status_t check_authorization( psodium_conf *conf, 
                                         request_rec *r, 
                                         bool checkIfAuditor,
                                         apr_bool_t *auth_out );
}

static apr_status_t pso_marshall_send_VERSIONS( Versions *v, 
                                      const char *globule_export_path,
                                      request_rec *r );

static apr_status_t pso_marshall_sendwrite_VERSIONS( Versions *v, 
                                      apr_bool_t toNetwork,
                                      const char *globule_export_path,
                                      request_rec *r,
                                      const char *filename,
                                      apr_pool_t *pool );

static apr_status_t write_marshalled( apr_bool_t toNetwork, apr_bucket_brigade *bb, 
                               request_rec *r, apr_file_t *file );


extern "C"
{
                                      
int pso_master_process_request( psodium_conf *conf, request_rec *r )
{
    apr_status_t status=APR_SUCCESS;
    const char *want_slave_val = NULL;

    DPRINTF( DEBUG1, ("psodium: master: Request %s is for master?\n", r->unparsed_uri )); 
    
    want_slave_val = apr_table_get( r->headers_in, WANT_SLAVE_HEADER );
    if (want_slave_val == NULL)
        want_slave_val = FALSE_STR;


    apr_bool_t liar=false;
    status = pso_master_client_is_known_liar( conf, r, &liar );
    if (liar)
    {
        r->status = HTTP_FORBIDDEN;
        apr_bucket_brigade *bb=NULL;
        ap_set_content_type( r, "text/plain" );
        status = pso_error_in_html_to_bb( r, "pSodium has established you have been lying about slaves' performance. So we're not serving you, goodbye.", &bb );
        if (status == APR_SUCCESS)
            status = ap_pass_brigade( r->output_filters, bb);
        return OK;
    }
    else if (!strncasecmp( r->unparsed_uri, PSO_GETSLAVEINFO_URI_PREFIX, 
                     strlen( PSO_GETSLAVEINFO_URI_PREFIX )))
    {
        apr_size_t decoded_len=0, decoded_len2=0;
        char *decoded_query=NULL, *master_uri_str=NULL, *ptr=NULL;
        char *slave_id = NULL;
        digest_t *digest_array=NULL;
        apr_bucket_brigade *bb=NULL;
        apr_size_t  digest_array_len=0;

        DPRINTF( DEBUG1, ("psodium: master: Got GETSLAVEINFO request!\n" )); 
        /*
         * Protect against malicious input from client!
         */
        if (r->parsed_uri.query == NULL || !strcmp( r->parsed_uri.query, "" ))
        {
            DOUT( DEBUG0, "Client sent bad GETSLAVEINFO request" );
            // TODO: Add to LyingClients
            
            r->status = HTTP_BAD_REQUEST;
            return OK;
        }
        
        decoded_len = apr_base64_decode_len( r->parsed_uri.query ); 
        decoded_query = (char *)apr_palloc( r->pool, decoded_len );
        decoded_len2 = apr_base64_decode( decoded_query, r->parsed_uri.query );
        
        DPRINTF( DEBUG0, ("Decoded len is %d, len2 is %d\n", decoded_len, decoded_len2 ));
        slave_id = decoded_query;

        DPRINTF( DEBUG1, ("psodium: master: Info wanted on slave %s\n", slave_id ));         
        status = pso_master_send_slaveinfo( conf, r, slave_id );
    }
    else if(!strncasecmp( r->unparsed_uri, PSO_GETDIGEST_URI_PREFIX,
                     strlen( PSO_GETDIGEST_URI_PREFIX )))
    {
        apr_size_t decoded_len=0, decoded_len2=0;
        char *decoded_query=NULL, *slave_uri_path=NULL, *ptr=NULL;
        char *slave_id = NULL;
        digest_t *digest_array=NULL;
        apr_bucket_brigade *bb=NULL;
        apr_size_t  digest_array_len=0;

        DPRINTF( DEBUG1, ("psodium: master: Got GETDIGEST request!\n" )); 
        /*
         * Protect against malicious input from client!
         */
        if (r->parsed_uri.query == NULL || !strcmp( r->parsed_uri.query, "" ))
        {
            DOUT( DEBUG0, "Client sent bad GETDIGESTS request" );
            // TODO: Add to LyingClients
            
            r->status = HTTP_BAD_REQUEST;
            return OK;
        }
        
        decoded_len = apr_base64_decode_len( r->parsed_uri.query ); 
        decoded_query = (char *)apr_palloc( r->pool, decoded_len );
        decoded_len2 = apr_base64_decode( decoded_query, r->parsed_uri.query );

        /** Split query string in slave_id and master URL (DIRTY) */
        ptr = strchr( decoded_query, '#' );
        if (ptr == NULL || ptr == decoded_query+strlen( decoded_query ))
        {
            DOUT( DEBUG0, "Client sent bad GETDIGESTS request" );
            // TODO: Add to LyingClients
            
            r->status = HTTP_BAD_REQUEST;
            return OK;
        }
        slave_uri_path = ptr+1;
        *ptr = '\0';
        slave_id = decoded_query;

        status = pso_master_find_RETURN_digests( conf, slave_id, slave_uri_path, &digest_array, &digest_array_len, r->pool );
        if (status != APR_SUCCESS)
        {
            char *error_str = apr_pstrcat( r->pool, 
                                           "Error looking up digests for ", 
                                            slave_uri_path, " at slave ", 
                                            slave_id, CRLF, NULL );
            
            pso_error_in_html_to_bb( r, error_str, &bb );
            status = ap_pass_brigade(r->output_filters, bb );
        }
        else
        {
            status = pso_master_send_digests( r, digest_array, digest_array_len );
        }
    }
    else if(!strncasecmp( r->unparsed_uri, PSO_BADPLEDGE_URI_PREFIX,
                     strlen( PSO_BADPLEDGE_URI_PREFIX )))
    {
        apr_size_t decoded_len=0, decoded_len2=0;
        char *decoded_query=NULL, *ptr=NULL, *slave_id = NULL;
        const char *pledge_base64=NULL;
        digest_t *digest_array=NULL;
        apr_bucket_brigade *bb=NULL;
        apr_size_t  digest_array_len=0;
        
        DPRINTF( DEBUG1, ("psodium: master: Got BADPLEDGE request!\n" )); 
        /*
         * Protect against malicious input from client!
         */
        if (r->parsed_uri.query == NULL || !strcmp( r->parsed_uri.query, "" ))
        {
            DOUT( DEBUG0, "Client sent bad BADPLEDGE request" );
            // TODO: Add to LyingClients
            
            r->status = HTTP_BAD_REQUEST;
            return OK;
        }
        
        decoded_len = apr_base64_decode_len( r->parsed_uri.query ); 
        decoded_query = (char *)apr_palloc( r->pool, decoded_len );
        decoded_len2 = apr_base64_decode( decoded_query, r->parsed_uri.query );

        /** Split query string in slave_id and pledge (DIRTY) */
        ptr = strchr( decoded_query, '#' );
        if (ptr == NULL || ptr == decoded_query+strlen( decoded_query ))
        {
            DOUT( DEBUG0, "Client sent bad BADPLEDGE request" );
            // TODO: Add to LyingClients
            
            r->status = HTTP_BAD_REQUEST;
            return OK;
        }
        pledge_base64 = ptr+1;
        *ptr = '\0';
        slave_id = decoded_query;
        apr_size_t pledge_curr_length = decoded_len2-strlen( decoded_query );
        char *pledge_data = NULL;
        decode_pledge( r->pool, pledge_base64, &pledge_data, &pledge_curr_length );
        char *error_str=NULL;
        apr_bool_t clientIsLying=FALSE,slaveIsBad=FALSE;
        status = pso_master_handle_clientfeedback( conf, r, slave_id, pledge_data, pledge_curr_length, &error_str, &clientIsLying, &slaveIsBad );
        status = pso_master_reply2clientfeedback( r, status, error_str, clientIsLying, slaveIsBad, slave_id, pledge_data, pledge_curr_length );
    }
    else if(!strncasecmp( r->unparsed_uri, PSO_UPDATEEXPIRY_URI_PREFIX,
                     strlen( PSO_UPDATEEXPIRY_URI_PREFIX )))
    {
        char *ptr=NULL,*slave_uri_str=NULL,*time_str=NULL;
        apr_bool_t auth=FALSE;    

        DPRINTF( DEBUG1, ("psodium: master: Got UPDATEEXPIRY request!\n" )); 

        status = check_authorization( conf, r, false, &auth );
        if (status == APR_SUCCESS)
        {   
            if (!auth)
            {
                r->status = HTTP_UNAUTHORIZED;
                return OK;
            }
            else
                DOUT( DEBUG3, "UPDATEEXPIRY request authenticated." );
        }
        /*
         * Protect against malformed input from client!
         */
        if (r->parsed_uri.query == NULL || !strcmp( r->parsed_uri.query, "" ))
        {
            DOUT( DEBUG0, "Client (=Globule master) sent bad UPDATEEXPIRY request" );
            r->status = HTTP_BAD_REQUEST;
            return OK;
        }
        
        char *query = apr_pstrdup( r->pool, r->parsed_uri.query );

        /** Split query string in time and filename */
        ptr = strchr( query, 'h' );
        if (ptr == NULL)
        {
            DOUT( DEBUG0, "Client (=Globule master) sent bad UPDATEEXPIRY request" );
            r->status = HTTP_BAD_REQUEST;
            return OK;
        }
        slave_uri_str = apr_pstrdup( r->pool, ptr );
        *ptr = '\0';
        time_str = query;

#ifndef notQUICK      
        apr_uri_t slave_uri;
        apr_status_t status=apr_uri_parse( r->pool, slave_uri_str, &slave_uri );
        if (status != APR_SUCCESS)
        {
            DOUT( DEBUG0, "Client (=Globule master) sent bad UPDATEEXPIRY request" );
            r->status = HTTP_BAD_REQUEST;
            return OK;
        }
#else
        // First figure out what master URL was:
        // Can't do this in function, otherwise we get problems with
        // linking DigestsUtil.cc with auditor.
        const char *doc_root = ap_document_root( r );
        if (strlen( doc_root ) > strlen( filename ))
        {
            DOUT( DEBUG0, "Client (=Globule master) sent bad UPDATEEXPIRY request" );
            r->status = HTTP_BAD_REQUEST;
            return OK;
        }
        apr_size_t url_idx = strlen( doc_root );
        if (doc_root[ url_idx-1 ] == '/')
            url_idx--; // make sure URL starts with /
        char *uri_str = filename+url_idx;
#endif
      
        char *error_str=NULL;
        status = pso_master_update_expiry( conf, r, slave_uri_str, time_str, &error_str );
        if (status != APR_SUCCESS)
        {
            apr_bucket_brigade *bb=NULL;
            pso_error_in_html_to_bb( r, error_str, &bb );
            status = ap_pass_brigade(r->output_filters, bb );
        }
    }
    else if(!strncasecmp( r->unparsed_uri, PSO_GETMODIFICATIONS_URI_PREFIX,
                     strlen( PSO_GETMODIFICATIONS_URI_PREFIX )))
    {
        // Auditor asks us to clean up VERSIONS and send it a marshalled
        // copy. This request should be authenticated.
        //
        apr_bool_t auth=FALSE;
        
        DPRINTF( DEBUG1, ("psodium: master: Got GETMODIFICATIONS request from client (auditor?)!\n" )); 
        
        status = check_authorization( conf, r, true, &auth );
        if (status == APR_SUCCESS)
        {   
            if (auth)
            {
                if (r->parsed_uri.query == NULL || !strcmp( r->parsed_uri.query, "" ))
                {
                    DOUT( DEBUG0, "Auditor sent bad GETMODIFICATIONS request" );
                    r->status = HTTP_BAD_REQUEST;
                    return OK;
                }
                
                apr_time_t audit_start_time;
                DPRINTF( DEBUG3, ("Start time of audit is %s\n", r->parsed_uri.query ));
                int ret = pso_atoN( r->parsed_uri.query, &audit_start_time, sizeof( audit_start_time ) );
                // Not trippin here, just testing if timestamp parsing went ok.
                char *time_str=NULL;
                ret = pso_Ntoa( &audit_start_time, sizeof( audit_start_time ), &time_str, r->pool );
                DPRINTF( DEBUG1, ("psodium: master: Audit start time is %s\n", time_str ));

                //QUICK const char *globule_export_path = apr_table_get( r->notes, GLOBULE_EXPORT_PATH_NOTE );
                const char *globule_export_path = "";

                status = pso_master_send_MODIFICATIONS( conf, r, audit_start_time, globule_export_path );
            }
            else
                r->status = HTTP_UNAUTHORIZED;
        }
    }
    else if(!strncasecmp( r->unparsed_uri, PSO_PUTLYINGCLIENTS_URI_PREFIX,
                 strlen( PSO_PUTLYINGCLIENTS_URI_PREFIX )))
    {
        // Receiving LYINGCLIENTS addition from auditor
        // TODO authenticate
        apr_bool_t auth=FALSE;
        status = check_authorization( conf, r, true, &auth );
        if (status == APR_SUCCESS)
        {   
            if (auth)
            {
                // Go to C++ domain
                LyingClientsVector *lc = (LyingClientsVector *)conf->lyingclients;

                status = pso_master_handle_auditor_feedback( r, lc );
            }
            else
                r->status = HTTP_UNAUTHORIZED;
        }
    }
    else if(!strncasecmp( r->unparsed_uri, PSO_PUTBADSLAVES_URI_PREFIX,
                 strlen( PSO_PUTBADSLAVES_URI_PREFIX )))
    {
        // Receiving BADSLAVES addition from auditor
        // TODO authenticate
        apr_bool_t auth=FALSE;
        
        DPRINTF( DEBUG1, ("psodium: master: Got PUTBADSLAVES request from auditor!\n" )); 
        
        status = check_authorization( conf, r, true, &auth );
        if (status == APR_SUCCESS)
        {   
            if (auth)
            {
                // Go to C++ domain
                BadSlavesVector *bs = (BadSlavesVector *)conf->badslaves;
        
                status = pso_master_handle_auditor_feedback( r, bs );
            }
            else
                r->status = HTTP_UNAUTHORIZED;
        }
    }
#ifdef STANDALONE    
    else if(!strcmp( want_slave_val, TRUE_STR ))
    {
        // REMOVE
        // Emulate redirect to slave
        //
        DPRINTF( DEBUG1, ("psodium: master: Emulating redirect by Globule redirector!\n" )); 
        status = pso_master_send_redirect( r );
    }
#endif    
    else 
    {
        DPRINTF( DEBUG1, ("psodium: master: Request not for master!\n" )); 
        return DECLINED;
    }

    if (status != APR_SUCCESS)
        ap_die( status, r); // status should be an access_status value actually
    else
        ap_finalize_request_protocol(r);
    return OK;

}




/*
 * Client wants to know the auditor addr and slave's public key
 */
apr_status_t pso_master_send_slaveinfo( psodium_conf *conf, 
                                           request_rec *r,
                                           char *slave_id )
{
    apr_bucket_brigade *bb=NULL;
    apr_status_t status=APR_SUCCESS;
    apr_bucket *eos_bucket=NULL;

    DPRINTF( DEBUG1, ("psodium: master: Returning %s's pubkey and auditor address to client\n", slave_id ));

    ap_set_content_type(r, "text/plain");

    status = pso_slaveinfo_to_bb( conf, r, slave_id, &bb );
    if (status != APR_SUCCESS)
        return status;

    eos_bucket = apr_bucket_eos_create( bb->bucket_alloc );
    if (eos_bucket == NULL)
    {
        DPRINTF( DEBUG0, ("Cannot create reply to GETSLAVEINFO\n" ));
        return APR_ENOMEM;
    }
    APR_BRIGADE_INSERT_TAIL( bb, eos_bucket );
 
    status = ap_pass_brigade( r->output_filters, bb);
    return status;
}




#ifdef STANDALONE
static apr_status_t pso_master_send_redirect( request_rec *r )
{
    apr_status_t status=APR_SUCCESS;
    apr_bucket_brigade *b;
    apr_bucket *eos_bucket;
    char *slave_uri = "http://130.37.193.64:37000/sjaak.html";
    const char *badslaves_str = NULL;
    
    badslaves_str = apr_table_get( r->headers_in, BADSLAVES_HEADER );
    if (badslaves_str != NULL)
    {
        // Don't redirect again to these slaves
        
        
        //TODO
    }
    
    DPRINTF( DEBUG1, ("psodium: master: Sending 302 redirect.\n" )); 

    r->status = HTTP_MOVED_TEMPORARILY;
    apr_table_set( r->headers_out, "Location", slave_uri );

    ap_set_content_type(r, "text/plain");
    b = apr_brigade_create(r->pool, r->connection->bucket_alloc );
    if (b == NULL)
        return APR_ENOMEM;
    
    status = apr_brigade_printf( b, NULL, NULL, "This is a HTTP redirect to <a href=\"%s\">%s</A> generated by pSodium simulating NetAirt/Globule.\n", slave_uri, slave_uri );
    if (status != APR_SUCCESS)
        return status;
    status = apr_brigade_puts( b, NULL, NULL, CRLF );
    if (status != APR_SUCCESS)
        return status;

    eos_bucket = apr_bucket_eos_create( b->bucket_alloc );
    if (eos_bucket == NULL)
        return APR_ENOMEM;
    APR_BRIGADE_INSERT_TAIL( b, eos_bucket ); 

    status = ap_pass_brigade(r->output_filters, b);
    return status;
}
#endif



static apr_status_t pso_master_send_digests( request_rec *r, 
                                      digest_t *digest_array, 
                                      apr_size_t digest_array_length )
{
    apr_status_t status=APR_SUCCESS;
    apr_bucket_brigade *b;
    apr_bucket *eos_bucket;
    apr_off_t i=0;

    DPRINTF( DEBUG1, ("psodium: master: Sending getDigest reply.\n" )); 

    r->status = HTTP_OK;

    ap_set_content_type(r, "text/plain");
    b = apr_brigade_create(r->pool, r->connection->bucket_alloc );
    if (b == NULL)
        return APR_ENOMEM;

    status = apr_brigade_puts( b, NULL, NULL, DIGESTS_START_KEYWORD );
    if (status != APR_SUCCESS)
        return status;

    status = apr_brigade_printf( b, NULL, NULL, "%s %d%s", DIGESTS_COUNT_KEYWORD, digest_array_length, CRLF );
    if (status != APR_SUCCESS)
        return status;

    for (i=0; i<digest_array_length; i++)
    {
        DPRINTF( DEBUG1, ("psodium: master: Sending getDigest digest[%d] is %s\n", i, digest2string( digest_array[i], OPENSSL_DIGEST_ALG_NBITS/8, r->pool ) )); 
        status = apr_brigade_printf( b, NULL, NULL, "%s%s", digest2string( digest_array[i], OPENSSL_DIGEST_ALG_NBITS/8, r->pool ), CRLF );
        if (status != APR_SUCCESS)
            return status;
    }

    status = apr_brigade_puts( b, NULL, NULL, DIGESTS_END_KEYWORD );
    if (status != APR_SUCCESS)
        return status;

    status = apr_brigade_puts( b, NULL, NULL, CRLF );
    if (status != APR_SUCCESS)
        return status;

    eos_bucket = apr_bucket_eos_create( b->bucket_alloc );
    if (eos_bucket == NULL)
        return APR_ENOMEM;
    APR_BRIGADE_INSERT_TAIL( b, eos_bucket ); 

    status = ap_pass_brigade(r->output_filters, b);
    return status;
}




apr_status_t pso_master_send_MODIFICATIONS( psodium_conf *conf, 
                                            request_rec *r,
                                            apr_time_t audit_start_time,
                                            const char *globule_export_path )
{
    apr_status_t status=APR_SUCCESS, marshstatus=APR_SUCCESS;

    // Go to C++ domain
    Versions *v = (Versions *)conf->versions;
    rmmmemory *shm_alloc = rmmmemory::shm();
    
    // 2. Lock VERSIONS
    LOCK( ((AprGlobalMutex *)conf->versions_lock) );
    
    // 1. Remove old entries
    apr_time_t thresh = audit_start_time-AUDIT_INTERVAL;
    
    for (Versions::iterator i = v->begin(); i != v->end(); i++)
    {
        Slave2DigestsMap *s2d = i->second;
        for (Slave2DigestsMap::iterator j = s2d->begin(); j != s2d->end(); j++)
        {
            Time2DigestsMap *t2d = j->second;
            for (Time2DigestsMap::iterator k = t2d->begin(); k != t2d->end(); k++)
            {
                if (thresh > k->first)
                {
                    DPRINTF( DEBUG1, ("psodium: master: Removing digest %lld t=%lld\n", k->first, thresh ));
                    t2d->erase( k );
                    //delete k->first; is apr_time_t
                    ResponseInfo *ri = (ResponseInfo *)k->second;
                    operator delete( ri->digest, shm_alloc );
                    operator delete( k->second, shm_alloc ); // g++ overload don't work as expected
                    // see http://gcc.gnu.org/ml/gcc-bugs/2001-05/msg00031.html
                }
            }
            
            if (t2d->empty())
            {
                s2d->erase( j );
                operator delete( (void *)j->first, shm_alloc );
                operator delete( j->second, shm_alloc );
            }
        }

        if (s2d->empty())
        {
            v->erase( i );
            operator delete( (void *)i->first, shm_alloc );
            operator delete( i->second, shm_alloc );
        }
    }

    // 2. Send the stripped VERSIONS
    marshstatus = pso_marshall_send_VERSIONS( v, globule_export_path, r );
    
    // 4. Unlock VERSIONS
    UNLOCK( ((AprGlobalMutex *)conf->versions_lock) );
    
    return marshstatus;
}


static apr_status_t pso_master_handle_auditor_feedback( request_rec *r, 
                                                        IPAddrPortVector *vec )
{
    apr_status_t status = APR_SUCCESS;
    apr_bucket_brigade *bb=NULL;
    apr_bucket *e=NULL, *eos_bucket=NULL;

    /* Return OK if no failures*/
    r->status = HTTP_OK;

    bb = apr_brigade_create(r->pool, r->connection->bucket_alloc );
    if (bb == NULL)
        return APR_ENOMEM;
    
    while (ap_get_brigade( r->input_filters, 
                          bb, 
                          AP_MODE_READBYTES, 
                          APR_BLOCK_READ, 
                          PSO_SOCK_BLOCK_SIZE) == APR_SUCCESS) 
    {
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
            if (block_len != 0)
            {
                try
                {
                    vec->addFromMarshalledBlock( block, block_len );
                }
                catch( AprError e )
                {
                    r->status = HTTP_BAD_REQUEST;
                    break;
                }
            }
            
            if (APR_BUCKET_IS_EOS(e))
                break;
        }
        apr_brigade_cleanup( bb );
        if (APR_BUCKET_IS_EOS(e))
            break;
    }

    // Send something back
    eos_bucket = apr_bucket_eos_create( bb->bucket_alloc );
    if (eos_bucket == NULL)
        return APR_ENOMEM;
    APR_BRIGADE_INSERT_TAIL( bb, eos_bucket ); 

    status = ap_pass_brigade( r->output_filters, bb);
    
    return status;
}


static apr_status_t pso_master_handle_clientfeedback( psodium_conf *conf,
                                                      request_rec *r, 
                                                      char *slave_id, 
                                                      char *pledge_data,
                                                      apr_size_t pledge_curr_length,
                                                      char **error_str_out,
                                                      apr_bool_t *clientIsLying_out,
                                                      apr_bool_t *slaveIsBad_out )
{
    apr_status_t status = APR_SUCCESS;
    char *actual_slave_id=NULL;
    
    // Client sent us a pledge, allegedly bad.
    status = pso_master_check_pledge( conf, slave_id, pledge_data, 
                               pledge_curr_length, clientIsLying_out, 
                               slaveIsBad_out, &actual_slave_id,
                               error_str_out, 
                               r->pool );
    if (status != APR_SUCCESS)
    {
        *error_str_out = apr_pstrcat( r->pool, "Error handling client's badPledge: ", *error_str_out, NULL );
        DPRINTF( DEBUG1, ("pso: master: %s\n", *error_str_out ));
        return status;
    }
    
    DPRINTF( DEBUG3, ("Checked pledge, clientLies %d, slaveLies %d\n", *clientIsLying_out, *slaveIsBad_out ));

    // Go to C++ domain
    BadSlavesVector *bs = (BadSlavesVector *)conf->badslaves;
    LyingClientsVector *lc = (LyingClientsVector *)conf->lyingclients;
    
    if (*clientIsLying_out)
    {
        char *remote_ipaddr=NULL;
        apr_port_t remote_port;
        apr_sockaddr_ip_get(&remote_ipaddr, r->connection->remote_addr );
        apr_sockaddr_port_get(&remote_port, r->connection->remote_addr );

        try
        {
            DPRINTF( DEBUG1, ("pso: master: Blacklisting client %s:%hu for falsely reporting a bad pledge!\n", remote_ipaddr, remote_port ));
            lc->insert( remote_ipaddr, remote_port );
        }
        catch( AprError e )
        {
            *error_str_out = "Internal error blacklisting client";
            DPRINTF( DEBUG1, ("pso: master: %s\n", *error_str_out ));
            return e.getStatus();
        }
    }
    if (*slaveIsBad_out)
    {
        DPRINTF( DEBUG1, ("pso: master: Blacklisting slave %s for sending a bad pledge!\n", actual_slave_id )); 
        // Convert slave ID to (ipaddr,port)
        char *ptr = strchr( actual_slave_id, ':' );
        char *port_str = ptr+1;
        *ptr = '\0';
        apr_port_t port=0;
        int ret = pso_atoN( port_str, &port, sizeof( port ) );
        try
        {
            bs->insert( actual_slave_id, port );
        }
        catch( AprError e )
        {
            *error_str_out = "Internal error blacklisting slave";
            DPRINTF( DEBUG1, ("pso: master: %s\n", *error_str_out ));
            return e.getStatus();
        }
    }
    
    return APR_SUCCESS;
}



static apr_status_t pso_master_reply2clientfeedback( request_rec *r, 
                                                     apr_status_t old_status, 
                                                     char *error_str,
                                                     apr_bool_t clientIsLying,
                                                     apr_bool_t slaveIsBad,
                                                     char *slave_id, 
                                                     char *pledge_data, 
                                                     apr_size_t pledge_curr_length )
{
    apr_status_t status=APR_SUCCESS;
    apr_bucket_brigade *bb=NULL;

    if (old_status == APR_SUCCESS)
    {
        if (slaveIsBad)
        {
            status = pso_create_badcontent_reply_bb( r, &bb,  
                                                 slave_id,
                                                 pledge_data, 
                                                 pledge_curr_length,
                                                 error_str );
        }
        else
        {
            status = pso_create_clientlied_reply_bb( r, &bb,  
                                                 slave_id,
                                                 pledge_data, 
                                                 pledge_curr_length,
                                                 error_str );
        }

    }
    else
    {
        char aprmsg[ PSO_STATUS_STR_LEN ];
        
        r->status = HTTP_INTERNAL_SERVER_ERROR;
        
        (void)apr_strerror( old_status, aprmsg, PSO_STATUS_STR_LEN );
        char *msg = apr_pstrcat( r->pool, error_str, ":", aprmsg, NULL );
        pso_error_in_html_to_bb( r, msg, &bb );
    }
    
    status = ap_pass_brigade(r->output_filters, bb );
    return status;
}



static apr_status_t check_authorization( psodium_conf *conf, 
                                         request_rec *r, 
                                         bool checkIfAuditor,
                                         apr_bool_t *auth_out )
{
    apr_size_t decoded_len=0, decoded_len2=0;
    apr_size_t soffset=0;
    const char *auth=NULL;
    char *decoded=NULL,*base64userpass=NULL,*ptr=NULL;
    char *userid=NULL,*password=NULL;
    
    *auth_out = FALSE;
    
    if (checkIfAuditor && !strcmp( conf->auditor_passwd, "" ))
        return APR_SUCCESS;
    
    auth = apr_table_get( r->headers_in, "Authorization" );
    if (auth == NULL)
        return APR_SUCCESS;
    soffset = pso_find_keyword( (char *)auth, 0, strlen( auth ), "Basic" );
    if (soffset == -1)
        return APR_SUCCESS;
    soffset = pso_skip_whitespace( (char *)auth, soffset+strlen( "Basic"), strlen( auth ));
    base64userpass = pso_copy_tostring( (char *)auth, soffset, strlen( auth ), r->pool );
    
    DPRINTF( DEBUG5, ("psodium: master: Got authorization header: base is [%s]\n", base64userpass ));
    
    decoded_len = apr_base64_decode_len( base64userpass ); 
    decoded = (char *)apr_palloc( r->pool, decoded_len );
    decoded_len2 = apr_base64_decode( decoded, base64userpass );

    DPRINTF( DEBUG5, ("psodium: master: Got authorization header: decoded is [%s]\n", decoded ));
    
    ptr = strchr( decoded, ':' );
    password = ptr+1;
    *ptr = '\0';
    userid = decoded;
    
    DPRINTF( DEBUG3, ("psodium: master: Got authorization header: %s %s\n", userid, password ));
    if (checkIfAuditor && !strcmp( userid, AUDITOR_USERID ) && !strcmp( password, conf->auditor_passwd ))
    {
        *auth_out = TRUE;
    }
    else if (!checkIfAuditor)
    {
        const char *intraserverauth = conf->intraserverauthpwd;
        if (intraserverauth != NULL)
        {
            if (!strcmp( userid, intraserverauth ) && !strcmp( password, intraserverauth ))
            {
                *auth_out = TRUE;
            }
        }
    }
    if (*auth_out == FALSE)
    {
        if (checkIfAuditor)
        {
            DPRINTF( DEBUG1, ("Authentication failed for auditor!" ));
        }
        else
        {
            DPRINTF( DEBUG1, ("Authentication failed for client!" ));
        }
    }
    return APR_SUCCESS;
}


} // extern "C"


/*
 * C++ only after this point
 */


#define DEFAULT_IOVECS  32
/*
 * pre: Versions locked
 * TODO: size of Response also in marshalled version
 */
static apr_status_t pso_marshall_send_VERSIONS( Versions *v, 
                                      const char *globule_export_path,
                                      request_rec *r )
{
    return pso_marshall_sendwrite_VERSIONS( v, TRUE, globule_export_path, r, NULL, NULL );
}


/*
 * pre: Versions locked
 * TODO: size of Response also in marshalled version
 */
apr_status_t pso_master_marshall_write_VERSIONS( Versions *v, 
                                      const char *filename,
                                      apr_pool_t *pool )
{
    return pso_marshall_sendwrite_VERSIONS( v, FALSE, "/dummy/", NULL, filename, pool );
}



/*
 * It's counterpart, the unmarshalling code is in utillib/DigestUtil.cc
 * We can't move this code there because this code uses Apache calls
 * like ap_set_content_type(), which are not allowed in utillib, as that
 * is also linked against the auditor which is a standalone process
 * (sans Apache).
 */
static apr_status_t pso_marshall_sendwrite_VERSIONS( Versions *v, 
                                      apr_bool_t toNetwork,
                                      const char *globule_export_path,
                                      request_rec *r,
                                      const char *filename,
                                      apr_pool_t *filepool )
{
    apr_status_t status=APR_SUCCESS;
    apr_bucket_brigade *bb = NULL;
    apr_bucket *eos_bucket=NULL;
    apr_bucket_alloc_t *allocator;
    apr_file_t *file=NULL;
    apr_pool_t *pool=NULL;

    if (toNetwork)
    {
        r->status = HTTP_OK;
        ap_set_content_type(r, "text/plain");
        allocator = r->connection->bucket_alloc;
        pool = r->pool;
    }
    else
    {
        pool = filepool;
        allocator = apr_bucket_alloc_create( pool );
        status = apr_file_open( &file, filename, (APR_CREATE|APR_WRITE|APR_BINARY|APR_TRUNCATE), APR_OS_DEFAULT, pool );
        if (status != APR_SUCCESS)
            return status;

    }
    bb = apr_brigade_create( pool, allocator );
    if (bb == NULL)
        return APR_ENOMEM;

    status = apr_brigade_printf( bb, NULL, NULL, "%s %s%s", EXPORT_PATH_KEYWORD, globule_export_path, CRLF );
    if (status != APR_SUCCESS)
        return status;
    
    status = apr_brigade_printf( bb, NULL, NULL, "%s%s", VERSIONS_START_KEYWORD, CRLF );
    if (status != APR_SUCCESS)
        return status;

    for (Versions::iterator i = v->begin(); i != v->end(); i++)
    {
        const Gstring *uri_g = i->first;
        Slave2DigestsMap *s2d = i->second;

        DPRINTF( DEBUG5, ("Marshalling VERSIONS URI %s", uri_g->c_str() ));

        status = apr_brigade_printf( bb, NULL, NULL, "%s %s%s", REQUEST_URI_KEYWORD, uri_g->c_str(), CRLF );
        if (status != APR_SUCCESS)
            return status;
        status = apr_brigade_printf( bb, NULL, NULL, "%s%s", SLAVE2DIG_START_KEYWORD, CRLF );
        if (status != APR_SUCCESS)
            return status;
        
        for (Slave2DigestsMap::iterator j = s2d->begin(); j != s2d->end(); j++)
        {
            const Gstring *slave_id_g = j->first;
            Time2DigestsMap *t2d = j->second;

            status = apr_brigade_printf( bb, NULL, NULL, "%s %s%s", SLAVEID_KEYWORD, slave_id_g->c_str(), CRLF );
            if (status != APR_SUCCESS)
                return status;
            status = apr_brigade_printf( bb, NULL, NULL, "%s%s", TIME2DIG_START_KEYWORD, CRLF );
            if (status != APR_SUCCESS)
                return status;
            
            for (Time2DigestsMap::iterator k = t2d->begin(); k != t2d->end(); k++)
            {
                apr_time_t expiry = k->first;
                ResponseInfo *ri = (ResponseInfo *)k->second;
                digest_t digest = ri->digest;
                
                status = apr_brigade_printf( bb, NULL, NULL, "%s %lld %s%s", EXPIRYDIGEST_KEYWORD, expiry, digest2string( digest, OPENSSL_DIGEST_ALG_NBITS/8, pool), CRLF );
                if (status != APR_SUCCESS)
                    return status;
            }
            status = apr_brigade_printf( bb, NULL, NULL, "%s%s", TIME2DIG_END_KEYWORD, CRLF );
            if (status != APR_SUCCESS)
                return status;
        }
        
        status = apr_brigade_printf( bb, NULL, NULL, "%s%s", SLAVE2DIG_END_KEYWORD, CRLF );
        if (status != APR_SUCCESS)
            return status;

        /*
         * MODIFICATIONS/VERSIONS is sent in blocks, where a block is all info
         * regarding a single URI, which I believe to be a small enough block 
         * size.
         */

        status = write_marshalled( toNetwork, bb, r, file );
        apr_brigade_destroy( bb );
        if (status != APR_SUCCESS)
            return status;
        
        bb = apr_brigade_create( pool, allocator );
        if (bb == NULL)
            return APR_ENOMEM;
    }

    status = apr_brigade_printf( bb, NULL, NULL, "%s%s", VERSIONS_END_KEYWORD, CRLF );
    if (status != APR_SUCCESS)
        return status;

    eos_bucket = apr_bucket_eos_create( (bb)->bucket_alloc );
    if (eos_bucket == NULL)
        return APR_ENOMEM;
    APR_BRIGADE_INSERT_TAIL( bb, eos_bucket ); 


    status = write_marshalled( toNetwork, bb, r, file );
    apr_brigade_destroy( bb );
    if (status != APR_SUCCESS)
        return status;

    return APR_SUCCESS;
}


static apr_status_t write_marshalled( apr_bool_t toNetwork, apr_bucket_brigade *bb, 
                               request_rec *r, apr_file_t *file )
{
    apr_status_t status=APR_SUCCESS;
    if (toNetwork)
    {
        status = ap_pass_brigade(r->output_filters, bb );    
        if (status != APR_SUCCESS)
            return status;
    }
    else
    {
        DOUT( DEBUG5, "Writing marshalled VERSIONS block to disk" );

        /* APR needs some improvement here */
        struct iovec *vec=NULL;
        int nalloc=DEFAULT_IOVECS;
        do
        {
            vec = new struct iovec[nalloc];
            int nvecs = nalloc;

            status = apr_brigade_to_iovec( bb, vec, &nvecs );

            DPRINTF( DEBUG3, ("Marshall VERSIONS, converted bb to %d iovecs\n", nvecs ));

            if (status == APR_SUCCESS && nvecs != nalloc)
            {
                apr_size_t bytes_written=0;
                status = apr_file_writev( file, vec, nvecs, &bytes_written );
                if (status == APR_SUCCESS)
                {
                    delete[] vec;
                    break;
                }
                else
                {
                    delete[] vec;
                    return status;
                }
            }
            else if (status == APR_SUCCESS && nvecs == nalloc)
            {
                // Hmmm, the brigade may have more buckets that we gave
                // it iovecs. The APR interface don't provide an elegant
                // solution for this. What we do just give it more iovecs.
                // Alternative is to delete the first n buckets that were
                // successfully translated to an iovec (after writing them
                // out), however, that assumes a 1-to-1 correspondence 
                // between bucket and iovec.
                nalloc += DEFAULT_IOVECS;
                delete[] vec;
            }
            else
            {
                delete[] vec;
                return status;
            }

        } while( 1 );
    }

    return APR_SUCCESS;
}
