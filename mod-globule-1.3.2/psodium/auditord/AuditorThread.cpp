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
// C++
#include "AuditorMain.hpp"
#include "utillib/Pledge.hpp"
#include "../globule/netw/Url.hpp"


static apr_status_t parse_and_check_pledgefwd( char *elem, apr_size_t elem_len, 
                                        apr_pool_t *pool );
static void find_digests( char *uri_str,
                   char *slave_id,
                   digest_t **digest_array_out, 
                   apr_time_t **expiry_array_out,
                   apr_size_t *digest_array_length_out,
                   apr_pool_t *pool );
static apr_status_t blacklist_client( char *client_ipaddr_str, 
                                      apr_port_t client_port, 
                                      LyingClientsVector *lc );

/*
 * Start function for audit threads
 */
void *APR_THREAD_FUNC auditthread_func( apr_thread_t *thd, void *data )
{
    apr_status_t status = APR_SUCCESS, getstatus = APR_SUCCESS;
    char *elem=NULL;
    apr_size_t elem_len=0;
    bool more=true;
    apr_pool_t *parentpool = (apr_pool_t *)data, *mypool=NULL;
    
    while( 1 )
    {
        // Wait for timerthread, or for turn
        DPIDPRINTF( DEBUG1, ("psoaudit: Auditor thread waiting for turn\n" ));
        try
        {
            _sema->down();
        }
        catch( AprError e )
        {
            fatal( e.getStatus() );
        }
     
        // TODO: grab multiple pledgeforwards per entry, to reduce
        // the thread sync overhead
        
        DPIDPRINTF( DEBUG1, ("psoaudit: Auditor thread retrieving pledgefwd\n" ));
        try
        {
            more = _curr_audited_table->getNextElement( &elem, &elem_len );
        }
        catch( AprError e )
        {
            getstatus = e.getStatus();
        }
        
        // Give someone else a go
        try
        {
            DPIDPRINTF( DEBUG4, ("psoaudit: gotElement, before sem up\n" ));
            _sema->up();
            DPIDPRINTF( DEBUG4, ("psoaudit: gotElement, after sem up\n" ));
        }
        catch( AprError e )
        {
            fatal( e.getStatus() );
        }
        
        if (more==false)
        {
            DPIDPRINTF( DEBUG1, ("psoaudit: Audited hashtable's empty, sleeping for %d seconds\n", apr_time_sec( AUDITOR_NAP ) ));
            //DPIDPRINTF( DEBUG1, ("Z") );
            apr_sleep( AUDITOR_NAP );
        }
        
        if (getstatus != APR_SUCCESS || more==false)
        {
            continue;
        }
        
        // TODO: create/destroy pool not per-pledgefwd
        status = apr_pool_create( &mypool, parentpool );
        if (status != APR_SUCCESS)
            fatal( status );
        
        DPIDPRINTF( DEBUG1, ("psoaudit: Auditor thread checking pledgefwd\n" ));
        (void)parse_and_check_pledgefwd( elem, elem_len, mypool );
        delete[] elem; // ~Kansas
        
        apr_pool_destroy( mypool );
    }
}




