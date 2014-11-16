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
/* 
 * File:   psodium.h
 * Author: arno
 *
 * Created on April 27, 2004, 2:32 PM
 */

#ifndef _PSODIUM_H
#define _PSODIUM_H

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#include <math.h>
#include <apr.h>
#include <apr_time.h>

#ifdef  __cplusplus
extern "C" {
#endif

//#define PSODIUMTESTSLAVEBAD   // let slave corrupt some of its output

// Password for Globule<->pSodium authentication
extern const char *intraserverauthpwd;
  
/*
 * General
 */

#ifndef max
#define max(a,b) (((a)>(b))?(a):(b))
#endif
// Crappy C++ include shit 
#define pso_max(a,b) (((a)>(b))?(a):(b))
/* 
 * WARNING! Update any of the following in pSodium's mod_psodium.h too 
 * when these are changed!!! 
 */
#define GLOBULE_EXPORT_PATH_NOTE                    "Globule-Export-Path-Is"
#define PSODIUM_BADSLAVES_NOTE                      "pSodium-Bad-Slaves"
#define PSODIUM_RECORD_DIGEST_EXPIRYTIME_NOTE       "pSodium-Record-Digest-Expiry-Time"
#define PSODIUM_RECORD_DIGEST_AUTH_SLAVEID_NOTE     "pSodium-Record-Digest-Authenticated-SlaveID"
#define PSODIUM_RECORD_DIGEST_URI_NOTE              "pSodium-Record-Digest-URI"
#define MAX_WRITE_PROP              ((apr_time_t)5*60*1000*1000LL) /* 5 mins in microseconds */

// The average throughput between a master and a slave, used to calculate 
// a better MaxMSContentProp. 
#define AVG_MS_THROUGHPUT                  1 /* kibibyte per second (1024 bytes/s) */
// The minimum MaxMSContentProp, that is the minimum time we give a master to transfer
// a response to a slave. We may give it more time, if the response is large. How much
// time depends on the AVG_MS_THROUGHPUT.
#define MIN_MAX_MS_CONTENT_PROP            ((apr_time_t)3*60*1000*1000LL)

/* Update in pSodium's mod_psodium.h if this changes!!! */
#define PSO_UPDATEEXPIRY_URI_PREFIX     "/psodium/updateExpiry" // Globule -> pSodium master only

/* -1.0 because apr_time_t is apr_int64_t, so unsigned
 * Don't use MAX_INT or something, cause we add some times together, and that
 * can cause overflow. Hence the 7.9 instead of 8 */
#define INFINITE_TIME   (apr_time_t)(-1+pow( 2.0, -1.0+(double)(7.9*sizeof( apr_time_t ))))


/* End volatile */

#ifdef  __cplusplus
}
#endif

#endif  /* _psodium_H */

