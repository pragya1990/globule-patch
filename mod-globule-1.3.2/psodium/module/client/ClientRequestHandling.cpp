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

extern "C" 
{

/*=================================== Implementation =======================*/

/*
 * This handles http:// URLs, and other URLs using a remote proxy over http
 * If proxyhost is NULL, then contact the server directly, otherwise
 * go via the proxy.
 * Note that if a proxy is used, then URLs other than http: can be accessed,
 * also, if we have trouble which is clearly specific to the proxy, then
 * we return DECLINED so that we can try another proxy. (Or the direct
 * route.)
 */
int pso_client_http_handler( request_rec *r, psodium_conf *conf,
                          char *url, const char *proxyname, 
                          apr_port_t proxyport)
{
     //
     // CLIENT BEHAVIOR
     //
    
    /* We adopt the following procedure, pending a scheme for authenticating
     * masters.
     *
     * 1. If the server in Host: is not in the key cache,  we forward the 
     *    request to that server.
     * a. If the server responds with a HTTP redirect with a header indicating 
     *    it is the master, we assume it is, and the Location: header in
     *    the reply indicates the slave. The body contains the auditor address
     *    and the slave's public key.
     * b. We add the key to the key cache under the slave_id.
     * c. We return the HTTP redirect to the client.
     *
     * a. If the server does not respond with a HTTP redirect, we assume
     *    it is not pSodium enabled and bounce the request, such that mod_proxy
     *    could handle it.
     * 
     * 2. If the server in Host: *is* in the key cache, we forward the
     *    request to the server with a X-pSodium-Want-Pledge: True header.
     * a. The server will respond with the desired reply and a pledge.
     *    If it doesn't send a pledge, some master claimed it is a slave 
     *    or it is a slave that doesn't have pSodium turned on.
     *
     *    In both cases, we ask the master for a different slave.
     *    HOW????
     */
    const char *server_id=NULL;
    char *master_id=NULL, *pubkey=NULL;
    int access_status=OK;
    apr_status_t status = APR_SUCCESS;

    // TODO: HTTP 1.0
    server_id = apr_table_get( r->headers_in, "Host" );
    if (server_id == NULL)
        return HTTP_VERSION_NOT_SUPPORTED;

    DPRINTF( DEBUG1, ("psodium: client: Handling proxy request to host %s\n", server_id )); 

    if (pso_client_connecting_to_bad_slave( conf, r->pool, server_id ))
    {
        apr_bucket_brigade *bb=NULL;
        status = pso_create_badslave_reply_bb( r, &bb );
        if (status == APR_SUCCESS)
        {
            status = ap_pass_brigade(r->output_filters, bb );
            return OK;
        }
        else
        {
            return ap_proxyerror(r, HTTP_CONFLICT,
                             apr_pstrcat( r->pool,"URI ", url, " points to bad slave according to pSodium, try the original server.",
                                         NULL));
        }
    }

    status = pso_find_pubkey( conf, server_id, &pubkey, r->pool );
    if (status != APR_SUCCESS)
        return HTTP_INTERNAL_SERVER_ERROR;

    if (pubkey == NULL)
    {
        // Unknown server, assuming a master
        DPRINTF( DEBUG1, ("psodium: client: Handling proxy request: Request is to unknown server %s, assuming it is a master\n", server_id )); 
        int access_status = pso_client_redirect_request( conf, r, server_id, url, proxyname, proxyport );
        if (access_status != OK)
            return access_status;
        
        if (r->status != HTTP_MOVED_TEMPORARILY)
        {
            DPRINTF( DEBUG1, ("psodium: client: Master did not return a 302 redirect, assuming it handles the request itself\n" )); 
            return OK;
        }
        // Now ask master for slave's key (no, we cannot combine the two and not change Globule)
        // And it fits better with a separated redirector/master design in the future.
        return pso_client_getslaveinfo_request( conf, r, server_id, url, proxyname, proxyport );
    }
    else
    {
        // Contacting slave.
        DPRINTF( DEBUG1, ("psodium: client: Handling proxy request: Request to known server %s, assuming it is a slave\n", server_id )); 
        return pso_client_content_request( conf, r, server_id, url, proxyname, proxyport );
    }
}



/*
 * Called from modified_proxy_http.c
 */
pso_response_action_t pso_client_handle_http_response( psodium_conf *conf, 
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
    const char *command = NULL;
    char *error_str=NULL;
    apr_bool_t  match=FALSE;
    char *pledge_str=NULL;


    DPRINTF( DEBUG1, ("psodium: client: HTTP Response handler: Checking what to do with response...\n" )); 

    // Determine what we're doing.
    command = apr_table_get( r->notes, RECEIVER_COMMAND_NOTE );
    if (command == NULL)
    {
        DPRINTF( DEBUG1, ("psodium: client: HTTP Response handler: What to do? No instructions given?!\n" )); 
        return RESPONSE_ACTION_SEND;
    }
    else if (!strcmp( command, IGNORE_REDIRECT_REPLY_COMMAND ))
    {
        /*
         * We are processing a reply from the master that tells us which slave
         * to use.
         */
        return pso_client_redirect_response( conf, r, bb, brigade_len );   
    }
    else if (!strcmp( command, RECORD_SLAVEINFO_COMMAND ))
    {
        /*
         * We are processing a reply from the master that tells us the public 
         * of the slave to use and the auditor address for this site.
         * We just extract this info and record it in shmem.
         */
        return pso_client_getslaveinfo_response( conf, r, bb, brigade_len, newbb_out );   
    }
    else if (!strcmp( command, KEEP_PLEDGE4AUDIT_COMMAND ) || !strcmp( command, KEEP_REPLY4DOUBLECHK_COMMAND ))
    {
        /*
         * We are processing a reply from the slave that has a pledge appended.
         * If we don't double check with the master, we temporarily store the 
         * reply until we can verify that the pledge is OK, and then forward it.
         * If we DO double check, we store the reply and only return it after we
         * got the list of valid digests from the master and matched one with
         * the pledge.
         */
        apr_bool_t doublecheck=FALSE;
        if (!strcmp( command, KEEP_REPLY4DOUBLECHK_COMMAND ))
            doublecheck=TRUE;

         return pso_client_content_response( doublecheck, conf, r, rp, 
                                  content_length_str,
                                  content_idx, 
                                  content_length,
                                  pledge_length,
                                  bb,
                                  brigade_len, 
                                  newbb_out,
                                  temp_file_out,
                                  md_ctxp,
                                  digest_array_out, 
                                  digest_array_length_out );
    }
    else if (!strcmp( command, DOUBLECHECK_COMMAND ))
    {
        /*
         * We are processing a reply from the master containing the list of 
         * valid digests
         */
        return pso_client_getdigests_response( conf, r, rp, 
                                  content_length_str,
                                  content_idx, 
                                  content_length,
                                  pledge_length,
                                  bb,
                                  brigade_len, 
                                  newbb_out,
                                  temp_file_out,
                                  md_ctxp,
                                  digest_array_out, 
                                  digest_array_length_out );
    }
    else if (!strcmp( command, IGNORE_PUTPLEDGE_REPLY_COMMAND ))
    {
        // Processing the reply of the auditor to our putPledge (=we ignore it)
        return pso_client_auditor_response( conf, r, rp, 
                                  content_length_str,
                                  content_idx, 
                                  content_length,
                                  pledge_length,
                                  bb,
                                  brigade_len, 
                                  newbb_out,
                                  temp_file_out,
                                  md_ctxp,
                                  digest_array_out, 
                                  digest_array_length_out );
    }
    else if (!strcmp( command, FORWARD_COMMAND ))
    {
        DPRINTF( DEBUG1, ("psodium: client: Simply forwarding reply to browser.\n" )); 
        return RESPONSE_ACTION_SEND;
    }
    else
    {
        DPRINTF( DEBUG1, ("pso: client: unhandled RECEIVER command %s.\n", command )); 
    }

    return RESPONSE_ACTION_SEND;
}


} // extern "C"

