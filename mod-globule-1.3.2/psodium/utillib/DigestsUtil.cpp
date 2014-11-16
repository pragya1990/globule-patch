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
/*
 * WARNING!!!
 * This file must not contain any calls to Apache functions such as 
 * ap_set_content_type(), ap_document_root(), etc. Only pure APR stuff
 * otherwise it won't link with the standalone auditor process.
 */

// To have INT64_C be defined
#define __STDC_CONSTANT_MACROS
#include <stdint.h>

#include "mod_psodium.h"
#include "Versions.hpp"
#include "../globule/alloc/Allocator.hpp"
#include "../globule/netw/Url.hpp"

#include <apr_base64.h>

#ifndef max
#define max(a,b) (((a)>(b))?(a):(b))
#endif


extern "C"
{

static apr_status_t pso_get_value( apr_file_t *file, 
                            char *keyword, 
                            char **valstr_out, 
                            apr_size_t *soff_out, 
                            apr_size_t *kwoff_out,
                            char **error_str_out );

static apr_status_t pso_master_find_digests( psodium_conf *conf, 
                                      char *uri_str, 
                                      char *slave_id,
                                      apr_bool_t RETURNdigests,
                                      apr_bool_t RETIREDdigests,
                                      digest_t **digest_array_out, 
                                      apr_time_t **expiry_array_out, 
                                      apr_size_t *digest_array_length_out, 
                                      apr_pool_t *pool );



/*
 * Implementation
 */

apr_status_t pso_master_record_digest( psodium_conf *conf, 
                                       request_rec *r, 
                                       digest_t md, 
                                       digest_len_t md_len,
                                       apr_off_t rsize,
                                       const char *uri_str,
                                       const char *slave_id, 
                                       const char *expiry_time_str )
{
    apr_time_t et=0L;
    apr_status_t status=APR_SUCCESS;
    
    if (expiry_time_str == NULL || slave_id == NULL)
    {
        DPRINTF( DEBUG1, ("psodium: master: Cannot record digest, r->notes not set correctly.\n" )); 
        return APR_EINVAL;
    }
    else
    {
        int ret = pso_atoN( expiry_time_str, &et, sizeof( et ) );
        if (ret == -1)
        {
            DPRINTF( DEBUG1, ("psodium: master: Cannot record digest, r->notes not set correctly: expiry not number\n" )); 
            return APR_EINVAL;
        }
        DPRINTF( DEBUG3, ("Recording digest with time %s=%lld (inf=%lld=0x%llx)\n", expiry_time_str, et, INFINITE_TIME, INFINITE_TIME ));
    }
    
    DPRINTF( DEBUG1, ("psodium: master: Recording digest for %s %s\n", slave_id, uri_str )); 

    if (md_len != OPENSSL_DIGEST_ALG_NBITS/8)
    {
        DPRINTF( DEBUG1, ("psodium: master: md_len (%d) not ALG BITS (%d)!\n", md_len, OPENSSL_DIGEST_ALG_NBITS/8 ));
        return APR_EINVAL;
    }
    
    // 1. Go to C++ domain
    
    Versions *v = (Versions *)conf->versions;
    rmmmemory* shm_alloc = rmmmemory::shm();

    // 2. Lock VERSIONS
    LOCK( ((AprGlobalMutex *)conf->versions_lock) );

    // CAREFUL! don't do OUTSIDE LOCK!!!
#ifdef PSODIUMDEBUG
    print_Versions( v, r->pool );
#endif  

    // NORMALIZE
    Url uri( r->pool, uri_str );
    uri.normalize( r->pool );

    //QUICK
    const char *from_str = apr_table_get( r->headers_in, "X-From-Replica" );
    Url slave_url( r->pool, from_str );
    slave_url.normalize( r->pool );
  
    // 3. Insert digest into VERSIONS
    //QUICK Gstring uri_g( uri.path() ); // TODO shm_alloc
    Gstring uri_g( "" ); // TODO shm_alloc
    Versions::iterator i = v->find( &uri_g );

    Slave2DigestsMap *s2d = NULL;
    if (i == v->end())
    {
        // No slaves recorded for this URI
        DPRINTF( DEBUG1, ("psodium: master: No recorded slaves, create new s2d\n" )); 
        s2d = new( shm_alloc ) Slave2DigestsMap();
        //QUICK (*v)[ new( shm_alloc ) Gstring( uri.path() ) ] = s2d;
        (*v)[ new( shm_alloc ) Gstring( "" ) ] = s2d;
    }
    else
    {
        s2d = i->second;
        DPRINTF( DEBUG1, ("psodium: master: Found %d recorded slaves\n", s2d->size() )); 
    }
        
    //QUICK Gstring slave_id_g( slave_id ); // TODO shm_alloc
    Gstring slave_id_g( slave_url(r->pool) ); // TODO shm_alloc
    Slave2DigestsMap::iterator j = s2d->find( &slave_id_g );
    Time2DigestsMap *t2d = NULL;
    if (j == s2d->end())
    {
        // No digests recorded for this slave
        // QUIC
        DOUT( DEBUG1, "psodium: master: No digests recorded for slave " << slave_url(r->pool) << " creating new t2d\n" ); 
        t2d = new( shm_alloc ) Time2DigestsMap();
        //QUICK (*s2d)[ new( shm_alloc ) Gstring( slave_id ) ] = t2d;
        (*s2d)[ new( shm_alloc ) Gstring( slave_url(r->pool)) ] = t2d;
    }
    else
    {
        t2d = j->second;
        DPRINTF( DEBUG1, ("psodium: master: Found %d recorded digests for slave %s\n", t2d->size(), slave_id )); 
    }   

    // The key and values in the t2d map must all be in shared mem!
    ResponseInfo *ri = new( shm_alloc ) ResponseInfo();
    ri->digest = new( shm_alloc ) unsigned char[ OPENSSL_DIGEST_ALG_NBITS/8 ];
    memcpy( ri->digest, md, md_len ); 
    ri->size = rsize;
    (*t2d)[ et ] = ri;

#ifdef OLDTEST    
    unsigned char *val1 = new( shm_alloc ) unsigned char[ OPENSSL_DIGEST_ALG_NBITS/8 ];
    strcpy( (char *)val1, "ThisRemixIsDaMelang\0" );
    val2 = new( shm_alloc ) unsigned char[ OPENSSL_DIGEST_ALG_NBITS/8 ];
    strcpy( (char *)val2, "WordsAreStrongerTha\0" );
    
    //(*t2d)[ 10 ] = val1;
    //(*t2d)[ 12 ] = val2;
    t2d->insert( Time2DigestsMap::value_type( 10L, val1) );
    t2d->insert( Time2DigestsMap::value_type( 12L, val2) );
#endif    
    // CAREFUL! don't do OUTSIDE LOCK!!!
#ifdef PSODIUMDEBUG
    DPRINTF( DEBUG1, ("psodium: master: Done recording, versions is:\n" ));
    print_Versions( v, r->pool );
#endif  
    
    // 4. Unlock VERSIONS
    UNLOCK( ((AprGlobalMutex *)conf->versions_lock) );

    return APR_SUCCESS;
}            


apr_status_t pso_master_find_RETURN_digests( psodium_conf *conf, 
                                      char *slave_id, 
                                      char *slave_uri_path, 
                                      digest_t **digest_array_out, 
                                      apr_size_t *digest_array_length_out, 
                                      apr_pool_t *pool )
{
    apr_time_t *expiry_array=NULL;
    SlaveRecord *slaverec=NULL;

    DPRINTF( DEBUG3, ("Looking up RETURN digests for %s%s\n", slave_id, slave_uri_path ));

#ifndef notQUICK
    char *slave_url_str = apr_pstrcat( pool, "http://", slave_id, slave_uri_path, NULL );
    Url slave_url( pool, slave_url_str );
    slave_url.normalize( pool );
    char *master_uri_path = slave_url(pool);;   
#else
    /*
     * Convert slave URI string to master URI, which is the index of Versions
     */
    apr_status_t status = pso_find_slaverec( conf, slave_id, &slaverec, pool );
    if (status != APR_SUCCESS || slaverec == NULL)
    {
        DPRINTF( DEBUG1, ("psodium: master: Cannot translate slave URI to original URI, returning empty list\n" )); 
        *digest_array_out = NULL;
        return status;
    }
    char *master_uri_path = pso_slave_info_to_master_uri_path( slave_id, slaverec, slave_uri_path, conf->master_uri_prefix, pool );
#endif
  
    return pso_master_find_digests( conf, master_uri_path, slave_id, TRUE, FALSE, 
                                      digest_array_out, 
                                      &expiry_array,
                                      digest_array_length_out, 
                                      pool );
}



apr_status_t pso_master_find_RETURN_RETIRED_digests( psodium_conf *conf, 
                                      char *slave_id, 
                                      char *slave_uri_path,
                                      digest_t **digest_array_out, 
                                      apr_time_t **expiry_array_out,
                                      apr_size_t *digest_array_length_out, 
                                      apr_pool_t *pool )
{
    SlaveRecord *slaverec=NULL;

#ifndef notQUICK
    char *slave_url_str = apr_pstrcat( pool, "http://", slave_id, slave_uri_path, NULL );
    Url slave_url( pool, slave_url_str );
    slave_url.normalize(pool);
    char *master_uri_path = slave_url(pool);    
#else  
    /*
     * Convert slave URI string to master URI, which is the index of Versions
     */
    apr_status_t status = pso_find_slaverec( conf, slave_id, &slaverec, pool );
    if (status != APR_SUCCESS || slaverec == NULL)
    {
        DPRINTF( DEBUG1, ("psodium: master: Cannot translate slave URI to original URI, returning empty list\n" )); 
        *digest_array_out = NULL;
        return status;
    }
    char *master_uri_path = pso_slave_info_to_master_uri_path( slave_id, slaverec, slave_uri_path, conf->master_uri_prefix, pool );
#endif
  
    return pso_master_find_digests( conf, master_uri_path, slave_id, TRUE, TRUE, 
                                      digest_array_out,
                                      expiry_array_out,
                                      digest_array_length_out, 
                                      pool );
}





static apr_status_t pso_master_find_digests( psodium_conf *conf, 
                                      char *norm_uri_str, 
                                      char *slave_id,
                                      apr_bool_t RETURNdigests,
                                      apr_bool_t RETIREDdigests,
                                      digest_t **digest_array_out, 
                                      apr_time_t **expiry_array_out,
                                      apr_size_t *digest_array_length_out, 
                                      apr_pool_t *pool )
{
    apr_time_t et=apr_time_sec(0),now;
    apr_status_t status=APR_SUCCESS;
    
    DPRINTF( DEBUG1, ("psodium: master: Finding digest for slave %s, normalized url %s\n", slave_id, norm_uri_str )); 

    *digest_array_length_out = 0;
    
    // 1. Go to C++ domain
    Versions *v = (Versions *)conf->versions;
    rmmmemory* shm_alloc = rmmmemory::shm();

    // 2. Lock VERSIONS
    LOCK( ((AprGlobalMutex *)conf->versions_lock) );

    // CAREFUL! don't do OUTSIDE LOCK!!!
#ifdef PSODIUMDEBUG
    DPRINTF( DEBUG3, ("psodium: master: Current VERSIONS:\n" )); 
    print_Versions( v, pool );
#endif  

    // Ignoring time spent finding the digests
    now = apr_time_now();
    
    // 3. Find digest in VERSIONS
    //QUICK Gstring uri_g( norm_uri_str ); // TODO shm_alloc
    Gstring uri_g( "" ); // TODO shm_alloc
    Versions::iterator i = v->find( &uri_g );
    if (i == v->end())
    {
        // No slaves recorded for this URI
        DPRINTF( DEBUG1, ("psodium: master: No recorded slaves, returning empty list\n" )); 
        *digest_array_out = NULL;
    }
    else
    {
        Slave2DigestsMap *s2d = i->second;
        DPRINTF( DEBUG1, ("psodium: master: Found %d recorded slaves\n", s2d->size() )); 
        
        //QUICKGstring slave_id_g( slave_id ); // TODO shm_alloc
        Gstring slave_id_g( norm_uri_str ); // TODO shm_alloc
        Slave2DigestsMap::iterator j = s2d->find( &slave_id_g );
        
        if (j == s2d->end())
        {
            // No digests recorded for this slave
            //QUICKDPRINTF( DEBUG1, ("psodium: master: No digests found for slave %s, returning empty list.\n", slave_id )); 
            DPRINTF( DEBUG1, ("psodium: master: No digests found for slave %s, returning empty list.\n", norm_uri_str )); 
            *digest_array_out = NULL;
        }
        else
        {
            Time2DigestsMap *t2d = j->second;
            //QUICKDPRINTF( DEBUG1, ("psodium: master: Found %d recorded Digests for slave %s\n", t2d->size(), slave_id )); 
            DPRINTF( DEBUG1, ("psodium: master: Found %d recorded Digests for slave %s\n", t2d->size(), norm_uri_str )); 
            // ALLOCATING TOO MUCH
            *digest_array_out = (digest_t *)apr_palloc( pool, t2d->size()*sizeof( digest_t ) );
            *expiry_array_out = (apr_time_t *)apr_palloc( pool, t2d->size()*sizeof( apr_time_t ) );
            apr_size_t count=0;
            for (Time2DigestsMap::iterator i = t2d->begin(); i != t2d->end(); i++)
            {
                DPRINTF( DEBUG1, ("psodium: master: Digest #%d exp is %lld ptr is %p\n", count, i->first, i->second )); 
                
                apr_time_t expiry = i->first;
                ResponseInfo *ri = (ResponseInfo *)i->second;
                apr_time_t secs = ri->size / (AVG_CS_THROUGHPUT*1024);
                apr_time_t max_cs_content_prop = max( MIN_MAX_CS_CONTENT_PROP, apr_time_from_sec( secs ));

                DPRINTF( DEBUG1, ("psodium: master: Digest has expiry %lld, time now is %lld, max_cs_cp %lld\n", expiry, now, max_cs_content_prop )); 
                
                // Only matching digests
                if (RETURNdigests)
                {
                    // If invalidate or TTL
                    if (expiry == 0 || (expiry + max_cs_content_prop + MAX_CM_GETDIGEST_PROP > now))
                    {
                        // Valid digest
                        DPRINTF( DEBUG1, ("psodium: master: Digest valid max_cs_content_prop %lld\n", max_cs_content_prop )); 
                    }
                    else
                        continue;
                }
                if (RETIREDdigests)
                {
                    // RETIRED list
                    if ((expiry + max_cs_content_prop + MAX_CM_GETDIGEST_PROP 
                        + MAX_CM_GETDIGEST_REPLY_PROP + MAX_CM_BADPLEDGE_PROP) > now)
                    {
                        // Digest on RETIRED list
                        // Valid digest
                        DPRINTF( DEBUG1, ("psodium: master: Digest retired max_cs_content_prop %lld\n", max_cs_content_prop )); 
                    }
                    else
                        continue;
                    
                }
                // ULTRASAFE: i->second is pointer into VERSIONS. Therefore if 
                // we have a cleanup of VERSIONS between this point and the 
                // point where we create the network
                // version of digest_array we may loose data if we free()'s
                // the memory the digest pointers point to as part of the 
                // cleanup.

                digest_t copy = (digest_t )apr_palloc( pool, OPENSSL_DIGEST_ALG_NBITS/8 );
                memcpy( copy, ri->digest, OPENSSL_DIGEST_ALG_NBITS/8 );
                (*digest_array_out)[count] = copy;
                (*expiry_array_out)[count] = expiry;
                count++;
            }
            *digest_array_length_out = count;
            
            { 
                int k =0;
                for (k=0; k<*digest_array_length_out; k++)
                {
                    DPRINTF( DEBUG1, ("psodium: master: Selected digest[%d] %s", k, digest2string( (*digest_array_out)[k], OPENSSL_DIGEST_ALG_NBITS/8, pool) ));
                }
            }
        }   
    }
    
    // 4. Unlock VERSIONS
    UNLOCK( ((AprGlobalMutex *)conf->versions_lock) );

    return APR_SUCCESS;
}            


char *digest2string( digest_t md, digest_len_t md_len, apr_pool_t *pool )
{
    apr_size_t encoded_len=0, encoded_len2=0;
    char *encoded=NULL;
    // BASE64 code works on strings
    encoded_len = apr_base64_encode_len( md_len );
    encoded = (char *)apr_palloc( pool, encoded_len ); 
    encoded_len2 = apr_base64_encode( encoded, (const char *)md, md_len );
    
    return encoded;
}


} // extern "C"


