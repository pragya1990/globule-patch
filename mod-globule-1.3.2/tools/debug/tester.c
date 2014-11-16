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
#include <httpd.h>
#include <apr.h>
#include <apr_file_io.h>
#include <apr_shm.h>
#include "myapr_rmm.h"
#include <stdio.h>

#define CHECK(ARG) \
  do{if(ARG){fprintf(stderr,"call " #ARG " failed\n");exit(1);}}while(0)

int
main(int argc, char *argv[])
{
  apr_rmm_t* rmmhandle;
  apr_shm_t* shmhandle;
  apr_pool_t* pool;
  apr_allocator_t *allocer;
  int i;
  int sizes[] = { 8, 5, 16, 400 };
  apr_rmm_off_t m1, m2, m3, m4, m5;
  const apr_size_t reqsize = 10485760;

  CHECK(apr_allocator_create(&allocer));
  CHECK(apr_pool_create_ex(&pool, NULL, NULL, allocer));
  CHECK(apr_shm_create(&shmhandle, reqsize, NULL, pool));
  CHECK(apr_rmm_init(&rmmhandle, NULL,
                     apr_shm_baseaddr_get(shmhandle),
                     apr_shm_size_get(shmhandle), pool));

  for(i=0; i<10000; i++) {
    m1 = apr_rmm_malloc(rmmhandle, 33);
    m2 = apr_rmm_malloc(rmmhandle, sizes[i%4]);
    m3 = apr_rmm_malloc(rmmhandle, 80);
    printf("%lld %lld %lld\n",(long long)m1,(long long)m2,(long long)m3);
    apr_rmm_free(rmmhandle, m2);
  }

  CHECK(apr_rmm_destroy(rmmhandle));
  CHECK(apr_shm_destroy(shmhandle));
  apr_pool_destroy(pool);
  apr_allocator_destroy(allocer);
  exit(0);
}

/****************************************************************************/
