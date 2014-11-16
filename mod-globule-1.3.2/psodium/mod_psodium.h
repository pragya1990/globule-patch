/* ====================================================================
 * The Apache Software License, Version 1.1
 *
 * Copyright (c) 2000-2003 The Apache Software Foundation.  All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. The end-user documentation included with the redistribution,
 *    if any, must include the following acknowledgment:
 *       "This product includes software developed by the
 *        Apache Software Foundation (http://www.apache.org/)."
 *    Alternately, this acknowledgment may appear in the software itself,
 *    if and wherever such third-party acknowledgments normally appear.
 *
 * 4. The names "Apache" and "Apache Software Foundation" must
 *    not be used to endorse or promote products derived from this
 *    software without prior written permission. For written
 *    permission, please contact apache@apache.org.
 *
 * 5. Products derived from this software may not be called "Apache",
 *    nor may "Apache" appear in their name, without prior written
 *    permission of the Apache Software Foundation.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE APACHE SOFTWARE FOUNDATION OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the Apache Software Foundation.  For more
 * information on the Apache Software Foundation, please see
 * <http://www.apache.org/>.
 *
 * Portions of this software are based upon public domain software
 * originally written at the National Center for Supercomputing Applications,
 * University of Illinois, Urbana-Champaign.
 */

/*========================================================================= 
   mod_psodium.h
   -------------
   Main header file for the pSodium module. 
   
   Must compile with both C and C++ compiler! 
  ========================================================================= */

#ifndef MOD_PSODIUM_H
#define MOD_PSODIUM_H 

#include <httpd.h>
#include <http_config.h>
#include <openssl/evp.h> // OPENSSL
#include <math.h>

// To enable extra debug output (e.g. printing of Versions datastruct)
// uncomment the following.
// #define PSODIUMDEBUG