/*----------------------------------------------------------------------------
 * C++ only after this point
 *----------------------------------------------------------------------------
 */


static void
replacePathPrefix(apr_pool_t* pool, Url& uri,
                  const char *old_path_prefix, const char *new_path_prefix)
{
  char* ptr = strstr(uri.path(), old_path_prefix);
  if(ptr != uri.path())
    return;
  char *new_url_str = apr_pstrcat(pool, uri.scheme(), "://",
                                  uri.uri()->hostinfo, new_path_prefix,
                                  ptr + strlen(old_path_prefix), NULL);
  uri = Url(pool, new_url_str);
};

char * pso_slave_info_to_master_uri_path( const char *slave_id,
                                                SlaveRecord *slaverec, 
                                                const char *slave_uri_path,
                                                const char *master_uri_prefix,
                                                apr_pool_t *pool )
{
    const char *bogus_url_str = apr_pstrcat( pool, "http://", slave_id, slave_uri_path, NULL );
    const char *slave_uri_prefix = slaverec->uri_prefix->c_str();

    Url slave_uri( pool, bogus_url_str );
    Url slave_prefix( pool, slave_uri_prefix );
    slave_uri.normalize( pool );
    slave_prefix.normalize( pool );
    replacePathPrefix(pool, slave_uri, slave_prefix.path(), master_uri_prefix);
    char *master_uri_path = apr_pstrdup( pool, slave_uri.path() );
    return master_uri_path;
}




