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
#include "../globule/utilities.h"

#define APR_WANT_STRFUNC
#define APR_WANT_MEMFUNC
#include "apr_want.h"

#include <math.h>

/* 
 * Return offset of next non-whitespace character
 * @param buf  source
 * @param offset    offset in source
 * @param length  length of source
 * @return new offset or -1 for end of source.
 */
apr_size_t pso_skip_whitespace( char *buf, apr_size_t offset, apr_size_t length )
{
    apr_size_t i=0;
    for (i=offset; i<length; i++)
    {
        if (buf[i] != APR_ASCII_BLANK && buf[i] != APR_ASCII_TAB 
            && buf[i] != APR_ASCII_CR && buf[i] != APR_ASCII_LF)
            return i;
    }
    return -1; // end of buffer
}


/* 
 * Return offset of next occurence of a keyword
 * @param buf  source
 * @param offset    offset in source
 * @param length  length of source
 * @return new offset or -1 for end of source.
 */
apr_size_t pso_find_keyword( char *buf, apr_size_t offset, apr_size_t length, char *keyword )
{
    apr_size_t i=offset;

    DPRINTF( DEBUG5, ("find keyword from %d to %d\n", offset, length ));
    if (length == 0)
        return -1;
    
    for (i=offset; i<length-strlen( keyword )+1; i++)
    {
        if (buf[i] == keyword[0])
        {
            if (!strncasecmp( &buf[i], keyword, strlen( keyword )))
                return i;
        }
    }

    return -1; //end of buffer
}


/* 
 * Return a string that is a copy of the specified region
 * @param src source
 * @param soff    start offset in source
 * @param eoff    end offset in source
 * @param pool  pool to allocate string from
 * @return zero-terminated string with copy of region
 */
char *pso_copy_tostring( char *src, apr_size_t soff, apr_size_t eoff, apr_pool_t *pool )
{
    char *buf = (char *)apr_palloc( pool, eoff-soff+1 ); // +'\0'
    memcpy( buf, src+soff, eoff-soff );
    buf[ eoff-soff ] = '\0';
    return buf;
}


char *pso_copy_tostring_malloc( char *src, apr_size_t soff, apr_size_t eoff )
{
    char *buf = (char *)malloc( eoff-soff+1 ); // +'\0'
    memcpy( buf, src+soff, eoff-soff );
    buf[ eoff-soff ] = '\0';
    return buf;
}




apr_size_t pso_find_file_keyword( apr_file_t *file, apr_size_t soffset, char *keyword, apr_bool_t CaseSens )
{
    char c;
    char buf[ strlen( keyword )-1 ];
    apr_off_t off=0;
    apr_status_t status=APR_SUCCESS;
    
    if (strlen( keyword ) == 0)
        return -1;
    
    off = soffset;
    while( 1 )
    {
        status = apr_file_seek( file, APR_SET, &off );
        if (status != APR_SUCCESS)
            return -1;

        status = apr_file_getc( &c, file );
        if (status != APR_SUCCESS)
            return -1;
        if (c == keyword[0])
        {
            apr_size_t nbytes = strlen( keyword )-1;
            
            status = apr_file_read( file, buf, &nbytes );
            if (status != APR_SUCCESS)
                return -1;
            else if (nbytes != strlen( keyword )-1)
                return -1;
            
            if (CaseSens)
            {
                if (!strncmp( &keyword[1], buf, strlen( keyword )-1))
                {
                    return off;
                }
            }
            else
            {
                if (!strncasecmp( &keyword[1], buf, strlen( keyword )-1))
                {
                    return off;
                }
            }
        }
        off++;
    }
}


apr_size_t pso_skip_file_whitespace( apr_file_t *file, apr_size_t soffset )
{
    char c;
    apr_off_t off=0;
    apr_status_t status=APR_SUCCESS;
    
    off = soffset;
    status = apr_file_seek( file, APR_SET, &off );
    if (status != APR_SUCCESS)
        return -1;
    
    while( 1 )
    {
        status = apr_file_getc( &c, file );
        if (status != APR_SUCCESS)
            return -1;

        if (c != APR_ASCII_BLANK && c != APR_ASCII_TAB 
            && c != APR_ASCII_CR && c != APR_ASCII_LF)
        {
            return off;
        }
        off++;
    }
}


