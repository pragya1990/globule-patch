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

/*============================================================================
 * pSodium output filter.
 * This filter is used:
 * 1. The master to generate a digest of the reply that will be served to 
 *    clients that wish to double check a slave (and for auditing) 
 * 2. A slave to add a pledge to the reply it generates.
 * 3. The master to add the auditor address and slave public key to a HTTP 
 *    302 redirect.
 *
 * The pso_output_filter function can be called multiple times per reply! 
 * ============================================================================
 */


#include "mod_psodium.h"
#include "../globule/utilities.h"

/*
 * Prototypes
 */


apr_status_t pso_slave_forward_bb_append_pledge( psodium_conf *conf, 
                                           ap_filter_t *f, 
                                           request_rec *r, 
                                           apr_bucket_brigade *bb );
apr_status_t pso_master_forward_bb_record_digest( psodium_conf *conf, 
                                           ap_filter_t *f, 
                                           request_rec *r, 
                                           apr_bucket_brigade *bb,
                                           const char *uri_str, 
                                           const char *slave_id, 
                                           const char *expiry_time_str );
apr_status_t pso_master_forward_bb_append_pubkey( psodium_conf *conf, 
                                           ap_filter_t *f, 
                                           request_rec *r, 
                                           apr_bucket_brigade *bb );
apr_status_t append_and_forward( ap_filter_t *f, 
                                   request_rec *r, 
                                   apr_bucket_brigade *orig_bb,
                                   apr_bucket *origtail_bucket,
                                   apr_bucket_brigade *newtail_bb );

apr_status_t pso_output_filter( ap_filter_t *f, apr_bucket_brigade *bb)
{
    const char *uri_str = NULL;
    const char *slave_id = NULL;
    const char *expiry_time_str = NULL;
    const char *want_pledge = NULL;
    const char *want_slave = NULL;
    request_rec *r = f->r;
    apr_uri_t slave_uri;
    //psodium_outputfilter_context  *ctx = f->ctx;
    psodium_conf *conf = ap_get_module_config(r->server->module_config,
                                                    &psodium_module);
    
    DPRINTF( DEBUG1, ("\npsodium: any: Filtering output\n" )); 
    
    /* only work on main request/no subrequests */
    if (r->main) 
    {
        DPRINTF( DEBUG1, ("psodium: any: Not filtering output because main!?\n" )); 
        ap_remove_output_filter(f); //Arno: correct copy 'n paste from 2.0.47?
        return ap_pass_brigade(f->next, bb);
    }

    uri_str = apr_table_get( r->notes, OUTFILT_RECORD_DIGEST_URI_NOTE );
    slave_id = apr_table_get( r->notes, OUTFILT_RECORD_DIGEST_AUTH_SLAVEID_NOTE );
    expiry_time_str = apr_table_get( r->notes, OUTFILT_RECORD_DIGEST_EXPIRYTIME_NOTE );
    want_pledge = apr_table_get( r->headers_in, WANT_PLEDGE_HEADER );

    // No %p in apr_log() stuff that is used underneath
    DPRINTF( DEBUG0, ("psodium: any: Which r->notes are set? uri=%lx, slave= %lx, et= %lx\n", (long)uri_str, (long)slave_id, (long)expiry_time_str ));

    if (want_pledge != NULL)
    {
        return pso_slave_forward_bb_append_pledge( conf, f, r, bb );
    }
    else if (expiry_time_str != NULL && slave_id != NULL && uri_str != NULL)
    {
        return pso_master_forward_bb_record_digest( conf, f, r, bb, uri_str, slave_id, expiry_time_str );
    }
    else
    {
        apr_off_t readbytes;
        apr_brigade_length( bb, 0, &readbytes);

        DPRINTF( DEBUG5, ("psodium: any: Output filter just forwarding %d bytes...\n", readbytes )); 

        //print_brigade( bb );
        //print_brigade( bb );

        return ap_pass_brigade(f->next, bb);
    }
}



/*
 * We're a slave and the client wants a pledge with its reply.
 */
