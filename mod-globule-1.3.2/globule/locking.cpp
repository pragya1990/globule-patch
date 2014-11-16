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
#include "locking.hpp"
#include <apr_strings.h>
#include <httpd.h>
#include <http_log.h>
extern "C" {
#include <ap_mpm.h>
};
#ifndef WIN32
extern "C" {
#include <sys/types.h>
#include <unistd.h>
#include <unixd.h>
};
#endif

#include "alloc/Allocator.hpp"

int Lock::defnlocks = 64;
int Lock::_nlocks = 0;
int* Lock::_navailable;
apr_global_mutex_t*  Lock::_global;
apr_global_mutex_t** Lock::_availables;
apr_global_mutex_t** Lock::_muxes;
char** Lock::_muxfnames;

inline void
Lock::lock(apr_global_mutex_t* mux)
  throw(ResourceError)
{
  if(apr_global_mutex_lock(mux) != APR_SUCCESS)
    throw ResourceError("internal lock error");
}

inline void
Lock::unlock(apr_global_mutex_t* mux)
  throw(ResourceError)
{
  if(apr_global_mutex_unlock(mux) != APR_SUCCESS)
    throw ResourceError("internal lock error");
}

inline bool
Lock::trylock(apr_global_mutex_t* mux)
  throw(ResourceError)
{
  apr_status_t status = apr_global_mutex_trylock(mux);
  if(APR_STATUS_IS_EBUSY(status) || APR_STATUS_IS_ENOTIMPL(status))
    return false;
  if(status != APR_SUCCESS)
    throw ResourceError("internal lock error");
  return true;
}

apr_global_mutex_t*
Lock::reserve()
  throw(ResourceError)
{
  if(*_navailable == 0) {
    // throw ResourceError("available system lock");
    fprintf(stderr,"GLOBULE: LOCKING SYSTEM OVERLOADED\n"); // FIXME
    fflush(stderr);
    while(*_navailable == 0) {
      unlock(_global);
      apr_sleep(1000);
      lock(_global);
    }
  }
  return _availables[--*_navailable];
}

void
Lock::release(apr_global_mutex_t* mux)
  throw()
{
  _availables[(*_navailable)++] = mux;
}

void
Lock::reserve(int concurrency)
  throw(ResourceError)
{
  if(*_navailable < concurrency) {
    // throw ResourceError("available system lock");
    fprintf(stderr,"GLOBULE: LOCKING SYSTEM OVERLOADED\n"); // FIXME
    fflush(stderr);
    while(*_navailable == 0) {
      unlock(_global);
      apr_sleep(1000);
      lock(_global);
    }
  }
  *_navailable -= concurrency;
  memcpy(_locks, &_availables[*_navailable],
         concurrency * sizeof(apr_global_mutex_t*));
}

void
Lock::release()
  throw()
{
  memcpy(&_availables[*_navailable], _locks,
         _concurrency * sizeof(apr_global_mutex_t*));
  *_navailable += _concurrency;
}

char*
Lock::mklockfname(apr_pool_t* pool)
  throw(ResourceError)
{
  /* The following would work to obtain a temporary filename, were it not
   * for windblows;
   *     char lock_name[L_tmpnam];
   *     tmpnam(lock_name);
   *     apr_global_mutex_create(&_lock,lock_name,APR_LOCK_DEFAULT,p);
   */
  const char* tempdir;
  apr_status_t status = apr_temp_dir_get(&tempdir, pool);
  if(status == APR_SUCCESS) {
    const char* prefix = "g10bu13XXXXXX";
    apr_file_t* fp;
    char* lockname = apr_pstrcat(pool, tempdir, "/", prefix, NULL);
    status = apr_file_mktemp(&fp, lockname, 0, pool);
    if(status == APR_SUCCESS) {
      apr_file_close(fp);
      return lockname;
    }
  }
  return 0;
}

