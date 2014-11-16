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
#include "apr_signal.h"

void *APR_THREAD_FUNC signalthread_func( apr_thread_t *thd, void *data );
void *APR_THREAD_FUNC timerthread_func( apr_thread_t *thd, void *data );
apr_status_t handle_switch( int *idx_out );
apr_status_t synchronized_switch_tables( int *idx_out );
apr_status_t get_MODIFICATIONS( apr_time_t audit_start_time, apr_pool_t *pool );
apr_status_t file_chunked2identity( apr_file_t *chunked_file, 
                                    apr_file_t *ident_file, 
                                    bool *noChunked_out  );
apr_status_t handle_connection( apr_socket_t *sock2, 
                                apr_sockaddr_t *localsa, 
                                apr_pool_t *req_pool );
apr_status_t store_pledgeforward( char *buf, apr_size_t buf_len,
                                  char *client_ipaddr,
                                  apr_port_t client_port,
                                  apr_pool_t *pool );
apr_status_t send_ipaddrportvector( char *uri_str, 
                                    IPAddrPortVector *vec, 
                                    apr_pool_t *pool );

static void global_cleanup( void );
static int signal_handler( int sig );


// Global variables
apr_pool_t *_mainpool=NULL, *_tablepool[ NTABLES ];
apr_thread_t *timerthread=NULL;
apr_thread_t *signalthread=NULL;
apr_thread_t **tharray=NULL;

OnDiskHashTable **_tables=NULL;
OnDiskHashTable *_curr_audited_table;
OnDiskHashTable *_curr_store_table;
AprSemaphore    *_sema=NULL;
apr_thread_mutex_t   *_store_mutex=NULL;

char *modiftemp_filename_prefix="/arno/psodium/auditor/MODIFICATIONS";
char *_master_ipaddr_str="130.37.193.64";
apr_port_t _master_port=37000;

rmmmemory *_shm_alloc=NULL;
Versions *_modifications=NULL;
char *_globule_export_path=NULL;
SlaveDatabase *_slavedb=NULL;
LyingClientsVector *_lying_clients=NULL;
BadSlavesVector *_bad_slaves=NULL;

char *_base64_userpass=NULL;


void usage()
{
    printf( "Usage: auditor <master host:port> <slavekeysfile> <temp-dir> <log-dir>\n" );
}

