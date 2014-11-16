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
/*============================================================================
 * IPAddrPortVector.hpp
 * A vector of (host,port) pairs. It uses shared memory 
 * so it can also be used in mod_psodium.
 * 
 * Author: arno
 *============================================================================
 */
#ifndef _IPAddrPortVector_H
#define _IPAddrPortVector_H

#include "../globule/alloc/Allocator.hpp"
#include "utillib/AprError.hpp"
#include <apr.h>
#include "utillib/globule/ConcurrencyControl.hpp"
#include <set>
#include <string>
using std::string;

#define IPADDRPORTVEC_START_KEYWORD     "----- BEGIN IPADDRPORTVEC -----"
#define IPADDRPORTVEC_END_KEYWORD     "----- END IPADDRPORTVEC -----"
#define IPADDRPORTNO_KEYWORD              "IPADDRPORTNO"


struct ipaddrport
{
    char *ipaddr_str;
    apr_port_t  port;
};

// STL Comparator
struct ltipaddrport
{
  bool operator()(const struct ipaddrport* s1, const struct ipaddrport* s2) const
  {
      // Sort on ipaddr, then on portno
    return strcmp(s1->ipaddr_str, s2->ipaddr_str) < 0 || (!strcmp(s1->ipaddr_str, s2->ipaddr_str) && s1->port < s2->port);
  }
};

//typedef Gvector(struct ipaddrport *)  IPAddrPortBaseClass;
typedef std::set<const struct ipaddrport *,ltipaddrport,rmmallocator<const struct ipaddrport *> >    IPAddrPortBaseClass;


class IPAddrPortVector : public IPAddrPortBaseClass
{
protected:
    AprGlobalMutex *_mutex;
    rmmmemory *_shm_alloc;
public:
    IPAddrPortVector( rmmmemory *shm_alloc, const char *LockFilenamePrefix, apr_pool_t *pool ) throw( AprError )
    {
        _mutex = AprGlobalMutex::allocateMutex();
        _shm_alloc = shm_alloc;
    }


    ~IPAddrPortVector() throw( AprError )
    {
        operator delete( _mutex, _shm_alloc );
    }

    
    inline void lock()
    {
        LOCK( _mutex );
    }

    inline void unlock()
    {
        UNLOCK( _mutex );
    }
    
    void insert( char *ipaddr_str, apr_port_t port ) throw( AprError )
    {
        DPRINTF( DEBUG1, ("psodium: any: Putting %s:%hu on LYINGCLIENTS/BADSLAVES list\n", ipaddr_str, port ));
        char *shm_ipaddr = new( _shm_alloc ) char[ strlen( ipaddr_str )+1 ]; // +1 for string
        strcpy( shm_ipaddr, ipaddr_str );
        struct ipaddrport *rec = new( _shm_alloc ) ipaddrport;
        rec->ipaddr_str = shm_ipaddr;
        rec->port = port;

        this->lock();
        // Huh???
        ((IPAddrPortBaseClass *)this)->insert( ((IPAddrPortBaseClass *)this)->begin(), rec );
        this->unlock();
    }

