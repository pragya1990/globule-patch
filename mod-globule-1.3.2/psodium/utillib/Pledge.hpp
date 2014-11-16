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
#ifndef Pledge_HPP
#define Pledge_HPP

#define HOSTNOTSET  "*"

#include "mod_psodium.h"
#include "../globule/utilities.h"
#include <apr_base64.h>
#include <apr_time.h>
#include <openssl/evp.h>
#include <openssl/err.h>

#include <iostream>

class Pledge
{
public:
    char        *_slave_id; // zero-terminated string
    request_rec *_r;        // copy of request
    apr_time_t  _timestamp;  
    digest_t    _md;
    digest_len_t    _md_len;
    char *_pledge_data;  
    apr_size_t _pledge_len;
    
    Pledge( char *slave_id, request_rec *r, digest_t md, digest_len_t  md_len )
    {
        _slave_id = slave_id;
        _r = r;
        _timestamp = apr_time_now();
        _md = md;
        _md_len = md_len;
    }

    Pledge() // used in combination with unmarshall()
    {
    }
    
    apr_status_t marshallAndSign( apr_bool_t dry_run,
                          EVP_PKEY *private_key, 
                          char **marshalled_out, 
                          apr_size_t *marshalled_len_out )
    {
        char *plaintext = NULL, *time_str=NULL;
        const char *host = NULL;
        char *encoded = NULL;
        apr_size_t encoded_len, encoded_len2=0;
        EVP_MD_CTX  md_ctx;
        unsigned char *sig;
        unsigned int sig_len=0;
        int err=0;
        
        sig = (unsigned char *)apr_palloc( _r->pool, EVP_PKEY_size( private_key )*sizeof( char ) );
        
        host = getHostHeader();
        
        int ret = pso_Ntoa( &_timestamp, sizeof( _timestamp ), &time_str, _r->pool );
        
        // BASE64 the digest
        encoded_len = apr_base64_encode_len( _md_len );
        encoded = (char *)apr_palloc( _r->pool, encoded_len );
        encoded_len2 = apr_base64_encode( encoded, (const char *)_md, _md_len );
        
        plaintext = apr_pstrcat( _r->pool,  \
                         PLEDGE_START_KEYWORD, CRLF, \
                         SLAVEID_KEYWORD, " ", _slave_id, CRLF, \
                         REQUEST_URI_KEYWORD, " ", _r->unparsed_uri, CRLF \
                         REQUEST_HOST_KEYWORD, " ", host, CRLF, \
                         TIMESTAMP_KEYWORD, " ", time_str, CRLF, \
                         DIGEST_KEYWORD, " ", encoded, CRLF, \
                         NULL );
        
        if (dry_run == FALSE)
        {    
            // OPENSSL
            EVP_SignInit( &md_ctx, OPENSSL_DIGEST_ALG );
            EVP_SignUpdate( &md_ctx, plaintext, strlen( plaintext ) ); // exluding terminating '\0'
            err = EVP_SignFinal( &md_ctx, sig, &sig_len, private_key );
            if (err == 0)
            {
                DOUT( DEBUG0, "psodium: slave: Pledge: Cannot create signature." );
                return APR_EINVAL;
            }
            
            // BASE64 the sig
            encoded_len = apr_base64_encode_len( sig_len );
            encoded = (char *)apr_palloc( _r->pool, encoded_len ); 
            encoded_len2 = apr_base64_encode( encoded, (const char *)sig, sig_len );
            
            DOUT( DEBUG1, "plaintext=" << plaintext << "encoded=^" << encoded << "$SIGEND$" );
        }
        else
        {
            apr_off_t i=0;
            // Dry run to determine pledge's length, don't do crypto.
            // BUG: encoded_len returns memory needed, which may be larger than
            // the actual resultant string, which we know only after we encode
            // :-(
            encoded_len = apr_base64_encode_len( OPENSSL_SIGNATURE_ALG_NBYTES );
            encoded = (char *)apr_palloc( _r->pool, encoded_len ); 
            for (i=0; i<encoded_len; i++)
                encoded[i] = 'z';
            encoded[ encoded_len-1 ] = '\0'; // turn into string
        }
        
        *marshalled_out = apr_pstrcat( _r->pool, plaintext, \
                                        SIGNATURE_KEYWORD, " ", encoded, CRLF, \
                                        PLEDGE_END_KEYWORD, CRLF, \
                                        NULL );
        *marshalled_len_out = strlen( *marshalled_out );
        return APR_SUCCESS;
    }
    
    
    apr_status_t verifySignature( EVP_PKEY *pub_key, apr_bool_t *OK_out, char **error_str_out, apr_pool_t *pool )
    {
        apr_size_t soffset=0, eoffset=0, sigstartoffset=0;
        EVP_MD_CTX  md_ctx;
        apr_size_t decoded_len=0, decoded_len2;
        digest_t decoded = NULL;
        char *sig_str=NULL;
        int err=0;
    
        char *marshalled = _pledge_data;
        apr_size_t marshalled_len = _pledge_len;
        
        // SIGNATURE
        sigstartoffset = pso_find_keyword( marshalled, 0, marshalled_len, SIGNATURE_KEYWORD );
        if (soffset == -1)
        {
            *error_str_out = "Cannot find SIGNATURE keyword in pledge.";
            return APR_EINVAL;
        }
        soffset = pso_skip_whitespace( marshalled, sigstartoffset+strlen( SIGNATURE_KEYWORD ), marshalled_len );
        
        eoffset = pso_find_keyword( marshalled, soffset, marshalled_len, CRLF);
        if (eoffset == -1)
        {
            *error_str_out = "Cannot find CRLF after SIGNATURE keyword in pledge.";
            return APR_EINVAL;
        }
        
        sig_str = pso_copy_tostring( marshalled, soffset, eoffset, pool );

        decoded_len = apr_base64_decode_len( sig_str ); 
        decoded = (digest_t)apr_palloc( pool, decoded_len );
        decoded_len2 = apr_base64_decode( (char *)decoded, sig_str );
        
        // OPENSSL
        EVP_VerifyInit( &md_ctx, OPENSSL_DIGEST_ALG );
        EVP_VerifyUpdate( &md_ctx, marshalled, sigstartoffset );
        err = EVP_VerifyFinal( &md_ctx, decoded, decoded_len2, pub_key );
        if (err == -1)
        {
            *error_str_out = (char *)apr_palloc( pool, OPENSSL_MAX_ERROR_STRLEN );
            ERR_error_string( ERR_get_error(), *error_str_out );
            return APR_EINVAL;
        }
        else
        {
            if (err == 1)
                *OK_out = TRUE;
            else
                *OK_out = FALSE;
            return APR_SUCCESS;
        }
    }
    
    
    apr_status_t unmarshall( char *marshalled, apr_size_t marshalled_len, char **error_str_out, apr_pool_t *pool )
    {
        apr_size_t soffset=0, eoffset=0;
        int ret=0;
        char *temp=NULL;
        apr_size_t decoded_len=0, decoded_len2;
        digest_t decoded = NULL;

        _pledge_data = marshalled;
        _pledge_len = marshalled_len;
        
        // SLAVEID
        soffset = pso_find_keyword( marshalled, 0, marshalled_len, SLAVEID_KEYWORD );
        if (soffset == -1)
        {
            *error_str_out = "Cannot find slaveID";
            return APR_EINVAL;
        }
        soffset += strlen( SLAVEID_KEYWORD );
        soffset = pso_skip_whitespace( marshalled, soffset, marshalled_len );
        
        eoffset = pso_find_keyword( marshalled, soffset, marshalled_len, CRLF );
        if (eoffset == -1)
        {
            *error_str_out = "Cannot find CRLF after slaveID";
            return APR_EINVAL;
        }

        _slave_id = pso_copy_tostring( marshalled, soffset, eoffset, pool );
        
        soffset = eoffset+strlen( CRLF );

        // REQUEST 1
        soffset = pso_find_keyword( marshalled, soffset, marshalled_len, REQUEST_URI_KEYWORD );
        if (soffset == -1)
        {
            *error_str_out = "Cannot find REQUEST_URI";
            return APR_EINVAL;
        }
        soffset += strlen( REQUEST_URI_KEYWORD );
        soffset = pso_skip_whitespace( marshalled, soffset, marshalled_len );
        
        eoffset = pso_find_keyword( marshalled, soffset, marshalled_len, CRLF );
        if (eoffset == -1)
        {
            *error_str_out = "Cannot find CRLF after REQUEST_URI";
            return APR_EINVAL;
        }

        _r = (request_rec *)apr_palloc( pool, sizeof( request_rec ) );
        
        _r->unparsed_uri = pso_copy_tostring( marshalled, soffset, eoffset, pool );
        
        soffset = eoffset+strlen( CRLF );

        // REQUEST 2
        soffset = pso_find_keyword( marshalled, soffset, marshalled_len, REQUEST_HOST_KEYWORD );
        if (soffset == -1)
        {
            *error_str_out = "Cannot find REQUEST_HOST";
            return APR_EINVAL;
        }
        soffset += strlen( REQUEST_HOST_KEYWORD );
        soffset = pso_skip_whitespace( marshalled, soffset, marshalled_len );

        eoffset = pso_find_keyword( marshalled, soffset, marshalled_len, CRLF );
        if (eoffset == -1)
        {
            *error_str_out = "Cannot find CRLF after REQUEST_HOST";
            return APR_EINVAL;
        }

        temp = pso_copy_tostring( marshalled, soffset, eoffset, pool );

        // Only if set
        if (strcmp( temp, HOSTNOTSET ))
        {
            _r->headers_in = apr_table_make( pool, 1 );
            apr_table_set( _r->headers_in, "Host", temp );
        }
        
        soffset = eoffset+strlen( CRLF );
        
        // TIMESTAMP
        soffset = pso_find_keyword( marshalled, soffset, marshalled_len, TIMESTAMP_KEYWORD );
        if (soffset == -1)
        {
            *error_str_out = "Cannot find TIMESTAMP";
            return APR_EINVAL;
        }
        soffset += strlen( TIMESTAMP_KEYWORD );
        soffset = pso_skip_whitespace( marshalled, soffset, marshalled_len );
        
        eoffset = pso_find_keyword( marshalled, soffset, marshalled_len, CRLF );
        if (eoffset == -1)
        {
            *error_str_out = "Cannot find CRLF after TIMESTAMP";
            return APR_EINVAL;
        }

        temp = pso_copy_tostring( marshalled, soffset, eoffset, pool );
        ret = pso_atoN( temp, &_timestamp, sizeof( _timestamp ) ); 
        if (ret == -1)
        {
            *error_str_out = "TIMESTAMP not a number";
            return APR_EINVAL;
        }
        
        soffset = eoffset+strlen( CRLF );
        

        // DIGEST
        soffset = pso_find_keyword( marshalled, soffset, marshalled_len, DIGEST_KEYWORD );
        if (soffset == -1)
        {
            *error_str_out = "Cannot find DIGEST";
            return APR_EINVAL;
        }
        soffset += strlen( DIGEST_KEYWORD );
        soffset = pso_skip_whitespace( marshalled, soffset, marshalled_len );
        
        eoffset = pso_find_keyword( marshalled, soffset, marshalled_len, CRLF );
        if (eoffset == -1)
        {
            *error_str_out = "Cannot find CRLF after DIGEST";
            return APR_EINVAL;
        }

        temp = pso_copy_tostring( marshalled, soffset, eoffset, pool );

        decoded_len = apr_base64_decode_len( temp ); 
        decoded = (digest_t)apr_palloc( pool, decoded_len );
        decoded_len2 = apr_base64_decode( (char *)decoded, temp );

        _md = decoded;
        _md_len = decoded_len2;
        
        soffset = eoffset+strlen( CRLF );
        
        return APR_SUCCESS;
    }
    