int main( int argc, char *argv[] )
{
    apr_socket_t *sock=NULL;
    apr_socket_t *sock2=NULL;
    //const char *bind_to_ipaddr = NULL;
    const char *bind_to_ipaddr = "130.37.193.64";
    apr_sockaddr_t *localsa = NULL;
    int family = APR_INET;
    apr_status_t status=APR_SUCCESS;
    char *serverroot=NULL;
    char *psodiumtempdir=NULL;
    char *hashtabledir="/arno/psodium/auditor/";
    const char *slavekeysfilename="/arno/psodium/master/conf/slavekeys.txt";
    const char *logdir="", *pidfilename="";
    char *fullfilenameprefix=NULL;
    char *auditor_passwd=NULL;
    int i=0;
    int nthreads=DEFAULT_NTHREADS;   // number of threads to create, dependent on #processors
    apr_file_t *pidfile=NULL;

    /*
     * Setup APR
     */
    
    status = apr_initialize( );
    if (status != APR_SUCCESS)
        fatal(status );

    status = apr_pool_create( &_mainpool, NULL );
    if (status != APR_SUCCESS)
        fatal(status );

    atexit( global_cleanup );

    if (argc != 5+1)
    {   
        usage();
        fatal( APR_EINVAL );
    }
    else
    {
        int i=1;

        char *hostport=apr_pstrdup( _mainpool, argv[i++] );
        char *ptr = strstr( hostport, ":" );
        if (ptr == NULL)
        {
            usage();
            fatal( APR_EINVAL );
        }
        char *port_str = ptr+1;
        *ptr = '\0';
        char *hostname = hostport; 

        int nitems = sscanf( port_str, "%hu", &_master_port );
        if (nitems != 1)
        {
            usage(); 
            fatal( APR_EINVAL );
        }

        apr_sockaddr_t *temp_sa=NULL;
        status = apr_sockaddr_info_get( &temp_sa, hostname, APR_UNSPEC, _master_port, APR_IPV4_ADDR_OK, _mainpool );
        if (status != APR_SUCCESS) 
            fatal( status );
        apr_sockaddr_ip_get( &_master_ipaddr_str, temp_sa);

        slavekeysfilename=argv[i++];

        if (argv[i][strlen( argv[i] )-1] != '/')
            psodiumtempdir=apr_pstrcat( _mainpool, argv[i], "/", NULL );
        else
            psodiumtempdir=argv[i];
        i++;

        logdir=argv[i++];

        // SECURITY RISK: The password is visible in the public process
        // table of the host. I.e. can't use current auditor on shared machine!
        auditor_passwd=argv[i++];

        hashtabledir= apr_pstrcat( _mainpool, psodiumtempdir, "auditor/", NULL );
        modiftemp_filename_prefix = apr_pstrcat( _mainpool, hashtabledir, "MODIFICATIONS", NULL );

        DPRINTF( DEBUG0, ("psoaudit: Auditing master %s/%s:%hu\n", hostname, _master_ipaddr_str, _master_port ));

        pidfilename=apr_pstrcat( _mainpool, logdir, "/psodium-auditor.pid", NULL );
    }

    // Write Process ID to file
    status = apr_file_open( &pidfile, pidfilename, APR_CREATE|APR_WRITE|APR_TRUNCATE, APR_OS_DEFAULT, _mainpool );
    if (status != APR_SUCCESS)
    {
        DOUT( DEBUG0,"Error creating file with process ID " << pidfilename );
        fatal( status );
    }
    else
    {
        // WINDOWSPORT
        pid_t pid=getpid();
        char *pidstr=NULL;
        int ret = pso_Ntoa( &pid, sizeof( pid ), &pidstr, _mainpool );
        apr_size_t written=strlen( pidstr );
        status = apr_file_write( pidfile, pidstr, &written );
        if (status != APR_SUCCESS)
        {
            DOUT( DEBUG0,"Error creating file with process ID " << pidfilename );
            fatal( status );
        }
        else
        {
            status = apr_file_close( pidfile );
            if (status != APR_SUCCESS)
            {
                DOUT( DEBUG0,"Error creating file with process ID " << pidfilename );
                fatal( status );
            }
        }
    }

    status = apr_dir_make_recursive( psodiumtempdir, APR_OS_DEFAULT, _mainpool );
    if (status != APR_SUCCESS && !APR_STATUS_IS_EEXIST( status ))
    {
        DOUT( DEBUG0,"Error creating temp storage directory " << psodiumtempdir );
        fatal( status );
    }


    status = apr_dir_make_recursive( hashtabledir, APR_OS_DEFAULT, _mainpool );
    if (status != APR_SUCCESS && !APR_STATUS_IS_EEXIST( status ))
    {
        DOUT( DEBUG0,"Error creating pledge-hashtable directory " << hashtabledir );
        fatal( status );
    }


    /*
     * It appears we have to do a proper cleanup of global mutexes otherwise
     * Linux runs out of semaphores (see ipcs command). So install
     * a signal thread that will cleanup, as sending a signal is the
     * only way to terminate the forever running auditor.
     */
    status = apr_setup_signal_thread();
    if (status != APR_SUCCESS)
        fatal( status );

    status = apr_thread_create( &signalthread, NULL, signalthread_func, NULL, _mainpool );
    if (status != APR_SUCCESS)
        fatal( status );

    /*
     * Setup basic authentication to master
     */
    char *userid=AUDITOR_USERID;
    char *password=auditor_passwd;
    char *userpass = apr_pstrcat( _mainpool, userid, ":", password, NULL );
    
    DPRINTF( DEBUG3, ("Basic authentication for auditor is %s\n", userpass ));

    apr_size_t encoded_len = apr_base64_encode_len( strlen( userpass ) );
    _base64_userpass = new char[ encoded_len ];
    apr_size_t encoded_len2 = apr_base64_encode( _base64_userpass, userpass, strlen( userpass ) );
    
    /*
     * ASSUMPTION: shmem just as cheap as regular mem when used by single
     * process, so I won't bother writing extra support that enables us
     * to use the shared mem C++ classes of pSodium (e.g. SlaveDatabase)
     * in regular memory.
     */
    rmmmemory::shm(_mainpool, UNSHAREDMEM_SIZE);

    /*
     * Load database with slave public keys
     */
    // No need for mutex, as for STL containers it holds:
    // "simultaneous read accesses to to shared containers are safe"
    //
    _slavedb = new( _shm_alloc ) SlaveDatabase();
    
    status = pso_master_read_slavekeys( slavekeysfilename, _slavedb, NULL, _shm_alloc, _mainpool );
    if (status != APR_SUCCESS)
        fatal(status );

    try
    {
        _lying_clients = new( _shm_alloc ) LyingClientsVector( _shm_alloc, "", _mainpool );
        _bad_slaves = new( _shm_alloc ) BadSlavesVector( _shm_alloc, "", _mainpool );
        
#ifdef TEST
        _lying_clients->insert( (char *)"128.2.193.11", (apr_port_t)512 );
        _lying_clients->insert( (char *)"224.0.6.11", (apr_port_t)46789 );
        _bad_slaves->insert( (char *)"10.12.13.99", (apr_port_t)321 );
        _bad_slaves->insert( (char *)"102.103.222.255", (apr_port_t)55545 );
#endif
    }
    catch( AprError e )
    {
        fatal( e.getStatus() );
    }
    
    /*
     * Setup on-disk hashtables
     */
    // Use APR pool's cleanup facilities (in theory, this program only stops when killed)
    _tables  = new OnDiskHashTable*[ NTABLES ];
    for (i=0; i<NTABLES; i++)
    {
        char id[ NUMBER_MAX_STRLEN(int)+1+1 ]; // +1 for '-', +1 for string
        
        status = apr_pool_create( &_tablepool[i], _mainpool );
        if (status != APR_SUCCESS)
            fatal(status );

        sprintf( id, "%d-", i ); 
        fullfilenameprefix = arnopr_strcat( 481, hashtabledir, HASHSLOT_FILENAME_PREFIX, id, NULL );
        _tables[i] = new OnDiskHashTable();
        try
        {
            _tables[i]->init( fullfilenameprefix, _tablepool[i] );
            //DPRINTF( DEBUG3, ("Just initialized table[%d]=%p\n", i, _tables[i] ));
        }
        catch( AprError e )
        {
            fatal( e.getStatus() );
        }
        delete[] fullfilenameprefix; // ~Kansas
    }

    // Must be done before threads are started
    _curr_store_table = _tables[ 0 ];
    _curr_audited_table = _tables[ 1 ];
    
    /*
     * Setup synchronization
     */
    try
    {
        _sema = new AprSemaphore( _mainpool );
    }
    catch( AprError e )
    {
        fatal( e.getStatus() );
    }

    status = apr_thread_mutex_create( &_store_mutex, APR_THREAD_MUTEX_DEFAULT, _mainpool );
    if (status != APR_SUCCESS)
        fatal(status );
    
    /*
     * Create auditing threads
     */
    status = apr_thread_create( &timerthread, NULL, timerthread_func, NULL, _mainpool );
    if (status != APR_SUCCESS)
        fatal( status );
    
    tharray = new apr_thread_t*[ nthreads ];
    for (int i=0; i<nthreads; i++)
    {
        apr_pool_t *threadpool=NULL;
        
        // Each thread gets its own APR pool, so we don't have to mutex that
        status = apr_pool_create( &threadpool, NULL );
        if (status != APR_SUCCESS)
            fatal(status );
        
        status = apr_thread_create( &tharray[i], NULL, auditthread_func, threadpool, _mainpool );
        if (status != APR_SUCCESS)
            fatal( status );
    }

    
    /*
     * Setup listening socket
     */
    
    if (bind_to_ipaddr) 
    {
        /* First, parse/resolve ipaddr so we know what address family of
         * socket we need.  We'll use the returned sockaddr later when
         * we bind.
         */
        status = apr_sockaddr_info_get( &localsa, bind_to_ipaddr, APR_UNSPEC, AUDITOR_PORT, 0, _mainpool );
        if (status != APR_SUCCESS)
            fatal( status );
        family = localsa->family;
    }

    status = apr_socket_create_ex( &sock, family, SOCK_STREAM, APR_PROTO_TCP, _mainpool);
    if (status != APR_SUCCESS)
        fatal(status );

    status = apr_socket_opt_set( sock, APR_SO_REUSEADDR, 1);
    if (status != APR_SUCCESS)
        fatal(status );

    if (localsa==NULL) 
    {
        apr_socket_addr_get( &localsa, APR_LOCAL, sock );
        apr_sockaddr_port_set( localsa, AUDITOR_PORT );
    }
    
    {
        char *local_ipaddr, *remote_ipaddr;
        apr_port_t local_port, remote_port;

        apr_sockaddr_ip_get( &local_ipaddr, localsa);
        apr_sockaddr_port_get( &local_port, localsa);
        DPIDPRINTF( DEBUG1, ("Listening socket: %s:%u\n", local_ipaddr, local_port ));
    }


    status = apr_socket_bind( sock, localsa);
    if (status != APR_SUCCESS)
        fatal(status );

    status = apr_socket_listen( sock, 5);
    if (status != APR_SUCCESS)
        fatal(status );

    
    
    /*
     * We have lift off
     */
    while( 1 )
    {
        apr_pool_t *req_pool=NULL;
        status = apr_pool_create( &req_pool, _mainpool );
        if (status != APR_SUCCESS)
            fatal(status );

        status = apr_socket_accept( &sock2, sock, req_pool );
        if (status != APR_SUCCESS)
            nonret_non_fatal(status );

        status = handle_connection( sock2, localsa, req_pool );
        if (status != APR_SUCCESS)
            nonret_non_fatal(status );

        apr_pool_destroy( req_pool ); 
    }
    status = apr_socket_close( sock);
    if (status != APR_SUCCESS)
        fatal(status );
        
    //apr_pool_destroy( mainpool ); // "also destroys all subpools"
    
    return 0;
}


