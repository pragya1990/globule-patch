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
// 
// File:   BadSlavesUtil.cc
// Author: arno
//
// Created on September 10, 2003, 11:28 AM
//
#include "mod_psodium.h"
#include "BadSlavesVector.hpp"

extern "C" {

void pso_master_badslaves_to_note2globule( psodium_conf *conf, request_rec *r )
{
    if (conf->role & PSODIUM_MASTER_ROLE)
    {
        char *list_str = NULL;
        // Go to C++ domain
        BadSlavesVector *bs = (BadSlavesVector *)conf->badslaves;

        list_str = apr_pstrcat( r->pool, "", NULL );

        for (BadSlavesVector::iterator i = bs->begin(); i != bs->end(); i++)
        {
            const struct ipaddrport *pair = *i;
            char *port_str=NULL;
            port_str = apr_psprintf( r->pool, "%hu", pair->port );
            if (strlen( list_str ) > 0)
                list_str = apr_pstrcat( r->pool, list_str, ";", NULL ); // add separator
            list_str = apr_pstrcat( r->pool, list_str, pair->ipaddr_str, ":", port_str, NULL );
        }

        if (strlen( list_str ) > 0)
        {
            DOUT( DEBUG0, "PSODIUM SETTING BAD SLAVES NOTE" );

            apr_table_set( r->notes, PSODIUM_BADSLAVES_NOTE, list_str );
        }
        else
            DOUT( DEBUG0, "PSODIUM SETTING not BAD SLAVES NOTE" );
    }
}



apr_bool_t pso_client_connecting_to_bad_slave( psodium_conf *conf, 
                                         apr_pool_t *pool, 
                                         const char *server_id )
{
    char *hostname = apr_pstrdup( pool, server_id );
    char *ptr = strchr( hostname, ':' );
    if (ptr == NULL)
        return 0;
    apr_port_t port;
    int ret = pso_atoN( ptr+1, &port, sizeof( port ));
    *ptr = '\0';

    // Go to C++ domain
    BadSlavesVector *bs = (BadSlavesVector *)conf->badslaves;
    for (BadSlavesVector::iterator i = bs->begin(); i != bs->end(); i++)
    {
        const struct ipaddrport *pair = *i;
        if (!strcmp( pair->ipaddr_str, hostname ) && pair->port == port)
        {
            return 1;
        }
    }
    return 0;
}


void pso_client_mark_bad_slave( psodium_conf *conf, apr_pool_t *pool,const char *slave_id )
{
    // Go to C++ domain
    BadSlavesVector *bs = (BadSlavesVector *)conf->badslaves;
    DPRINTF( DEBUG1, ("pso: client: Blacklisting slave %s for sending a bad pledge!\n", slave_id )); 
    // Convert slave ID to (ipaddr,port)
    char *hostname = apr_pstrdup( pool, slave_id );
    char *ptr = strchr( hostname, ':' );
    char *port_str = ptr+1;
    *ptr = '\0';
    apr_port_t port=0;
    int ret = pso_atoN( port_str, &port, sizeof( port ) );
    try
    {
        // insert will copy data to shm and lock stuff
        bs->insert( hostname, port );
    }
    catch( AprError e )
    {
        DOUT( DEBUG1, "pso: client: Internal error blacklisting slave:" << e.getStatus() );
    }
}


} // extern "C"