char *pso_copy_file_tostring( apr_file_t *file, apr_size_t soff, apr_size_t eoff )
{
    char *buf = NULL;
    apr_status_t status = APR_SUCCESS;
    
    buf = malloc( eoff-soff+1 ); // +'\0' for string
    
    status = apr_file_seek( file, APR_SET, (apr_off_t *)&soff );
    if (status != APR_SUCCESS)
        return NULL;

    apr_size_t nbytes = eoff-soff;
    buf[ nbytes ] = '\0';

    status = apr_file_read( file, buf, &nbytes );
    if (status != APR_SUCCESS)
        return NULL;
    else if (nbytes != (eoff-soff))
        return NULL;

    return buf;
}


char *pso_copy_file_toblock( apr_file_t *file, apr_size_t soff, apr_size_t eoff )
{
    char *buf = NULL;
    apr_status_t status = APR_SUCCESS;
    
    buf = malloc( eoff-soff );
    
    status = apr_file_seek( file, APR_SET, (apr_off_t *)&soff );
    if (status != APR_SUCCESS)
        return NULL;

    apr_size_t nbytes = eoff-soff;
    status = apr_file_read( file, buf, &nbytes );
    if (status != APR_SUCCESS)
        return NULL;
    else if (nbytes != (eoff-soff))
        return NULL;

    return buf;
}










#define BLOCK_LEN   24

apr_status_t pso_create_random_basename( char **basename, apr_pool_t *pool )
{
    apr_status_t status=APR_SUCCESS;
    char block[BLOCK_LEN];
    char *encoded_block, hex[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
    apr_size_t encoded_len, encoded_len2=0;
    apr_off_t i=0;

    *basename = NULL;

    status = apr_generate_random_bytes( block, BLOCK_LEN );
    if (status != APR_SUCCESS)
        return status;

    if (pool == NULL)
        encoded_block = malloc( 2*BLOCK_LEN + 1 );
    else
        encoded_block = apr_palloc( pool, 2*BLOCK_LEN + 1 );

    // BASE64 won't work with / : and stuff.
    for (i=0; i<BLOCK_LEN; i++)
    {
        unsigned char c,hi,lo;
        c = block[i];
        hi = c / 16;
        lo = c % 16;
        encoded_block[2*i] = hex[ hi ];
        encoded_block[2*i+1] = hex[ lo ];
    }
    encoded_block[ 2*BLOCK_LEN ] = '\0';

    *basename = encoded_block;
    return APR_SUCCESS;
}


apr_status_t pso_create_random_number( long *random_out )
{
    char block[sizeof(long)];
    long f=0L;
    apr_status_t status=APR_SUCCESS;
    int i=0;
    
    status = apr_generate_random_bytes( block, sizeof( long ));
    if (status != APR_SUCCESS)
        return status;

    for (i=0; i<sizeof(long); i++)
    {
        f += block[i] * pow10( 256.0, (double)(sizeof(long)-i-1) );
    }
    *random_out = f;
    
    return APR_SUCCESS;
}


#include <errno.h>

static const char *sizeof2fmt( size_t sizeofN )
{
    if (sizeofN == sizeof( unsigned char ))
        return "%hhu";
    else if (sizeofN == sizeof( unsigned short int ))
        return "%hu";
    else if (sizeofN == sizeof( unsigned int ))
        return "%u";
    else if (sizeofN == sizeof( unsigned long int ))
        return "%lu";
    else if (sizeofN == sizeof( unsigned long long int ))
        return "%llu";
    else
        return "%hhu"; // safe for sscanf
}


int pso_atoN( const char *a, void *N, size_t sizeofN )
{
    const char *fmt=sizeof2fmt( sizeofN );
    int nitems=0;

    nitems = sscanf( a, fmt, N );
    if (nitems != 1)
    {
        errno=ERANGE;
        return -1;
    }
    return 0;
}


int pso_Ntoa( void *N, size_t sizeofN, char **a_out, apr_pool_t *pool )
{
    const char *fmt=sizeof2fmt( sizeofN );
    int nitems=0;

    if (sizeofN == sizeof( unsigned char ))
    {
        unsigned char *n = N;
        *a_out = apr_psprintf( pool, fmt, *n );
    }
    else if (sizeofN == sizeof( unsigned short int ))
    {
        unsigned short int *n = N;
        *a_out = apr_psprintf( pool, fmt, *n );
    }
    else if (sizeofN == sizeof( unsigned int ))
    {
        unsigned int *n = N;
        *a_out = apr_psprintf( pool, fmt, *n );
    }
    else if (sizeofN == sizeof( unsigned long int ))
    {
        unsigned long int *n = N;
        *a_out = apr_psprintf( pool, fmt, *n );
    }
    else if (sizeofN == sizeof( unsigned long long int ))
    {
        unsigned long long int *n = N;
        *a_out = apr_psprintf( pool, fmt, *n );
    }
    else    
        return -1;

    return 0;
}