/*
 * Start function for timer thread that controls table switches
 */
void *APR_THREAD_FUNC signalthread_func( apr_thread_t *thd, void *data )
{
    apr_status_t status = APR_SUCCESS;
    status = apr_signal_thread( signal_handler );
    if (status != APR_SUCCESS)
        fatal( status );
    
    // loop-di-loop waiting for signal, according to apr_thread_proc.h
}


static int signal_handler( int sig )
{
    DPRINTF( DEBUG1, ("psodium: auditor: signal handler %d\n", sig ));
    global_cleanup();
    exit( -1 );
    // return 1
}


static void global_cleanup( void )
{
    int i=0;
    
    // I get SEGV when deleteing the _tables[i], so let OS do it. 
#ifdef LATER    
    // not cleaning data structures in shared mem
    delete _shm_alloc;
    
    // semaphores and mutexes are destroyed by pools
    
    if (_tables != NULL)
    {
        for (i=0; i<NTABLES; i++)
        {
            if (_tables[i] != NULL)
                delete _tables[i];
        }
        delete[] _tables;
    }

    // Also kills all subpools, i.e. _tablepool[]
    apr_pool_destroy( _mainpool );
#endif
    
    apr_terminate();
}







apr_status_t handle_connection( apr_socket_t *sock2, 
                                apr_sockaddr_t *localsa, 
                                apr_pool_t *req_pool )
{
    char *local_ipaddr, *remote_ipaddr;
    apr_port_t local_port, remote_port;
    apr_sockaddr_t *remotesa=NULL;
    int protocol;
    apr_status_t status=APR_SUCCESS;
    char buf[ MAX_PLEDGEFORWARD_SIZE+1 ];
    apr_size_t buf_left=0, bytes_read=0;
    
    apr_socket_protocol_get( sock2, &protocol);
    if (protocol != APR_PROTO_TCP) 
    {
        DPIDPRINTF( DEBUG1, ("Error: protocol not conveyed from listening socket "
                "to connected socket!\n" ));
        return APR_EINVALSOCK;
    }
    
    apr_socket_addr_get( &remotesa, APR_REMOTE, sock2);
    apr_sockaddr_ip_get( &remote_ipaddr, remotesa);
    apr_sockaddr_port_get( &remote_port, remotesa);
    apr_socket_addr_get( &localsa, APR_LOCAL, sock2);
    apr_sockaddr_ip_get( &local_ipaddr, localsa);
    apr_sockaddr_port_get( &local_port, localsa);
    DPIDPRINTF( DEBUG1, ("Server socket: %s:%u -> %s:%u\n", local_ipaddr, 
            local_port, remote_ipaddr, remote_port));

    // Timeout if client don't close connection
    status = apr_socket_opt_set( sock2, APR_SO_TIMEOUT, CLIENTSOCKET_TIMEOUT );
    if (status != APR_SUCCESS)
        fatal(status );
    
    
    // TODO: Check client not on LYINGCLIENTS list
    
    buf_left = MAX_PLEDGEFORWARD_SIZE;
    
    apr_size_t totalbr=0;
    buf[ MAX_PLEDGEFORWARD_SIZE ] ='\0'; // for DIRTY deed
    char *end_of_request = apr_pstrcat( req_pool, CRLF, CRLF, NULL );
    do
    {
        DPIDPRINTF( DEBUG5, ("psoaudit: Reading pledgeforward from client\n" ));
        
        bytes_read = buf_left;
        status = apr_socket_recv( sock2, &buf[ MAX_PLEDGEFORWARD_SIZE - buf_left ], &bytes_read );
        if (status != APR_SUCCESS)
        {
            nonret_non_fatal( status );
            break;
        }
        
        buf_left -= bytes_read;
        
        totalbr += bytes_read;

        // DIRTY, make sure we terminate the connection as soon as data is in
        // A browser talking to the pSodium proxy will be stalled until we
        // accept the proxy's input here. Malicious clients will be cutoff
        // by the timeout we set.
        if (strstr( buf, end_of_request ))
            break;
        
    } while( buf_left > 0 );

    DPIDPRINTF( DEBUG5, ("psoaudit: Read %d bytes from client %d\n", MAX_PLEDGEFORWARD_SIZE - buf_left, totalbr ));

    char *response = arnopr_strcat( 481, "HTTP/1.1 200 OK", CRLF, CRLF, NULL );
    bytes_read = strlen( response );
    status = apr_socket_send( sock2, response, &bytes_read);
    if (status != APR_SUCCESS)
    {
        apr_socket_close(sock2);
        nonret_non_fatal( status );
    }
    
    if (status == APR_SUCCESS || status == APR_TIMEUP) // TIMEUP facilitates debug
    {
        DPIDPRINTF( DEBUG5, ("psoaudit: Storing pledgeforward from client\n" ));
        
        status = store_pledgeforward( buf, MAX_PLEDGEFORWARD_SIZE - buf_left, remote_ipaddr, remote_port, req_pool );
        if (status != APR_SUCCESS)
            return_non_fatal(status );
    }

    status = apr_socket_shutdown( sock2, APR_SHUTDOWN_READ);
    if (status != APR_SUCCESS)
        nonret_non_fatal(status );

    status = apr_socket_close( sock2);
    if (status != APR_SUCCESS)
        return_non_fatal(status );
    
    return APR_SUCCESS;
}