#ifdef __cplusplus
extern "C" 
{
#endif
#include <mod_proxy.h> // for enum enctype

// Arno: blind copy & rename from mod_proxy.h 

/* Create a set of PSO_DECLARE(type), PSO_DECLARE_NONSTD(type) and 
 * PSO_DECLARE_DATA with appropriate export and import tags for the platform
 */
#if !defined(WIN32)
#define PSO_DECLARE(type)            type
#define PSO_DECLARE_NONSTD(type)     type
#define PSO_DECLARE_DATA
#elif defined(PSO_DECLARE_STATIC)
#define PSO_DECLARE(type)            type __stdcall
#define PSO_DECLARE_NONSTD(type)     type
#define PSO_DECLARE_DATA
#elif defined(PSO_DECLARE_EXPORT)
#define PSO_DECLARE(type)            __declspec(dllexport) type __stdcall
#define PSO_DECLARE_NONSTD(type)     __declspec(dllexport) type
#define PSO_DECLARE_DATA             __declspec(dllexport)
#else
#define PSO_DECLARE(type)            __declspec(dllimport) type __stdcall
#define PSO_DECLARE_NONSTD(type)     __declspec(dllimport) type
#define PSO_DECLARE_DATA             __declspec(dllimport)
#endif

// End copy.

#define PSODIUM_CLIENT_ROLE     0x01
#define PSODIUM_MASTER_ROLE     0x02
#define PSODIUM_SLAVE_ROLE      0x04
typedef unsigned int    psodium_role_t;


#define PSO_STATUS_STR_LEN     256
#define PSO_FILE_BLOCK_SIZE   4096
#define PSO_SOCK_BLOCK_SIZE   4096
#define TRUE_STR                "True"
#define FALSE_STR               "False"

#define AUDITOR_USERID      "auditor"

#define KEY_START_KEYWORD "-----BEGIN CERTIFICATE-----"
#define KEY_END_KEYWORD   "-----END CERTIFICATE-----"
#define AUDITOR_ADDR_KEYWORD    "AUDITORADDR"
#define SLAVE_URI_PREFIX_KEYWORD    "SLAVEURIPREFIX"

#define PLEDGE_START_KEYWORD "-----BEGIN PLEDGE-----"
#define PLEDGE_END_KEYWORD   "-----END PLEDGE-----"
#define SLAVEID_KEYWORD     "SLAVEID"
#define REQUEST_URI_KEYWORD     "REQUEST_URI"
#define REQUEST_HOST_KEYWORD    "REQUEST_HOST"
#define TIMESTAMP_KEYWORD   "TIMESTAMP"
#define DIGEST_KEYWORD      "DIGEST"
#define SIGNATURE_KEYWORD      "SIGNATURE"
#define MAX_PLEDGE_LENGTH   2048 /* bytes */

#define DIGESTS_START_KEYWORD "-----BEGIN DIGESTS-----"
#define DIGESTS_COUNT_KEYWORD "Count:"
#define DIGESTS_END_KEYWORD   "-----END DIGESTS-----"

#define EXPORT_PATH_KEYWORD    "EXPORTPATH"
#define VERSIONS_START_KEYWORD "-----BEGIN VERSIONS-----"
#define VERSIONS_END_KEYWORD "-----END VERSIONS-----"

#define SLAVE2DIG_START_KEYWORD "-----BEGIN SLAVE2DIG-----"
#define SLAVE2DIG_END_KEYWORD "-----END SLAVE2DIG-----"

#define TIME2DIG_START_KEYWORD "-----BEGIN TIME2DIG-----"
#define TIME2DIG_END_KEYWORD "-----END TIME2DIG-----"

#define EXPIRYDIGEST_KEYWORD          "EXPIRYDIGEST"


#define OPENSSL_DIGEST_ALG      EVP_sha1()
#define OPENSSL_DIGEST_ALG_NBITS    160
#define OPENSSL_SIGNATURE_ALG_NBYTES    128     /* SHA1 with RSA */
/* "At least 120 char[s]" ERR_error_string man page */
#define OPENSSL_MAX_ERROR_STRLEN    256 
#define APR_MAX_ERROR_STRLEN        256

#ifndef NUMBER_MAX_STRLEN
#define NUMBER_MAX_STRLEN(x)   ((size_t)(1.0+log10( pow( 2.0, (double)(8.0*sizeof( x ))) )+1.0))
#endif
  
#define PSO_GETSLAVEINFO_URI_PREFIX        "/psodium/getSlaveInfo"
#define PSO_GETDIGEST_URI_PREFIX        "/psodium/getDigests"
#define PSO_PUTPLEDGE_URI_PREFIX        "/psodium/putDigest"
#define PSO_BADPLEDGE_URI_PREFIX        "/psodium/badPledge"
#define PSO_GETMODIFICATIONS_URI_PREFIX      "/psodium/getMODIFICATIONS" // auditor only
#define PSO_PUTLYINGCLIENTS_URI_PREFIX    "/psodium/addLYINGCLIENTS" //auditor only
#define PSO_PUTBADSLAVES_URI_PREFIX    "/psodium/addBADSLAVES" //auditor only
/* Update Globule's pSodium.h if the next def changes!! */
#define PSO_UPDATEEXPIRY_URI_PREFIX     "/psodium/updateExpiry" // Globule -> pSodium master only

#define WANT_SLAVE_HEADER                "X-pSodium-Want-Slave"
#define WANT_PLEDGE_HEADER               "X-pSodium-Want-Pledge"
#define PLEDGE_LENGTH_HEADER             "X-pSodium-Pledge-Length"
#define BADSLAVES_HEADER                 "X-pSodium-Client-DontWant-Slaves"

#define OUTPUT_FILTER_NAME       "PSODIUMFILTER"


/* 
 * The RECEIVER_COMMAND_NOTE controls what the client does when receiving
 * a response.
 */
#define RECEIVER_COMMAND_NOTE            "pSodium-Receiver-Command"
#define IGNORE_REDIRECT_REPLY_COMMAND    "Command-Ignore-RedirectReply"
#define RECORD_SLAVEINFO_COMMAND         "Command-Record-SlaveInfo"
#define DOUBLECHECK_COMMAND              "Command-DoubleCheck&ForwardReply"   
#define KEEP_PLEDGE4AUDIT_COMMAND        "Command-Keep-Pledge4Audit"   
#define KEEP_REPLY4DOUBLECHK_COMMAND     "Command-Keep-Reply4DoubleCheck"   
#define IGNORE_PUTPLEDGE_REPLY_COMMAND   "Command-Ignore-PutPledgeReply"   
#define FORWARD_COMMAND                  "Command-Forward"

#define STORED_SLAVEID_NOTE              "pSodium-Stored-Slave-ID"
#define STORED_PLEDGE_NOTE                    "pSodium-Stored-Pledge" // temp storage for pledge
#define STORED_PLEDGE_LENGTH_NOTE        "pSodium-Stored-Pledge-Length" // current size of temp storage 

#define SAVED_REPLY_HEADERS_NOTE_PREFIX     "pSodium-Saved#"

#define TEMP_STORAGE_FILENAME_NOTE          "pSodium-Temp-Storage-Filename"
#define TEMP_UPDATED_CONTENT_LENGTH_NOTE    "pSodium-Temp-Updated-CL-Note"
#define TEMP_SLAVEID_NOTE                   "pSodium-SlaveID-Note"

#define PLEDGE_CORRECT_NOTE                 "pSodium-Pledge-Correct"

/* 
 * The SENDER_COMMAND_NOTE controls what the client does. In particular,
 * it will control whether a client sends a putBadPledge.
 */
#define SENDER_COMMAND_NOTE             "pSodium-Sender-Command"
#define SEND_BADPLEDGE_COMMAND          "Command-Send-BadPledge"
#define SWITCH_SLAVE_COMMAND            "Command-Switch-Slave"
#define PROCEED_COMMAND                 "Command-Proceed-As-Normal"


// *** WARNING! If these values are updated, also update them in 
// *** Globule's psodium.h!
// *** 
#define GLOBULE_EXPORT_PATH_NOTE                    "Globule-Export-Path-Is"
#define PSODIUM_BADSLAVES_NOTE                      "pSodium-Bad-Slaves"
#define OUTFILT_RECORD_DIGEST_URI_NOTE              "pSodium-Record-Digest-URI"
#define OUTFILT_RECORD_DIGEST_AUTH_SLAVEID_NOTE     "pSodium-Record-Digest-Authenticated-SlaveID"
#define OUTFILT_RECORD_DIGEST_EXPIRYTIME_NOTE       "pSodium-Record-Digest-Expiry-Time"
#define MAX_WRITE_PROP              ((apr_time_t)5*60*1000*1000LL) /* 5 mins in microseconds */

// If this is updated in globule/http/HttpDefs.hpp or the format of the value
// changes -> update!
#define FROM_REPLICA_HEADER         "X-From-Replica"



/*
 * Times
 */

// The maximum allowed clock skew between hosts using pSodium
#define CLOCK_DELTA                         ((apr_time_t)5*60*1000*1000LL) /* microseconds */
// The average throughput between a client and a slave, used to calculate 
// a better MaxCSContentProp. 
#define AVG_CS_THROUGHPUT                  1 /* kibibyte per second (1024 bytes/s) */
// The minimum MaxCSContentProp, that is the minimum time we give a slave to transfer
// a response to a client. We may give it more time, if the response is large. How much
// time depends on the AVG_CS_THROUGHPUT.
#define MIN_MAX_CS_CONTENT_PROP            ((apr_time_t)3*60*1000*1000LL)

/* WARNING! Update these in Globule's psodium.h too if these are changed!!! */
// The average throughput between a master and a slave, used to calculate 
// a better MaxMSContentProp. 
#define AVG_MS_THROUGHPUT                  1 /* kibibyte per second (1024 bytes/s) */
// The minimum MaxMSContentProp, that is the minimum time we give a master to transfer
// a response to a slave. We may give it more time, if the response is large. How much
// time depends on the AVG_MS_THROUGHPUT.
#define MIN_MAX_MS_CONTENT_PROP            ((apr_time_t)3*60*1000*1000LL)

#define MAX_CM_GETDIGEST_PROP               ((apr_time_t)3*60*1000*1000LL)
#define MAX_CM_GETDIGEST_REPLY_PROP         ((apr_time_t)3*60*1000*1000LL)
#define MAX_CM_BADPLEDGE_PROP               ((apr_time_t)3*60*1000*1000LL)

#define AUDIT_INTERVAL              ((apr_time_t)3600*1000*1000LL) /* microsecs */

/* -1.0 because apr_time_t is apr_int64_t, so unsigned
 * Don't use MAX_INT or something, cause we add some times together, and that
 * can cause overflow. Hence the 7.9 instead of 8 */
#define INFINITE_TIME   (apr_time_t)(-1+pow( 2.0, -1.0+(double)(7.9*sizeof( apr_time_t ))))



/*
 * I have placed a hook in the mod_proxy code that handles the client proxy
 * receiving a reply. The hook function returns one of the following
 * actions to instructs the hook code what to do with the current 
 * reply brigade.
 */
typedef enum
{
    RESPONSE_ACTION_SEND,       // send original brigade
    RESPONSE_ACTION_SEND_NEWBB, // send new brigade
    RESPONSE_ACTION_GETNEXTBB,   // ignore current, fetch new brigade 
                                //   (original saved to temp storage)
    RESPONSE_ACTION_NOSEND      // no send now, return                               
} pso_response_action_t;


typedef int     apr_bool_t; // boolean type
typedef unsigned char * digest_t;
typedef unsigned int    digest_len_t;

// Arno: blind copy & rename from mod_proxy.h 
typedef struct 
{
    apr_array_header_t *proxies;
    apr_array_header_t *sec_proxy;
    apr_array_header_t *aliases;
    apr_array_header_t *raliases;
    apr_array_header_t *noproxies;
    apr_array_header_t *dirconn;
    apr_array_header_t *allowed_connect_ports;
    const char *domain;         /* domain name to use in absence of a domain name in the request */
    int req;                    /* true if proxy requests are enabled */
    char req_set;
    enum {
      proxy_via_off,
      proxy_via_on,
      proxy_via_block,
      proxy_via_full
    } viaopt;  
    /* how to deal with proxy Via: headers */
    char viaopt_set;
    apr_size_t recv_buffer_size;
    char recv_buffer_size_set;
    apr_size_t io_buffer_size;
    char io_buffer_size_set;
    long maxfwd;
    char maxfwd_set;
    /** 
     * the following setting masks the error page
     * returned from the 'proxied server' and just 
     * forwards the status code upwards.
     * This allows the main server (us) to generate
     * the error page, (so it will look like a error
     * returned from the rest of the system 
     */
    int error_override;
    int error_override_set;
    int preserve_host;
    int preserve_host_set;
    apr_interval_time_t timeout;
    apr_interval_time_t timeout_set;
    enum {
      proxy_bad_error,
      proxy_bad_ignore,
      proxy_bad_body
    } badopt;                   /* how to deal with bad headers */
    char badopt_set;
// End of copy.
#ifdef PSODIUM
    psodium_role_t role;
    /* Name of file containing slave's public keys */
    char *slavekeysfilename;  
    char slavekeysfilename_set;
    char *auditor_hostname;
    apr_port_t auditor_port;
    char *auditor_passwd;
    char auditoraddr_set;
    /* Name of file directory for scratch files */
    char *tempstoragedirname;  
    char tempstoragedirname_set;
    /* slave_id as used by master */
    char *slaveid;             
    char slaveid_set;
    /* Name of file containing private key */
    char *slaveprivatekeyfilename;  
    char slaveprivatekeyfilename_set;
    /* Passwd to use for intra-server auth (e.g. by Globule) */
    char *intraserverauthpwd; 
    char intraserverauthpwd_set; 
    // OPENSSL
    /* Private key of slave for signing pledges */
    EVP_PKEY *slaveprivatekey;  

        /* Name of file containing private key */
    char *doublecheckprob_str;  // stringified version of float
    float doublecheckprob;
    char doublecheckprob_set;
    char *badcontent_img_filename;
    char *badcontent_img_mimetype;
    char badcontent_img_set;
    char *badslave_img_filename;
    char *badslave_img_mimetype;
    char badslave_img_set;
    char *clientlied_img_filename;
    char *clientlied_img_mimetype;
    char clientlied_img_set;

    char *badslavesfilename;
    char badslavesfilename_set;
    char *lyingclientsfilename;
    char lyingclientsfilename_set;
    char *versionsfilename;
    char versionsfilename_set;

    // Needed for server cleanup
    apr_pool_t *conf_pool;

    /* mutex for VERSIONS, PERFORMANCE: more fine-grained locking? */
    void *versions_lock;
    /* pointer to Versions */
    void *versions;                 

    /* mutex for SlaveDatabase, PERFORMANCE: more fine-grained locking? */
    void *slavedb_lock;
     /* The in-core list of public keys read from keys file */
    void *slavedatabase;       

    /* List of clients that sent bad pledges (to us or to auditor). Lock builtin. */
    void *lyingclients;         
    /* If master: list of slaves that sent incriminating pledges 
       (e.g. to auditor). If client: slaves that returned bad pledges.
       Lock builtin. */
    void *badslaves;
    /* The export path as set by GlobuleExport and communicated by Globule to
       pSodium via the GLOBULE_EXPORT_PATH_NOTE in r->notes */
    char *master_uri_prefix;
#endif    
} psodium_conf;


/* for PsodiumConfig.c */
extern module AP_MODULE_DECLARE_DATA psodium_module;

#ifndef max
#define max(a,b) (((a)>(b))?(a):(b))
#endif


/* ============================= DigestsUtil.cc ============================ */

PSO_DECLARE(apr_status_t) pso_master_record_digest( psodium_conf *conf, 
                                                    request_rec *r,  
                                                    digest_t md,
                                                    digest_len_t md_len,
                                                    apr_off_t rsize,
                                                    const char *uri_str,
                                                    const char *slave_id,  
                                                    const char *expiry_time_str );

PSO_DECLARE(apr_status_t) pso_master_find_RETURN_digests( psodium_conf *conf, 
                                                   char *slave_id, 
                                                   char *slave_uri_path,
                                                   digest_t **digest_array_out, 
                                                   apr_size_t *digest_array_length, 
                                                   apr_pool_t *pool );

PSO_DECLARE(apr_status_t) pso_master_find_RETURN_RETIRED_digests( psodium_conf *conf, 
                                      char *slave_id, 
                                      char *slave_uri_path,
                                      digest_t **digest_array_out, 
                                      apr_time_t **expiry_array_out,
                                      apr_size_t *digest_array_length_out, 
                                      apr_pool_t *pool );


PSO_DECLARE(char *) digest2string( digest_t md, 
                                   digest_len_t md_len, 
                                   apr_pool_t *pool );

#ifdef __cplusplus
} // extern "C"
#include "utillib/Versions.hpp"
#include "utillib/SlaveRecord.hpp"

