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
#include "Allocator.hpp"
#include <httpd.h>
#include <http_log.h>
#ifndef SINGLECHILD
#include <apr_proc_mutex.h>
#include "locking.hpp"
extern "C" {
#ifndef WIN32
#include <unixd.h>
#endif /* !WIN32 */
};
#endif

using std::bad_alloc;
using std::nothrow_t;

// namespace globule {

rmmmemory*    rmmmemory::_alloc;
#ifndef SINGLECHILD
apr_shm_t*    rmmmemory::shmhandle;
apr_rmm_t*    rmmmemory::rmmhandle;
apr_anylock_t rmmmemory::shmlock;
char*         rmmmemory::shmlockfname;
#endif
apr_size_t    rmmmemory::_shmsize;
apr_size_t    rmmmemory::_prealloc;
#ifdef RECORDSHMUSAGE
apr_size_t*   rmmmemory::_bytesinuse = 0;
apr_size_t*   rmmmemory::_itemsinuse = 0;
#endif
#ifdef RECORDSHMALLOC
apr_file_t*   rmmmemory::_logfp;
#endif

#ifndef SINGLECHILD
#ifdef RECORDSHMALLOC
#undef allocate
#undef deallocate
#undef reallocate
#endif
#endif

void
rmmmemory::shm(apr_pool_t* pconf, apr_size_t shmem_sz,
               apr_size_t prealloc, void** pptr)
{
#ifdef RECORDSHMUSAGE
  prealloc += sizeof(apr_size_t) * 2;
#endif
#ifdef SINGLECHILD
  if(pptr)
    *pptr = malloc(prealloc);
#else
  void *shmaddr;
  apr_size_t availsize;
  apr_status_t status;
#ifdef RECORDSHMALLOC
// APR_FOPEN_CREATE|APR_FOPEN_TRUNCATE|APR_FOPEN_APPEND,
  if(getenv("SHMLOG") && *(getenv("SHMLOG"))) {
    status = apr_file_open(&_logfp, getenv("SHMLOG"),
#if (APR_MAJOR_VERSION > 0)
                           APR_FOPEN_APPEND|APR_FOPEN_TRUNCATE|APR_FOPEN_WRITE|
                           APR_FOPEN_CREATE|APR_FOPEN_XTHREAD,
#else
                           APR_APPEND|APR_TRUNCATE|APR_WRITE|
                           APR_CREATE|APR_XTHREAD,
#endif
                           APR_OS_DEFAULT, pconf);
    if(status != APR_SUCCESS) {
      ap_log_perror(APLOG_MARK, APLOG_ERR, status, pconf,
                    "Cannot open shared memory usage log");
      _logfp = NULL;
    }
  } else
    _logfp = NULL;
#endif

  if((status = apr_shm_create(&shmhandle, shmem_sz, NULL, pconf))) {
#ifndef STANDALONE_APR
    ap_log_perror(APLOG_MARK, APLOG_EMERG, status, pconf,
                  "Cannot allocate enough shared memory");
#else
    return;
#endif
  }
  _shmsize  = shmem_sz;
  _prealloc = prealloc;
  shmaddr   = apr_shm_baseaddr_get(shmhandle);
  availsize = apr_shm_size_get(shmhandle);
  if(pptr)
    *pptr   = (struct sharedstate *) shmaddr;
  shmaddr   = &((apr_byte_t *)shmaddr)[prealloc];
  availsize -= prealloc;
#ifdef RECORDSHMUSAGE
  _bytesinuse = (apr_size_t*)*pptr;   *pptr = ((apr_byte_t*)*pptr) + sizeof(apr_size_t);
  _itemsinuse = (apr_size_t*)*pptr;   *pptr = ((apr_byte_t*)*pptr) + sizeof(apr_size_t);
  *_bytesinuse = prealloc;
  *_itemsinuse = 1;
#endif
  shmlockfname = Lock::mklockfname(pconf);
  shmlock.type = apr_anylock_t::apr_anylock_procmutex;
  status = apr_proc_mutex_create(&shmlock.lock.pm, shmlockfname,
                                 APR_LOCK_DEFAULT, pconf);
  if(status != APR_SUCCESS) {
#ifndef STANDALONE_APR
    ap_log_perror(APLOG_MARK, APLOG_CRIT, status, pconf,
                  "Cannot create memory segment process lock");
#else
    ;
#endif
  }
#ifndef WIN32
  if((status = unixd_set_proc_mutex_perms(shmlock.lock.pm)) != APR_SUCCESS) {
#ifndef STANDALONE_APR
    ap_log_perror(APLOG_MARK, APLOG_CRIT, status, pconf,
                 "Error setting permissions on segment process lock");
#else
    ;
#endif
  }
#endif

  status = apr_rmm_init(&rmmhandle, &shmlock, shmaddr, availsize, pconf);
  if(status != APR_SUCCESS) {
#ifndef STANDALONE_APR
    ap_log_perror(APLOG_MARK, APLOG_EMERG, status, pconf,
                  "Cannot allocate relocatable memory segment");
#else
    return;
#endif
  }
#endif
  _alloc = new rmmallocator<char>;
}