apr_status_t store_pledgeforward( char *buf, apr_size_t buf_len,
                                  char *client_ipaddr,
                                  apr_port_t client_port,
                                  apr_pool_t *pool )

{
    int max_port_strlen = NUMBER_MAX_STRLEN( apr_port_t );
    char *port_fixedlen_str=NULL, *client_port_str=NULL;
    char *element=NULL, *clientinfo=NULL;
    apr_size_t element_len=0;
    apr_status_t status = APR_SUCCESS, insertstatus = APR_SUCCESS;

    /* Decode pledge from HTTP putDigest request:
     * GET /psodium/putDigest?<Base64Pledge> HTTP/1.1
     * I currently use the GET method, instead of PUT, as it is a bit
     * harder to implement in the current mod_psodium that is based
     * on mod_proxy.
     */
    apr_size_t soffset = pso_find_keyword( buf, 0, buf_len, "?" );
    if (soffset == -1)
        return APR_EINVAL;
    apr_size_t eoffset = pso_find_keyword( buf, soffset, buf_len, " HTTP" );
    if (eoffset == -1)
        return APR_EINVAL;
    soffset++; // skip "?"
    
    char *encoded = new char[ eoffset-soffset+1 ];
    memcpy( encoded, &buf[soffset], eoffset-soffset );
    encoded[ eoffset-soffset ] = '\0';

    apr_size_t decoded_len=0, decoded_len2;
    decoded_len = apr_base64_decode_len( encoded ); 
    char *decoded = new char[ decoded_len ];
    decoded_len2 = apr_base64_decode( (char *)decoded, encoded );
    delete[] encoded;
    
    // Append client ipaddr + port
    // TODO: use bucket brigade?
    int ret = pso_Ntoa( &client_port, sizeof( client_port ), &client_port_str, pool );
    port_fixedlen_str = apr_psprintf( pool, "%-*s", max_port_strlen, client_port_str );
    clientinfo = arnopr_strcat( 481, 
                             CLIENT_IPADDRSTR_KEYWORD, " ", client_ipaddr, CRLF,
                             CLIENT_PORTSTR_KEYWORD, " ", port_fixedlen_str, CRLF,
                             NULL );

    DPRINTF( DEBUG1, ("psoaudit: Auditor received a pledge from %s:%s, storing for next audit cycle: %s\n", client_ipaddr, client_port_str, decoded ));

    element = arnopr_memcat( &element_len, decoded, decoded_len2, clientinfo, strlen( clientinfo ), NULL );
    delete[] clientinfo;
    delete[] decoded;
    
    
    // Write to disk
    // Hope grabbing this lock each time is not too expensive
    status = apr_thread_mutex_lock( _store_mutex );
    if (status != APR_SUCCESS)
        fatal( status ); // no lock, no sync, no work
    
    try
    {
        _curr_store_table->insert( element, element_len );
        delete[] element;
    }
    catch( AprError e )
    {
        insertstatus = e.getStatus();
    }

    status = apr_thread_mutex_unlock( _store_mutex );
    if (status != APR_SUCCESS)
        fatal( status ); // no lock, no sync, no work
    
    return insertstatus;
}








/*
 * Start function for timer thread that controls table switches
 */
