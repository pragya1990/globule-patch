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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <httpd.h>
#include <apr.h>
#include <apr_anylock.h>
#include <apr_file_io.h>
#include <apr_file_info.h>
#include "myapr_rmm.h"
#include <stddef.h>

#define MAGICFREE     0x837bc96fd3c445eULL
#define MAGICOCCUPIED 0x7bc96f47d445e3cULL
#define MAGICMARKER   0x846204575037127ULL

/* Define the following item when you want the shared memory to be constantly
 * checked for consitency.  This takes a huge performance hit, so unless you
 * are debugging and have some overshoot in memory, don't.
 * If you want to also dump the memory trace, look for the printf in the
 * NOTDEFINED clause.  Putting that in will print out a trace of all entries.
 */
/* #define RMMDEBUG */

#ifdef RMMDEBUG
#define APR_RMM_VALIDATE(RMM) myapr_rmm_validate(RMM)
#else
#define APR_RMM_VALIDATE(RMM)
#endif

typedef apr_int64_t align_t;

struct chunk {
  apr_uint64_t    magic;
  apr_size_t      size;
  apr_rmm_off_t   prev;
  apr_rmm_off_t   next;
  align_t data;
};
struct header {
  apr_rmm_off_t   first;
  apr_size_t      size;
  struct chunk    data;
};
struct apr_rmm_t {
  apr_anylock_t * lock;
  struct header * membuf;
};

#define MARKER(P) (*(apr_uint64_t*)&(((unsigned char *)&(P->data))[(P)->size]))
#define BLOCK(B) ((struct chunk *)&(((char*)rmm->membuf))[B])
#define BLOCKADDR(B) (((char*)B)-((char*)rmm->membuf))

const apr_size_t emptychunksize
  = sizeof(struct chunk) - sizeof(align_t) + sizeof(apr_uint64_t);

/****************************************************************************/

void
myapr_rmm_validate(apr_rmm_t *rmm)
{
  int count = 0;
  struct chunk *blk;
  if(APR_ANYLOCK_LOCK(rmm->lock))
    ap_assert(!"lock failed");
  blk = BLOCK(rmm->membuf->first);
  if(blk->prev)
    ap_assert(!"Linked list head has previous");
  do {
#ifdef NOTDEFINED
    fprintf(stderr,"block %d %s addr=%p offset=%d size=%d\n", count,
            (blk->magic == MAGICFREE?"free ":"alloc"),
            blk, BLOCKADDR(blk), blk->size);
    fflush(stderr);
#endif
    if(blk->magic != MAGICFREE && blk->magic != MAGICOCCUPIED)
      ap_assert(!"Wrong chunk begin marker");
    if(MARKER(blk) != MAGICMARKER)
      ap_assert(!"Wrong chunk end marker");
    if(blk->next != 0 && BLOCK(blk->next) < blk)
      ap_assert(!"Linked list not in order");
    if(blk->next != 0 && BLOCK(BLOCK(blk->next)->prev) != blk)
      ap_assert(!"Linked list not correctly linked");
    blk = BLOCK(blk->next);
    ++count;
  } while(blk != (void*)rmm->membuf);
  if(APR_ANYLOCK_UNLOCK(rmm->lock))
    ap_assert(!"unlock failed");
}

/****************************************************************************/

apr_status_t
myapr_rmm_init(apr_rmm_t **rmmptr, apr_anylock_t *lock,
               void* membuf, apr_size_t memsize, 
               apr_pool_t *cont)
{
  apr_rmm_t *rmm;
  struct chunk *blk;
  memsize = (memsize / sizeof(align_t)) * sizeof(align_t);
  *rmmptr = rmm = (apr_rmm_t*) malloc(sizeof(struct apr_rmm_t));
  rmm->lock    = lock;
  rmm->membuf  = (void *) membuf;
  blk = &rmm->membuf->data;
  rmm->membuf->size  = memsize;
  rmm->membuf->first = BLOCKADDR(blk);
  blk->magic = MAGICFREE;
  blk->size  = memsize
             - ( ((char*)(&blk->data)) - (char*)(rmm->membuf) )
             - sizeof(apr_uint64_t);
  blk->next  = 0;
  blk->prev  = 0;
  MARKER(blk) = MAGICMARKER;
  APR_RMM_VALIDATE(rmm);
  return APR_SUCCESS;
}

