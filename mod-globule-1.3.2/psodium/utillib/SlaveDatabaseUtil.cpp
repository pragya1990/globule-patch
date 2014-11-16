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

#define APR_WANT_STRFUNC
#define APR_WANT_MEMFUNC
#include "apr_want.h"
#include "apr_general.h"
#include "apr_errno.h"
#include "apr_file_io.h"
#include "http_protocol.h"

#include "SlaveDatabase.hpp"
#include "../globule/apr/AprGlobalMutex.hpp"

extern "C" {

/*
 * Prototypes
 */
static apr_status_t pso_pubkey_not_found_to_bb( request_rec *r, 
                                        const char *slave_id,
                                        apr_bucket_brigade **bb_out );
static apr_status_t pso_actual_pubkey_to_bb( request_rec *r, 
                                       const char *slave_pubkey, 
                                       const char *slave_uri_prefix,
                                       const char *auditor_hostname, 
                                       const apr_port_t auditor_port,
                                       apr_bucket_brigade **bb_out );

static apr_status_t parse_slavekeyfile( char *file_buf, apr_size_t file_len, 
                                        SlaveDatabase *slavedb, 
                                        AprGlobalMutex *slavedb_lock,
                                        rmmmemory *shm_alloc,
                                        char **error_str_out,
                                        apr_pool_t *pool );
static apr_status_t pso_addDB_slave_record( SlaveDatabase *slavedb,
                           AprGlobalMutex *slavedb_lock,
                           rmmmemory *shm_alloc,
                           const char *slave_id, 
                           const char *master_id, 
                           const char *pubkey_pem, 
                           const char *auditor_addr,
                           const char *slave_uri_prefix );


/*
 * Implementation
 */

apr_status_t pso_slaveinfo_to_bb( psodium_conf *conf, 
                                           request_rec *r, 
                                           const char *slave_id,
                                           apr_bucket_brigade **bb_out )
{
    apr_status_t status=APR_SUCCESS;
    SlaveRecord *slaverec=NULL;

    status = pso_find_slaverec( conf, slave_id, &slaverec, r->pool );
    if (status != APR_SUCCESS)
        return status;

    if (slaverec == NULL)
    {
        status = pso_pubkey_not_found_to_bb( r, slave_id, bb_out );
    }
    else
    {
        status = pso_actual_pubkey_to_bb( r, slaverec->pubkey_pem->c_str(), slaverec->uri_prefix->c_str(), conf->auditor_hostname, conf->auditor_port, bb_out );
    }
    return status;
}



static apr_status_t pso_actual_pubkey_to_bb( request_rec *r, 
                                       const char *slave_pubkey, 
                                       const char *slave_uri_prefix,
                                       const char *auditor_hostname, 
                                       const apr_port_t auditor_port,
                                       apr_bucket_brigade **bb_out )
{
    apr_status_t status=APR_SUCCESS;

    DPRINTF( DEBUG1, ("psodium: master: Creating pubkey + auditoraddr reply.\n" )); 

    *bb_out = apr_brigade_create(r->pool, r->connection->bucket_alloc );
    if (*bb_out == NULL)
        return APR_ENOMEM;
    
    status = apr_brigade_printf( *bb_out, NULL, NULL, "%s %s%s%s %s:%hu%s", 
                                SLAVE_URI_PREFIX_KEYWORD, slave_uri_prefix, CRLF,
                                AUDITOR_ADDR_KEYWORD, auditor_hostname, auditor_port, CRLF );
    if (status != APR_SUCCESS)
        return status;
    status = apr_brigade_puts( *bb_out, NULL, NULL, slave_pubkey );
    if (status != APR_SUCCESS)
        return status;
    status = apr_brigade_puts( *bb_out, NULL, NULL, CRLF );

    return status;
}



static apr_status_t pso_pubkey_not_found_to_bb( request_rec *r, 
                                        const char *slave_id,
                                        apr_bucket_brigade **bb_out ) 
{
    apr_status_t status=APR_SUCCESS;

    DPRINTF( DEBUG1, ("psodium: master: Creating pubkey NOTFOUND reply.\n" )); 

    *bb_out = apr_brigade_create(r->pool, r->connection->bucket_alloc );
    if (*bb_out == NULL)
        return APR_ENOMEM;
    
    status = apr_brigade_printf( *bb_out, NULL, NULL, 
                                "NOTFOUND A public key for %s was not found.%s", 
                                slave_id, CRLF );
    return status;
}




apr_status_t pso_add_slave_record( psodium_conf *conf,
                           const char *slave_id, 
                           const char *master_id, 
                           const char *pubkey_pem, 
                           const char *auditor_addr,
                           const char *slave_uri_prefix )
{
    // Got to C++ domain
    SlaveDatabase *slavedb = (SlaveDatabase *)conf->slavedatabase;
    rmmmemory *shm_alloc = rmmmemory::shm();

    
    return pso_addDB_slave_record( slavedb, (AprGlobalMutex *)conf->slavedb_lock, shm_alloc,
                                slave_id, master_id, pubkey_pem,
                                auditor_addr, slave_uri_prefix );
}


/* Called by C code */
apr_status_t pso_find_pubkey( psodium_conf *conf, const char *slave_id, char **pubkey_out, apr_pool_t *pool )

{
    apr_status_t status=APR_SUCCESS;
    SlaveRecord *slaverec=NULL;

    status = pso_find_slaverec( conf, slave_id, &slaverec, pool );
    if (status != APR_SUCCESS)
        return status;

    if (slaverec == NULL)
    {
        *pubkey_out = NULL;
    }
    else
    {
        // BUG: leaking memory???
        *pubkey_out = apr_pstrcat( pool, slaverec->pubkey_pem->c_str(), NULL ); 
    }
    return APR_SUCCESS;
}


apr_status_t pso_find_slaverec( psodium_conf *conf, 
                                const char *slave_id, 
                                SlaveRecord **slaverec_out, 
                                apr_pool_t *pool )
{
    apr_status_t status=APR_SUCCESS;
    
    // 1. Go to C++ domain
    SlaveDatabase *slavedb = (SlaveDatabase *)conf->slavedatabase;
    rmmmemory *shm_alloc = rmmmemory::shm();

    // 2. Lock slavedb
    LOCK( ((AprGlobalMutex *)conf->slavedb_lock) );


    DPRINTF( DEBUG1, ("pso_find_slaverec: keysfn %s, slavedb %p, shm_alloc %p, slave_id .%s.\n", conf->slavekeysfilename, conf->slavedatabase, shm_alloc, slave_id )); 
    print_SlaveDatabase( slavedb, pool );

    Gstring g( slave_id ); // TODO shm_alloc
    SlaveDatabase::iterator i = slavedb->find( &g );
//    DPRINTF( DEBUG1, ("pso_find_slaverec: first = %p, second = %p\n", i->first, i->second )); 

    *slaverec_out = NULL;
    if (i != slavedb->end())
        *slaverec_out = i->second;

    // 4. Unlock slavedb
    /* BUG: if somebody updates slaverecord this lock may not cover all concurrency */
    UNLOCK( ((AprGlobalMutex *)conf->slavedb_lock) );

    return APR_SUCCESS;
}



apr_status_t pso_find_master_id( psodium_conf *conf, const char *slave_id, char **master_id_out, apr_pool_t *pool )
{
    apr_status_t status=APR_SUCCESS;

    // 1. Go to C++ domain
    SlaveDatabase *slavedb = (SlaveDatabase *)conf->slavedatabase;
    rmmmemory *shm_alloc = rmmmemory::shm();

    // 2. Lock slavedb
    LOCK( ((AprGlobalMutex *)conf->slavedb_lock) );

    Gstring g( slave_id ); // TODO shm_alloc
    SlaveDatabase::iterator i = slavedb->find( &g );

    DPRINTF( DEBUG5, ("psodium: any: Searching SlaveDatabase: first = %p, second = %p\n", i->first, i->second )); 

    SlaveRecord *slaverec = NULL;
    if (i != slavedb->end())
        slaverec = i->second;

    // 4. Unlock slavedb
    /* BUG: if somebody updates slaverecord this lock may not cover all concurrency */
    UNLOCK( ((AprGlobalMutex *)conf->slavedb_lock) );

    if (slaverec == NULL)
    {
        *master_id_out = NULL;
    }
    else
    {
        *master_id_out = apr_pstrcat( pool, slaverec->master_id->c_str(), NULL ); // BUG: leaking memory???
    }
    return APR_SUCCESS;
}


apr_status_t pso_master_read_conf_slavekeys( psodium_conf *conf, 
                                             apr_pool_t *pool )
{
    // Go to C++ domain
    SlaveDatabase *slavedb = (SlaveDatabase *)conf->slavedatabase;
    rmmmemory *shm_alloc = rmmmemory::shm();
    return pso_master_read_slavekeys( conf->slavekeysfilename, slavedb, 
                                      (AprGlobalMutex *)conf->slavedb_lock, shm_alloc, pool );
}


} // extern "C"





