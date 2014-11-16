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
#include "mod_psodium.h"
#include "utillib/LyingClientsVector.hpp"

apr_status_t pso_master_client_is_known_liar( psodium_conf *conf, 
                                         request_rec *r, 
                                         apr_bool_t *liar_out )
{
    *liar_out = false;
    char *remote_ipaddr_str;

    // First check if request is from lying client, if so, refuse the muthuh
    LyingClientsVector *lc = (LyingClientsVector *)conf->lyingclients;
    apr_status_t status = apr_sockaddr_ip_get( &remote_ipaddr_str, r->connection->remote_addr );
    if (status != APR_SUCCESS)
    {
        return status;
    }

    for (LyingClientsVector::iterator i = lc->begin(); i != lc->end(); i++)
    {
        const struct ipaddrport *pair = *i;
        if (!strcmp( remote_ipaddr_str, pair->ipaddr_str ))
        {
            *liar_out = TRUE;
            return APR_SUCCESS;
        }
    }
    return APR_SUCCESS;
}