void *APR_THREAD_FUNC timerthread_func( apr_thread_t *thd, void *data )
{
    int idx=0;
    apr_status_t status = APR_SUCCESS;
    
    // Wait until first batch of pledgeforwards is in
    DPIDPRINTF( DEBUG1, ("psoaudit: Timer waiting for first interval...\n" ));
    apr_sleep( AUDIT_INTERVAL );
    
    handle_switch( &idx );
    
    // Release the hordes
    DPIDPRINTF( DEBUG1, ("psoaudit: Timer first unleashing auditor threads\n" ));
    try
    {
        _sema->up();
    }
    catch( AprError e )
    {
        fatal( e.getStatus() );
    }
    
    while( 1 )
    {
        // Wait till next interval
        DPIDPRINTF( DEBUG1, ("psoaudit: Timer waiting for next interval...\n" ));
        apr_sleep( AUDIT_INTERVAL );
        
        // Hold the hordes
        DPIDPRINTF( DEBUG1, ("psoaudit: Timer leashing auditor threads\n" ));
        try
        {
            _sema->down();
        }
        catch( AprError e )
        {
            fatal( e.getStatus() );
        }

        handle_switch( &idx );

        
        // (re)Release the hordes
        DPIDPRINTF( DEBUG1, ("psoaudit: Timer unleashing auditor threads\n" ));
        try
        {
            _sema->up();
        }
        catch( AprError e )
        {
            fatal( e.getStatus() );
        }
    }
}



apr_status_t handle_switch( int *idx_out )
{
    apr_pool_t  *temppool=NULL;
    apr_status_t status=APR_SUCCESS;
    apr_time_t sleept = AUDITOR_NAP;

    // 1. Absolute start of audit
    apr_time_t audit_start_time=apr_time_now();

    // 2. Switch hashtables
    synchronized_switch_tables( idx_out );

    /* 3. Try to get MODIFICATIONS from master.
     * If master is down we try continuously.
     * This creates a DoS possibility: if the bad slaves make the master
     * unreachable, the auditor won't check the pledges.
     */
    status = apr_pool_create( &temppool, NULL );
    if (status != APR_SUCCESS)
        fatal( status );

    sleept = AUDITOR_NAP;
    do
    {
        status = get_MODIFICATIONS( audit_start_time, temppool );
        if (status != APR_SUCCESS)
        {
            DPRINTF( DEBUG1, ("psoaudit: Master appears to be down, sleeping %lld s before another getMODIFICATIONS\n", apr_time_sec( sleept ) ));
            apr_sleep( sleept );
            sleept *= (sleept/2); // exp backoff
        }
    }
    while( status != APR_SUCCESS);

print_Versions( _modifications, temppool );

    apr_pool_destroy( temppool );
    temppool = NULL;
    
    /* 4. Send LYINGCLIENTS to master.
     * If master is down we try continuously.
     * This creates a DoS possibility: if the bad slaves make the master
     * unreachable, the auditor won't check the pledges.
     */
    if (_lying_clients->size() > 0)
    {
        status = apr_pool_create( &temppool, NULL );
        if (status != APR_SUCCESS)
            fatal( status );

        sleept = AUDITOR_NAP;
        do
        {
            DPIDPRINTF( DEBUG0, ("We identified %d LYINGCLIENTS, sending them to master:\n", _lying_clients->size() ));
            status = send_ipaddrportvector( PSO_PUTLYINGCLIENTS_URI_PREFIX, _lying_clients, temppool );
            if (status != APR_SUCCESS)
            {
                DPRINTF( DEBUG1, ("psoaudit: Master appears to be down, sleeping before another putLYINGCLIENTS\n" ));
                apr_sleep( sleept );
                sleept *= (sleept/2); // exp backoff
            }
        }
        while( status != APR_SUCCESS);

        apr_pool_destroy( temppool );
    }

    
    /* 4. Send BADSLAVES to master.
     * If master is down we try continuously.
     * This creates a DoS possibility: if the bad slaves make the master
     * unreachable, the auditor won't check the pledges.
     */
    if (_bad_slaves->size() > 0)
    {
        status = apr_pool_create( &temppool, NULL );
        if (status != APR_SUCCESS)
            fatal( status );

        sleept = AUDITOR_NAP;
        do
        {
            DPIDPRINTF( DEBUG0, ("We identified %d BADSLAVES, sending them to master:\n", _bad_slaves->size() ));
            status = send_ipaddrportvector( PSO_PUTBADSLAVES_URI_PREFIX, _bad_slaves, temppool );
            if (status != APR_SUCCESS)
            {
                DPRINTF( DEBUG1, ("psoaudit: Master appears to be down, sleeping before another putBADSLAVES\n" ));
                apr_sleep( sleept );
                sleept *= (sleept/2); // exp backoff
            }
        }
        while( status != APR_SUCCESS);

        apr_pool_destroy( temppool );
    }

    
    return APR_SUCCESS;
}


apr_status_t synchronized_switch_tables( int *idx_out )
{
    apr_status_t status;
    
    DPIDPRINTF( DEBUG1, ("psoaudit: Timer thread switching table, old is %d\n", *idx_out ));
    
    // "Lock" thread storing pledgeforwards in table
    status = apr_thread_mutex_lock( _store_mutex );
    if (status != APR_SUCCESS)
        fatal( status ); // no lock, no sync, no work

    // Switch tables
    _curr_audited_table = _tables[ *idx_out ];
    *idx_out = *idx_out + 1;
    *idx_out = *idx_out % NTABLES;
    _curr_store_table = _tables[ *idx_out ];
    try
    {
        //DPRINTF( DEBUG3, ("Switching to table[%d]=%p \n", *idx_out, _curr_store_table ));
        _curr_store_table->clear( _tablepool[ *idx_out ] );
    }
    catch( AprError e )
    {
        fatal( e.getStatus() );
    }

    // "Unlock" thread storing pledgeforwards in table
    status = apr_thread_mutex_unlock( _store_mutex );
    if (status != APR_SUCCESS)
        fatal( status ); // no lock, no sync, no work
    
    return status;
}