apr_status_t
myapr_rmm_destroy(apr_rmm_t *rmm)
{
  APR_RMM_VALIDATE(rmm);
  free(rmm);
  return APR_SUCCESS;
}

apr_status_t
myapr_rmm_attach(apr_rmm_t **rmm, apr_anylock_t *lock,
                 void* membuf, apr_pool_t *cont)
{
  *rmm = (apr_rmm_t*) malloc(sizeof(struct apr_rmm_t));
  (*rmm)->lock    = lock;
  (*rmm)->membuf  = (struct header *) membuf;
  APR_RMM_VALIDATE(*rmm);
  return APR_SUCCESS;
}

apr_status_t
myapr_rmm_detach(apr_rmm_t *rmm)
{
  apr_rmm_destroy(rmm);
  return APR_SUCCESS;
}

apr_rmm_off_t
myapr_rmm_malloc(apr_rmm_t *rmm, apr_size_t reqsize)
{
  apr_rmm_off_t rtnaddr = 0;
  struct chunk *blk, *found = NULL;
  APR_RMM_VALIDATE(rmm);
  if(APR_ANYLOCK_LOCK(rmm->lock))
    ap_assert(!"lock failed");
  blk = BLOCK(rmm->membuf->first);
  reqsize = ((reqsize+sizeof(align_t)-1)/sizeof(align_t))*sizeof(align_t);
  /* The algorithm used is very sub-optimal, a single double-linked list holds
   * all blocks, and the best fit is search linearly.  It works, but is flow.
   * The main problem is that if we would ever run out of memory, we are SOL
   * therefor a best-fit is preferable over first-fit.
   */
  do {
    if(blk->magic == MAGICFREE && blk->size >= reqsize)
      if(!found || found->size > blk->size)
        found = blk;
    blk =  BLOCK(blk->next);
  } while(blk != (void*)rmm->membuf);
  if(found) {
    if(found->size > reqsize + emptychunksize) {
      blk = (struct chunk*)&((unsigned char*)&found->data)[reqsize+sizeof(apr_uint64_t)];
      blk->magic    = MAGICFREE;
      blk->size     = found->size - emptychunksize - reqsize;
      blk->next     = found->next;
      blk->prev     = BLOCKADDR(found);
      MARKER(blk)   = MAGICMARKER;
      if(found->next)
        BLOCK(found->next)->prev = BLOCKADDR(blk);
      found->next   = BLOCKADDR(blk);
      found->size   = reqsize;
      MARKER(found) = MAGICMARKER;
    }
    found->magic = MAGICOCCUPIED;
    memset(&found->data, 0xff, found->size);
    rtnaddr = BLOCKADDR(found);
  }
  if(APR_ANYLOCK_UNLOCK(rmm->lock))
    ap_assert(!"unlock failed");
  APR_RMM_VALIDATE(rmm);
  return rtnaddr;
}

