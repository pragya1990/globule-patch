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

static void dump_reply_to_debugoutput( apr_bucket_brigade *bb );

static char *pso_client_make_into_auditor_putPledge_request( psodium_conf *conf, 
                                                      request_rec *r, 
                                                      const char *slave_id,
                                                      const char *pledge_base64 );
static void pso_client_reset_auditor_putPledge_request( request_rec *r );


int pso_client_auditor_request( psodium_conf *conf, 
                                 request_rec *r,
                                 const char *slave_id,
                                 char *url, 
                                 const char *proxyname, 
                                 apr_port_t proxyport )
{
    const char *pledge_base64=NULL;
    char *putpledge_url=NULL;
    int access_status=OK;
    
    // Send pledge to auditor (only proper ones that where not double checked)
    pledge_base64 = retrieve_pledge_encoded( r );
    putpledge_url = pso_client_make_into_auditor_putPledge_request( conf, r, slave_id, pledge_base64 );
    DPRINTF( DEBUG1, ("pso: client: ==> PHASE 5: Sending pledge to auditor.\n" )); 
    access_status = renamed_ap_proxy_http_handler( r, conf, putpledge_url, proxyname, proxyport );
    DPRINTF( DEBUG1, ("psodium: client: PUTPLEDGE to auditor returned %d\n", access_status ));
    pso_client_reset_auditor_putPledge_request( r );

    return OK; // Don't care what putDigest got
}


pso_response_action_t pso_client_auditor_response( psodium_conf *conf, 
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
    // Whatever the auditor says back, I'm not interested.
    DPRINTF( DEBUG1, ("psodium: client: Ignoring putDigest reply.\n" )); 
    dump_reply_to_debugoutput( bb );
    return RESPONSE_ACTION_NOSEND;
}



static void dump_reply_to_debugoutput( apr_bucket_brigade *bb )
{
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
            DPRINTF( DEBUG1, ("psodium: client: Auditor said: %*s", block_len, block ));
    }
}




        
static char *pso_client_make_into_auditor_putPledge_request( psodium_conf *conf, 
                                                             request_rec *r, 
                                                             const char *slave_id,
                                                             const char *pledge_base64 )
{
    char *encoded_query = NULL, *url = NULL;
    apr_size_t encoded_len, encoded_len2=0;
    apr_status_t status=APR_SUCCESS;
    SlaveRecord *slaverec=NULL;

    status = pso_find_slaverec( conf, slave_id, &slaverec, r->pool );
    if (status != APR_SUCCESS)
        return "http://unknown.slave/internal/error";

    char *hostport=apr_pstrdup( r->pool, slaverec->auditor_addr->c_str() );
    char *ptr = strchr( hostport, ':' );
    if (ptr == NULL)
        return "http://unknown.slave/internal/error";
    char *auditor_port_str = ptr+1;
    *ptr = '\0';
    char *auditor_hostname = hostport;
    url = apr_pstrcat( r->pool, "http://", auditor_hostname, ":", 
                              auditor_port_str, 
                              PSO_PUTPLEDGE_URI_PREFIX, "?",
                              pledge_base64, NULL );
    
    apr_table_set( r->notes, RECEIVER_COMMAND_NOTE, IGNORE_PUTPLEDGE_REPLY_COMMAND );
    
    return url;
}


static void pso_client_reset_auditor_putPledge_request( request_rec *r )
{
    apr_table_unset( r->notes, RECEIVER_COMMAND_NOTE );
}


