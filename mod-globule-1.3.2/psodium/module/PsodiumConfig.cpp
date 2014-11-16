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


/* ========================================================================= 
 * Functions for handling config parameters. Called from mod_psodium.c
 * i.e. C code.
 * =========================================================================
 */
#include "mod_psodium.h"

#include "../globule/utilities.h"
#include "../globule/resource/BaseHandler.hpp"
#include "utillib/Versions.hpp"
#include "utillib/SlaveDatabase.hpp"
#include "utillib/LyingClientsVector.hpp"
#include "utillib/BadSlavesVector.hpp"
#include <openssl/err.h>

static apr_status_t post_config_create_master_datastructures( psodium_conf *conf, 
                                                rmmmemory *shm_alloc, 
                                                apr_pool_t *conf_p );

static apr_status_t post_config_create_client_datastructures( psodium_conf *conf, 
                                                rmmmemory *shm_alloc, 
                                                apr_pool_t *conf_p );

extern "C" {

static apr_status_t pso_stop_parent_server(void *c);
static apr_status_t pso_stop_child_server( void *c );



AP_DECLARE_DATA ap_filter_rec_t *pso_output_filter_handle;


const char *pso_set_slavekeysfile( cmd_parms *parms, void *dummy, const char *arg)
{
    psodium_conf *conf = NULL;
    conf = (psodium_conf *)ap_get_module_config( parms->server->module_config, &psodium_module);
    if (arg[0] != '/')
    {
        conf->slavekeysfilename = apr_pstrcat( parms->pool, ap_server_root, "/", arg, NULL );
    }
    else
        conf->slavekeysfilename = (char *)arg;
    conf->slavekeysfilename_set = 1;
    conf->role |= PSODIUM_MASTER_ROLE;
    return NULL;
}


const char *pso_set_auditoraddr( cmd_parms *parms, void *dummy, 
                                 const char *arg1, const char *arg2 )
{
    psodium_conf *conf = NULL;
    conf = (psodium_conf *)ap_get_module_config( parms->server->module_config, &psodium_module);

    char *hostport_str = apr_pstrdup( parms->pool, arg1 );
    char *ptr = strchr( hostport_str, ':' );
    if (ptr == NULL)
    {
        return "No port in auditor address!";
    }
    char *port_str = ptr+1;
    *ptr = '\0';
    char *host_str=hostport_str;
    apr_port_t port=0;
    int ret = pso_atoN( port_str, &port, sizeof( port ) );
    if (ret == -1)
        return "Port number is not a number";
    conf->auditor_hostname = host_str;
    conf->auditor_port = port;

    conf->auditor_passwd = (char *)arg2;
    conf->auditoraddr_set = 1;
    conf->role |= PSODIUM_MASTER_ROLE;
    return NULL;
}


const char *pso_set_tempstoragedir( cmd_parms *parms, void *dummy, const char *arg)
{
    psodium_conf *conf = NULL;
    conf = (psodium_conf *)ap_get_module_config( parms->server->module_config, &psodium_module);
    if (arg[0] != '/')
    {
        conf->tempstoragedirname = apr_pstrcat( parms->pool, ap_server_root, "/", arg, NULL );
    }
    else
        conf->tempstoragedirname = (char *)arg;
    conf->tempstoragedirname_set = 1;
    return NULL;
}


const char *pso_set_slaveid( cmd_parms *parms, void *dummy, const char *arg)
{
    psodium_conf *conf = NULL;
    conf = (psodium_conf *)ap_get_module_config( parms->server->module_config, &psodium_module);
    char *ptr = strchr( arg, ':' );
    if (ptr == NULL)
    {
        return "No port in slaveID (=hostname:port)!";
    }
    conf->slaveid = (char *)arg;
    conf->slaveid_set = 1;
    conf->role |= PSODIUM_SLAVE_ROLE;
    return NULL;
}


const char *pso_set_slaveprivatekeyfilename( cmd_parms *parms, void *dummy, const char *arg)
{
    psodium_conf *conf = NULL;
    conf = (psodium_conf *)ap_get_module_config( parms->server->module_config, &psodium_module);
    if (arg[0] != '/')
    {
        conf->slaveprivatekeyfilename = apr_pstrcat( parms->pool, ap_server_root, "/", arg, NULL );
    }
    else
        conf->slaveprivatekeyfilename = (char *)arg;
    conf->slaveprivatekeyfilename_set = 1;
    conf->role |= PSODIUM_SLAVE_ROLE;
    return NULL;
}



const char * pso_set_doublecheckprob( cmd_parms *parms, 
                                      void *dummy, 
                                      const char *arg)
{
    psodium_conf *conf = NULL;
    conf = (psodium_conf *)ap_get_module_config( parms->server->module_config, &psodium_module);
    conf->doublecheckprob_str= (char *)arg;
    conf->doublecheckprob_set = 1;
    conf->role |= PSODIUM_CLIENT_ROLE;
    return NULL;
}

const char *pso_set_badcontent_img( cmd_parms *parms, void *dummy, 
                                 const char *arg1, const char *arg2 )
{
    psodium_conf *conf = NULL;
    conf = (psodium_conf *)ap_get_module_config( parms->server->module_config, &psodium_module);
    if (arg1[0] != '/')
    {
        conf->badcontent_img_filename = apr_pstrcat( parms->pool, ap_server_root, "/", arg1, NULL );
    }
    else
        conf->badcontent_img_filename = (char *)arg1;
    conf->badcontent_img_mimetype = (char *)arg2;
    conf->badcontent_img_set = 1;
    return NULL;
}


const char *pso_set_badslave_img( cmd_parms *parms, void *dummy, 
                                 const char *arg1, const char *arg2 )
{
    psodium_conf *conf = NULL;
    conf = (psodium_conf *)ap_get_module_config( parms->server->module_config, &psodium_module);
    if (arg1[0] != '/')
    {
        conf->badslave_img_filename = apr_pstrcat( parms->pool, ap_server_root, "/", arg1, NULL );
    }
    else
        conf->badslave_img_filename = (char *)arg1;
    conf->badslave_img_mimetype = (char *)arg2;
    conf->badslave_img_set = 1;
    return NULL;
}


const char *pso_set_clientlied_img( cmd_parms *parms, void *dummy, 
                                 const char *arg1, const char *arg2 )
{
    psodium_conf *conf = NULL;
    conf = (psodium_conf *)ap_get_module_config( parms->server->module_config, &psodium_module);
    if (arg1[0] != '/')
    {
        conf->clientlied_img_filename = apr_pstrcat( parms->pool, ap_server_root, "/", arg1, NULL );
    }
    else
        conf->clientlied_img_filename = (char *)arg1;
    conf->clientlied_img_mimetype = (char *)arg2;
    conf->clientlied_img_set = 1;
    return NULL;
}



const char *pso_set_intraserverauthpwd( cmd_parms *parms, void *dummy, const char *arg)
{
    psodium_conf *conf = NULL;
    conf = (psodium_conf *)ap_get_module_config( parms->server->module_config, &psodium_module);
    conf->intraserverauthpwd = (char *)arg;
    conf->intraserverauthpwd_set= 1;
    return NULL;
}


const char *pso_set_badslavesfilename( cmd_parms *parms, void *dummy, const char *arg)
{
    psodium_conf *conf = NULL;
    conf = (psodium_conf *)ap_get_module_config( parms->server->module_config, &psodium_module);
    if (arg[0] != '/')
    {
        conf->badslavesfilename = apr_pstrcat( parms->pool, ap_server_root, "/", arg, NULL );
    }
    else
        conf->badslavesfilename = (char *)arg;
    conf->badslavesfilename_set = 1;
    return NULL;
}


const char *pso_set_lyingclientsfilename( cmd_parms *parms, void *dummy, const char *arg)
{
    psodium_conf *conf = NULL;
    conf = (psodium_conf *)ap_get_module_config( parms->server->module_config, &psodium_module);
    if (arg[0] != '/')
    {
        conf->lyingclientsfilename = apr_pstrcat( parms->pool, ap_server_root, "/", arg, NULL );
    }
    else
        conf->lyingclientsfilename= (char *)arg;
    conf->lyingclientsfilename_set = 1;
    conf->role |= PSODIUM_MASTER_ROLE;
    return NULL;
}



const char *pso_set_versionsfilename( cmd_parms *parms, void *dummy, const char *arg)
{
    psodium_conf *conf = NULL;
    conf = (psodium_conf *)ap_get_module_config( parms->server->module_config, &psodium_module);
    if (arg[0] != '/')
    {
        conf->versionsfilename= apr_pstrcat( parms->pool, ap_server_root, "/", arg, NULL );
    }
    else
        conf->versionsfilename= (char *)arg;
    conf->versionsfilename_set = 1;
    conf->role |= PSODIUM_MASTER_ROLE;
    return NULL;
}


void pso_add_output_filter_to_request( request_rec *r )
{
    psodium_conf *conf = NULL;
    // OPENSSL
    EVP_MD_CTX  *md_ctxp;

    conf = (psodium_conf *)ap_get_module_config( r->server->module_config, &psodium_module);
    
    /* Allow one pSodium to be both CLIENT and MASTER/SLAVE */
    if (!(conf->role & PSODIUM_CLIENT_ROLE && r->proxyreq))
    {
        DPRINTF( DEBUG0, ("psodium: any: Registering pSodium as OUTPUT FILTER!\n" ));

        /* Enable output filter
         * BUG: can we call this multiple times for the same connection, or would
         * that cause a list of filters (i.e. think of persistent connections
         * that handle multiple requests).
         *
         * For each request we pass a new context that is a OpenSSL message digest 
         * context.
         *
         * OPENSSL
         * Must be allocated from pool, not stack, otherwise we get problems in
         * slave mode where this call returns (Apache core/Globule executes actual 
         * request) and the output filter will then use a pointer to the MD_CTX
         * that is no longer valid (stack is gone).
         */
        md_ctxp = (EVP_MD_CTX *)apr_palloc( r->pool, sizeof( EVP_MD_CTX ) );
        DPRINTF( DEBUG4, ("psodium: any: initial md_ctx is %p\n", md_ctxp )); 
        EVP_MD_CTX_init( md_ctxp ); // superfluous?
        EVP_DigestInit( md_ctxp, OPENSSL_DIGEST_ALG );
        ap_add_output_filter_handle( pso_output_filter_handle, md_ctxp, r, r->connection );
    }
}


/*
 * Post Config
 */
int pso_post_config(apr_pool_t *conf_p, apr_pool_t *log_p,
                             apr_pool_t *temp_p, server_rec *s)
{
    static int callcount=0;
    psodium_conf *conf = NULL;
    int access_status=OK;
    apr_status_t status=APR_SUCCESS;
    void *flag;
    const char *userdata_key = "mod_psodium_init";
    char *error_str=NULL;

    /* 
     * Apache 2.0 reads the config file twice, once to test if it's OK and get
     * info on which log files to use, and once more to do the actual config.
     * Hence, this code is executed twice. 
     *
     * For the shared memory stuff this is not good. Therefore we do the work
     * only the second time. The below code seems the default way of waiting
     * for the second run.
     */
    apr_pool_userdata_get( &flag, userdata_key, s->process->pool);
    if (!flag) 
    {
        apr_pool_userdata_set( (const void *)1, userdata_key,
                               apr_pool_cleanup_null, s->process->pool);
        return OK;
    }
    else
        DPRINTF( DEBUG1, ("psodium: any: Post config called for second time, NOW doing shm stuff.\n" )); 

    DPRINTF( DEBUG5, ("psodium: any: Post config: conf_p is %p, log_p is %p, temp_p is %p\n", conf_p, log_p, temp_p )); 
    
    conf = (psodium_conf *)ap_get_module_config( s->module_config, &psodium_module);

    //
    // 1. General stuff
    // 
    
    /*
     * The magical world of C++, shared memory and Standard Template Library
     * (STL) all combined!
     */
    DPRINTF( DEBUG1, ("psodium: any: Post config: create pool\n" )); 
    
    // OPENSSL
    ERR_load_crypto_strings();

    DPRINTF( DEBUG0, ("psodium: any: Post config: Role is %d\n", conf->role ));
    if (!conf->tempstoragedirname_set)
    {
        conf->tempstoragedirname = apr_pstrcat( conf_p, ap_server_root, "/psodium-temp", NULL ); 
    }
    status = apr_dir_make_recursive( conf->tempstoragedirname, APR_OS_DEFAULT, conf_p );
    if (status != APR_SUCCESS && !APR_STATUS_IS_EEXIST( status ))
    {
        DOUT( DEBUG0,"Error creating temp storage directory " << conf->tempstoragedirname );
        return HTTP_SERVICE_UNAVAILABLE;  
    }
    else
    {
        // When started as root and changing to another user for regular operation
        // make sure the cachedir is writable by that user.
        status = BaseHandler::chownToCurrentUser( conf_p, conf->tempstoragedirname );
        if (status != APR_SUCCESS)
        {
            DOUT( DEBUG0, "Cannot change owner of temp storage directory " << conf->tempstoragedirname );
            return HTTP_SERVICE_UNAVAILABLE;  
        }
    }

    if (!conf->badslavesfilename_set)
    {
        conf->badslavesfilename = apr_pstrcat( conf_p, conf->tempstoragedirname, "/badslaves.txt", NULL );
    }

    if (!conf->lyingclientsfilename_set)
    {
        conf->lyingclientsfilename = apr_pstrcat( conf_p, conf->tempstoragedirname, "/lyingclients.txt", NULL );
    }

    if (!conf->versionsfilename_set)
    {
        conf->versionsfilename = apr_pstrcat( conf_p, conf->tempstoragedirname, "/versions.txt", NULL );
    }
    
    //
    // 2. Client and Master stuff
    //
    rmmmemory *shm_alloc=NULL;
    if ((conf->role & PSODIUM_MASTER_ROLE) || (conf->role & PSODIUM_CLIENT_ROLE))
    {
        DPRINTF( DEBUG5, ("psodium: client/master: Post config: Create slab of shared mem\n" )); 

        rmmmemory::shm(conf_p, (apr_size_t)2*1024*1024);

        DPRINTF( DEBUG5, ("psodium: client/master: Post config: Save pointers\n" )); 
    }


    //
    // 3. Client stuff
    // 
    if (conf->role & PSODIUM_CLIENT_ROLE)
    {
        status = post_config_create_client_datastructures( conf, shm_alloc, conf_p );
        if (status != APR_SUCCESS)
            return HTTP_SERVICE_UNAVAILABLE;

        if (conf->doublecheckprob_set)
        {
            int nitems = sscanf( conf->doublecheckprob_str, "%f", &conf->doublecheckprob );
            if (nitems == 0 || conf->doublecheckprob < 0.0 || conf->doublecheckprob > 100.0)
            {
                DPRINTF( DEBUG0, ("psodium: client: PsodiumClientDoubleCheckProbability not a number 0-100.\n" ));
                return HTTP_SERVICE_UNAVAILABLE;
            }
            conf->doublecheckprob /= 100.0; // percentage -> [0,1] interval
        }
    }


    //
    // 4. Master stuff
    //
    if (conf->role & PSODIUM_MASTER_ROLE)
    {
        // a.
        status = post_config_create_master_datastructures( conf, shm_alloc, conf_p );
        if (status != APR_SUCCESS)
            return HTTP_SERVICE_UNAVAILABLE;

        // b.
        if (!conf->slavekeysfilename_set)
        {
            DPRINTF( DEBUG0, ("psodium: master: No value set for PsodiumMasterSlaveKeys and there is no default.\n" ));
            return HTTP_SERVICE_UNAVAILABLE;

        }
        else
        {
            DPRINTF( DEBUG5, ("psodium: master: Before read slavekeys\n" )); 

            status = pso_master_read_conf_slavekeys( conf, conf_p ); // just guessing at which pool to use
            if (status != APR_SUCCESS)
                return HTTP_SERVICE_UNAVAILABLE;  
        }
    }
        

    //
    // 5. Slave stuff
    //
    if (conf->role & PSODIUM_SLAVE_ROLE)
    {
        if (!conf->slaveprivatekeyfilename_set)
        {
            DPRINTF( DEBUG0, ("No filename set for slave's private key and there's no default.\n" ));
            return HTTP_SERVICE_UNAVAILABLE;
        }
        else
        {
            DPRINTF( DEBUG5, ("psodium: slave: Before reading slave private key\n" ));
            status = pso_slave_read_privatekey( conf, conf_p, &error_str ); // just guessing at which pool to use
            if (status != APR_SUCCESS)
            {
                DPRINTF( DEBUG0, ("Could not read slave's private key from %s: %s\n", conf->slaveprivatekeyfilename, error_str )); 
                return HTTP_SERVICE_UNAVAILABLE;  
            }
            DPRINTF( DEBUG5, ("psodium: slave: After reading slave private\n" )); 
        }
    }

    // According to Apache Modelling project $4.6.2, the config pool exists
    // until restart.
    conf->conf_pool = conf_p;
    apr_pool_cleanup_register(conf_p,conf,pso_stop_parent_server,pso_stop_child_server);
    
    return OK;
}


void pso_child_init( apr_pool_t *child_pool, server_rec *s )
{
    DOUT( DEBUG4, "Child reinitializing all global mutexes" );
    // APR interface requires children to init mutexes.
    apr_status_t status = AprGlobalMutex::static_child_init_all_mutexes( child_pool );
    if (status != APR_SUCCESS)
    {
        DOUT( DEBUG0, "Error reinitializing global mutex for child: "<<AprError( status ));
    }
    
    // TODO: APR interface also requires children to reattach shared memory
    // segments.
}




static apr_status_t pso_stop_parent_server(void *c) 
{
    DOUT( DEBUG0, "Shutting down pSodium..." );
    
    // Save
    psodium_conf *conf = (psodium_conf *)c;
    apr_pool_t *conf_pool = conf->conf_pool;

    // Go to C++ domain
    Versions *v = (Versions *)conf->versions;
    LyingClientsVector *lc = (LyingClientsVector *)conf->lyingclients;
    BadSlavesVector *bs = (BadSlavesVector *)conf->badslaves;

    if (v != NULL && v->size() > 0)
    {
        DOUT( DEBUG3, "Saving VERSIONS to disk" );

        apr_status_t status = pso_master_marshall_write_VERSIONS( v,  
                                      conf->versionsfilename,
                                      conf_pool );
        if (status != APR_SUCCESS)
        {
            DOUT( DEBUG0, "Error saving VERSIONS to " << conf->versionsfilename << ": " << AprError( status ) );
        }
    }
    if (lc != NULL)
    {
        try
        {
            lc->marshallToFile( conf->lyingclientsfilename, conf_pool );
        }
        catch( AprError e )
        {
            DOUT( DEBUG0, "Error saving LYINGCLIENTS to " << conf->lyingclientsfilename << ": " << e );
        }
    }
    if (bs != NULL)
    {
        try
        {
            bs->marshallToFile( conf->badslavesfilename, conf_pool );
        }
        catch( AprError e )
        {
            DOUT( DEBUG0, "Error saving BADSLAVES to " << conf->badslavesfilename << ": " << e );
        }
    }
    return OK;
}


static apr_status_t pso_stop_child_server( void *c )
{
        return OK;  
}




}
// extern "C"