/*
 * C++ only after this point
 */

apr_status_t pso_master_read_slavekeys( const char *slavekeysfilename, 
                                        SlaveDatabase *slavedb, 
                                        AprGlobalMutex *slavedb_lock,
                                        rmmmemory *shm_alloc,
                                        apr_pool_t *pool )
{
    apr_status_t status=APR_SUCCESS;
    apr_finfo_t file_info_s, *file_info=&file_info_s;
    apr_file_t  *file = NULL;
    char status_str[ PSO_STATUS_STR_LEN ], *error_str=".";
    char *file_buf=NULL;
    apr_size_t nbytes=PSO_FILE_BLOCK_SIZE;

    DPRINTF( DEBUG1, ("psodium: master: Reading slave keys: from %s\n", slavekeysfilename ));
    
    // SCALE: To simplify things I read the whole file at once 
    status = apr_stat( file_info, slavekeysfilename, APR_FINFO_SIZE, pool );
    if (status == APR_SUCCESS)
    {
        file_buf = (char *)apr_palloc( pool, file_info->size );
        if (file_buf == NULL)
        {
            status = APR_ENOMEM;
        }
        else
        {
            DPRINTF( DEBUG5, ("psodium: master: Reading slave keys: length is %d\n", file_info->size ));

            status = apr_file_open( &file, slavekeysfilename, APR_READ, 
                                    APR_OS_DEFAULT, pool );
            if (status == APR_SUCCESS)
            {
                apr_size_t offset=0;
                for( offset=0; offset<file_info->size; offset+=nbytes )
                {
                    DPRINTF( DEBUG5, ("psodium: master: Reading slave keys: reading??\n" ));

                    status = apr_file_read( file, file_buf+offset, &nbytes );
                    if (status != APR_SUCCESS)
                        break;
                }

                DPRINTF( DEBUG5, ("psodium: master: Reading slave keys: Done reading??\n" ));

                if (status == APR_SUCCESS)
                {
                    status = parse_slavekeyfile( file_buf, file_info->size, 
                    slavedb, slavedb_lock, shm_alloc,
                    &error_str, pool );
                }
            }
        }
    }

    if (status != APR_SUCCESS)
    {
        (void)apr_strerror( status, status_str, PSO_STATUS_STR_LEN );
        DPRINTF( DEBUG0, ("psodium: master: Reading slave keys: Error reading file: ", status_str, ":", error_str ));
        return APR_EINVAL;
    }
    return APR_SUCCESS;
}
    



