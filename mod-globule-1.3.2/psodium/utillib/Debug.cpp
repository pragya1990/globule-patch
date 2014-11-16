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
//APL

#include "mod_psodium.h"
#include "Versions.hpp"
#include "Slave2DigestsMap.hpp"
#include "Time2DigestsMap.hpp"
#include "SlaveDatabase.hpp"


void print_Versions( Versions *v, apr_pool_t *pool )
{
    for (Versions::iterator i = v->begin(); i != v->end(); i++)
    {
        DPRINTF( DEBUG1, ("psodium: debug: URI %s\n", i->first->c_str() )); 
        print_Slave2DigestsMap( i->second, pool );
    }
}


void print_Slave2DigestsMap( Slave2DigestsMap *s2d, apr_pool_t *pool )
{
    for (Slave2DigestsMap::iterator i = s2d->begin(); i != s2d->end(); i++)
    {
        DPRINTF( DEBUG1, ("psodium: debug: slave %s\n", i->first->c_str() )); 
        print_Time2DigestsMap( i->second, pool );
    }
}


void print_Time2DigestsMap( Time2DigestsMap *t2d, apr_pool_t *pool )
{
    ResponseInfo *ri;
    for (Time2DigestsMap::iterator i = t2d->begin(); i != t2d->end(); i++)
    {
        ri = (ResponseInfo *)i->second;
        char *size_str=NULL;
        int ret = pso_Ntoa( &ri->size, sizeof( ri->size ), &size_str, pool ); 
        DPRINTF( DEBUG1, ("psodium: debug: (expiry %lld, digest %s, size %s)\n", i->first, digest2string( ri->digest, OPENSSL_DIGEST_ALG_NBITS/8, pool ), size_str )); 
    }
}


void print_SlaveRecord( SlaveRecord *sr, apr_pool_t *pool )
{
    DPRINTF( DEBUG1, ("psodium: debug: SlaveRecord is (master %s, pubkey %s)\n", sr->master_id->c_str(), sr->pubkey_pem->c_str() )); 
}

void print_SlaveDatabase( SlaveDatabase *v, apr_pool_t *pool )
{
    for (SlaveDatabase::iterator i = v->begin(); i != v->end(); i++)
    {
        DPRINTF( DEBUG1, ("psodium: debug: SlaveRecord for slaveid=%s\n", i->first->c_str() )); 
        print_SlaveRecord( i->second, pool );
    }
}


#include <apr_buckets.h>

void print_brigade( apr_bucket_brigade *bb )
{
    apr_bucket *e=NULL;

    APR_BRIGADE_FOREACH(e, bb) 
    {
        const char *block;
        apr_size_t block_len=0;

        DPRINTF( DEBUG1, ("pso: output: print brigade: %s\n", e->type->name )); 

        if (APR_BUCKET_IS_FLUSH(e)) 
        {
            DPRINTF( DEBUG1, ("pso: output: print brigade: flush.\n" )); 
            continue;
        }
        if (APR_BUCKET_IS_EOS(e)) 
        {
            DPRINTF( DEBUG1, ("pso: output: print brigade: eos.\n" )); 
            break;
        }

        /* read */
        apr_bucket_read(e, &block, &block_len, APR_BLOCK_READ);

        DPRINTF( DEBUG1, ("pso: output: print brigade: data %d bytes.\n", block_len )); 
    }
}