/*
 * Non C-linkage procedures
 */

    
static apr_status_t post_config_create_master_datastructures( psodium_conf *conf, 
                                                rmmmemory *shm_alloc, 
                                                apr_pool_t *conf_p )
{                                                
    apr_status_t status=APR_SUCCESS;
    char *lock_basename = NULL, *lock_fullname=NULL;

    /*
     * VERSIONS is in private shared mem: i.e. memory shared by all pSodium 
     * in the Apache processes
     */

    // Setup concurrency control on VERSIONS
    AprGlobalMutex *lock = AprGlobalMutex::allocateMutex( );
    conf->versions_lock = lock;
    Versions *v = new( shm_alloc ) Versions();
    conf->versions = v;

    // Read VERSIONS from disk (if any)
    apr_file_t *versionsfile=NULL;
    status = apr_file_open( &versionsfile, conf->versionsfilename, APR_READ|APR_BINARY|APR_BUFFERED, APR_OS_DEFAULT, conf_p );
    if (status == APR_SUCCESS)
    {
        char *export_path=NULL, *error_str=NULL;
        status = pso_unmarshall_VERSIONS_from_file( versionsfile, 
                                                v, 
                                                shm_alloc,
                                                &export_path,
                                                &error_str );
        apr_file_close( versionsfile );
        if (status != APR_SUCCESS)                                               
        {
            DOUT( DEBUG0, "Error reading " << conf->versionsfilename << ": " << error_str << ": " << AprError( status ) );
            return status;
        }
    }
    else if (!APR_STATUS_IS_ENOENT( status ))
        return status;

#ifdef TEST    
    DPRINTF( DEBUG1, ("psodium: master: Allocating 2 digests" ));
    unsigned char *val1 = new( shm_alloc ) unsigned char[ OPENSSL_DIGEST_ALG_NBITS/8 ];
    strcpy( (char *)val1, "ZzTOPSheGotLegsArms\0" );
    unsigned char *val2 = new( shm_alloc ) unsigned char[ OPENSSL_DIGEST_ALG_NBITS/8 ];
    strcpy( (char *)val2, "YourLoveIsKingSadeZ\0" );
    ResponseInfo *ri1 = new( shm_alloc )ResponseInfo( val1, 0 );
    ResponseInfo *ri2 = new( shm_alloc )ResponseInfo( val2, 2 );
    
    DPRINTF( DEBUG1, ("first alloc of C++ object via shm_alloc\n" ));
    
    Time2DigestsMap *t2d = new( shm_alloc ) Time2DigestsMap();
    (*t2d)[ 3101234123412341234L ] = ri1;
    (*t2d)[ 3120000000000000000L ] = ri2;
    
    print_Time2DigestsMap( t2d, conf_p );

    Slave2DigestsMap *s2d = new( shm_alloc ) Slave2DigestsMap();
    (*s2d)[ new( shm_alloc ) Gstring( "128.37.193.40:37000" ) ] = t2d;
    
    
    (*v)[ new( shm_alloc ) Gstring( "http://130.37.193.64:37000/sjaak.html" ) ] = s2d;

    
    t2d = new( shm_alloc ) Time2DigestsMap();
    (*t2d)[ 7101234123412341234L ] = ri1;
    (*t2d)[           712000000L ] = ri2;
    
    s2d = new( shm_alloc ) Slave2DigestsMap();
    (*s2d)[ new( shm_alloc ) Gstring( "192.37.193.40:3822" ) ] = t2d;
    
    
    (*v)[ new( shm_alloc ) Gstring( "/harry.html" ) ] = s2d;

    
    print_Versions( v, conf_p );
#endif

    status = post_config_create_client_datastructures( conf, shm_alloc, conf_p );
    if (status != APR_SUCCESS)
    {
        return status;
    }
    
    /*
     * LYINGCLIENTS is in private shared mem, only in the master pSodium
     * Lock builtin.
     */
    LyingClientsVector *lc = new( shm_alloc ) LyingClientsVector( shm_alloc, conf->tempstoragedirname, conf_p );
    conf->lyingclients = lc;

    apr_pool_t *subpool=NULL;
    status = apr_pool_create( &subpool, conf_p );
    if (status != APR_SUCCESS)
    {
        DOUT( DEBUG0, "Cannot create subpool needed to read config files" );
        return status;
    }
    try
    {
        
        lc->unmarshallFromFile( conf->lyingclientsfilename, conf_p );
    }
    catch( AprError e )
    {
        DOUT( DEBUG0, "Error reading " << conf->lyingclientsfilename << ": " << e );
        status = e.getStatus();
    }

    apr_pool_destroy( subpool );

    return status;
}


