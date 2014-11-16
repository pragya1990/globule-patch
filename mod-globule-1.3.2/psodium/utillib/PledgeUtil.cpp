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
#include "Pledge.hpp"
#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/x509.h>
#include <openssl/pem.h>

static apr_status_t extract_digest_from_pledge( char *pledge_data,
                                                apr_size_t pledge_curr_length,
                                                char **pledge_digest_str_out,
                                                char **error_str_out,
                                                apr_pool_t *pool );


extern "C" {


void retrieve_pledge( request_rec *r, char **pledge_data_out, apr_size_t *pledge_curr_length_out )
{ 
    /*
     * We need to save the pledge while processing the browser's request. 
     * This means we have to potentially keep it while we contact the master
     * to double check. This means we cannot use memory allocated in the
     * HTTP response handler (like newbb_out). Hence, we use the r->notes 
     * table, and use BASE64 encoding to encode the pledge, as it is input
     * from the slave, which may be maliciously sending binary data!
     */
    const char *pledge_base64 = apr_table_get( r->notes, STORED_PLEDGE_NOTE );
    //const char *pledge_len_str = apr_table_get( r->notes, STORED_PLEDGE_LENGTH_NOTE );
    //int ret = pso_atoN( pledge_len_str, pledge_curr_length_out, sizeof( *pledge_curr_length_out ) );
    decode_pledge( r->pool, pledge_base64, pledge_data_out, pledge_curr_length_out );
}
   

const char *retrieve_pledge_encoded( request_rec *r )
{
    return apr_table_get( r->notes, STORED_PLEDGE_NOTE );
}

void decode_pledge( apr_pool_t *pool, const char *pledge_base64,  char **pledge_data_out, apr_size_t *pledge_curr_length_out )
{
    if (pledge_base64 == NULL)
    {
        *pledge_data_out = NULL;
        *pledge_curr_length_out=0;
    }
    else
    {
        apr_size_t decoded_len=0, decoded_len2=0;

        decoded_len = apr_base64_decode_len( pledge_base64 ); 
        *pledge_data_out = (char *)apr_palloc( pool, decoded_len );
        decoded_len2 = apr_base64_decode( *pledge_data_out, pledge_base64 );
        //ap_assert( decoded_len2 <= decoded_len );
        *pledge_curr_length_out = decoded_len2;
    }
}


void store_pledge( request_rec *r, char *pledge_data, apr_size_t pledge_curr_length )
{ 
    // Store pledge again as BASE64 in r->notes
    apr_size_t encoded_len = apr_base64_encode_len( pledge_curr_length );
    char *pledge_base64 = (char *)apr_palloc( r->pool, encoded_len+1 );
    apr_size_t encoded_len2 = apr_base64_encode( pledge_base64, pledge_data, pledge_curr_length );
    //ap_assert( encoded_len >= encoded_len2 );
    // APR don't guarantee zero-terminatedness
    if (pledge_base64[ encoded_len ] != '\0')
        pledge_base64[ encoded_len ] = '\0';
    apr_table_set( r->notes, STORED_PLEDGE_NOTE, pledge_base64 );
    char *pledge_base64_len_str=NULL;
    int ret = pso_Ntoa( &encoded_len2, sizeof( encoded_len2 ), &pledge_base64_len_str, r->pool );
    apr_table_set( r->notes, STORED_PLEDGE_LENGTH_NOTE, pledge_base64_len_str );
}



// TODO: write a more efficient version
apr_status_t pso_slave_determine_marshalled_pledge_length( psodium_conf *conf, 
                                                           request_rec *r, 
                                                           apr_size_t *pledge_length_out )
{
    apr_status_t status = APR_SUCCESS;
    //Alloc Pledge on stack
    digest_len_t  md_len = OPENSSL_DIGEST_ALG_NBITS/8;
    unsigned char md[ OPENSSL_DIGEST_ALG_NBITS/8 ]; // no digest_t, otherwise 
                                                    // I have to allocate Pledge
                                                    // off stack somewhere
    char *marshalled_pledge=NULL;
    apr_size_t marshalled_pledge_len=0;
    Pledge p( conf->slaveid, r, md, md_len );

    DPRINTF( DEBUG5, ("psodium: slave: Marshalling and signing pledge (dryrun for length).\n" )); 

    status = p.marshallAndSign( TRUE, NULL, &marshalled_pledge, pledge_length_out );
    if (status != APR_SUCCESS)
        return status;

    DPRINTF( DEBUG5, ("psodium: slave: Done marshalling and signing pledge (dryrun for length).\n" )); 

    return APR_SUCCESS;
}



apr_status_t pso_slave_pledge_to_bb( psodium_conf *conf,
                               request_rec *r, 
                               digest_t md,
                               digest_len_t  md_len,
                               apr_bucket_brigade **bb_out )
{
    apr_status_t status=APR_SUCCESS;
    char *marshalled_pledge=NULL;
    apr_size_t marshalled_pledge_len=0;
    Pledge p( conf->slaveid, r, md, md_len );

    DPRINTF( DEBUG1, ("psodium: slave: Marshalling and signing pledge (for real).\n" )); 

    status = p.marshallAndSign( FALSE, conf->slaveprivatekey, &marshalled_pledge, &marshalled_pledge_len );
    if (status != APR_SUCCESS)
        return status;

    DPRINTF( DEBUG5, ("psodium: slave: Done marshalling and signing pledge (for real).\n" )); 

    *bb_out = apr_brigade_create(r->pool, r->connection->bucket_alloc );
    if (*bb_out == NULL)
    {
        return APR_ENOMEM;
    }

    DPRINTF( DEBUG1, ("psodium: slave: Writing pledge to bucket brigade.\n" )); 

    return apr_brigade_write( *bb_out, NULL, NULL, marshalled_pledge, marshalled_pledge_len );
}



apr_status_t pso_client_check_pledge( psodium_conf *conf,
                               request_rec *our_r,
                               digest_t our_md,
                               digest_len_t  our_md_len,
                               char *slave_id,
                               char *pledge_data, 
                               apr_size_t pledge_curr_length,
                               apr_bool_t *OK_out,
                               char **error_str_out, 
                               apr_pool_t *pool )
{
    apr_status_t status=APR_SUCCESS;
    char *pubkey_pem=NULL;
    apr_bool_t match=FALSE;
    Pledge p;

    // DEBUG:pledge not guaranteed to be string!
    DPRINTF( DEBUG1, ("psodium: client: check pledge: $%s$ measlen %d, admlen %d\n", pledge_data, strlen( pledge_data ), pledge_curr_length )); 
    DPRINTF( DEBUG1, ("psodium: client: check pledge: our digest %s \n", digest2string( our_md, our_md_len, pool) )); 

    // 1. Unmarshall pledge
    status = p.unmarshall( pledge_data, pledge_curr_length, error_str_out, pool );
    if (status != APR_SUCCESS)
        return status;

    // 2. Get slave's public key
    status = pso_find_pubkey( conf, slave_id, &pubkey_pem, pool );
    if (status != APR_SUCCESS || pubkey_pem == NULL)
    {
        *error_str_out = "Cannot find slave's public key in slave cache.";
        *OK_out = FALSE;
        return APR_SUCCESS;
    }
    
    DPRINTF( DEBUG1, ("psodium: client: check pledge: got pubkey\n" )); 

    // 3. Check signature
    status = pso_client_check_pledge_sig( p, 
                                          pubkey_pem, 
                                          OK_out,
                                          error_str_out, 
                                          pool );
    if (status != APR_SUCCESS)
        return status;
    else if (*OK_out == FALSE)
    {
        if (*error_str_out != NULL)
            DPRINTF( DEBUG3, ("Error checking signature: %s\n", *error_str_out ));
        return APR_SUCCESS;
    }

    // 4. Do checks
    *OK_out = p.clientCheck( slave_id, our_r, our_md, our_md_len, error_str_out );

    return APR_SUCCESS;                              
}   




apr_status_t pso_client_check_pledge_sig( Pledge p, 
                                          char *pubkey_pem, 
                                          apr_bool_t *OK_out,
                                          char **error_str_out, 
                                          apr_pool_t *pool )
{
    // 1. Convert PEM to something OPENSSL can use
    // OPENSSL
    BIO *bio = BIO_new_mem_buf( pubkey_pem, -1 );
    if (bio== NULL)
    {
        *error_str_out = (char *)apr_palloc( pool, OPENSSL_MAX_ERROR_STRLEN );
        ERR_error_string( ERR_get_error(), *error_str_out );
        return APR_EOF;
    }

    X509 *x509 = PEM_read_bio_X509( bio, NULL, NULL, NULL );
    if (x509 == NULL)
    {
        *error_str_out = (char *)apr_palloc( pool, OPENSSL_MAX_ERROR_STRLEN );
        ERR_error_string( ERR_get_error(), *error_str_out );
        return APR_EOF;
    }
    EVP_PKEY *pubkey = X509_get_pubkey( x509 );
    if (pubkey == NULL)
    {
        *error_str_out = (char *)apr_palloc( pool, OPENSSL_MAX_ERROR_STRLEN );
        ERR_error_string( ERR_get_error(), *error_str_out );
        return APR_EOF;
    }

    // 2. Verify signature
    apr_status_t status = p.verifySignature( pubkey, OK_out, error_str_out, pool );
    if (status != APR_SUCCESS)
        return status;
    if (*OK_out == FALSE)
    {
        *error_str_out = "Signature on pledge is not slave's!";
    }
    return APR_SUCCESS;
}






apr_status_t pso_client_check_pledge_against_digests( digest_t *digest_array, 
                                               apr_size_t digest_array_length, 
                                               char *pledge_data,
                                               apr_size_t pledge_curr_length,
                                               apr_bool_t *match_out,
                                               char **error_str_out, 
                                               apr_pool_t *pool )
{
    apr_status_t status = APR_SUCCESS;
    apr_off_t i=0;
    Pledge p;

    DPRINTF( DEBUG1, ("psodium: client: Doing double check: Checking pledge against digests: pledge_data = ^%s$\n", pledge_data )); 
    if (pledge_curr_length == 0)
    {
        *error_str_out = "Slave did not return a pledge.";
        return APR_EINVAL;
    }
        
    // 1. Unmarshall pledge
    status = p.unmarshall( pledge_data, pledge_curr_length, error_str_out, pool );
    if (status != APR_SUCCESS)
        return status;

    for (i=0; i<digest_array_length; i++)
    {
        DPRINTF( DEBUG1, ("psodium: client: Doing double check: Slave digest %s <> %s master.\n", 
                        digest2string( p._md, p._md_len, pool ), 
                        digest2string( digest_array[i], OPENSSL_DIGEST_ALG_NBITS/8, 
                        pool ) )); 
        if (!memcmp( p._md, digest_array[i], OPENSSL_DIGEST_ALG_NBITS/8 ))
        {
            *match_out = TRUE;
            return APR_SUCCESS;
        }
    }
    *match_out == FALSE;
    return APR_SUCCESS;
}




apr_status_t pso_auditor_check_pledge_against_digests( Pledge p,
                                               digest_t *digest_array, 
                                               apr_time_t *expiry_array,
                                               apr_size_t digest_array_length, 
                                               apr_bool_t *match_out,
                                               char **error_str_out, 
                                               apr_pool_t *pool )
{
    apr_status_t status = APR_SUCCESS;
    apr_off_t i=0;

    if (digest_array_length == 0)
    {
        *match_out = FALSE;
        return APR_SUCCESS;
    }
    
    // Check pledge against digest in described procedure:
    // 1. Start at end of list
    // 2. If the digest matches, the following condition must NOT hold, 
    //    with Ts timestamp from pledge and expiry time of digest Te:
    //      Ts - ClockDelta > Te
    //   If it does hold the pledge is bad.
    //
    for (i=digest_array_length-1; i>=0; i--)
    {
        DPRINTF( DEBUG1, ("psodium: auditor: Checking pledge against digests: Slave digest %s <> %s master.\n", 
                        digest2string( p._md, p._md_len, pool ), 
                        digest2string( digest_array[i], OPENSSL_DIGEST_ALG_NBITS/8, 
                        pool ) )); 
        if (!memcmp( p._md, digest_array[i], OPENSSL_DIGEST_ALG_NBITS/8 ))
        {
            if ((p._timestamp - CLOCK_DELTA) > expiry_array[i])
            {
                DPRINTF( DEBUG1, ("psodium: auditor: Checking pledge against digests: Digest matches, but pledge too old! %lld > %lld\n", (p._timestamp - CLOCK_DELTA), expiry_array[i] ));
                *match_out = FALSE;
            }
            else
            {
                *match_out = TRUE;
            }
            return APR_SUCCESS;
        }
    }
    *match_out == FALSE;
    return APR_SUCCESS;
}


apr_status_t pso_master_check_pledge( psodium_conf *conf,
                               char *slave_id,
                               char *pledge_data,
                               apr_size_t pledge_curr_length,
                               apr_bool_t *clientIsLying_out,
                               apr_bool_t *slaveIsBad_out,
                               char **actual_slave_id_out,
                               char **error_str_out, 
                               apr_pool_t *pool )
{
    apr_status_t status=APR_SUCCESS;
    char *pubkey_pem=NULL;
    apr_bool_t match=FALSE, sigOK=FALSE;
    Pledge p;
    digest_t *digest_array=NULL;
    apr_time_t *expiry_array=NULL;
    apr_size_t digest_array_length=0;

    *clientIsLying_out = FALSE;
    *slaveIsBad_out = FALSE;
    
    DPRINTF( DEBUG1, ("psodium: master: Checking pledge: $%s$\n", pledge_data )); 

    // 1. Unmarshall pledge
    status = p.unmarshall( pledge_data, pledge_curr_length, error_str_out, pool );
    if (status != APR_SUCCESS)
        return status;

    // 2. Get slave's public key
    status = pso_find_pubkey( conf, slave_id, &pubkey_pem, pool );
    if (status != APR_SUCCESS)
    {
        *error_str_out = "Cannot find client-indicated slave's public key in slave cache.";
        return status;
    }
    else if (pubkey_pem == NULL)
    {
        // Client's lying. No return yet, try slaveID in pledge first
    }
    else
    {
        DPRINTF( DEBUG4, ("psodium: master: Check pledge: got pubkey\n" )); 

        // 3a. Check signature
        status = pso_client_check_pledge_sig( p, 
                                          pubkey_pem, 
                                          &sigOK,
                                          error_str_out, 
                                          pool );
        if (status != APR_SUCCESS) //error_str set
            return status;
    }

    DPRINTF( DEBUG4, ("psodium: master: Check pledge: signature is %d: %s\n", sigOK, *error_str_out ));     

    if (pubkey_pem == NULL || sigOK == FALSE)
    {
        // Client is LYING!, the slave_id it gave does not point to pub key
        // OR the pub key doesn't match the signature
        
        *clientIsLying_out = TRUE;
    
        DPRINTF( DEBUG4, ("psodium: master: Check pledge: Client is lying, trying slave ID in pledge to retrieve pubkeyis\n" ));    

        // 3b. Get slave's public key via ID from pledge
        status = pso_find_pubkey( conf, p._slave_id, &pubkey_pem, pool );
        if (status != APR_SUCCESS)
        {
            *error_str_out = "Cannot find pledge-indicated slave's public key in slave cache.";
            return status;
        }
        else if (pubkey_pem == NULL)
        {
            // Slave crooked too, slaveID in pledge not right. To use this pledge
            // as proof, however, we need to go over the list of all slave pub keys
            // to see if one matches. That's an expensive operation which I won't do.
            // Just mark client as LYING.
            return APR_SUCCESS;
        }

        DPRINTF( DEBUG4, ("psodium: master: Check pledge: got pubkey2\n" )); 

        // 3c. Check signature
        status = pso_client_check_pledge_sig( p, 
                                              pubkey_pem, 
                                              &sigOK,
                                              error_str_out, 
                                              pool );
        if (status != APR_SUCCESS)
            return status;
        else if (sigOK == FALSE)
        {
            // Neither the client-provided slave_id nor the slave_id in the
            // Pledge matches the signature, give up, put client on LYINGCLIENTS
            // (See previous remark)
            DPRINTF( DEBUG4, ("psodium: master: Check pledge: Neither pub key do not verify sig: %s\n", *error_str_out ));
            return APR_SUCCESS;
        }
        else
        {
            // Sig OK, slave pledge still looks OK at this point
            *actual_slave_id_out = p._slave_id;
        }
    }
    else
    {
        *actual_slave_id_out = slave_id;
        if (strcmp( slave_id, p._slave_id ))
        {
            // BADSLAVE: pledge signed with pub key, but slave ID in pledge is 
            // different from what the client uses. Could be setup error,
            // treating it as a bad slave.
            *slaveIsBad_out = TRUE;
            return APR_SUCCESS;
        }
    }
        
        
    // 4. Do checks
    status = pso_master_find_RETURN_RETIRED_digests( conf, 
                                      *actual_slave_id_out,
                                      p._r->unparsed_uri,  
                                      &digest_array, 
                                      &expiry_array,
                                      &digest_array_length, 
                                      pool );
    if (status != APR_SUCCESS)
    {
        *error_str_out = "Cannot find RETURN+RETIRED digests";
        return status;
    }
    status  = pso_auditor_check_pledge_against_digests( p,
                                               digest_array, 
                                               expiry_array,
                                               digest_array_length, 
                                               &match,
                                               error_str_out, 
                                               pool );
   if (status != APR_SUCCESS)
   {
        *error_str_out = "Error checking bad pledge against digests";
        return status;
   }
   if (match)
   {
       // Slave's good, client lying, cheating bastard
       *slaveIsBad_out = FALSE;
       *clientIsLying_out = TRUE;
   }
   else
   {
       // Slave's dirty, doublecrossing XXX, hugs for client
       *slaveIsBad_out = TRUE;
       *clientIsLying_out = FALSE;
   }
    
    return APR_SUCCESS;          
}   






apr_status_t pso_slave_read_privatekey( psodium_conf *conf, apr_pool_t *pool, char **error_str_out )
{
    // TODO: platform independence? mod_ssl has its own version of
    // PEM_read_X509 allegedly for platform independence.
    FILE *fp=NULL;

    fp = fopen( conf->slaveprivatekeyfilename, "r" );
    if (fp == NULL)
    {
        *error_str_out = strerror( errno );
        return APR_EOF;
    }

    conf->slaveprivatekey = PEM_read_PrivateKey( fp, NULL, NULL, NULL );
    fclose( fp );    
    if (conf->slaveprivatekey == NULL)
    {
        *error_str_out = (char *)apr_palloc( pool, OPENSSL_MAX_ERROR_STRLEN );
        ERR_error_string( ERR_get_error(), *error_str_out );
        return APR_EOF;
    }
    
    return APR_SUCCESS;
}

} // extern "C"