/* 
 * Parse slave keys file
 * @param file_buf  file contents
 * @param file_len  length of file
 * @param slavekeys table with slave keys
 * @param error_str_out  OUT parameter for an error string
 * @param pool  pool to use
 * @return status
 *
 @remark
 * Format:
 * Importer http://<slave id><import path>
 * -----BEGIN CERTIFICATE----
 * <pem encoded cert>
 * -----END CERTFIICATE----
 * Importer http://<slave id><import path>
 * etc.
 */
#define ID_KEYWORD  "Importer"

static apr_status_t parse_slavekeyfile( char *file_buf, apr_size_t file_len, 
                                 SlaveDatabase *slavedb,
                                 AprGlobalMutex *slavedb_lock,
                                 rmmmemory *shm_alloc,
                                 char **error_str_out,
                                 apr_pool_t *pool )
{
    const char *slave_id=NULL, *slave_uri_prefix=NULL;
    apr_size_t offset=0,toff=0;
    char *eol=NULL, *line=NULL;
    char **line_argv;
    apr_status_t    status=APR_SUCCESS;

    DPRINTF( DEBUG5, ("psodium: master: Reading slave keys: parsing first find_kw CRLF\n" ));

    // OS compatibility: sniff what the end-of-line is: LF, CRLF, CR (Macintosh)
    toff = pso_find_keyword( file_buf, 0, file_len, CRLF );
    if (toff == -1)
    {
        // Unix | Mac
        char lf[2] = { APR_ASCII_LF, '\0' };
        toff = pso_find_keyword( file_buf, 0, file_len, lf );
        if (toff == -1)
        {
            // Mac
            lf[0]=APR_ASCII_CR;
        }
        eol = lf;
    }
    else
        eol = CRLF;

DPRINTF( DEBUG5, ("psodium: master: Reading slave keys: parse: strlen eol is %d %d", strlen( eol ), eol[0]  ));


    // Parse file
    while( 1 )
    {
        char *pubkey = NULL;
DPRINTF( DEBUG5, ("psodium: master: Reading slave keys: parse: first skip white space\n" ));

        offset = pso_skip_whitespace( file_buf, offset, file_len );
        if (offset == -1)
        {
            // EXCLUSIVE ACCESS, no need for mutex
            if (slavedb->size()==0)
            {
                status = APR_EINVAL;
                *error_str_out = "File empty";
            }
            else
            {
                // No more data in file
                status = APR_SUCCESS;
            }
            break;
        }

DPRINTF( DEBUG5, ("psodium: master: Reading slave keys: parse: find eol from non white space\n" ));

        toff = pso_find_keyword( file_buf, offset, file_len, eol );
        if (toff == -1)
        {
            status = APR_EINVAL;
            *error_str_out = "File empty";
            break;
        }
        line = pso_copy_tostring( file_buf, offset, toff, pool );

DPRINTF( DEBUG5, ("psodium: master: Reading slave keys: parse: got line <%s>\n", line ));

        status = apr_tokenize_to_argv( line, &line_argv, pool );
        if (status != APR_SUCCESS)
        {
            *error_str_out = "Cannot tokenize ID line";
            break;
        }
        if (!strcasecmp( line_argv[0], ID_KEYWORD ))
        {
            if (line_argv[1] == NULL)
            {
                status = APR_EINVAL;
                *error_str_out = "ID line misses value";
                break;
            }

            apr_uri_t uri;
            status = apr_uri_parse( pool, line_argv[1], &uri );
            slave_id = uri.hostinfo; // slave ID
            slave_uri_prefix = line_argv[1];
            if (slave_uri_prefix[ strlen( slave_uri_prefix )-1 ] != '/')
                slave_uri_prefix = apr_pstrcat( pool, slave_uri_prefix, "/", NULL );

DPRINTF( DEBUG5, ("psodium: master: Reading slave keys: parse: got id <%s>\n", slave_id ));

        }
        offset = toff;
        offset = pso_find_keyword( file_buf, offset, file_len, KEY_START_KEYWORD );
        if (offset == -1)
        {
            status = APR_EINVAL;
            *error_str_out = "ID line not followed by certificate, BEGIN missing";
            break;
        }

DPRINTF( DEBUG5, ("psodium: master: Reading slave keys: parse: got start cert >\n" ));

        toff = pso_find_keyword( file_buf, offset, file_len, KEY_END_KEYWORD );
        if (toff == -1)
        {
            status = APR_EINVAL;
            *error_str_out = "BEGIN CERTIFICATE not followed by END";
            break;
        }

DPRINTF( DEBUG5, ("psodium: master: Reading slave keys: parse: got end cert >\n" ));

        pubkey = pso_copy_tostring( file_buf, offset, toff+strlen( KEY_END_KEYWORD), pool );

        // Add record to SlaveDatabase
        pso_addDB_slave_record( slavedb, slavedb_lock, shm_alloc, slave_id, "EmptyMasterID", pubkey, "EmptyAuditorAddr", slave_uri_prefix );
        offset = toff+strlen( KEY_END_KEYWORD );

        DPRINTF( DEBUG1, ("psodium: master: Reading slave keys: Registering slave's cert under ID %s\nSlave's certificate is #%s#\n", slave_id, pubkey ));
        
    }
  
    DPRINTF( DEBUG5, ("psodium: master: Reading slave keys: Final SlaveDatabase is:\n" ));
    // EXCLUSIVE access, no need for mutex
    // print_SlaveDatabase( slavedb, pool );
    
    return status;
}