    apr_bool_t clientCheck( char *slave_id, 
                      request_rec *r, 
                      digest_t md, 
                      digest_len_t  md_len,
                      char **error_str_out )
    {
        const char *this_host=NULL, *that_host=NULL;
        apr_time_t currentTime, Ts;
        
        if (strcmp( slave_id, this->_slave_id ))
        {
            *error_str_out = apr_psprintf( r->pool, "Slave ID in pledge is not slave ID as set by master! orig %s <> %s.", slave_id, this->_slave_id );
            return FALSE;
        }
        
        // CAREFUL: the original unparsed_uri is likely to be a proxy request, 
        // i.e., has a server part, and the slave will only have gotten a uri
        // without server part.
        
        if (strcmp( r->parsed_uri.path, this->_r->unparsed_uri ))
        {
            *error_str_out = apr_psprintf( r->pool, "Requested URI in pledge is not the original! orig %s <> %s.", r->uri, this->_r->unparsed_uri );
            return FALSE;
        }
            
        this_host = getHostHeaderIntl( this->_r );
        that_host = getHostHeaderIntl( r );
        if (strcmp( this_host, that_host ))
        {
            *error_str_out = apr_psprintf( r->pool, "Requested Host: in pledge is not the original! orig %s <> %s.", this_host, that_host );
            return FALSE;
        }
        
        currentTime = apr_time_as_msec( apr_time_now() ); // ignoring processing time since reception
        Ts = apr_time_as_msec( this->_timestamp );
        if ( (currentTime-CLOCK_DELTA) <= Ts && Ts <= (currentTime+CLOCK_DELTA))
        {
        }
        else
        {
            apr_time_t diff = Ts-currentTime;
            char *time_str=NULL;
            int ret = pso_Ntoa( &diff, sizeof( diff ), &time_str, r->pool );
            *error_str_out = apr_psprintf( r->pool, "Pledge too old! diff %s", time_str );
            return FALSE;
        }
        
        if (memcmp( md, this->_md, OPENSSL_DIGEST_ALG_NBITS/8 ))
        {
            *error_str_out = "Digest in pledge doesn't match calculated digest!";
            return FALSE;
        }
        
        return TRUE;
    }
    
    const char *getHostHeader()
    {
        return this->getHostHeaderIntl( _r );
    }
    
protected:     
    const char *getHostHeaderIntl( request_rec *r )
    {
        const char *host = NULL;
        host = apr_table_get( this->_r->headers_in, "Host" );
        if (host == NULL)
            host = HOSTNOTSET;
        return host;
    }
        
};

#endif
