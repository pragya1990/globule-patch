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
#ifndef _AuditorMain_hpp
#define _AuditorMain_hpp

// To have INT64_C be defined
#define __STDC_CONSTANT_MACROS
#include <stdint.h>

#include "mod_psodium.h"
#include "OnDiskHashTable.hpp"
#include "utillib/AprSemaphore.hpp"
#include "utillib/SlaveDatabase.hpp"
#include "utillib/LyingClientsVector.hpp"
#include "utillib/BadSlavesVector.hpp"

#include <apr.h>

#include <unistd.h>
//#define DPIDPRINTF( level, format, ... )   DPRINTF( level, "T#%d: ", getpid() ); DPRINTF( level, format, ## __VA_ARGS__ );
#define DPIDPRINTF DPRINTF

// inline such that we get the right line number for the error.
#define nonret_non_fatal( s ) { \
                        char msgbuf[ APR_MAX_ERROR_STRLEN ]; \
                        DPIDPRINTF( DEBUG1, ("Error (%d): %s\n", s, apr_strerror( s, msgbuf, sizeof( msgbuf)))); \
                    }

#define return_non_fatal( s ) { \
                        char msgbuf[ APR_MAX_ERROR_STRLEN ]; \
                        DPIDPRINTF( DEBUG1, ("Error (%d): %s\n", s, apr_strerror( s, msgbuf, sizeof( msgbuf)))); \
                        return s; \
                    }

#define fatal( s )  { \
                        char msgbuf[ APR_MAX_ERROR_STRLEN ]; \
                        DPIDPRINTF( DEBUG1, ("Error (%d): %s\n", s, apr_strerror( s, msgbuf, sizeof( msgbuf)))); \
                        exit( -1 ); \
                    }


#define AUDITOR_PORT                38000
#define MAX_PLEDGEFORWARD_SIZE      4096    // not too big! DoS risk
#define RECVBUF_SIZE                4096
#define CLIENTSOCKET_TIMEOUT        apr_time_from_sec(60) /* param in secs */ // not too high, DoS risk. Really need multiproc auditor, like Apache 
#define GETMODSOCKET_TIMEOUT        apr_time_from_sec(60) /* param in secs */
#define CLIENT_IPADDRSTR_KEYWORD    "CLIENTIPADDR"
#define CLIENT_PORTSTR_KEYWORD      "CLIENTPORT"

#define HASHSLOT_FILENAME_PREFIX    "Hashtable"
#define NTABLES                     2 /* number of tables to switch between */
#define DEFAULT_NTHREADS            1
#define AUDITOR_NAP                  apr_time_from_sec(30) /* param in secs */

#undef AUDIT_INTERVAL
#define AUDIT_INTERVAL              apr_time_from_sec(30) /* param in secs */

// The amount of memory available for pSodium C++ classes (i.e., classes 
// normally used in shared mem
//
#define UNSHAREDMEM_SIZE                (apr_size_t)2*1024*1024


#define CREATE_FLAGS           (APR_CREATE|APR_READ|APR_WRITE|APR_BINARY|APR_BUFFERED|APR_DELONCLOSE|APR_TRUNCATE)
//DEBUG #define CREATE_FLAGS           (APR_CREATE|APR_READ|APR_WRITE|APR_BINARY|APR_BUFFERED|APR_TRUNCATE)

#define MODIFTEMP_FILENAME_CHUNKED_POSTFIX   ".chunked"
#define MODIFTEMP_FILENAME_IDENTITY_POSTFIX   ".identity"


/*
 * AuditorThread.cc
 */
void *APR_THREAD_FUNC auditthread_func( apr_thread_t *thd, void *data );

/*
 * Util
 */
char *arnopr_strcat( int dummy, ... );
char *arnopr_memcat( apr_size_t *concat_len_out, ... );

/*
 * Global variables
 */
extern OnDiskHashTable **_tables;
extern OnDiskHashTable *_curr_audited_table;
extern OnDiskHashTable *_curr_store_table;
extern AprSemaphore    *_sema;
extern apr_thread_mutex_t   *_store_mutex;

extern Versions *_modifications;
extern char *_globule_export_path;
extern SlaveDatabase *_slavedb;
extern LyingClientsVector *_lying_clients;
extern BadSlavesVector *_bad_slaves;

extern char *_base64_userpass; // for HTTP Basic Authentication of 
                               // getMODIFICATIONS, putLYINGCLIENTS,
                               // putBADSLAVES

#endif // _AuditorMain_hpp