apr_status_t
myapr_rmm_free(apr_rmm_t *rmm, apr_rmm_off_t entity)
{
  struct chunk *neighbblk, *blk;
  APR_RMM_VALIDATE(rmm);
  if(APR_ANYLOCK_LOCK(rmm->lock))
    ap_assert(!"lock failed");
  blk = BLOCK(entity);
  if(blk->magic != MAGICOCCUPIED)
    ap_assert(!"Memory being freed isn't allocated");
  blk->magic = MAGICFREE;
  memset(&blk->data, 0xff, blk->size);
  if(blk->next) {
    neighbblk = BLOCK(blk->next);
    if(neighbblk->magic == MAGICFREE) {
      blk->next  = neighbblk->next;
      blk->size += neighbblk->size + emptychunksize;
      if(neighbblk->next)
        BLOCK(neighbblk->next)->prev = BLOCKADDR(blk);
    }
  }
  if(blk->prev) {
    neighbblk = BLOCK(blk->prev);
    if(neighbblk->magic == MAGICFREE) {
      if(blk->next)
        BLOCK(blk->next)->prev = BLOCKADDR(neighbblk);
      neighbblk->next  = blk->next;
      neighbblk->size += blk->size + emptychunksize;
      blk = neighbblk;
    }
  }
  if(APR_ANYLOCK_UNLOCK(rmm->lock))
    ap_assert(!"unlock failed");
  APR_RMM_VALIDATE(rmm);
  return APR_SUCCESS;
}

apr_rmm_off_t
myapr_rmm_realloc(apr_rmm_t *rmm, void *entity, apr_size_t reqsize)
{
  apr_size_t oldsize;
  apr_rmm_off_t newarea;
  struct chunk *oldchunk;
  oldchunk = (struct chunk *) &rmm->membuf[myapr_rmm_offset_get(rmm,entity)];
  oldsize  = oldchunk->size;
  newarea  = myapr_rmm_malloc(rmm, reqsize);
  memcpy(myapr_rmm_addr_get(rmm,newarea), entity, oldsize);
  myapr_rmm_free(rmm, myapr_rmm_offset_get(rmm, entity));
  return newarea;
}

apr_rmm_off_t
myapr_rmm_calloc(apr_rmm_t *rmm, apr_size_t reqsize)
{
  apr_rmm_off_t area;
  area = myapr_rmm_malloc(rmm, reqsize);
  memset(myapr_rmm_addr_get(rmm, area), 0x00, reqsize);
  APR_RMM_VALIDATE(rmm);
  return area;
}

void *
myapr_rmm_addr_get(apr_rmm_t *rmm, apr_rmm_off_t entity)
{
  APR_RMM_VALIDATE(rmm);
  return &(BLOCK(entity)->data);
}

apr_rmm_off_t
myapr_rmm_offset_get(apr_rmm_t *rmm, void* entity)
{
  return BLOCKADDR(((char*)entity) - offsetof(struct chunk, data));
}

apr_size_t
myapr_rmm_overhead_get(int n)
{
  return n * (sizeof(struct chunk) - sizeof(align_t) + sizeof(apr_uint64_t));
}

void
myapr_rmm_usage(apr_rmm_t *rmm, apr_size_t* size,
                apr_size_t* bytesused, apr_size_t* bytesavailable,
                apr_size_t* nitems)
{
  apr_size_t used=0, avail=0;
  struct chunk *blk;
  apr_rmm_off_t curr;
  int count = 0;
  myapr_rmm_validate(rmm);
  if(APR_ANYLOCK_LOCK(rmm->lock))
    ap_assert(!"lock failed");
  for(curr=rmm->membuf->first; curr; curr=blk->next) {
    blk = BLOCK(curr);
    switch(blk->magic) {
    case MAGICFREE:
      avail += blk->size;
      break;
    case MAGICOCCUPIED:
      used += blk->size;
      ++count;
      break;
    }
  }
  if(size)
    *size = rmm->membuf->size;
  if(APR_ANYLOCK_UNLOCK(rmm->lock))
    ap_assert(!"unlock failed");
  if(bytesused)
    *bytesused = used;
  if(bytesavailable)
    *bytesavailable = avail;
  if(nitems)
    *nitems = count;
}