    /*
     * Marshall vector and send it in Transfer-Encoding: chunked over socket
     */
    void marshallAndSendChunked( apr_socket_t *sock ) throw( AprError )
    {
        apr_status_t status=APR_SUCCESS;
        IPAddrPortBaseClass *self = (IPAddrPortBaseClass *)this;
        int i=0;
        apr_size_t len=0,clen=0;
        char num_str[ NUMBER_MAX_STRLEN( apr_size_t )+1 ]; // This is for decimal, but we must use hex
        
        // chunked
        sprintf( num_str, "%x%s", strlen( IPADDRPORTVEC_START_KEYWORD )+strlen( CRLF ), CRLF );
        clen = strlen( num_str );
        status = apr_socket_send( sock, num_str, &clen );
        if (status != APR_SUCCESS)
            throw AprError( status ); // no new
        
        len = strlen( IPADDRPORTVEC_START_KEYWORD );
        status = apr_socket_send( sock, IPADDRPORTVEC_START_KEYWORD, &len );
        if (status != APR_SUCCESS)
            throw AprError( status ); // no new
        len = strlen( CRLF );
        status = apr_socket_send( sock, CRLF, &len );
        if (status != APR_SUCCESS)
            throw AprError( status ); // no new
        // chunked
        clen = strlen( CRLF );
        status = apr_socket_send( sock, CRLF, &clen );
        if (status != APR_SUCCESS)
            throw AprError( status ); // no new

        
        for (IPAddrPortVector::iterator i = self->begin(); i != self->end(); i++)
        {
            const struct ipaddrport *pair = *i;
            apr_size_t len = strlen( IPADDRPORTNO_KEYWORD ) 
                             + strlen( " " ) 
                             + strlen( pair->ipaddr_str ) 
                             + strlen( ":") +  NUMBER_MAX_STRLEN( apr_port_t ) 
                             + strlen( CRLF )+ 1; // +1 for string
            char buf[ len ];

            
            len = sprintf( buf, "%s %s:%hu%s", IPADDRPORTNO_KEYWORD, pair->ipaddr_str, pair->port, CRLF );

            // chunked
            sprintf( num_str, "%x%s", len, CRLF );
            clen = strlen( num_str );
            status = apr_socket_send( sock, num_str, &clen );
            if (status != APR_SUCCESS)
                throw AprError( status ); // no new
            
            status = apr_socket_send( sock, buf, &len );
            if (status != APR_SUCCESS)
                throw AprError( status ); // no new

            // chunked
            clen = strlen( CRLF );
            status = apr_socket_send( sock, CRLF, &clen );
            if (status != APR_SUCCESS)
                throw AprError( status ); // no new
        }

        // chunked
        sprintf( num_str, "%x%s", strlen( IPADDRPORTVEC_END_KEYWORD )+strlen( CRLF ), CRLF );
        clen = strlen( num_str );
        status = apr_socket_send( sock, num_str, &clen );
        if (status != APR_SUCCESS)
            throw AprError( status ); // no new

        
        len = strlen( IPADDRPORTVEC_END_KEYWORD );
        status = apr_socket_send( sock, IPADDRPORTVEC_END_KEYWORD, &len );
        if (status != APR_SUCCESS)
            throw AprError( status ); // no new
        len = strlen( CRLF );
        status = apr_socket_send( sock, CRLF, &len );
        if (status != APR_SUCCESS)
            throw AprError( status ); // no new

        // chunked
        clen = strlen( CRLF );
        status = apr_socket_send( sock, CRLF, &clen );
        if (status != APR_SUCCESS)
            throw AprError( status ); // no new

        // Indicate that was last chunk, and there are no trailers (cf.headers)
        
        // chunked
        sprintf( num_str, "%x%s%s", 0, CRLF, CRLF );
        clen = strlen( num_str );
        status = apr_socket_send( sock, num_str, &clen );
        if (status != APR_SUCCESS)
            throw AprError( status ); // no new
    }
    

    /*
     * pre: The chunked transfer encoding has been removed
     * (Apache input filter does this automatically)
     * pre: The chunks as created in marshallAndSendChunked() are preserved.
     */
    void addFromMarshalledBlock( const char *block, apr_size_t block_len ) throw( AprError )
    {
        apr_port_t portnum=0;
        int ret = 0;

        DPRINTF( DEBUG4, ("Unmarshalling block %s of length %ld\n", block, block_len ));

        apr_size_t soffset=0, eoffset=0;
        soffset = 0;
        while( soffset < block_len)
        {
            soffset = pso_skip_whitespace( (char *)block, soffset, block_len );
            if (soffset == -1)
                break;
        
            eoffset = pso_find_keyword( (char *)block, soffset, block_len, CRLF );
            if (eoffset == -1)
            {
                // No CRLF yet, save this block
                DPRINTF( DEBUG1, ("psodium: master: IPAddrPortVector: ************* intelligent unmarshaller not yet implemented!\n" ));
                throw AprError( APR_EINVAL );
            }
            

            char *line = pso_copy_tostring_malloc( (char *)block, soffset, eoffset );

            if (!strcmp( line, IPADDRPORTVEC_START_KEYWORD ))
            {
            }
            else if(!strcmp( line, IPADDRPORTVEC_END_KEYWORD ))
            {
                free( line );
                break;
            }
            else if (!strncmp( line, IPADDRPORTNO_KEYWORD, strlen( IPADDRPORTNO_KEYWORD )))
            {
                char *ptr = strchr( line, ' ' );
                if (ptr == NULL)
                    throw AprError( APR_EINVAL );

                char *ipaddrport_str = ptr+1; 
                ptr = strchr( ipaddrport_str, ':' );
                if (ptr == NULL)
                {
                    DPRINTF( DEBUG1, ("psodium: master: IPAddrPortVector: Not hostname:port pair while unmarshalling!\n" ));
                    throw AprError( APR_EINVAL );
                }
                char *port_str = ptr+1;
                *ptr = '\0';
                
                ret = pso_atoN( port_str, &portnum, sizeof( portnum ) );
                if (ret == -1)
                    throw AprError( APR_EINVAL );
                this->insert( ipaddrport_str, portnum ); // locks
            }
            else
            {
                DPRINTF( DEBUG1, ("psodium: master: IPAddrPortVector: Bogus keyword while unmarshalling!\n" ));
                throw AprError( APR_EINVAL );
            }
            soffset = eoffset+strlen( CRLF );
            
            free( line );
        }
    }