static apr_status_t parse_and_check_pledgefwd( char *elem, apr_size_t elem_len, apr_pool_t *pool )
{
    // Breakup element into pledge and client info
    apr_off_t soffset=0, eoffset=0;
    apr_port_t client_port;
    char *error_str = NULL;
    apr_status_t status=APR_SUCCESS;
    apr_bool_t match=false;
    
    DPIDPRINTF( DEBUG1, ("psoaudit: Parse elem_len is %d\n", elem_len ));
    
    apr_size_t bla;
    char *str = arnopr_memcat( &bla, elem, elem_len, " ", 2, NULL ); 
    DPIDPRINTF( DEBUG5, ("psoaudit: Parse elem is %s\n", str ));
    
    
    // CLIENT_IPADDRSTR
    soffset = pso_find_keyword( elem, soffset, elem_len, CLIENT_IPADDRSTR_KEYWORD );
    if (soffset == -1)
        return APR_EINVAL;
    soffset = pso_skip_whitespace( elem, soffset+strlen( CLIENT_IPADDRSTR_KEYWORD ), elem_len );
    if (soffset == -1)
        return APR_EINVAL;
    
    eoffset = pso_find_keyword( elem, soffset, elem_len, CRLF );
    if (eoffset == -1)
        return APR_EINVAL;

    char *client_ipaddr_str = pso_copy_tostring( elem, soffset, eoffset, pool );
    
    // CLIENT_PORTSTR
    soffset = pso_find_keyword( elem, soffset, elem_len, CLIENT_PORTSTR_KEYWORD );
    if (soffset == -1)
        return APR_EINVAL;
    soffset = pso_skip_whitespace( elem, soffset+strlen( CLIENT_PORTSTR_KEYWORD ), elem_len );
    if (soffset == -1)
        return APR_EINVAL;
    
    eoffset = pso_find_keyword( elem, soffset, elem_len, CRLF );
    if (eoffset == -1)
        return APR_EINVAL;

    char *client_port_str = pso_copy_tostring( elem, soffset, eoffset, pool );
    int ret = pso_atoN( client_port_str, &client_port, sizeof( client_port ) );
    if (ret == -1)
        return APR_EINVAL;

    // Check LYINGCLIENTS list before considering pledge
    for (LyingClientsVector::iterator i = _lying_clients->begin(); i != _lying_clients->end(); i++)
    {
        const struct ipaddrport *pair = *i;
        if (!strcmp( client_ipaddr_str, pair->ipaddr_str ))
        {
            // Wormtongue is back, thou shalt not pass!
            DPRINTF( DEBUG5, ("Refusing pledge from LYINGCLIENT %s:%s\n", client_ipaddr_str, client_port_str ));
            return APR_SUCCESS;
        }
    }


    // Get pledge
    soffset = pso_find_keyword( elem, 0, elem_len, PLEDGE_START_KEYWORD );
    if (soffset == -1)
    {
        // This is an error in the client's input, blacklist!
        DPIDPRINTF( DEBUG1, ("psoaudit: Error extracting pledge: pledge's start delimiter not found, bad client!\n" ));
        (void)blacklist_client( client_ipaddr_str, client_port, _lying_clients );
        return APR_EINVAL;
    }
    
    eoffset = pso_find_keyword( elem, soffset, elem_len, PLEDGE_END_KEYWORD );
    if (eoffset == -1)
    {
        // This is an error in the client's input, blacklist!
        DPIDPRINTF( DEBUG1, ("psoaudit: Error extracting pledge: pledge's end delimiter not found, bad client!\n" ));
        (void)blacklist_client( client_ipaddr_str, client_port, _lying_clients );
        return APR_EINVAL;
    }
    eoffset += strlen( PLEDGE_END_KEYWORD );

    char *pledge_str = pso_copy_tostring( elem, soffset, eoffset, pool );

    
    /*
     * Check pledge
     */

    Pledge p;

    status = p.unmarshall( pledge_str, strlen( pledge_str ), &error_str, pool );
    if (status != APR_SUCCESS)
    {
        DPIDPRINTF( DEBUG1, ("psoaudit: Error unmarshalling pledge: %s\n", error_str ));
        // This is an error in the client's input, blacklist!
        (void)blacklist_client( client_ipaddr_str, client_port, _lying_clients );
        return status;
    }

    // 1. Find info on slave
    Gstring slaveid_g( p._slave_id );
    SlaveDatabase::iterator i = _slavedb->find( &slaveid_g );
    if (i == _slavedb->end())
    {
        DPIDPRINTF( DEBUG1, ("psoaudit: Slave unknown! %s\n", p._slave_id ));
        return APR_EINVAL;
    }
    SlaveRecord *slaverec = i->second;

    // 2. Find valid digests
#ifndef notQUICK
    char *slave_url_str = apr_pstrcat( pool, "http://", p._slave_id, p._r->unparsed_uri, NULL );
    Url slave_url( pool, slave_url_str );
    slave_url.normalize( pool );
    char *master_uri_path = slave_url(pool); // cast to (char *); 
#else  
    char *master_uri_path = pso_slave_info_to_master_uri_path( p._slave_id, 
                                                        slaverec, 
                                                        p._r->unparsed_uri, 
                                                        _globule_export_path,
#endif  
    digest_t *digest_array=NULL;
    apr_time_t *expiry_array=NULL;
    apr_size_t digest_array_len=0;
    
    find_digests( master_uri_path, p._slave_id, &digest_array, &expiry_array, &digest_array_len, pool );
    
    { 
        int k =0;
        for (k=0; k<digest_array_len; k++)
        {
            DPRINTF( DEBUG1, ("psoaudit: print digest[%d] (%lld, %s)\n", k, expiry_array[k], digest2string( digest_array[k], OPENSSL_DIGEST_ALG_NBITS/8, pool) ));
        }
    }
    
    // 2. See if digest in pledge matches
    status = pso_auditor_check_pledge_against_digests( p,
                                               digest_array, 
                                               expiry_array,
                                               digest_array_len, 
                                               &match,
                                               &error_str, 
                                               pool );
    if (status != APR_SUCCESS)
    {
        DPIDPRINTF( DEBUG1, ("psoaudit: Error checking pledge against MODIFICATIONS: %s\n", error_str ));
        // This is an error in the client's input, blacklist!
        (void)blacklist_client( client_ipaddr_str, client_port, _lying_clients );
        return status;
    }
    else if (match)
    {
        // Pledge legal, continue
        DPIDPRINTF( DEBUG1, ("psoaudit: Pledge matches digest, good slave, good client" ));
        return APR_SUCCESS;
    }
    else
    {
        DPIDPRINTF( DEBUG1, ("psoaudit: Pledge doesn't match" ));
    }
                                               
    // 3. Only where there's no good digest, check signature
    status = pso_client_check_pledge_sig( p, (char *)slaverec->pubkey_pem->c_str(), &match, &error_str, pool );
    if (status != APR_SUCCESS)
    {
        DPIDPRINTF( DEBUG1, ("psoaudit: Error checking sig on pledge: %s\n", error_str ));
        // This is an error in the client's input, blacklist!
        (void)blacklist_client( client_ipaddr_str, client_port, _lying_clients );
        return status;
    }

    if (match)
    {
        // Found an incriminating pledge
        apr_size_t i=0;
        char slave_id[ strlen( p._slave_id )+1 ]; //+1 for string
        strcpy( slave_id, p._slave_id );
        
        DPIDPRINTF( DEBUG1, ("psoaudit: Found incriminating pledge from slave %s!\n", p._slave_id ));
        DPRINTF( DEBUG1, ("Incriminating pledge is %s\n", pledge_str ));
        DPRINTF( DEBUG1, ("Reported by %s:%s\n", client_ipaddr_str, client_port_str ));
        DPRINTF( DEBUG1, ("Slave's pub key is %s\n", slaverec->pubkey_pem->c_str() ));
        for (i=0; i<digest_array_len; i++)
        {
            DPRINTF( DEBUG1, ("Valid digest was (%lld,%s)\n", expiry_array[i], digest2string( digest_array[i], OPENSSL_DIGEST_ALG_NBITS/8, pool ) ));
        }
        
        
        char *ptr = strchr( slave_id, ':' );
        char *port_str = ptr+1;
        *ptr = '\0';
        apr_port_t port=0;
        int ret = pso_atoN( port_str, &port, sizeof( port ) );
        if (ret == -1)
        {
            DPRINTF( DEBUG1, ("Slave ID in pledge not parsable to host:port!" ));
        }
        else
        {
            DPRINTF( DEBUG3, ("Adding to bad slaves list" ));
            try
            {
                _bad_slaves->insert( slave_id, port );
            }
            catch( AprError e )
            {
                nonret_non_fatal( e.getStatus() );
            }
        }
    }
    else
    {
        DPIDPRINTF( DEBUG1, ("psoaudit: Sig on pledge not made with pubkey identified by slave ID in pledge: %s!\n", p._slave_id ));
        // This is an error in the client's input, blacklist!
        (void)blacklist_client( client_ipaddr_str, client_port, _lying_clients );
    }
    
    return APR_SUCCESS;
}




static void find_digests( char *uri_str,
                   char *slave_id,
                   digest_t **digest_array_out, 
                   apr_time_t **expiry_array_out,
                   apr_size_t *digest_array_length_out,
                   apr_pool_t *pool )
{
    DPRINTF( DEBUG1, ("psoaudit: Find digests uri %s slave %s da %p ea %p modif %p\n", uri_str, slave_id, expiry_array_out, digest_array_out, _modifications ));  
    
    // NORMALIZE
    Url uri( pool, uri_str );
    uri.normalize( pool );

    //QUICK Gstring uri_g( uri.path() ); // TODO shm_alloc
    Gstring uri_g( "" ); // TODO shm_alloc
    Versions::iterator i = _modifications->find( &uri_g );
    if (i == _modifications->end())
    {
        // No slaves recorded for this URI
        DPRINTF( DEBUG1, ("psoaudit: No recorded slaves, returning empty list\n" )); 
        *digest_array_length_out = 0;
    }
    else
    {
        Slave2DigestsMap *s2d = i->second;
        DPRINTF( DEBUG1, ("psoaudit: Found %d recorded slaves\n", s2d->size() )); 
        
        Gstring slave_id_g( slave_id ); // TODO shm_alloc
        Slave2DigestsMap::iterator j = s2d->find( &slave_id_g );
        Time2DigestsMap *t2d = j->second;
        if (t2d == NULL)
        {
            // No digests recorded for this slave
            DPRINTF( DEBUG1, ("psoaudit: No digests found for slave %s, returning empty list.\n", slave_id )); 
            *digest_array_length_out = 0;
        }
        else
        {
            DPRINTF( DEBUG1, ("psoaudit: Found %d recorded Digests for slave %s\n", t2d->size(), slave_id )); 

            *expiry_array_out = (apr_time_t *)apr_palloc( pool, t2d->size()*sizeof( apr_time_t ) );
            *digest_array_out = (digest_t *)apr_palloc( pool, t2d->size()*sizeof( digest_t ) );
            apr_size_t count=0;
            for (Time2DigestsMap::iterator i = t2d->begin(); i != t2d->end(); i++)
            {
                DPRINTF( DEBUG5, ("psoaudit: Digest #%d exp is %ld ptr is %p\n", count, i->first, i->second )); 
                ResponseInfo *ri = (ResponseInfo *)i->second; 
                (*expiry_array_out)[count] = i->first;
                (*digest_array_out)[count] = ri->digest;
                count++;
            }
            *digest_array_length_out = count;
        }   
    }
}


static apr_status_t blacklist_client( char *client_ipaddr_str, apr_port_t client_port, LyingClientsVector *lc )
{
    try
    {
        lc->insert( client_ipaddr_str, client_port );
        return APR_SUCCESS;
    }
    catch( AprError e )
    {
        return e.getStatus();
    }
}
