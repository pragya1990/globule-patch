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
#ifndef _LOCKING_HPP
#define _LOCKING_HPP

#include <apr.h>
#include <apr_global_mutex.h>

#ifndef DEBUGLOCK
#define DEBUGLOCK(CLASS,OPERATION)
#define DEBUGACTION(CLASS,METHOD,OPERATION)
#else
#undef DEBUGLOCK
#define DEBUGLOCK(CLASS,OPERATION) do { fprintf(stderr,"LOCK %06d %s::%s %p\n",getpid(),#CLASS,#OPERATION,this); fflush(stderr); } while(0)
#define DEBUGACTION(CLASS,METHOD,OPERATION) do { fprintf(stderr,"ACTION %06d %s::%s.%s\n",getpid(),#CLASS,#METHOD,#OPERATION); fflush(stderr); } while(0)
#endif

class ResourceError
{
private:
  const char* _message;
public:
  inline ResourceError(const char* msg) throw() : _message(msg) { };
  inline const char* message() const { return _message; };
};

class ResourceException
{
private:
  const char* _message;
public:
  inline ResourceException(const char* msg) throw() : _message(msg) { };
  inline const char* message() const { return _message; };
};

class Lock
{
public:
  static const int minnlocks = 16;
  static int defnlocks;
private:
  static int _nlocks;
  static int* _navailable;
  static apr_global_mutex_t*  _global;
  static apr_global_mutex_t** _availables;
  static apr_global_mutex_t** _muxes;
  static char** _muxfnames;
private:
  int  _concurrency;
  bool _exclusive;
  int  _sharers;
  apr_global_mutex_t** _locks;
private:
  inline void lock(apr_global_mutex_t* mux) throw(ResourceError);
  inline void unlock(apr_global_mutex_t* mux) throw(ResourceError);
  inline bool trylock(apr_global_mutex_t* mux) throw(ResourceError);
  apr_global_mutex_t* reserve() throw(ResourceError);
  static void release(apr_global_mutex_t* mux) throw();
  void reserve(int concurrency) throw(ResourceError);
  void release() throw();
public:
  static char* mklockfname(apr_pool_t* pool) throw(ResourceError);
  static void initialize(apr_pool_t* pool) throw(ResourceError,ResourceException);
  static void finalize(apr_pool_t* pool) throw();
  Lock(int concurrency=1) throw();
  ~Lock() throw();
  inline void lock() throw(ResourceError) { lock_exclusively(); };
  bool try_lock_exclusively() throw(ResourceError);
  void lock_exclusively() throw(ResourceError);
  void lock_shared() throw(ResourceError);
  void unlock() throw(ResourceError);
};

#endif