void *
myapr_rmm_dump(apr_rmm_t *rmm, apr_pool_t *pool,
               int *nobjects, apr_rmm_off_t **addrs, apr_size_t **sizes)
{
  int count;
  apr_rmm_off_t curr;
  struct chunk *blk;
  char *buffer;
  if(APR_ANYLOCK_LOCK(rmm->lock))
    ap_assert(!"lock failed");
  if((buffer = apr_palloc(pool, rmm->membuf->size))) {
    memcpy(buffer, rmm->membuf, rmm->membuf->size);
    if(APR_ANYLOCK_UNLOCK(rmm->lock))
      ap_assert(!"unlock failed");
    for(curr=((struct header *)buffer)->first, count=0; curr; curr=blk->next) {
      blk = (struct chunk *)&buffer[curr];
      if(blk->magic == MAGICOCCUPIED)
        ++count;
    }
    ++count; /* add the last block, which is a free block with size=0 */
    *nobjects = count;
    if(addrs)
      if(!(*addrs = apr_palloc(pool, sizeof(apr_rmm_off_t) * count)))
        addrs = NULL;
    if(sizes)
      if(!(*sizes = apr_palloc(pool, sizeof(apr_size_t) * count)))
        sizes = NULL;
    for(curr=((struct header*)buffer)->first, count=0;
        curr; curr=blk->next)
      {
        blk = (struct chunk *)&buffer[curr];
        if(blk->magic == MAGICOCCUPIED) {
          if(addrs)
            (*addrs)[count] = curr;
          if(sizes)
            (*sizes)[count] = blk->size + emptychunksize;
          ++count;
        }
      }
    (*addrs)[count] = rmm->membuf->size;
    (*sizes)[count] = 0;
  } else {
    if(APR_ANYLOCK_UNLOCK(rmm->lock))
      ap_assert(!"unlock failed");
    *nobjects = -1;
  }
  return buffer;
}

/****************************************************************************/

#ifdef MAIN
#include <stdio.h>
#include <apr_shm.h>
#include <apr_strings.h>