apr_status_t get_MODIFICATIONS( apr_time_t audit_start_time, apr_pool_t *pool )
{
    apr_file_t *file_c=NULL, *file_i;
    apr_socket_t *sock=NULL;
    apr_sockaddr_t *remote_sa = NULL, *local_sa = NULL;
    apr_port_t remote_port,local_port;
    char *local_ipaddr=NULL, *remote_ipaddr=NULL;
    char buf[ RECVBUF_SIZE ];
    char *error_str=NULL;
    apr_status_t status=APR_SUCCESS;
    apr_size_t length=0;
    
    char *fn_c = arnopr_strcat( 481, modiftemp_filename_prefix, MODIFTEMP_FILENAME_CHUNKED_POSTFIX, NULL );

    DOUT( DEBUG3, "Temp file for encoded MODIFICATIONS is " << fn_c );
    status = apr_file_open( &file_c, fn_c, CREATE_FLAGS, APR_OS_DEFAULT, pool );
    delete[] fn_c;
    if (status != APR_SUCCESS)
        return_non_fatal( status );
    
    DPRINTF( DEBUG1, ("psoaudit: getMOD: Making socket address...\n"));
    
    status = apr_sockaddr_info_get( &remote_sa, _master_ipaddr_str, APR_INET, _master_port, 0, pool);
    if (status != APR_SUCCESS) 
        return_non_fatal( status );

    DPRINTF( DEBUG5,  ("psoaudit: getMOD:  Creating new socket..\n"));
    status = apr_socket_create(&sock, remote_sa->family, SOCK_STREAM, pool);
    if( status != APR_SUCCESS) 
        return_non_fatal( status );
        
    DPRINTF( DEBUG5,  ("psoaudit: getMOD: Setting socket timeout..\n"));
    status = apr_socket_timeout_set(sock, GETMODSOCKET_TIMEOUT );
    if( status != APR_SUCCESS) 
        return_non_fatal( status );
    
    DPRINTF( DEBUG5,  ("psoaudit: getMOD: Connecting to socket..\n"));
    status = apr_socket_connect(sock, remote_sa);
    if( status != APR_SUCCESS) 
    {
        apr_socket_close(sock);
        return_non_fatal( status );
    }

    apr_socket_addr_get(&remote_sa, APR_REMOTE, sock);
    apr_sockaddr_ip_get(&remote_ipaddr, remote_sa);
    apr_sockaddr_port_get(&remote_port, remote_sa);
    apr_socket_addr_get(&local_sa, APR_LOCAL, sock);
    apr_sockaddr_ip_get(&local_ipaddr, local_sa);
    apr_sockaddr_port_get(&local_port, local_sa);
    DPRINTF( DEBUG1,  ("psoaudit: getMOD: %s:%u -> %s:%u\n", local_ipaddr, local_port, remote_ipaddr, remote_port));

    // Don't want chunked transfer response
    char *ast_str=NULL, *port_str=NULL;
    int ret = pso_Ntoa( &audit_start_time, sizeof( audit_start_time ), &ast_str, pool ); 
    ret = pso_Ntoa( &_master_port, sizeof( _master_port ), &port_str, pool ); 
    char *request = apr_psprintf( pool, "GET %s?%s HTTP/1.1%sHost: %s:%s%sAccept-Encoding: chunked;q=0, identity;q=1.0%sAuthorization: Basic %s%s%s", 
                                    PSO_GETMODIFICATIONS_URI_PREFIX, ast_str, CRLF,
                                    _master_ipaddr_str, port_str, CRLF,
                                    /* accept enc */ CRLF, 
                                    _base64_userpass, CRLF,
                                    CRLF );
    
    DPRINTF( DEBUG5,  ("psoaudit: getMOD: Trying to send data over socket..\n"));
    length = strlen( request );
    status = apr_socket_send( sock, request, &length);
    if (status != APR_SUCCESS)
    {
        apr_socket_close(sock);
        return_non_fatal( status );
    }
   
    bool masterUp=false; // to handle case where master is down
    while( 1 )
    {
        DPRINTF( DEBUG5,  ("psoaudit: getMOD: Trying to receive data over socket..\n"));
        
        length = RECVBUF_SIZE; 
        status = apr_socket_recv( sock, buf, &length);
        // Special semantics: bytes may have been received and status may be APR_EOF
        if (status != APR_SUCCESS && status != APR_EOF && status != APR_TIMEUP)
        {
            apr_socket_close(sock);
            return_non_fatal( status );
        }

        if (length > 0)
        {
            status = apr_file_write( file_c, buf, &length );
            if (status != APR_SUCCESS)
            {
                apr_socket_close(sock);
                return_non_fatal( status );
            }
            masterUp=true;
        }
        
        if (status == APR_EOF || status == APR_TIMEUP)
            break;
    }
        
    DPRINTF( DEBUG5,  ("psoaudit: getMOD: Shutting down socket..\n"));
    status = apr_socket_shutdown(sock, APR_SHUTDOWN_WRITE);
    if (status != APR_SUCCESS)
    {
        apr_socket_close(sock);
        return_non_fatal( status );
    }

    DPRINTF( DEBUG5,  ("psoaudit: getMOD: Closing down socket..\n"));
    status = apr_socket_close(sock);
    if (status != APR_SUCCESS)
    {
        return_non_fatal( status );
    }
    
    if (!masterUp)
        return APR_EOF;
    
    /*
     * I can't get Apache to not send me MODIFICATIONS in chunked
     * transfer encoding. Quick hack to get rid of that stuff
     * before parsing MODIFICATIONS
     */
    char *fn_i = arnopr_strcat( 481, modiftemp_filename_prefix, MODIFTEMP_FILENAME_IDENTITY_POSTFIX, NULL );
    DOUT( DEBUG3, "Temp file for unencoded MODIFICATIONS is " << fn_i );
    status = apr_file_open( &file_i, fn_i, CREATE_FLAGS, APR_OS_DEFAULT, pool );
    delete[] fn_i;
    if (status != APR_SUCCESS)
        return_non_fatal( status );
    
    bool noChunked=false;
    status = file_chunked2identity( file_c, file_i, &noChunked );
    if (status != APR_SUCCESS)
        return_non_fatal( status );
    
    if (noChunked)
    {
        status = apr_file_close( file_i );
        if (status != APR_SUCCESS)
            return_non_fatal( status );
        file_i = file_c;
    }
    else
    {
        status = apr_file_close( file_c );
        if (status != APR_SUCCESS)
            return_non_fatal( status );
    }
    
    
    if (_modifications == NULL)
    {
        _modifications = new( _shm_alloc ) Versions();
    }
    else
    {
        // Clear MODIFICATIONS first
        pso_clear_VERSIONS( _modifications, _shm_alloc );
    }
    
    status = pso_unmarshall_VERSIONS_from_file( file_i, _modifications, _shm_alloc, &_globule_export_path, &error_str );
    if (status != APR_SUCCESS)
    {
        DPRINTF( DEBUG1, ("psoaudit: Cannot unmarshall MODIFICATIONS: %s\n", error_str ));
        return_non_fatal( status );
    }
    
    DPRINTF( DEBUG0, ("Globule's export path is %s\n", _globule_export_path ));

    return APR_SUCCESS;
}