/*
 * pre: Versions locked
 */

void pso_clear_VERSIONS( Versions *v, rmmmemory *shm_alloc )
{
    for (Versions::iterator i = v->begin(); i != v->end(); i++)
    {
        Slave2DigestsMap *s2d = i->second;
        for (Slave2DigestsMap::iterator j = s2d->begin(); j != s2d->end(); j++)
        {
            Time2DigestsMap *t2d = j->second;
            for (Time2DigestsMap::iterator k = t2d->begin(); k != t2d->end(); k++)
            {
                t2d->erase( k );
                // k->first is apr_time_t
                ResponseInfo *ri = (ResponseInfo *)k->second;
                operator delete( ri->digest, shm_alloc );
                operator delete( k->second, shm_alloc ); // g++ overload don't work as expected
            }
            s2d->erase( j );
            operator delete( (void *)j->first, shm_alloc );
            operator delete( j->second, shm_alloc );
        }
        v->erase( i );
        operator delete( (void *)i->first, shm_alloc );
        operator delete( i->second, shm_alloc );
    }
}



/*
 * TODO: size of Response also in marshalled version
 * The counterpart of this code is in MasterRequestHandling.cc
 * Can't be moved here because it contains Apache server calls
 * (i.e. non APR calls)
 */
apr_status_t pso_unmarshall_VERSIONS_from_file( apr_file_t *file, 
                                                Versions *v, 
                                                rmmmemory *shm_alloc,
                                                char **export_path_out,
                                                char **error_str_out )
{
    apr_size_t soffset=0,eoffset=0,oldoff=0,kwoffset=0;
    char *uri_str = NULL, *slaveid_str=NULL, *expdig_str=NULL;
    Slave2DigestsMap *s2d=NULL;
    Time2DigestsMap *t2d=NULL;
    apr_status_t status = APR_SUCCESS;
    
    /*
     * Ugly parser, sorry
     */
    soffset=0;
    status = pso_get_value( file, EXPORT_PATH_KEYWORD, export_path_out, &soffset, &kwoffset, error_str_out );
    if (status != APR_SUCCESS)
    {
        //if (kwoffset == -1)
        //    status = APR_SUCCESS;
        return status;
    }

    soffset = pso_find_file_keyword( file, soffset, VERSIONS_START_KEYWORD, TRUE );
    if (soffset == -1)
    {
        *error_str_out = "No MOD START";
        return APR_EINVAL;
    }

    while( 1 )
    {
        status = pso_get_value( file, REQUEST_URI_KEYWORD, &uri_str, &soffset, &kwoffset, error_str_out );
        if (status != APR_SUCCESS)
        {
            if (kwoffset == -1)
                status = APR_SUCCESS;
            break;
        }
        else
        {
            s2d = new( shm_alloc ) Slave2DigestsMap();
            Gstring *uri_g = new( shm_alloc ) Gstring( uri_str ); // TODO: uri_str via shm_alloc??
            (*v)[ uri_g ] = s2d;
            delete[] uri_str;
        }

        soffset = pso_find_file_keyword( file, soffset, SLAVE2DIG_START_KEYWORD, TRUE );
        if (soffset == -1)
        {
            *error_str_out = "No SLAVE2DIG START";
            return APR_EINVAL;
        }
        soffset += strlen( SLAVE2DIG_START_KEYWORD )+strlen( CRLF );

        oldoff=soffset;
        while( 1 )
        {
            status = pso_get_value( file, SLAVEID_KEYWORD, &slaveid_str, &soffset, &kwoffset, error_str_out );
            if (status != APR_SUCCESS)
            {
                if (kwoffset == -1)
                    status = APR_SUCCESS;
                break;
            }
            else
            {
                DPRINTF( DEBUG5, ("psodium: auditor: SLAVEID soff %d, oldoff %d, kwoff=%d (%s)\n", soffset, oldoff, kwoffset, slaveid_str ));
                if (kwoffset > oldoff)
                {
                    DPRINTF( DEBUG5, ("psodium: auditor: STOP2!\n" ));
                    soffset = oldoff;
                    delete[] slaveid_str;
                    break;
                }
                
                t2d = new( shm_alloc ) Time2DigestsMap();
                Gstring *slaveid_g = new( shm_alloc ) Gstring( slaveid_str ); // TODO: uri_str using shm_alloc
                (*s2d)[ slaveid_g ] = t2d;
                delete[] slaveid_str;
            }

            soffset = pso_find_file_keyword( file, soffset, TIME2DIG_START_KEYWORD, TRUE );
            if (soffset == -1)
            {
                *error_str_out = "No TIME2DIG START";
                return APR_EINVAL;
            }
            soffset += strlen( TIME2DIG_START_KEYWORD )+strlen( CRLF );

            oldoff=soffset;
            while( 1 )
            {
                status = pso_get_value( file, EXPIRYDIGEST_KEYWORD, &expdig_str, &soffset, &kwoffset, error_str_out );
                if (status != APR_SUCCESS)
                {
                    if (kwoffset == -1)
                        status = APR_SUCCESS;
                    break;
                }
                else
                {
                    DPRINTF( DEBUG5, ("psodium: auditor: Expdigest soff %d, oldoff %d, kwoff=%d (%s)\n", soffset, oldoff, kwoffset, expdig_str ));
                    if (kwoffset > oldoff)
                    {
                        DPRINTF( DEBUG5, ("psodium: auditor: STOP!\n" ));
                        soffset = oldoff;
                        delete[] expdig_str;
                        break;
                    }
                    oldoff=soffset;
                    
                    apr_time_t  expiry;
                    char *ptr = NULL;
                    apr_size_t decoded_len=0, decoded_len2;
                    digest_t decoded = NULL;

                    ptr = strchr( expdig_str, ' ' );
                    *ptr = '\0';
                    int ret = pso_atoN( expdig_str, &expiry, sizeof( expiry ) );
                    if (ret == -1)
                    {
                        *error_str_out = "Expiry not number";
                        delete[] expdig_str;
                        return APR_EINVAL;
                    }

                    ptr++;

                    decoded_len = apr_base64_decode_len( ptr ); 
                    decoded = new( shm_alloc ) unsigned char[ decoded_len ]; 
                    decoded_len2 = apr_base64_decode( (char *)decoded, ptr );

                    ResponseInfo *ri = new( shm_alloc ) ResponseInfo();
                    ri->digest = decoded;
                    ri->size = 481;
                    (*t2d)[ expiry ] = ri;
                    delete[] expdig_str;
                }
            }

            soffset = pso_find_file_keyword( file, soffset, TIME2DIG_END_KEYWORD, TRUE );
            if (soffset == -1)
            {
                *error_str_out = "No TIME2DIG END";
                return APR_EINVAL;
            }
            soffset += strlen( TIME2DIG_END_KEYWORD )+strlen( CRLF );
            oldoff=soffset;
        }

        soffset = pso_find_file_keyword( file, soffset, SLAVE2DIG_END_KEYWORD, TRUE );
        if (soffset == -1)
        {
            *error_str_out = "No SLAVE2DIG END";
            return APR_EINVAL;
        }
    }
    soffset = pso_find_file_keyword( file, soffset, VERSIONS_END_KEYWORD, TRUE );
    if (soffset == -1)
    {
        *error_str_out = "No VERSIONS END";
        return APR_EINVAL;
    }

    return status;
}


