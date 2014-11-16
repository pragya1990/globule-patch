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
#ifndef _MYAPR_RMM_H
#define _MYAPR_RMM_H

#include <apr.h>
#include <apr_pools.h>
#include <apr_errno.h>
#include <apu.h>
#include <apr_anylock.h>
#include <apr_rmm.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifdef NOTDEFINED
typedef struct apr_rmm_t apr_rmm_t;
typedef apr_size_t   apr_rmm_off_t;
#endif

extern apr_status_t myapr_rmm_init(apr_rmm_t **rmm, apr_anylock_t *lock,
                                   void* membuf, apr_size_t memsize, 
                                   apr_pool_t *cont);
extern apr_status_t myapr_rmm_destroy(apr_rmm_t *rmm);
extern apr_status_t myapr_rmm_attach(apr_rmm_t **rmm, apr_anylock_t *lock,
                                     void* membuf, apr_pool_t *cont);
extern apr_status_t myapr_rmm_detach(apr_rmm_t *rmm);
extern apr_rmm_off_t myapr_rmm_malloc(apr_rmm_t *rmm, apr_size_t reqsize);
extern apr_rmm_off_t myapr_rmm_realloc(apr_rmm_t *rmm, void *entity,
                                       apr_size_t reqsize);
extern apr_rmm_off_t myapr_rmm_calloc(apr_rmm_t *rmm, apr_size_t reqsize);
extern apr_status_t myapr_rmm_free(apr_rmm_t *rmm, apr_rmm_off_t entity);
extern void *myapr_rmm_addr_get(apr_rmm_t *rmm, apr_rmm_off_t entity);
extern apr_rmm_off_t myapr_rmm_offset_get(apr_rmm_t *rmm, void* entity);
extern apr_size_t myapr_rmm_overhead_get(int n);

extern void apr_rmm_validate(apr_rmm_t *rmm); /* not always available */

#ifndef HAVE_MYAPR
#define HAVE_MYAPR
#endif
extern void myapr_rmm_usage(apr_rmm_t *rmm, apr_size_t* size,
                            apr_size_t* bytesused, apr_size_t* bytesavailable,
                            apr_size_t* nitems);
extern void *myapr_rmm_dump(apr_rmm_t *rmm, apr_pool_t *pool, int *nobjects,
                            apr_rmm_off_t **addrs, apr_size_t **sizes);
extern void myapr_rmm_validate(apr_rmm_t *rmm);

#ifdef __cplusplus
}
#endif

#define  apr_rmm_init          myapr_rmm_init
#define  apr_rmm_destroy       myapr_rmm_destroy
#define  apr_rmm_attach        myapr_rmm_attach
#define  apr_rmm_detach        myapr_rmm_detach
#define  apr_rmm_malloc        myapr_rmm_malloc
#define  apr_rmm_realloc       myapr_rmm_realloc
#define  apr_rmm_calloc        myapr_rmm_calloc
#define  apr_rmm_free          myapr_rmm_free
#define  apr_rmm_addr_get      myapr_rmm_addr_get
#define  apr_rmm_offset_get    myapr_rmm_offset_get
#define  apr_rmm_overhead_get  myapr_rmm_overhead_get

#endif