static apr_status_t post_config_create_client_datastructures( psodium_conf *conf, 
                                                rmmmemory *shm_alloc, 
                                                apr_pool_t *conf_p )
{                                                
    apr_status_t status=APR_SUCCESS;
    char *lock_basename = NULL, *lock_fullname=NULL;

    /*
     * SlaveDatabase is in private shared mem, in both the client and 
     * the master pSodium.
     */
    
    // Setup concurrency control on SlaveDatabase
    AprGlobalMutex *lock = AprGlobalMutex::allocateMutex();
    conf->slavedb_lock = lock;
    
    SlaveDatabase *slavedb = new( shm_alloc ) SlaveDatabase();
    conf->slavedatabase = slavedb;


    /*
     * BADSLAVES is in PUBLIC shared mem, to be shared with Globule
     * Lock builtin.
     */
    // TODO: use public shm chunk, but that's impossible with the current
    // STL template based stuff!!!!!!!!!!!!!!!!!!!!!!!!!
    // Use some other IPC mechanism to talk to Globule!
    BadSlavesVector *bs = new( shm_alloc ) BadSlavesVector( shm_alloc, conf->tempstoragedirname, conf_p );
    conf->badslaves = bs;

    apr_pool_t *subpool=NULL;
    status = apr_pool_create( &subpool, conf_p );
    if (status != APR_SUCCESS)
    {
        DOUT( DEBUG0, "Cannot create subpool needed to read config files" );
        return status;
    }
    try
    {
        
        bs->unmarshallFromFile( conf->badslavesfilename, conf_p );
    }
    catch( AprError e )
    {
        DOUT( DEBUG0, "Error reading " << conf->badslavesfilename << ": " << e );
        status = e.getStatus();
    }

    apr_pool_destroy( subpool );

    return status;
}