void
Lock::initialize(apr_pool_t* pool)
  throw(ResourceError,ResourceException)
{
  int i;
  apr_status_t status;
  if(_nlocks == 0) {

    int nsparelocks = 4;
    apr_pool_t* sparespool;
    apr_pool_create(&sparespool, pool);
    apr_global_mutex_t** sparelocks = new apr_global_mutex_t*[nsparelocks];
    char** sparefnames = new char*[nsparelocks];
    for(i=0; i<nsparelocks; i++) {
      sparefnames[i] = mklockfname(sparespool);
      status = apr_global_mutex_create(&sparelocks[i], sparefnames[i],
                                       APR_LOCK_DEFAULT, sparespool);
      if(status != APR_SUCCESS) {
        throw ResourceError("absolute minimum number of system locks");
      }
    }

    if(ap_mpm_query(AP_MPMQ_MAX_DAEMON_USED,&i) == APR_SUCCESS) {
      defnlocks = i * 2;
    }
    _nlocks = defnlocks;
    _muxes = new apr_global_mutex_t*[_nlocks];
    _muxfnames = new char*[_nlocks];

    for(i=0; i<_nlocks; i++) {
      _muxfnames[i] = mklockfname(pool);
      status = apr_global_mutex_create(&_muxes[i], _muxfnames[i],
                                       APR_LOCK_DEFAULT, pool);
      if(status != APR_SUCCESS)
        break;
#ifndef STANDALONE_APR
#ifndef WIN32
      status = unixd_set_global_mutex_perms(_muxes[i]);
      if(status != APR_SUCCESS)
        ap_log_perror(APLOG_MARK, APLOG_CRIT, status, pool,
                      "Could not enable the use of required system resource "
                      "(permissions of global mutex)");
#endif
#endif
    }
    if(i != _nlocks) {
      apr_global_mutex_t** aux = _muxes;
      _muxes = new apr_global_mutex_t*[i];
      memcpy(_muxes, aux, sizeof(apr_global_mutex_t*) * i);
      delete[] aux;
      _nlocks = i;
    }

    /* free up mutexes which should be left available */
    for(i=0; i<nsparelocks; i++)
      apr_global_mutex_destroy(sparelocks[i]);
    delete[] sparelocks;
    delete[] sparefnames;
    apr_pool_destroy(sparespool);

    _global = _muxes[0];
    _navailable = new(rmmmemory::shm()) int;
    *_navailable = _nlocks - 1;
    _availables = new(rmmmemory::shm()) apr_global_mutex_t*[*_navailable];
    for(i=1; i<_nlocks; i++)
      _availables[i-1] = _muxes[i];
    if(_nlocks != defnlocks)
      if(_nlocks < minnlocks)
        throw ResourceError("minimum number of system locks");
      else
        throw ResourceException("fewer locks available as desirable; under heavy load this might criple your system");
  } else {
    for(int i=0; i<_nlocks; i++) {
      status = apr_global_mutex_child_init(&_muxes[i],_muxfnames[i],pool);
      if(status != APR_SUCCESS)
        throw ResourceError("initialize already obtained locks");
    }
  }
}

void
Lock::finalize(apr_pool_t* pool)
  throw()
{
  rmmmemory::destroy(_availables);
  rmmmemory::destroy(_navailable);
  for(int i=0; i<_nlocks; i++)
    apr_global_mutex_destroy(_muxes[i]);
  delete[] _muxes;
}

Lock::Lock(int concurrency)
  throw()
  : _concurrency(concurrency), _exclusive(false), _sharers(0)
{
  _locks = new(rmmmemory::shm()) apr_global_mutex_t*[_concurrency];
  for(int i=0; i<_concurrency; i++)
    _locks[i] = 0;
}

Lock::~Lock() throw()
{
  rmmmemory::destroy(_locks);
}

void
Lock::lock_exclusively()
  throw(ResourceError)
{
  int i;
  lock(_global);
  if(!_locks[0]) {
    reserve(_concurrency);
    _sharers = 1;
    _exclusive = true;
    for(i=0; i<_concurrency; i++)
      lock(_locks[i]);
    unlock(_global);
  } else {
    ++_sharers;
    unlock(_global);
    for(i=0; i<_concurrency; i++)
      lock(_locks[i]);
    _exclusive = true;
  }
}

bool
Lock::try_lock_exclusively()
  throw(ResourceError)
{
  int i;
  lock(_global);
  if(!_locks[0]) {
    reserve(_concurrency);
    _sharers = 1;
    _exclusive = true;
    for(i=0; i<_concurrency; i++)
      lock(_locks[i]);
    unlock(_global);
    return true;
  } else {
    ++_sharers;
    unlock(_global);
    for(i=0; i<_concurrency; i++)
      if(!trylock(_locks[i])) {
        while(i--)
          unlock(_locks[i]);
        return false;
      }
    _exclusive = true;
    return true;
  }
}

void
Lock::lock_shared()
  throw(ResourceError)
{
#ifdef WIN32
  int i = GetCurrentThreadId();
#else
  int i = getpid();
#endif
  i %= _concurrency;
  lock(_global);
  if(!_locks[0]) {
    reserve(_concurrency);
    _sharers = 1;
    _exclusive = false;
    lock(_locks[i]);
    unlock(_global);
  } else {
    ++_sharers;
    unlock(_global);
    lock(_locks[i]);
    _exclusive = false;
  }
}

void
Lock::unlock()
  throw(ResourceError)
{
  int i;
  lock(_global);
  if(_exclusive) {
    for(i=0; i<_concurrency; i++)
      unlock(_locks[i]);
  } else {
#ifdef WIN32
    i = GetCurrentThreadId();
#else
    i = getpid();
#endif
    i %= _concurrency;
    unlock(_locks[i]);
  }
  if(--_sharers == 0) {
    release();
    _locks[0] = 0;
  }
  unlock(_global);
}