apr_status_t file_chunked2identity( apr_file_t *chunked_file, apr_file_t *ident_file, bool *noChunked_out  )
{
    apr_status_t status=APR_SUCCESS;
    apr_size_t bytes_read=0, soffset=0, eoffset=0;
    
    *noChunked_out = false;
    
    soffset = pso_find_file_keyword( chunked_file, 0, "Transfer-Encoding:", FALSE );
    if (soffset == -1)
    {
        *noChunked_out = true;
        return APR_SUCCESS;
    }
    soffset = pso_skip_file_whitespace( chunked_file, soffset );
    if (soffset == -1)
        return APR_EINVAL;
    eoffset = pso_find_file_keyword( chunked_file, soffset, CRLF, TRUE );
    if (eoffset == -1)
        return APR_EINVAL;
    char *encoding_str = pso_copy_file_tostring( chunked_file, soffset, eoffset );
    
    DPRINTF( DEBUG5, ("psoaudit: Encoding is %s\n", encoding_str ));
    
    if (strcasestr( encoding_str, "chunked" ) == NULL)
    {
        *noChunked_out = true;
        return APR_SUCCESS;
    }

    /*
     * From RFC2616:
     *     length := 0
     *  read chunk-size, chunk-extension (if any) and CRLF
     *  while (chunk-size > 0) {
     *     read chunk-data and CRLF
     *     append chunk-data to entity-body
     *     length := length + chunk-size
     *     read chunk-size and CRLF
     *  }
     *  read entity-header
     *  while (entity-header not empty) {
     *     append entity-header to existing header fields
     *     read entity-header
     *  }
     *  Content-Length := length
     *  Remove "chunked" from Transfer-Encoding
     */
    // Find start of body
    char *crlfcrlf = arnopr_strcat( 481, CRLF, CRLF, NULL );
    soffset = pso_find_file_keyword( chunked_file, 0, crlfcrlf, TRUE );
    delete[] crlfcrlf;
    if (soffset == -1)
    {
        return APR_EINVAL;
    }
    soffset += strlen( CRLF )+strlen( CRLF );

    do
    {
        // Get chunk length
        eoffset = pso_find_file_keyword( chunked_file, soffset, CRLF, TRUE );
        char *chunk_str = pso_copy_file_tostring( chunked_file, soffset, eoffset );
        // Handle (and ignore) chunk extensions
        char *chunk_ext_str = strchr( chunk_str, ';' );
        if (chunk_ext_str != NULL)
            *chunk_ext_str = '\0';
        apr_size_t chunk_len=0;
        int nitems = sscanf( chunk_str, "%lx", &chunk_len );
        
        DPRINTF( DEBUG5, ("psoaudit: c2i: Reading chunk size %d [%s]\n", chunk_len, chunk_str ));
        
        delete[] chunk_str;
        if (nitems != 1)
            return APR_EINVAL;
        
        if (chunk_len == 0)
            break;
        
        soffset = eoffset+strlen( CRLF );
        eoffset = soffset+chunk_len;
        char *block = pso_copy_file_toblock( chunked_file, soffset, eoffset );

        apr_size_t bytes2writewritten = chunk_len;
        status = apr_file_write( ident_file, block, &bytes2writewritten );
        delete[] block;
        if (status != APR_SUCCESS)
            return status;
    
        soffset = eoffset+strlen( CRLF );
        
        DPRINTF( DEBUG5, ("psoaudit: c2i: new chunk-len soffset is %d\n", soffset ));
        
    } while( apr_file_eof( chunked_file ) == APR_SUCCESS );
    
    return APR_SUCCESS;
}