/*
 * Util methods for module only (i.e., ones that won't work in a standalone
 * APR-based program as they use Apache specific methods 
 * (e.g. ap_set_content_type()). Should really be in a separate file.
 */

apr_status_t pso_create_badcontent_reply_bb( request_rec *r, 
                                             apr_bucket_brigade **newbb_out,  
                                             char *slave_id,
                                             char *pledge_data, 
                                             apr_size_t pledge_curr_length,
                                             char *error_str )
{
    apr_status_t status = APR_SUCCESS;
    apr_bucket *eos_bucket=NULL;
    psodium_conf *conf = NULL;
    conf = (psodium_conf *)ap_get_module_config( r->server->module_config, &psodium_module);

    r->status = HTTP_OK;
    // Send a "content is bad" image back to the proxy, which it forwards
    // to the browser.
    if (conf->badcontent_img_set)
    {
        ap_set_content_type(r, conf->badcontent_img_mimetype );
        *newbb_out = apr_brigade_create(r->pool, r->connection->bucket_alloc );
        if (*newbb_out == NULL)
        {
            DOUT( DEBUG0, "Cannot allocate mem to reply to client" );
            return APR_ENOMEM;
        }

        apr_file_t *fd=NULL;
        char *fname = ap_server_root_relative( r->pool, conf->badcontent_img_filename );
        status = apr_file_open( &fd, fname, APR_READ, APR_OS_DEFAULT, r->pool );
        if (status != APR_SUCCESS)
        {
            DOUT( DEBUG0, "Cannot open file to send as reply to client" << fname );
            return status;
        }

        apr_finfo_t finfo;
        status = apr_stat( &finfo, fname, APR_FINFO_SIZE, r->pool );
        if (status != APR_SUCCESS)
        {
            DOUT( DEBUG0, "Cannot stat file to send as reply to client" << fname );
            return status;
        }

        apr_bucket *b = apr_bucket_file_create( fd, 0, finfo.size, r->pool, r->connection->bucket_alloc );
        if (b == NULL)
        {
            DOUT( DEBUG0, "Cannot allocate mem to reply to client" );
            return APR_ENOMEM;
        }
        APR_BRIGADE_INSERT_TAIL( *newbb_out, b ); 

        eos_bucket = apr_bucket_eos_create( (*newbb_out)->bucket_alloc );
        if (eos_bucket == NULL)
        {
            DOUT( DEBUG0, "Cannot allocate mem to reply to client" );
            return APR_ENOMEM;
        }
        APR_BRIGADE_INSERT_TAIL( *newbb_out, eos_bucket ); 
    }
    else
    {
        // Send text error
        ap_set_content_type(r, "text/plain");
        *newbb_out = apr_brigade_create(r->pool, r->connection->bucket_alloc );
        if (*newbb_out == NULL)
        {
            DOUT( DEBUG0, "Cannot allocate mem to reply to client" );
            return APR_ENOMEM;
        }

        // TODO: pledge not guaranteed to be string
        char *msg = NULL;
        if (pledge_data != NULL)
        {
            msg = apr_psprintf( r->pool, "pSodium: Slave %s returned bad content. This fact was successfully reported to the master. Slave's pledge on the content was: %s\n", slave_id, pledge_data );
        }
        else
            msg = apr_psprintf( r->pool, "pSodium: Slave %s's content lacked pledge: %s\n", slave_id, error_str );
        status = apr_brigade_puts( *newbb_out, NULL, NULL, msg );
        if (status != APR_SUCCESS)
        {
            DOUT( DEBUG0, "Cannot allocate mem to reply to client" );
            return status;
        }

        eos_bucket = apr_bucket_eos_create( (*newbb_out)->bucket_alloc );
        if (eos_bucket == NULL)
        {
            DOUT( DEBUG0, "Cannot allocate mem to reply to client" );
            return APR_ENOMEM;
        }
        APR_BRIGADE_INSERT_TAIL( *newbb_out, eos_bucket ); 
    }
    return APR_SUCCESS;
}