PSO_DECLARE(char *) pso_slave_info_to_master_uri_path( const char *slave_id,
                                                SlaveRecord *slaverec, 
                                                const char *slave_uri_path,
                                                const char *master_uri_prefix,
                                                apr_pool_t *pool );

PSO_DECLARE(void) pso_clear_VERSIONS( Versions *v, rmmmemory *shm_alloc );

PSO_DECLARE(apr_status_t) pso_unmarshall_VERSIONS_from_file( apr_file_t *file, 
                                                Versions *v,
                                                rmmmemory *shm_alloc,
                                                char **export_path_out,
                                                char **error_str_out );

extern "C" {
#endif

PSO_DECLARE(apr_status_t) pso_master_update_expiry( psodium_conf *conf, 
                                       request_rec *r, 
                                       char *filename, char *time_str, 
                                       char **error_str_out );




/* =========================== util.c ====================================== */

/* 
 * Return offset of next non-whitespace character
 * @param buf  source
 * @param offset    offset in source
 * @param length  length of source
 * @return new offset or -1 for end of source.
 */
PSO_DECLARE(apr_size_t) pso_skip_whitespace( char *buf, apr_size_t offset, 
                                                        apr_size_t length );


/* 
 * Return offset of next occurence of a keyword
 * @param buf  source
 * @param offset    offset in source
 * @param length  length of source
 * @return new offset or -1 for end of source.
 */
PSO_DECLARE(apr_size_t) pso_find_keyword( char *buf, apr_size_t offset, 
                                          apr_size_t length, char *keyword );


/* 
 * Return a string that is a copy of the specified region
 * @param src source
 * @param soff    start offset in source
 * @param eoff    end offset in source
 * @param pool  pool to allocate string from
 * @return zero-terminated string with copy of region
 */
PSO_DECLARE(char *) pso_copy_tostring( char *src, apr_size_t soff, 
                                       apr_size_t eoff, apr_pool_t *pool );

char *pso_copy_tostring_malloc( char *src, apr_size_t soff, apr_size_t eoff );


PSO_DECLARE(apr_size_t) pso_find_file_keyword( apr_file_t *file, 
                                               apr_size_t soffset, 
                                               char *keyword,
                                               apr_bool_t CaseSens );

PSO_DECLARE(apr_size_t) pso_skip_file_whitespace( apr_file_t *file, 
                                                  apr_size_t soffset );

PSO_DECLARE(char *) pso_copy_file_tostring( apr_file_t *file, 
                                            apr_size_t soff, 
                                            apr_size_t eoff );

PSO_DECLARE(char *) pso_copy_file_toblock( apr_file_t *file, 
                                           apr_size_t soff, 
                                           apr_size_t eoff );

/*
 * Return a random base file name (i.e., no directory separators). 
 * Safe for all OS-es.
 */
PSO_DECLARE(apr_status_t) pso_create_random_basename( char **basename_out, apr_pool_t *pool );

PSO_DECLARE(apr_status_t) pso_create_random_number( long *random_out );


/*
 * Convert a string to a natural number (>=0) in a safe way.
 * Usage:
 *  unsigned char c;
 *  int ret = pso_atoN( "123", &c, sizeof( c ));
 *
 * Return value is 0 if OK, -1 and errno=ERANGE if not OK.
 *
 * We need a safe version because on Linux specifying too large
 * integer conversions on sscanf (e.g. %llu when the target is just an unsigned
 * long int) can corrupt values on the stack or cause segmentation faults.
 */
int pso_atoN( const char *a, void *N, size_t sizeofN );

/*
 * Convert natural number to string in a safe way.
 */
int pso_Ntoa( void *N, size_t sizeofN, char **a_out, apr_pool_t *pool );


/* ========================= SlaveDatabaseUtil.cc ========================== */
/*
 * Lookup a slave's public key in conf->slavekeys
 */
PSO_DECLARE(apr_status_t) pso_master_read_conf_slavekeys( psodium_conf *conf, 
                                                          apr_pool_t *pool ); 

#ifdef __cplusplus
}// extern "C"
#include "utillib/SlaveDatabase.hpp"
#include "../globule/apr/AprGlobalMutex.hpp"
extern "C"
{

/* 
 * Read slave public keys from slavekeysfile into SlaveDatabase
 * @param slavekeysfilename
 * @param slavedb
 * @param slavedb_lock  may be NULL
 * @param pool  pool to use
 * @return status
 */
PSO_DECLARE(apr_status_t) pso_master_read_slavekeys( const char *slavekeysfilename, 
                                        SlaveDatabase *slavedb, 
                                        AprGlobalMutex *slavedb_lock,
                                        rmmmemory *shm_alloc,
                                        apr_pool_t *pool );

PSO_DECLARE(apr_status_t) pso_find_slaverec( psodium_conf *conf, 
                                             const char *slave_id, 
                                             SlaveRecord **slaverec_out, 
                                             apr_pool_t *pool );
#endif

PSO_DECLARE(apr_status_t) pso_find_pubkey( psodium_conf *conf, 
                                           const char *slave_id, 
                                           char **pubkey_out, 
                                           apr_pool_t *pool );

PSO_DECLARE(apr_status_t) pso_find_master_id( psodium_conf *conf, 
                                              const char *slave_id, 
                                              char **master_id_out, 
                                              apr_pool_t *pool );

PSO_DECLARE(apr_status_t) pso_add_slave_record( psodium_conf *conf, 
                           const char *slave_id, 
                           const char *master_id, 
                           const char *pubkey_pem, 
                           const char *auditor_addr,
                           const char *slave_uri_prefix );


/*
 * Convert auditor address and requested slave public key to bucket brigade.
 */
PSO_DECLARE(apr_status_t) pso_slaveinfo_to_bb( psodium_conf *conf, 
                                           request_rec *r, 
                                           const char *slave_id,
                                           apr_bucket_brigade **bb_out );


/* ============================= PledgeUtil.cc ============================= */

PSO_DECLARE(void) retrieve_pledge( request_rec *r, char **pledge_data_out, apr_size_t *pledge_curr_length_out );
PSO_DECLARE(const char *) retrieve_pledge_encoded( request_rec *r );
PSO_DECLARE(void) decode_pledge( apr_pool_t *pool, const char *pledge_base64,  char **pledge_data_out, apr_size_t *pledge_curr_length_out );
PSO_DECLARE(void) store_pledge( request_rec *r, char *pledge_data, apr_size_t pledge_curr_length );


PSO_DECLARE(apr_status_t) pso_slave_determine_marshalled_pledge_length( psodium_conf *conf, 
                                                           request_rec *r, 
                                                           apr_size_t *pledge_length_out );


PSO_DECLARE(apr_status_t) pso_slave_pledge_to_bb( psodium_conf *conf,
                               request_rec *r, 
                               digest_t md,
                               digest_len_t  md_len,
                               apr_bucket_brigade **bb_out );


PSO_DECLARE(apr_status_t) pso_slave_read_privatekey( psodium_conf *conf, 
                                        apr_pool_t *pool, 
                                        char **error_str_out );

PSO_DECLARE(apr_status_t) pso_client_check_pledge( psodium_conf *conf,
                               request_rec *our_r,
                               digest_t our_md,
                               digest_len_t  our_md_len,
                               char *slave_id,
                               char *pledge_data, 
                               apr_size_t pledge_curr_length,
                               apr_bool_t *OK_out, 
                               char **error_str_out, 
                               apr_pool_t *pool );


#ifdef __cplusplus
} // extern "C"
#include "utillib/Pledge.hpp"
extern "C" {
PSO_DECLARE(apr_status_t) pso_client_check_pledge_sig( Pledge p, 
                                          char *pubkey_pem, 
                                          apr_bool_t *OK_out,
                                          char **error_str_out, 
                                          apr_pool_t *pool );
PSO_DECLARE(apr_status_t) pso_auditor_check_pledge_against_digests( Pledge p,
                                               digest_t *digest_array, 
                                               apr_time_t *expiry_array,
                                               apr_size_t digest_array_length, 
                                               apr_bool_t *match_out,
                                               char **error_str_out, 
                                               apr_pool_t *pool );
#endif

PSO_DECLARE(apr_status_t) pso_client_check_pledge_against_digests( digest_t *digest_array, 
                                               apr_size_t digest_array_length, 
                                               char *pledge_data, 
                                               apr_size_t pledge_curr_length,
                                               apr_bool_t *match_out,
                                               char **error_str_out, 
                                               apr_pool_t *pool );


PSO_DECLARE(apr_status_t) pso_master_check_pledge( psodium_conf *conf,
                               char *slave_id,
                               char *pledge_data, 
                               apr_size_t pledge_curr_length,
                               apr_bool_t *clientIsLying_out,
                               apr_bool_t *slaveIsBad_out,
                               char **actual_slave_id_out,
                               char **error_str_out, 
                               apr_pool_t *pool );



/* ============================= BadSlavesUtil.cc ========================== */

PSO_DECLARE(void) pso_client_mark_bad_slave( psodium_conf *conf, 
                                             apr_pool_t *pool,
                                             const char *slave_id );

PSO_DECLARE(apr_bool_t) pso_client_connecting_to_bad_slave( psodium_conf *conf, 
                                         apr_pool_t *pool, 
                                         const char *server_id );

PSO_DECLARE(void) pso_master_badslaves_to_note2globule( psodium_conf *conf, 
                                             request_rec *r );


/* ============================= BadSlavesUtil.cc ========================== */

PSO_DECLARE(apr_status_t) pso_master_client_is_known_liar( psodium_conf *conf, 
                                         request_rec *r, 
                                         apr_bool_t *liar_out );


/* ============================= Debug.cc ============================= */

PSO_DECLARE(void) print_brigade( apr_bucket_brigade *bb );



/*
 * Module functions
 */


/* ============================= PsodiumConfig.cc =========================== */

extern ap_filter_rec_t *pso_output_filter_handle;

PSO_DECLARE(const char *) pso_set_slavekeysfile( cmd_parms *parms, void *dummy, 
                                                 const char *arg);
PSO_DECLARE(const char *) pso_set_auditoraddr( cmd_parms *parms, void *dummy, 
                                 const char *arg1, const char *arg2 );

PSO_DECLARE(const char *) pso_set_tempstoragedir( cmd_parms *parms, void *dummy, 
                                    const char *arg);

PSO_DECLARE(const char *) pso_set_slaveid( cmd_parms *parms, void *dummy, 
                                    const char *arg);

PSO_DECLARE(const char *) pso_set_slaveprivatekeyfilename( cmd_parms *parms, 
                                                           void *dummy, 
                                                           const char *arg);


PSO_DECLARE(const char *) pso_set_doublecheckprob( cmd_parms *parms, 
                                                   void *dummy, 
                                                   const char *arg);

PSO_DECLARE(const char *) pso_set_badcontent_img( cmd_parms *parms, void *dummy, 
                                 const char *arg1, const char *arg2 );

PSO_DECLARE(const char *) pso_set_badslave_img( cmd_parms *parms, void *dummy, 
                                 const char *arg1, const char *arg2 );

PSO_DECLARE(const char *) pso_set_clientlied_img( cmd_parms *parms, void *dummy, 
                                 const char *arg1, const char *arg2 );

PSO_DECLARE(const char *) pso_set_intraserverauthpwd( cmd_parms *parms, void *dummy, 
                                 const char *arg );

PSO_DECLARE(const char *) pso_set_badslavesfilename( cmd_parms *parms, 
                                                           void *dummy, 
                                                           const char *arg);

PSO_DECLARE(const char *) pso_set_lyingclientsfilename( cmd_parms *parms, 
                                                           void *dummy, 
                                                           const char *arg);

PSO_DECLARE(const char *) pso_set_versionsfilename( cmd_parms *parms, 
                                                    void *dummy, 
                                                    const char *arg );


PSO_DECLARE(int) pso_post_config(apr_pool_t *pconf, apr_pool_t *plog,
                             apr_pool_t *ptemp, server_rec *s);


PSO_DECLARE(void) pso_child_init( apr_pool_t *child_pool, server_rec *s );

PSO_DECLARE(void) pso_add_output_filter_to_request( request_rec *r );

/* should go in module-only (i.e., no auditor) util lib */
PSO_DECLARE(apr_status_t) pso_create_badcontent_reply_bb( request_rec *r, 
                                             apr_bucket_brigade **newbb_out,  
                                             char *slave_id,
                                             char *pledge_data, 
                                             apr_size_t pledge_curr_length,
                                             char *error_str_out ); 


PSO_DECLARE(apr_status_t) pso_create_badslave_reply_bb( request_rec *r, 
                                           apr_bucket_brigade **newbb_out );


apr_status_t pso_create_clientlied_reply_bb( request_rec *r, 
                                             apr_bucket_brigade **newbb_out,  
                                             char *slave_id,
                                             char *pledge_data, 
                                             apr_size_t pledge_curr_length,
                                             char *error_str );


/* ========================= output_filter.c ====================== */

PSO_DECLARE(apr_status_t) pso_output_filter(ap_filter_t *f, apr_bucket_brigade *bb);


/* ========================= MasterRequestHandling.cc ====================== */

PSO_DECLARE(int) pso_master_process_request( psodium_conf *conf, request_rec *r );

PSO_DECLARE(apr_status_t) pso_master_send_MODIFICATIONS( psodium_conf *conf, 
                                            request_rec *r,
                                            apr_time_t audit_start_time,
                                            const char *globule_export_path );

#ifdef __cplusplus
#include "utillib/Versions.hpp"
/*
 * pre: Versions locked
 */
apr_status_t pso_master_marshall_write_VERSIONS( Versions *v, 
                                      const char *filename,
                                      apr_pool_t *pool );
#endif


/* =================== ClientRequestHandling.cc ================== */

PSO_DECLARE(int) pso_client_http_handler( request_rec *r, psodium_conf *conf,
                          char *url, const char *proxyname, 
                          apr_port_t proxyport);

PSO_DECLARE(pso_response_action_t) pso_client_handle_http_response( psodium_conf *conf, 
                                  request_rec *r, 
                                  request_rec *rp, 
                                  const char *content_length_str,
                                  apr_size_t content_idx, 
                                  apr_size_t content_length,
                                  apr_size_t pledge_length,
                                  apr_bucket_brigade *bb,
                                  apr_off_t brigade_len, 
                                  apr_bucket_brigade **newbb_out,
                                  apr_file_t **temp_file_out,
                                  EVP_MD_CTX *md_ctxp,
                                  digest_t **digest_array_out, 
                                  apr_size_t *digest_array_length_out );


/* ===================== HandleMasterRedirect.cc ======================= */

PSO_DECLARE(int) pso_client_redirect_request( psodium_conf *conf, 
                                 request_rec *r,
                                 const char *server_id,
                                 char *url, 
                                 const char *proxyname, 
                                 apr_port_t proxyport );

PSO_DECLARE(pso_response_action_t) pso_client_redirect_response( psodium_conf *conf, 
                                 request_rec *r,
                                  apr_bucket_brigade *bb,
                                  apr_off_t brigade_len );


/* ===================== HandleMasterGetSlaveInfo.cc ======================= */

int pso_client_getslaveinfo_request( psodium_conf *conf, 
                                 request_rec *r,
                                 const char *server_id,
                                 char *url, 
                                 const char *proxyname, 
                                 apr_port_t proxyport );

pso_response_action_t pso_client_getslaveinfo_response( psodium_conf *conf, 
                                 request_rec *r,
                                  apr_bucket_brigade *bb,
                                  apr_off_t brigade_len,
                                  apr_bucket_brigade **newbb_out );


/* ===================== HandleSlaveGet.cc ======================= */

PSO_DECLARE(int) pso_client_content_request( psodium_conf *conf, 
                                request_rec *r, 
                                const char *server_id,
                                char *url, 
                                const char *proxyname, 
                                apr_port_t proxyport);

PSO_DECLARE(pso_response_action_t) pso_client_content_response( apr_bool_t doublecheck, 
                                  psodium_conf *conf, 
                                  request_rec *r, 
                                  request_rec *rp, 
                                  const char *content_length_str,
                                  apr_size_t content_idx, 
                                  apr_size_t content_length,
                                  apr_size_t pledge_length,
                                  apr_bucket_brigade *bb,
                                  apr_off_t brigade_len, 
                                  apr_bucket_brigade **newbb_out,
                                  apr_file_t **temp_file_out,
                                  EVP_MD_CTX *md_ctxp,
                                  digest_t **digest_array_out, 
                                  apr_size_t *digest_array_length_out );

PSO_DECLARE(apr_status_t) pso_client_restore_slave_reply_headers( request_rec *r );


/*====================== HandleMasterDigests ======================= */


PSO_DECLARE(int) pso_client_getdigests_request( psodium_conf *conf, 
                                 request_rec *r,
                                 char *master_id,
                                 char *slave_id,
                                 char *url, 
                                 const char *proxyname, 
                                 apr_port_t proxyport,
                                 apr_bool_t *badslave_out );

PSO_DECLARE(pso_response_action_t) pso_client_getdigests_response( psodium_conf *conf, 
                                  request_rec *r, 
                                  request_rec *rp, 
                                  const char *content_length_str,
                                  apr_size_t content_idx, 
                                  apr_size_t content_length,
                                  apr_size_t pledge_length,
                                  apr_bucket_brigade *bb,
                                  apr_off_t brigade_len, 
                                  apr_bucket_brigade **newbb_out,
                                  apr_file_t **temp_file_out,
                                  EVP_MD_CTX *md_ctxp,
                                  digest_t **digest_array_out, 
                                  apr_size_t *digest_array_length_out );


/*====================== HandleMasterBadPledge ======================= */

PSO_DECLARE(int) pso_client_badpledge_request_iff( psodium_conf *conf, 
                                 request_rec *r,
                                 char *master_id,
                                 char *slave_id,
                                 char *url, 
                                 const char *proxyname, 
                                 apr_port_t proxyport,
                                 apr_bool_t *sent_out );

/* ===================== HandleAuditorPutPledge ====================== */

PSO_DECLARE(int) pso_client_auditor_request( psodium_conf *conf, 
                                 request_rec *r,
                                 const char *slave_id,
                                 char *url, 
                                 const char *proxyname, 
                                 apr_port_t proxyport );

pso_response_action_t pso_client_auditor_response( psodium_conf *conf, 
                                  request_rec *r, 
                                  request_rec *rp, 
                                  const char *content_length_str,
                                  apr_size_t content_idx, 
                                  apr_size_t content_length,
                                  apr_size_t pledge_length,
                                  apr_bucket_brigade *bb,
                                  apr_off_t brigade_len, 
                                  apr_bucket_brigade **newbb_out,
                                  apr_file_t **temp_file_out,
                                  EVP_MD_CTX *md_ctxp,
                                  digest_t **digest_array_out, 
                                  apr_size_t *digest_array_length_out );


/* ============================ temp_storage.c ============================= */

PSO_DECLARE(apr_status_t) pso_bb_to_temp_storage( psodium_conf *conf, 
                                     request_rec *r, 
                                     apr_file_t **temp_file_out, 
                                     apr_bucket_brigade *bb,
                                     apr_bool_t delonclose );

PSO_DECLARE(apr_status_t) pso_temp_storage_to_filebb( request_rec *r,
                                                      apr_file_t *temp_file, 
                                                      apr_bucket_brigade **newbb_out );

PSO_DECLARE(apr_status_t) pso_reopen_temp_storage( psodium_conf *conf, 
                                     request_rec *r, 
                                     apr_file_t **temp_file_out );


/* ============================= pso_error.c =============================== */
PSO_DECLARE(apr_status_t) pso_error_in_html_to_bb( request_rec *r, 
                                      char *error_str, 
                                      apr_bucket_brigade **bb_out );

PSO_DECLARE(void) pso_error_send_connection_failed( request_rec *r, 
                                                int access_status, char *role );


/* ============================= mod_psodium.c ============================= */

// Arno: blind copy & rename from mod_proxy.h 

/**
 * Hook an optional psodium hook.  Unlike static hooks, this uses a macro
 * instead of a function.
 */
#define PSODIUM_OPTIONAL_HOOK(name,fn,pre,succ,order) \
        APR_OPTIONAL_HOOK(psodium,name,fn,pre,succ,order)

APR_DECLARE_EXTERNAL_HOOK(psodium, PSO, int, scheme_handler, (request_rec *r, 
                          psodium_conf *conf, char *url, 
                          const char *psodiumhost, apr_port_t psodiumport))
APR_DECLARE_EXTERNAL_HOOK(psodium, PSO, int, canon_handler, (request_rec *r, 
                          char *url))

APR_DECLARE_EXTERNAL_HOOK(psodium, PSO, int, create_req, (request_rec *r, request_rec *pr))
APR_DECLARE_EXTERNAL_HOOK(psodium, PSO, int, fixups, (request_rec *r)) 

PSO_DECLARE(int) renamed_ap_proxy_http_handler(request_rec *r, psodium_conf *conf,
                          char *url, const char *proxyname, 
                          apr_port_t proxyport);

/* modified_proxy_util.c */

PSO_DECLARE(request_rec *)renamed_ap_proxy_make_fake_req(conn_rec *c, request_rec *r);
PSO_DECLARE(int) renamed_ap_proxy_hex2c(const char *x);
PSO_DECLARE(void) renamed_ap_proxy_c2hex(int ch, char *x);
PSO_DECLARE(char *)renamed_ap_proxy_canonenc(apr_pool_t *p, const char *x, int len, enum enctype t,
                        int isenc);
PSO_DECLARE(char *)renamed_ap_proxy_canon_netloc(apr_pool_t *p, char **const urlp, char **userp,
                         char **passwordp, char **hostp, apr_port_t *port);
PSO_DECLARE(const char *)renamed_ap_proxy_date_canon(apr_pool_t *p, const char *x);
PSO_DECLARE(apr_table_t *)renamed_ap_proxy_read_headers(request_rec *r, request_rec *rp, char *buffer, int size, conn_rec *c);
PSO_DECLARE(int) renamed_ap_proxy_liststr(const char *list, const char *val);
PSO_DECLARE(char *)renamed_ap_proxy_removestr(apr_pool_t *pool, const char *list, const char *val);
PSO_DECLARE(int) renamed_ap_proxy_hex2sec(const char *x);
PSO_DECLARE(void) renamed_ap_proxy_sec2hex(int t, char *y);
PSO_DECLARE(int) renamed_ap_proxyerror(request_rec *r, int statuscode, const char *message);
PSO_DECLARE(int) renamed_ap_proxy_is_ipaddr(struct dirconn_entry *This, apr_pool_t *p);
PSO_DECLARE(int) renamed_ap_proxy_is_domainname(struct dirconn_entry *This, apr_pool_t *p);
PSO_DECLARE(int) renamed_ap_proxy_is_hostname(struct dirconn_entry *This, apr_pool_t *p);
PSO_DECLARE(int) renamed_ap_proxy_is_word(struct dirconn_entry *This, apr_pool_t *p);
PSO_DECLARE(int) renamed_ap_proxy_checkproxyblock(request_rec *r, psodium_conf *conf, apr_sockaddr_t *uri_addr);
PSO_DECLARE(int) renamed_ap_proxy_pre_http_request(conn_rec *c, request_rec *r);
PSO_DECLARE(apr_status_t) renamed_ap_proxy_string_read(conn_rec *c, apr_bucket_brigade *bb, char *buff, size_t bufflen, int *eos);
PSO_DECLARE(void) renamed_ap_proxy_table_unmerge(apr_pool_t *p, apr_table_t *t, char *key);
PSO_DECLARE(int) renamed_ap_proxy_connect_to_backend(apr_socket_t **, const char *, apr_sockaddr_t *, const char *, psodium_conf *, server_rec *, apr_pool_t *);
PSO_DECLARE(int) renamed_ap_proxy_ssl_enable(conn_rec *c);
PSO_DECLARE(int) renamed_ap_proxy_ssl_disable(conn_rec *c);

//End of copy.


#ifdef __cplusplus
#include "utillib/globule/ConcurrencyControl.hpp"

} // extern "C"
#endif

  
  
#endif