apr_status_t send_ipaddrportvector( char *uri_str, 
                                    IPAddrPortVector *vec, 
                                    apr_pool_t *pool )
{
    apr_socket_t *sock=NULL;
    apr_sockaddr_t *remote_sa = NULL, *local_sa = NULL;
    apr_port_t remote_port,local_port;
    char *local_ipaddr=NULL, *remote_ipaddr=NULL;
    char buf[ RECVBUF_SIZE ];
    char *error_str=NULL;
    apr_status_t status=APR_SUCCESS;
    apr_size_t length=0;
    
    DPRINTF( DEBUG1, ("psoaudit: sending vector of (ip,port): Making socket address...\n"));
    
    status = apr_sockaddr_info_get( &remote_sa, _master_ipaddr_str, APR_INET, _master_port, 0, pool);
    if (status != APR_SUCCESS) 
        return_non_fatal( status );

    DPRINTF( DEBUG5,  ("psoaudit: sending vector of (ip,port): Creating new socket..\n"));
    status = apr_socket_create(&sock, remote_sa->family, SOCK_STREAM, pool);
    if( status != APR_SUCCESS) 
        return_non_fatal( status );
        
    DPRINTF( DEBUG5,  ("psoaudit: sending vector of (ip,port): Setting socket timeout..\n"));
    status = apr_socket_timeout_set(sock, GETMODSOCKET_TIMEOUT );
    if( status != APR_SUCCESS) 
        return_non_fatal( status );
    
    DPRINTF( DEBUG5,  ("psoaudit: sending vector of (ip,port): Connecting to socket..\n"));
    status = apr_socket_connect(sock, remote_sa);
    if( status != APR_SUCCESS) 
    {
        apr_socket_close(sock);
        return_non_fatal( status );
    }

    apr_socket_addr_get(&remote_sa, APR_REMOTE, sock);
    apr_sockaddr_ip_get(&remote_ipaddr, remote_sa);
    apr_sockaddr_port_get(&remote_port, remote_sa);
    apr_socket_addr_get(&local_sa, APR_LOCAL, sock);
    apr_sockaddr_ip_get(&local_ipaddr, local_sa);
    apr_sockaddr_port_get(&local_port, local_sa);
    DPRINTF( DEBUG5, ("psoaudit: sending vector of (ip,port): %s:%u -> %s:%u\n", local_ipaddr, local_port, remote_ipaddr, remote_port));

    // Don't want chunked transfer response
    char *port_str=NULL;
    int ret = pso_Ntoa( &_master_port, sizeof( _master_port ), &port_str, pool ); 
    char *request = apr_psprintf( pool, "PUT %s HTTP/1.1%sHost: %s:%s%sTransfer-Encoding: chunked%sAuthorization: Basic %s%s%s", 
                                    uri_str, CRLF,
                                    _master_ipaddr_str, port_str, CRLF,
                                    /* transfer encoding */ CRLF,
                                    _base64_userpass, CRLF,
                                    CRLF );
    
    DPRINTF( DEBUG5,  ("psoaudit: sending vector of (ip,port): Trying to send data over socket..\n"));
    length = strlen( request );
    status = apr_socket_send( sock, request, &length);
    if (status != APR_SUCCESS)
    {
        apr_socket_close(sock);
        return_non_fatal( status );
    }

    try
    {
        vec->marshallAndSendChunked( sock );
    }
    catch( AprError e )
    {
        return_non_fatal( e.getStatus() );
    }
        
    bool masterUp=false; // to handle case where master is down
    // Otherwise we ignore the response from the master
    while( 1 )
    {
        DPRINTF( DEBUG5,  ("psoaudit: sending vector of (ip,port): Trying to receive data over socket..\n"));
        
        length = RECVBUF_SIZE; 
        status = apr_socket_recv( sock, buf, &length);
        // Special semantics: bytes may have been received and status may be APR_EOF
        if (status != APR_SUCCESS && status != APR_EOF && status != APR_TIMEUP)
        {
            apr_socket_close(sock);
            return_non_fatal( status );
        }

#if CDL >= DEBUG5        
        {
            char bla[ length+1 ];
            memcpy( bla, buf, length );
            bla[ length ] = '\0';
            DPRINTF( DEBUG1, ("psoaudit: Got data from master: ^%s$\n", bla ));
        } 
#endif                  
        if (length > 0)
            masterUp=true;
        
        if (status == APR_EOF || status == APR_TIMEUP)
            break;
    }

    DPRINTF( DEBUG5,  ("psoaudit: sending vector of (ip,port): Shutting down socket..\n"));
    status = apr_socket_shutdown(sock, APR_SHUTDOWN_WRITE);
    if (status != APR_SUCCESS)
    {
        apr_socket_close(sock);
        return_non_fatal( status );
    }

    DPRINTF( DEBUG5,  ("psoaudit: sending vector of (ip,port): Closing down socket..\n"));
    status = apr_socket_close(sock);
    if (status != APR_SUCCESS)
    {
        return_non_fatal( status );
    }
    
    if (!masterUp)
        return APR_EOF;
    
    return APR_SUCCESS;
}











/* =======================================================================
 * Util
 */



/*
 * cf. apr_pstrcat()
 * The caller is responsible for deallocating the memory of the return value.
 */
char *arnopr_strcat( int dummy, ... )
{
    va_list ap;
    char *ptr=NULL, *concat=NULL;
    apr_size_t total=0;
    
    va_start( ap, dummy );
    while( (ptr = va_arg( ap, char *)) != NULL)
    {
        total += strlen( ptr );
    }
    va_end( ap );
    
    concat = new char[ total+1 ]; // +1 for string
    concat[0] = '\0';

    va_start( ap, dummy );
    while( (ptr = va_arg( ap, char *)) != NULL)
    {
        strcat( concat, ptr );
    }
    va_end( ap );

    return concat;
}

/*
 * Example: memcat( &resultlen, s, strlen( s ), buf, buf_len, NULL );
 *
 * The caller is responsible for deallocating the memory of the return value.
 */
char *arnopr_memcat( apr_size_t *concat_len_out, ... )
{
    va_list ap;
    char *ptr=NULL, *concat=NULL;
    apr_size_t total=0,len=0,idx=0;
    
    va_start( ap, concat_len_out );
    while( (ptr = va_arg( ap, char *)) != NULL)
    {
        len = va_arg( ap, apr_size_t );
        total += len;
    }
    va_end( ap );
    
    concat = new char[ total ];
    *concat_len_out = total;

    va_start( ap, concat_len_out );
    while( (ptr = va_arg( ap, char *)) != NULL)
    {
        len = va_arg( ap, apr_size_t );
        memcpy( &concat[idx], ptr, len );
        idx += len;
    }
    va_end( ap );

    return concat;
}