apr_status_t pso_create_badslave_reply_bb( request_rec *r, 
                                           apr_bucket_brigade **newbb_out )
{
    apr_status_t status = APR_SUCCESS;
    apr_bucket *eos_bucket=NULL;
    psodium_conf *conf = NULL;
    conf = (psodium_conf *)ap_get_module_config( r->server->module_config, &psodium_module);

    // Send a "content is bad" image back to the proxy, which it forwards
    // to the browser.
    if (conf->badslave_img_set)
    {
        r->status = HTTP_OK;
        ap_set_content_type(r, conf->badslave_img_mimetype );
        *newbb_out = apr_brigade_create(r->pool, r->connection->bucket_alloc );
        if (*newbb_out == NULL)
        {
            DOUT( DEBUG0, "Cannot allocate mem to reply to client" );
            return APR_ENOMEM;
        }

        apr_file_t *fd=NULL;
        char *fname = ap_server_root_relative( r->pool, conf->badslave_img_filename );
        status = apr_file_open( &fd, fname, APR_READ, APR_OS_DEFAULT, r->pool );
        if (status != APR_SUCCESS)
        {
            DOUT( DEBUG0, "Cannot open file to send as reply to client" << fname );
            return status;
        }

        apr_finfo_t finfo;
        status = apr_stat( &finfo, fname, APR_FINFO_SIZE, r->pool );
        if (status != APR_SUCCESS)
        {
            DOUT( DEBUG0, "Cannot stat file to send as reply to client" << fname );
            return status;
        }

        apr_bucket *b = apr_bucket_file_create( fd, 0, finfo.size, r->pool, r->connection->bucket_alloc );
        if (b == NULL)
        {
            DOUT( DEBUG0, "Cannot allocate mem to reply to client" );
            return APR_ENOMEM;
        }
        APR_BRIGADE_INSERT_TAIL( *newbb_out, b ); 

        eos_bucket = apr_bucket_eos_create( (*newbb_out)->bucket_alloc );
        if (eos_bucket == NULL)
        {
            DOUT( DEBUG0, "Cannot allocate mem to reply to client" );
            return APR_ENOMEM;
        }
        APR_BRIGADE_INSERT_TAIL( *newbb_out, eos_bucket ); 
        return APR_SUCCESS;
    }
    else    
    {
        // Send text error
        ap_set_content_type(r, "text/plain");
        *newbb_out = apr_brigade_create(r->pool, r->connection->bucket_alloc );
        if (*newbb_out == NULL)
        {
            DOUT( DEBUG0, "Cannot allocate mem to reply to client" );
            return APR_ENOMEM;
        }

        // TODO: pledge not guaranteed to be string
        char *msg = apr_psprintf( r->pool, "pSodium: Attempting to recontact a bad slave\n" );
        status = apr_brigade_puts( *newbb_out, NULL, NULL, msg );
        if (status != APR_SUCCESS)
        {
            DOUT( DEBUG0, "Cannot allocate mem to reply to client" );
            return status;
        }

        eos_bucket = apr_bucket_eos_create( (*newbb_out)->bucket_alloc );
        if (eos_bucket == NULL)
        {
            DOUT( DEBUG0, "Cannot allocate mem to reply to client" );
            return APR_ENOMEM;
        }
        APR_BRIGADE_INSERT_TAIL( *newbb_out, eos_bucket ); 
    }
    return APR_SUCCESS;
}