apr_status_t pso_slave_forward_bb_append_pledge( psodium_conf *conf, 
                                           ap_filter_t *f, 
                                           request_rec *r, 
                                           apr_bucket_brigade *bb )
{
    apr_bucket *e=NULL, *last_bucket=NULL;
    apr_bucket_brigade *newtail_bb=NULL;
    apr_size_t pledge_len, content_len;
    char *temp_digest_str = NULL, *content_len_str=NULL, *pledge_len_str;
    const char *updated_cl = NULL;
    apr_status_t status=APR_SUCCESS;
    // OPENSSL
    EVP_MD_CTX *md_ctxp = (EVP_MD_CTX *)f->ctx;
    digest_t md;
    digest_len_t md_len;
    md = (digest_t)apr_palloc( r->pool, EVP_MAX_MD_SIZE );
    int err=0;

    DPRINTF( DEBUG5, ("psodium: slave: Appending pledge to reply: ctx is %p, md is %p\n", md_ctxp, md )); 


    updated_cl = apr_table_get( r->notes, TEMP_UPDATED_CONTENT_LENGTH_NOTE );
    if (updated_cl == NULL)
    {
        /* First brigade in request:
         *
         * Update the Content-Length header to include the pledge.
         *
         * Add pSodium-Pledge-Length header to confer pledge size.
         */
        status = pso_slave_determine_marshalled_pledge_length( conf, r, &pledge_len );
        if (status != APR_SUCCESS)
            return status;

        content_len_str = apr_table_get( r->headers_out, "Content-Length" );
        if (content_len_str != NULL)
        {
            int ret = pso_atoN( content_len_str, &content_len, sizeof( content_len ) );
            if (ret == -1)
            {
                DPRINTF( DEBUG0, ("psodium: slave: Appending pledge to reply: content length not number %s\n", content_len_str )); 
                return APR_EINVAL;
            }
            apr_size_t total = content_len+pledge_len;
            ret = pso_Ntoa( &total, sizeof( total ), &content_len_str, r->pool );
            apr_table_set( r->headers_out, "Content-Length", content_len_str );
        }

        DPRINTF( DEBUG5, ("psodium: slave: Appending pledge to reply: pledge has size %d\n", pledge_len )); 

        int ret = pso_Ntoa( &pledge_len, sizeof( pledge_len ), &pledge_len_str, r->pool );
        apr_table_set( r->headers_out, PLEDGE_LENGTH_HEADER, pledge_len_str );
        
        apr_table_set( r->notes, TEMP_UPDATED_CONTENT_LENGTH_NOTE, TRUE_STR );
    }

    /* Can't concat here, we need to calculate the digest */
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


    /*
     * Find the end of the old reply, so we can append the pledge
     */
    last_bucket = APR_BRIGADE_LAST(bb);
    if (!APR_BUCKET_IS_EOS( last_bucket ))
    {
        DPRINTF( DEBUG5, ("psodium: slave: output: Brigade doesn't contain EOS, forwarding...\n" )); 
        return ap_pass_brigade( f->next, bb );
    }

    DPRINTF( DEBUG5, ("psodium: slave: output: Calculating digest of reply for pledge\n" )); 

    /* Now create the pledge and append it to the reply */
    EVP_DigestFinal( md_ctxp, md, &md_len );

    DPRINTF( DEBUG5, ("psodium: slave: output: Marshalling pledge to bb.\n", pledge_len )); 

    status = pso_slave_pledge_to_bb( conf, r, md, md_len, &newtail_bb );
    if (status != APR_SUCCESS)
    {
        // Failure, return original bb
        return ap_pass_brigade( f->next, bb ); 
    }

    return append_and_forward( f, r, bb, last_bucket, newtail_bb );
}






/*
 * We're a master and instructed to record the digest of the outgoing reply
 * with the given expiry time.
 */
apr_status_t pso_master_forward_bb_record_digest( psodium_conf *conf, 
                                           ap_filter_t *f, 
                                           request_rec *r, 
                                           apr_bucket_brigade *bb,
                                           const char *uri_str,
                                           const char *slave_id, 
                                           const char *expiry_time_str )
{
    const char *content_len_str;
    apr_bucket *e;
    // OPENSSL
    EVP_MD_CTX *md_ctxp = (EVP_MD_CTX *)f->ctx;
    digest_t md;
    digest_len_t md_len;
    md = (digest_t)apr_palloc( r->pool, EVP_MAX_MD_SIZE );
    
    DPRINTF( DEBUG1, ("psodium: master: Output filter recording digest:\n" )); 

    for (e = APR_BRIGADE_FIRST(bb); e != APR_BRIGADE_SENTINEL(bb); e = APR_BUCKET_NEXT(e)) 
    {
        const char *block;
        apr_size_t block_len=0;
        apr_off_t  i=0;

        if (APR_BUCKET_IS_FLUSH(e)) 
        {
            continue;
        }

        /* read */
        apr_bucket_read(e, &block, &block_len, APR_BLOCK_READ);

        DPRINTF( DEBUG3, ("Digesting %d bytes\n", block_len ));

        // OPENSSL
        EVP_DigestUpdate( md_ctxp, block, block_len );

        if (APR_BUCKET_IS_EOS(e)) 
        {
            // Last bucket in reply, time to record.
            apr_size_t rsize;

            EVP_DigestFinal( md_ctxp, md, &md_len );

            DPRINTF( DEBUG3, ("Digest is %d bytes\n", md_len ));

            //DPRINTF( DEBUG1, ("psodium: master: Recording digest %s for url %s at slave %s: ", digest2string( md, md_len, r->pool ), r->unparsed_uri, slave_id )); 

            // Add to VERSIONS
            content_len_str = apr_table_get( r->headers_out, "Content-Length" );
            if (content_len_str == NULL)
            {
                DPRINTF( DEBUG0, ("psodium: master: Error recording digest: no content length!\n" )); 
            }
            else
            {
                int ret = pso_atoN( content_len_str, &rsize, sizeof( rsize ) );
                if (ret == -1)
                    DPRINTF( DEBUG1, ("psodium: master: Error recording digest: content length not number!\n" )); 
            }

            DPRINTF( DEBUG3, ("Digest2 is %d bytes\n", md_len ));
            pso_master_record_digest( conf, r, md, md_len, rsize, uri_str, slave_id, expiry_time_str );
        }
    }

    return ap_pass_brigade( f->next, bb );
}