void
rmmmemory::childinit(apr_pool_t* childpool)
{
  apr_status_t status;
  status = apr_proc_mutex_child_init(&shmlock.lock.pm,shmlockfname,childpool);
  if(status) {
    ap_log_perror(APLOG_MARK, APLOG_CRIT, status, childpool,
                  "Cannot reinitialize memory segment process lock");
  }
}

void*
rmmmemory::allocate(apr_size_t size)
  throw(std::bad_alloc)
{
  void *ptr;
#ifdef RECORDSHMUSAGE
  size += sizeof(apr_size_t);
#endif
#ifdef SINGLECHILD
  if(!(ptr = malloc(size)))
    throw std::bad_alloc();
#else
  apr_rmm_off_t mem;
  mem = apr_rmm_malloc(rmmhandle, size);
  if(!mem)
    throw std::bad_alloc();
  ptr = apr_rmm_addr_get(rmmhandle, mem);
#endif
#ifdef RECORDSHMUSAGE
  *(apr_size_t*)ptr = size - sizeof(apr_size_t);
  *_itemsinuse += 1;
  *_bytesinuse += size - sizeof(apr_size_t);
  ptr = ((apr_byte_t*)ptr) + sizeof(apr_size_t);
#endif
  return ptr;
}

char*
rmmmemory::allocate(const char* s) throw(std::bad_alloc)
{
  return strcpy((char*) allocate(strlen(s)+1), s);
}

void
rmmmemory::deallocate(void* ptr)
{
#ifdef RECORDSHMUSAGE
  ptr = ((apr_byte_t*)ptr) - sizeof(apr_size_t);
  *_itemsinuse -= 1;
  *_bytesinuse -= *(apr_size_t*)ptr;
#endif
#ifdef SINGLECHILD
  free(ptr);
#else
  apr_rmm_free(rmmhandle, apr_rmm_offset_get(rmmhandle, ptr));
#endif
}

void*
rmmmemory::reallocate(void* ptr, apr_size_t size)
  throw(std::bad_alloc)
{
#ifdef RECORDSHMUSAGE
  ptr = ((apr_byte_t*)ptr) - sizeof(apr_size_t);
  apr_size_t oldsize = *(apr_size_t*)ptr;
  size += sizeof(apr_size_t);
#endif
#ifdef SINGLECHILD
  ptr = realloc(ptr, size);
#else
  apr_rmm_off_t mem;
  mem = apr_rmm_realloc(rmmhandle, ptr, size);
  if(!mem)
    throw std::bad_alloc();
  ptr = apr_rmm_addr_get(rmmhandle, mem);
#endif
#ifdef RECORDSHMUSAGE
  *(apr_size_t*)ptr = size - sizeof(apr_size_t);
  *_bytesinuse += (size - sizeof(apr_size_t)) - oldsize;
  ptr = ((apr_byte_t*)ptr) + sizeof(apr_size_t);
#endif
  return ptr;
}

void
rmmmemory::usage(apr_size_t* size, apr_size_t* bytesused,
                 apr_size_t* bytesavail, apr_size_t* nitems)
{
#ifdef HAVE_MYAPR
  myapr_rmm_usage(rmmhandle, size, bytesused, bytesavail, nitems);
  if(size)
    *size += _prealloc;
  if(bytesused)
    *bytesused += _prealloc;
  if(nitems)
    *nitems += 1;
#else
#ifdef RECORDSHMUSAGE
  if(size)
    *size = _shmsize;
  if(bytesused)
    *bytesused = *_bytesinuse;
  if(bytesavail)
    *bytesavail = _shmsize - *_bytesinuse;
  if(nitems)
    *nitems = *_itemsinuse;
#else
  if(bytesused)
    *bytesused = 0;
  if(nitems)
    *nitems = 0;
#endif
#endif
}

void*
operator new(size_t sz, rmmmemory& mem)
  throw(std::bad_alloc)
{
  return mem.allocate(sz);
}

void*
operator new[](size_t sz, rmmmemory& mem)
  throw(std::bad_alloc)
{
  return mem.allocate(sz);
}

void*
operator new[](size_t sz, rmmmemory* mem)
  throw(std::bad_alloc)
{
  return mem->allocate(sz);
}

void*
operator new(size_t sz, rmmmemory* mem)
  throw(std::bad_alloc)
{
  return mem->allocate(sz);
}

void*
operator new(size_t sz, const nothrow_t&, rmmmemory& mem)
  throw()
{
  try {
    return operator new(sz, mem);
  } catch(bad_alloc) {
    return 0;
  }
}

void*
operator new[](size_t sz, const nothrow_t&, rmmmemory& mem)
  throw()
{
  try {
    return operator new[](sz, mem);
  } catch(bad_alloc) {
    return 0;
  }
}