apr_status_t pso_create_clientlied_reply_bb( request_rec *r, 
                                             apr_bucket_brigade **newbb_out,  
                                             char *slave_id,
                                             char *pledge_data, 
                                             apr_size_t pledge_curr_length,
                                             char *error_str )
{
    apr_status_t status = APR_SUCCESS;
    apr_bucket *eos_bucket=NULL;
    psodium_conf *conf = NULL;
    conf = (psodium_conf *)ap_get_module_config( r->server->module_config, &psodium_module);

    r->status = HTTP_OK;
    // Send a "content is bad" image back to the proxy, which it forwards
    // to the browser.
    if (conf->clientlied_img_set)
    {
        ap_set_content_type(r, conf->clientlied_img_mimetype );
        *newbb_out = apr_brigade_create(r->pool, r->connection->bucket_alloc );
        if (*newbb_out == NULL)
        {
            DOUT( DEBUG0, "Cannot allocate mem to reply to client" );
            return APR_ENOMEM;
        }

        apr_file_t *fd=NULL;
        char *fname = ap_server_root_relative( r->pool, conf->clientlied_img_filename );
        status = apr_file_open( &fd, fname, APR_READ, APR_OS_DEFAULT, r->pool );
        if (status != APR_SUCCESS)
        {
            DOUT( DEBUG0, "Cannot open file to send as reply to client" << fname );
            return status;
        }

        apr_finfo_t finfo;
        status = apr_stat( &finfo, fname, APR_FINFO_SIZE, r->pool );
        if (status != APR_SUCCESS)
        {
            DOUT( DEBUG0, "Cannot stat file to send as reply to client" << fname );
            return status;
        }

        apr_bucket *b = apr_bucket_file_create( fd, 0, finfo.size, r->pool, r->connection->bucket_alloc );
        if (b == NULL)
        {
            DOUT( DEBUG0, "Cannot allocate mem to reply to client" );
            return APR_ENOMEM;
        }
        APR_BRIGADE_INSERT_TAIL( *newbb_out, b ); 

        eos_bucket = apr_bucket_eos_create( (*newbb_out)->bucket_alloc );
        if (eos_bucket == NULL)
        {
            DOUT( DEBUG0, "Cannot allocate mem to reply to client" );
            return APR_ENOMEM;
        }
        APR_BRIGADE_INSERT_TAIL( *newbb_out, eos_bucket ); 
    }
    else
    {
        // Send text error
        ap_set_content_type(r, "text/plain");
        *newbb_out = apr_brigade_create(r->pool, r->connection->bucket_alloc );
        if (*newbb_out == NULL)
        {
            DOUT( DEBUG0, "Cannot allocate mem to reply to client" );
            return APR_ENOMEM;
        }

        // TODO: pledge not guaranteed to be string
        char *msg = NULL;
        if (pledge_data != NULL)
        {
            msg = apr_psprintf( r->pool, "pSodium: You have reported a bad pledge from %s, but pledge is correct. You are now listed as liar. %s\n", slave_id, pledge_data );
        }
        else
            msg = apr_psprintf( r->pool, "pSodium: Internal error reporting client lying about: %s: %s\n", slave_id, error_str );
        status = apr_brigade_puts( *newbb_out, NULL, NULL, msg );
        if (status != APR_SUCCESS)
        {
            DOUT( DEBUG0, "Cannot allocate mem to reply to client" );
            return status;
        }

        eos_bucket = apr_bucket_eos_create( (*newbb_out)->bucket_alloc );
        if (eos_bucket == NULL)
        {
            DOUT( DEBUG0, "Cannot allocate mem to reply to client" );
            return APR_ENOMEM;
        }
        APR_BRIGADE_INSERT_TAIL( *newbb_out, eos_bucket ); 
    }
    return APR_SUCCESS;
}