#ifndef CHECK
#define CHECK(ARG) \
  do{if(ARG){fprintf(stderr,"call " #ARG " failed\n");abort();}}while(0)
#endif

void
ap_log_assert(const char *expression, const char *file, int line)
{
  fprintf(stderr,"assertion \"%s\" failed at %s line %d\n",expression,
          file,line);
  abort();
}

int
main(int argc, char *argv[])
{
  apr_rmm_t* rmmhandle;
  apr_shm_t* shmhandle;
  apr_pool_t* pool;
  apr_allocator_t *allocer;
  const char* tempdir;
  char* tempfile;
  apr_file_t *filedes;
  apr_anylock_t lock;

  CHECK(apr_initialize());
  CHECK(apr_pool_create(&pool, NULL));
  CHECK(apr_temp_dir_get(&tempdir, pool));
  tempfile = apr_pstrcat(pool, tempdir, "/g10bu13XXXXXX", NULL);
  CHECK(apr_file_mktemp(&filedes, tempfile, 0, pool));
  apr_file_close(filedes);
  lock.type = apr_anylock_procmutex;
  CHECK(apr_proc_mutex_create(&lock.lock.pm,tempfile,APR_LOCK_DEFAULT,pool));

  switch(argc) {
  case 1: {
    apr_rmm_off_t m1, m2, m3, m4, m5;
    const apr_size_t reqsize = 10485760;
    CHECK(apr_shm_create(&shmhandle, reqsize, NULL, pool));
    CHECK(myapr_rmm_init(&rmmhandle, &lock,
                         apr_shm_baseaddr_get(shmhandle),
                         apr_shm_size_get(shmhandle), pool));

    fprintf(stderr, "alloc(56)..");  m1 = myapr_rmm_malloc(rmmhandle, 56);
    fprintf(stderr, "set..");        memset(myapr_rmm_addr_get(rmmhandle, m1), 0xaa, 56);
    fprintf(stderr, "free..");       myapr_rmm_free(rmmhandle, m1);
    fprintf(stderr, "\n");

    fprintf(stderr, "alloc(16).. ");   m1 = myapr_rmm_malloc(rmmhandle, 56);
    fprintf(stderr, "alloc(2).. ");    m2 = myapr_rmm_malloc(rmmhandle, 2);
    fprintf(stderr, "alloc(64).. ");   m3 = myapr_rmm_malloc(rmmhandle, 64);
    fprintf(stderr, "alloc(64').. ");  m4 = myapr_rmm_malloc(rmmhandle, 64);
    fprintf(stderr, "alloc(64\").. "); m5 = myapr_rmm_malloc(rmmhandle, 64);
    fprintf(stderr, "free(2).. ");     myapr_rmm_free(rmmhandle, m2);
    fprintf(stderr, "free(64).. ");    myapr_rmm_free(rmmhandle, m3);
    fprintf(stderr, "alloc(64).. ");   m3 = myapr_rmm_malloc(rmmhandle, 64);
    fprintf(stderr, "free(64').. ");   myapr_rmm_free(rmmhandle, m4);
    fprintf(stderr, "free(64\").. ");  myapr_rmm_free(rmmhandle, m5);
    fprintf(stderr, "free(16).. ");    myapr_rmm_free(rmmhandle, m1);
    fprintf(stderr, "alloc(18).. ");   m1 = myapr_rmm_malloc(rmmhandle, 16);
    fprintf(stderr, "\n\n");
    APR_RMM_VALIDATE(rmmhandle);
    fprintf(stderr, "\n\n");
    fprintf(stderr,"done.\n");
    break;
  }
  case 2: {
    apr_finfo_t finfo;
    apr_size_t size;
    apr_rmm_off_t* addrs;
    apr_size_t* sizes;
    unsigned char *addr;
    int count, c, i, j;
    CHECK(apr_stat(&finfo, argv[1], APR_FINFO_SIZE, pool));
    size = finfo.size;
    fprintf(stderr, "creating shared memory segment of %d bytes\n",size);
    CHECK(apr_shm_create(&shmhandle, finfo.size, NULL, pool));
    CHECK(myapr_rmm_init(&rmmhandle, &lock, apr_shm_baseaddr_get(shmhandle),
                         finfo.size, pool));
    fprintf(stderr, "reading in dumped state\n");
    CHECK(apr_file_open(&filedes, argv[1], APR_READ|APR_BINARY, 0, pool));
    CHECK(apr_file_read(filedes, apr_shm_baseaddr_get(shmhandle),
                        &size));
    fprintf(stderr, "validating\n");
    myapr_rmm_validate(rmmhandle);
    fprintf(stderr, "interpreting\n");
    myapr_rmm_dump(rmmhandle, pool, &count, &addrs, &sizes);
    for(c=0; c<count; c++) {
      addr = myapr_rmm_addr_get(rmmhandle, addrs[c]);
      if(sizes[c] >= 64*1024) {
        printf("%04lx:%04lx <<..%d..>>\n", addrs[c]>>16, addrs[c]&0xffff,
               sizes[c]);
      } else {
        for(i=0; i<sizes[c]; i+=16) {
          if(i==0)
            printf("%04lx:%04lx", addrs[c]>>16, addrs[c]&0xffff);
          else
            printf("         ");
          for(j=0; j<16; j++) {
            if((i+j)%4 == 0)
              putchar(' ');
            if(i+j >= sizes[c]) {
              putchar(' ');
              putchar(' ');
            } else
              printf("%02x",addr[i+j]);
          }
          putchar(' ');
          putchar(' ');
          for(j=0; j<16; j++) {
            if(i+j < sizes[c])
              if(isprint(addr[i+j]) && !isspace(addr[i+j]))
                putchar(addr[i+j]);
              else
                putchar('.');
          }
          putchar('\n');
        }
      }
    }
    fprintf(stderr, "finishing off\n");
    CHECK(apr_file_close(filedes));
    break;
  }
  }

  CHECK(myapr_rmm_destroy(rmmhandle));
  CHECK(apr_shm_destroy(shmhandle));
  apr_pool_destroy(pool);
  exit(0);
}

#endif

/****************************************************************************/