apr_status_t pso_get_value( apr_file_t *file, 
                            char *keyword, 
                            char **valstr_out, 
                            apr_size_t *soff_out,
                            apr_size_t *kwoff_out,
                            char **error_str_out )
{
    apr_size_t eoffset=0;
    
    *kwoff_out = pso_find_file_keyword( file, *soff_out, keyword, TRUE );
    if (*kwoff_out == -1)
    {
        *error_str_out = "No keyword";
        return APR_EINVAL;
    }
    *soff_out = *kwoff_out + strlen( keyword );
    *soff_out = pso_skip_file_whitespace( file, *soff_out );
    if (*soff_out == -1)
    {
        *error_str_out = "Skip whitespace1";
        return APR_EINVAL;
    }

    eoffset = pso_find_file_keyword( file, *soff_out, CRLF, TRUE );
    if (eoffset == -1)
    {
        *error_str_out = "No CRLF after keyword";
        return APR_EINVAL;
    }

    *valstr_out = pso_copy_file_tostring( file, *soff_out, eoffset );
    *soff_out = eoffset+strlen( CRLF );
    
    return APR_SUCCESS;
}


apr_status_t pso_master_update_expiry( psodium_conf *conf, request_rec *r, char *uri_str, char *time_str, char **error_str_out )
{
    DPRINTF( DEBUG1, ("psodium: master: Changing expiry time of %s to %s\n", uri_str, time_str ));

    apr_time_t expiry_t;
    int ret = pso_atoN( time_str, &expiry_t, sizeof( expiry_t ));
    if (ret == -1)
    {
        *error_str_out = "Time part not number!";
        return APR_EINVAL;
    }

    // 1. Go to C++ domain
    Versions *v = (Versions *)conf->versions;
    rmmmemory *shm_alloc = rmmmemory::shm();

    // 2. Lock VERSIONS
    LOCK( ((AprGlobalMutex *)conf->versions_lock) );

    // CAREFUL! don't do OUTSIDE LOCK!!!
#ifdef PSODIUMDEBUG 
    DPRINTF( DEBUG3, ("psodium: master: Current VERSIONS:\n" )); 
    print_Versions( v, r->pool );
#endif
  
    // 3. Normalize URI
    Url uri( r->pool, uri_str );
    uri.normalize( r->pool );

    // 3. Find digest in VERSIONS
    //QUICK Gstring uri_g( uri.path() ); // TODO shm_alloc
    Gstring uri_g( "" ); // TODO shm_alloc
    Versions::iterator i = v->find( &uri_g );
    if (i == v->end())
    {
        // No slaves recorded for this URI
        *error_str_out = apr_psprintf( r->pool, "No recorded slaves for %s, INTERNAL ERROR\n", uri_str ); 
        char *msg = apr_pstrcat( r->pool, "psodium: master: ", *error_str_out, NULL );
        DPRINTF( DEBUG1, (msg) );
        UNLOCK( ((AprGlobalMutex *)conf->versions_lock) );
        return APR_EINVAL; 
    }
    else
    {
        Slave2DigestsMap *s2d = i->second;
        DPRINTF( DEBUG1, ("psodium: master: Found %d recorded slaves\n", s2d->size() )); 
        
        if (s2d->size() == 0)
        {
            *error_str_out = apr_psprintf( r->pool, "Zero recorded slaves for %s, INTERNAL ERROR\n", i->first ); 
            char *msg = apr_pstrcat( r->pool, "psodium: master: ", *error_str_out, NULL );
            DPRINTF( DEBUG1, (msg ));

            UNLOCK( ((AprGlobalMutex *)conf->versions_lock) );
            return APR_EINVAL; 
        }
#ifndef notQUICK
        Gstring slave_id_g( (char *)uri_str ); // TODO shm_alloc
        Slave2DigestsMap::iterator j = s2d->find( &slave_id_g );
        Time2DigestsMap *t2d = NULL;
        if (j == s2d->end())
        {
            // No digests recorded for this slave
            DPRINTF( DEBUG1, ("psodium: master: No digests recorded for slave %s, INTERNAL ERROR\n", uri_str )); 

            UNLOCK( ((AprGlobalMutex *)conf->versions_lock) );
            return APR_EINVAL; 
        }
        else
        {
            t2d = j->second;
            DPRINTF( DEBUG1, ("psodium: master: Found %d recorded digests for slave %s\n", t2d->size(), uri_str ));
        }
        do
        {
#else
        for (Slave2DigestsMap::iterator j = s2d->begin(); j != s2d->end(); j++)
        {

           Time2DigestsMap *t2d = j->second;
#endif
          DPRINTF( DEBUG1, ("psodium: master: Found %d recorded Digests for slave %s\n", t2d->size(), j->first->c_str() ));

            const apr_time_t inf_time = INFINITE_TIME;
            Time2DigestsMap::iterator k = t2d->find( inf_time );
            if (k == t2d->end())
            {
                DOUT( DEBUG1, "Huh, no entry for infinite time?! INTERNAL ERROR\n" );
                continue;
            }
            // Update expiry time
            (*t2d)[ (const apr_time_t)expiry_t ] = k->second;
            t2d->erase( k );

            // There should just be one entry with INFINITE_TIME, 
            // just check to make sure
            k = t2d->find( inf_time );
            if (k != t2d->end())
            {
                DOUT( DEBUG1, "Huh, two entries for infinite time?! INTERNAL ERROR\n" );
            }
#ifndef notQUICK
        } while( 0 );
#else
        }
#endif
    }


    // CAREFUL! don't do OUTSIDE LOCK!!!
#ifdef PSODIUMDEBUG 
    DPRINTF( DEBUG3, ("psodium: master: Updated VERSIONS:\n" )); 
    print_Versions( v, r->pool );
#endif
  
    UNLOCK( ((AprGlobalMutex *)conf->versions_lock) );
    return APR_SUCCESS;
}