void*
operator new[](size_t sz, const nothrow_t&, rmmmemory* mem)
  throw()
{
  try {
    return operator new[](sz, mem);
  } catch(bad_alloc) {
    return 0;
  }
}

void*
operator new(size_t sz, const nothrow_t&, rmmmemory* mem)
  throw()
{
  try {
    return operator new(sz, mem);
  } catch(bad_alloc) {
    return 0;
  }
}

void*
operator new(size_t sz, void* ptr, rmmmemory& mem)
  throw()
{
  return ptr; // [Stroustrup97, p. 576]
}

void*
operator new[](size_t sz, void* ptr, rmmmemory& mem)
  throw()
{
  return ptr; // [Stroustrup97, p. 576]
}

void*
operator new[](size_t sz, void* ptr, rmmmemory* mem)
  throw()
{
  return ptr; // [Stroustrup97, p. 576]
}

void*
operator new(size_t sz, void* ptr, rmmmemory* mem)
  throw()
{
  return ptr; // [Stroustrup97, p. 576]
}

void
operator delete(void* ptr, rmmmemory& mem)
  throw()
{
  mem.deallocate(ptr);
}

void
operator delete[](void* ptr, rmmmemory& mem)
  throw()
{
  mem.deallocate(ptr);
}

void
operator delete[](void* ptr, rmmmemory* mem)
  throw()
{
  mem->deallocate(ptr);
}

void
operator delete(void* ptr, rmmmemory* mem)
  throw()
{
  mem->deallocate(ptr);
}

void
operator delete(void* ptr, const nothrow_t&, rmmmemory& mem)
  throw()
{
  operator delete(ptr, mem);
}

void
operator delete[](void* ptr, const nothrow_t&, rmmmemory& mem)
  throw()
{
  operator delete[](ptr, mem);
}

void
operator delete[](void* ptr, const nothrow_t&, rmmmemory* mem)
  throw()
{
  operator delete[](ptr, mem);
}

void
operator delete(void* ptr, const nothrow_t&, rmmmemory* mem)
  throw()
{
  operator delete(ptr, mem);
}

void
operator delete(void* ptr, void* p, rmmmemory& mem)
  throw()
{
  // [Stroustrup97, p. 576]
}

void
operator delete[](void* ptr, void* p, rmmmemory& mem)
  throw()
{
  // [Stroustrup97, p. 576]
}

void
operator delete[](void* ptr, void* p, rmmmemory* mem)
  throw()
{
  // [Stroustrup97, p. 576]
}

void
operator delete(void* ptr, void* p, rmmmemory* mem)
  throw()
{
  // [Stroustrup97, p. 576]
}

#ifndef SINGLECHILD
#ifdef RECORDSHMALLOC
#undef allocate
#undef deallocate
#undef reallocate
void*
rmmmemory::allocate_debug(char* file, int line, apr_size_t size)
  throw(std::bad_alloc)
{
  void* newptr;
  newptr = allocate(size);
  if(_logfp)
    apr_file_printf(_logfp, "SHM ALLOC %s:%d %" APR_UINT64_T_FMT
             " %d (used: %d bytes in %d objects)\n", file, line,
             (apr_uint64_t)newptr, size, *_bytesinuse, *_itemsinuse);
  return newptr;
}

char*
rmmmemory::allocate_debug(char* file, int line, const char *s)
  throw(std::bad_alloc)
{
  char* newptr;
  int size = (s ? (signed)strlen(s) : -1);
  newptr = allocate(s);
  if(_logfp)
    apr_file_printf(_logfp, "SHM STRING %s:%d %" APR_UINT64_T_FMT
             " %d (used: %d bytes in %d objects)\n", file, line,
             (apr_uint64_t)newptr, size,
             *rmmmemory::_bytesinuse, *rmmmemory::_itemsinuse);
  return newptr;
}

void
rmmmemory::deallocate_debug(char* file, int line, void* ptr)
  throw(std::bad_alloc)
{
  if(_logfp)
    apr_file_printf(_logfp, "SHM DEALLOC %s:%d %" APR_UINT64_T_FMT
             " (used: %d bytes in %d objects)\n", file, line,
             (apr_uint64_t)ptr, *_bytesinuse, *_itemsinuse);
  deallocate(ptr);
}

void*
rmmmemory::reallocate_debug(char* file, int line, void* ptr, apr_size_t size)
  throw(std::bad_alloc)
{
  void* newptr;
  newptr = reallocate(ptr,size);
  if(_logfp)
    apr_file_printf(_logfp, "SHM REALLOC %s:%d %" APR_UINT64_T_FMT
             "->%" APR_UINT64_T_FMT " %d (used: %d bytes in %d objects)\n",
             file, line, (apr_uint64_t)ptr, (apr_uint64_t)newptr, size,
             *_bytesinuse, *_itemsinuse);
  return newptr;
}
#endif
#endif

// } // namespace globule