static apr_status_t pso_addDB_slave_record( SlaveDatabase *slavedb,
                           AprGlobalMutex *slavedb_lock,
                           rmmmemory *shm_alloc,
                           const char *slave_id, 
                           const char *master_id, 
                           const char *pubkey_pem, 
                           const char *auditor_addr,
                           const char *slave_uri_prefix )
{
    apr_status_t status=APR_SUCCESS;
    
    DPRINTF( DEBUG5, ("psodium: master: Add slave rec: db %p, lock %p, aa %p, sid %p, mid %p, pk %p, aa %p\n", 
    slavedb, slavedb_lock, shm_alloc, slave_id, master_id, pubkey_pem, auditor_addr ));
    
    
    // 2. Lock slavedb
    if (slavedb_lock != NULL)
    {
        LOCK( ((AprGlobalMutex *)slavedb_lock) );
    }
    
    DPRINTF( DEBUG5, ("psodium: master: Add slave: past lock \n" )); 
    
    char *bla = new( shm_alloc )char;
    
    SlaveRecord *slaverec = new( shm_alloc ) SlaveRecord( new( shm_alloc ) Gstring( master_id ), 
                                            new( shm_alloc ) Gstring( pubkey_pem ), 
                                            new( shm_alloc ) Gstring( auditor_addr ),
                                            new( shm_alloc ) Gstring( slave_uri_prefix ) );
    const Gstring *slave_id_g = new( shm_alloc ) Gstring( slave_id );

    // 3. Insert
    (*slavedb)[ slave_id_g ] = slaverec;

    // 4. Unlock slavedb
    if (slavedb_lock != NULL)
    {
        UNLOCK( ((AprGlobalMutex *)slavedb_lock) );
    }
    return APR_SUCCESS;
}