    /*
     * Marshall vector and save it in the file
     */
    void marshallToFile( const char *filename, apr_pool_t *pool) throw( AprError )
    {
        apr_status_t status=APR_SUCCESS;
        IPAddrPortBaseClass *self = (IPAddrPortBaseClass *)this;
        apr_file_t *file=NULL;
        
        if (this->size() == 0)
            return;

        status = apr_file_open( &file, filename, APR_CREATE|APR_WRITE|APR_BINARY, APR_OS_DEFAULT, pool );
        if (status != APR_SUCCESS)
            throw AprError( status );

        for (IPAddrPortVector::iterator i = self->begin(); i != self->end(); i++)
        {
            const struct ipaddrport *pair = *i;
            char *port_str=NULL;
            int ret = pso_Ntoa( (void *)&pair->port, sizeof( pair->port ), &port_str, pool );
            const char *line = apr_pstrcat( pool, pair->ipaddr_str, ":", port_str, CRLF, NULL );
            apr_size_t bufsize_byteswritten=strlen( line );
            status = apr_file_write( file, line, &bufsize_byteswritten );
            if (status != APR_SUCCESS)
            {
                (void)apr_file_close( file );
                throw AprError( status ); // no new
            }
        }
        
        status = apr_file_close( file );
        if (status != APR_SUCCESS)
            throw AprError( status );
    }

    /*
     * Read vector and unmarshall from file 
     */
    void unmarshallFromFile( const char *filename, apr_pool_t *pool) throw( AprError )
    {
        apr_status_t status=APR_SUCCESS;
        int i=0;
        apr_file_t *file=NULL;
        char ch=0;
        string ipaddr_s="", port_s="";
        
        status = apr_file_open( &file, filename, APR_READ|APR_BINARY, APR_OS_DEFAULT, pool );
        if (APR_STATUS_IS_ENOENT( status ))
            return;
        else if (status != APR_SUCCESS)
            throw AprError( status );
        
        int flag=0;
        do
        {
            status = apr_file_getc( &ch, file );
            if (status == APR_EOF)
            {
                if (flag == 1)
                    this->insert( ipaddr_s, port_s );
                break;
            }
            else if (status != APR_SUCCESS)
            {
                (void)apr_file_close( file );
                throw AprError( status );
            }

            if (ch == ':')
            {
                // in port
                flag = 1; 
            }
            else if (flag == 1 && !isdigit(ch))
            {
                // end of port
                flag=2; // in EOL

                this->insert( ipaddr_s, port_s );
                ipaddr_s="";
                port_s="";
            }
            else if (flag == 2 && isalnum(ch))
            {
                flag=0; // in hostname/IP addr
            }

            if (isblank(ch) || ch == APR_ASCII_CR || ch == APR_ASCII_LF || ch == ':')
            {
                // Ignore whitespace. ':', and EOL
                continue;
            }

            if (flag == 0)
            {
               ipaddr_s+=ch; 
            }
            else if (flag == 1)
                port_s+=ch;

        } while( 1 );
        
        status = apr_file_close( file );
        if (status != APR_SUCCESS)
            throw AprError( status );
    }


    void insert( string ipaddr_s, string port_s ) throw( AprError )
    {   
        apr_port_t port;
        int ret = pso_atoN( port_s.c_str(), &port, sizeof( port ) );
        if (ret == -1)
        {
            DOUT( DEBUG0, "Port not number" );
            throw AprError( EINVAL );
        }
        this->insert( (char *)ipaddr_s.c_str(), port );
    }
};

#endif  /* _IPAddrPortVector_H */