apr_status_t append_and_forward( ap_filter_t *f, 
                                   request_rec *r, 
                                   apr_bucket_brigade *orig_bb,
                                   apr_bucket *origtail_bucket,
                                   apr_bucket_brigade *newtail_bb )
{
    apr_bucket *copied_origtail_bucket,*eos=NULL;
    apr_bucket_brigade *newbb=NULL;
    apr_status_t status=APR_SUCCESS;
    const char *block;
    apr_size_t block_len=0;

    
    /* Last bucket in the reply found!
     * Turn this bucket into a non-EOS bucket so we can append stuff.
     * We can't un-EOS with the APR interface so we have to copy.
     */

    /* Move buckets of old reply into a new brigade */
    newbb = apr_brigade_create(r->pool, r->connection->bucket_alloc ); // or: f->c->bucket_alloc
    if (newbb == NULL)
    {
        // Failure, return original bb
        return ap_pass_brigade( f->next, orig_bb );
    }

    {
        apr_off_t readbytes;
        apr_brigade_length( orig_bb, 0, &readbytes);
        DPRINTF( DEBUG5, ("psodium: any: output: Moving brigade total len %d to newbb\n", readbytes )); 
    }

    APR_BUCKET_REMOVE( origtail_bucket );
    if (!APR_BRIGADE_EMPTY( orig_bb ))
        APR_BRIGADE_CONCAT( newbb, orig_bb );

    DPRINTF( DEBUG5, ("psodium: any: output: At last bucket\n" )); 

    apr_bucket_read( origtail_bucket, &block, &block_len, APR_BLOCK_READ);
    DPRINTF( DEBUG5, ("psodium: any: output: Last bucket has size %d\n", block_len )); 
    if (block_len > 0)
    {
        copied_origtail_bucket = apr_bucket_pool_create( block, block_len, r->pool, newbb->bucket_alloc);
        APR_BRIGADE_INSERT_TAIL( newbb, copied_origtail_bucket );
    }
    DPRINTF( DEBUG5, ("psodium: any: output: After adding old tail to newbb\n" )); 

    DPRINTF( DEBUG5, ("psodium: any: output: Before adding new tail to newbb\n" )); 

    {
        apr_off_t readbytes;
        apr_brigade_length( newtail_bb, 0, &readbytes);
        DPRINTF( DEBUG5, ("psodium: any: output: New tail has total len %d\n", readbytes )); 
    }

    APR_BRIGADE_CONCAT( newbb, newtail_bb );

    // It appears the next filter stage can't deal with EOS buckets that are not empty,
    // although other code suggests such buckets are valid. So, we use an extra empty EOS.
    eos = apr_bucket_eos_create( newbb->bucket_alloc );
    if (eos == NULL)
    {
        // Failure, original bb is empty, so try this...
        return ap_pass_brigade( f->next, newbb );   // BROKEN AFTER CONCAT
    }
    APR_BRIGADE_INSERT_TAIL( newbb, eos );

    {
        apr_off_t readbytes;
        apr_brigade_length( newbb, 0, &readbytes);
        DPRINTF( DEBUG5, ("psodium: any: output: Moving brigade total len %d to next filter stage\n", readbytes )); 
    }

    // DEBUG5: print_brigade( newbb );

    return ap_pass_brigade( f->next, newbb );
}


