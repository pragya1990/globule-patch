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
#include "../globule/utilities.h"

apr_status_t pso_error_in_html_to_bb( request_rec *r, char *error_str, apr_bucket_brigade **bb_out )
{
    apr_status_t status=APR_SUCCESS;
    apr_bucket *eos;

    DPRINTF( DEBUG1, ("psodium: Sending ERROR message \"%s\"\n", error_str )); 
    r->status = HTTP_BAD_GATEWAY;
    r->status_line = "502 Bad Gateway ";


    ap_set_content_type(r, "text/plain");
    *bb_out = apr_brigade_create(r->pool, r->connection->bucket_alloc );
    if (*bb_out == NULL)
        return APR_ENOMEM;
    
    status = apr_brigade_printf( *bb_out, NULL, NULL, "mod-psodium encountered an error while processing the request: %s.\n", error_str );
    if (status != APR_SUCCESS)
        return status;
    status = apr_brigade_puts( *bb_out, NULL, NULL, CRLF );
    if (status != APR_SUCCESS)
        return status;

    eos = apr_bucket_eos_create( (*bb_out)->bucket_alloc );
    if (eos == NULL)
        return APR_ENOMEM;
    APR_BRIGADE_INSERT_TAIL( *bb_out, eos );

    return APR_SUCCESS;
}


void pso_error_send_connection_failed( request_rec *r, int access_status, char *role )
{
    char *msg = apr_psprintf( r->pool, "Attempt to connect to %s returned HTTP status %d\n", role, access_status );
    apr_bucket_brigade *bb=NULL;
    apr_status_t status = pso_error_in_html_to_bb( r, msg, &bb );
    if (status == APR_SUCCESS)
    {
        status = ap_pass_brigade( r->output_filters, bb);
    }

    if (status != APR_SUCCESS)
    {
        // Attempt to let Apache generate the error
        r->status = access_status;
    }
}

