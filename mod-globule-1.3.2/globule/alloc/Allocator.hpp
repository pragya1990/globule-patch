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
#ifndef _ALLOCATOR_HPP
#define _ALLOCATOR_HPP

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <new>
#include <limits.h>
#include <apr.h>
#ifdef SINGLECHILD
#include <stdlib.h>
#endif
#ifdef HAVE_MYAPR
#include "myapr_rmm.h"
#else
#include <apr_rmm.h>
#endif
#ifndef SINGLECHILD
#include <apr_shm.h>
#include <apr_anylock.h>
#endif

#include <stdio.h>
#include <stddef.h>
// namespace globule {

#define RECORDSHMUSAGE
//#define RECORDSHMALLOC

#ifdef RECORDSHMALLOC
#include <apr_file_io.h>
#endif

class rmmmemory
{
  friend class MemoryInfoFilter;
private:
  static rmmmemory* _alloc;
  static apr_size_t _shmsize;
  static apr_size_t _prealloc;
#ifdef RECORDSHMUSAGE
  static apr_size_t *_bytesinuse;
  static apr_size_t *_itemsinuse;
#endif
#ifdef RECORDSHMALLOC
  static apr_file_t* _logfp;
#endif
protected:
#ifndef SINGLECHILD
  static apr_shm_t*    shmhandle;
  static apr_rmm_t*    rmmhandle;
  static apr_anylock_t shmlock;
  static char*         shmlockfname;
#endif
  rmmmemory() { };
public:
  ~rmmmemory() { };
  static void* allocate(apr_size_t size) throw(std::bad_alloc);
  static char* allocate(const char* s) throw(std::bad_alloc);
  static void deallocate(void* ptr);
  static void* reallocate(void* ptr, apr_size_t size) throw(std::bad_alloc);
#ifdef RECORDSHMALLOC
#undef allocate
#undef deallocate
#undef reallocate
  static void* allocate_debug(char*, int, apr_size_t size)
    throw(std::bad_alloc);
  static char* allocate_debug(char*, int, const char* s)
    throw(std::bad_alloc);
  static void deallocate_debug(char*, int, void* ptr)
    throw(std::bad_alloc);
  static void* reallocate_debug(char*, int, void* ptr, apr_size_t size)
    throw(std::bad_alloc);
#endif
public:
  static inline rmmmemory* shm() { return rmmmemory::_alloc; };
  static void shm(apr_pool_t* pconf, apr_size_t shmsize,
                  apr_size_t prealloc = 0, void** pptr = 0);
  static void usage(apr_size_t* size, apr_size_t* bytesused,
                    apr_size_t* bytesavail, apr_size_t* nitems);
  static void childinit(apr_pool_t* childpool);
  template<class T> static inline void destroy(T* p) {
    p->~T();
#ifdef RECORDSHMALLOC
    deallocate_debug(__BASE_FILE__ ";" __FILE__,__LINE__,p);
#else
    deallocate(p);
#endif
  };
};

#ifdef max
#undef max
#endif
template<class T>
class rmmallocator : public rmmmemory
{
public:
  typedef T value_type;
  typedef size_t size_type;
  typedef ptrdiff_t difference_type;
  typedef T* pointer;
  typedef const T* const_pointer;
  typedef T& reference;
  typedef const T& const_reference;

private:
  inline size_type max(size_type a, size_type b) const {
    if(a>b) return a; else return b;
  };

public:
  inline pointer address(reference r) const {
    return (pointer)&r;
  };
#ifndef WIN32
  inline const_pointer address(const_reference r) const {
    return (const_pointer)&r;
  };
#endif
  inline rmmallocator() throw() {};
  template<class U> rmmallocator(const rmmallocator<U>&) throw() {};
  ~rmmallocator() throw() {};
  inline pointer allocate(size_type n, typename rmmallocator<T>::const_pointer hint=0)
    throw(std::bad_alloc)
  {
#ifdef RECORDSHMALLOC
    return static_cast<pointer>(rmmmemory::allocate_debug(__BASE_FILE__ ";"
                                           __FILE__, __LINE__, n * sizeof(T)));
#else
    return static_cast<pointer>(rmmmemory::allocate(n * sizeof(T)));
#endif
  };
  inline void deallocate(pointer p, size_type n)
  {
#ifdef RECORDSHMALLOC
    rmmmemory::deallocate_debug(__BASE_FILE__ ";" __FILE__, __LINE__,
                                (void*) p);
#else
    rmmmemory::deallocate((void*) p);
#endif
  };
  void construct(pointer p, const_reference val) { new(p) T(val); };
  void destroy(pointer p) { p->~T(); };
  size_type init_page_size() { 
    return max(size_type(1), size_type(4096/sizeof(T))); 
  };
  size_type max_size() const { 
    return max(size_type(1), size_type(UINT_MAX/sizeof(T))); 
  };
  template<class U> struct rebind { typedef rmmallocator<U> other; };
  static void initialize(apr_rmm_t* rmm) { rmmhandle = rmm; };
};

template<class T>
inline bool
operator==(const rmmallocator<T>&, const rmmallocator<T>&) throw()
{
  return true;
}

template<class T>
inline bool
operator!=(const rmmallocator<T>&, const rmmallocator<T>&) throw()
{
  return false;
}

#ifndef STANDALONE_APR

#include <map>
template<typename S, typename T>
class gmap
  : public std::map<S,T,std::less<S>,rmmallocator<std::pair<S,T> > >
{ };

template<typename S, typename T>
class gmultimap
  : public std::multimap<S,T,std::less<S>,rmmallocator<std::pair<S,T> > >
{ };

#include <list>
template<typename T>
class glist
  : public std::list<T,rmmallocator<T> >
{ };

#include <vector>
template<typename T>
class gvector
  : public std::vector<T,rmmallocator<T> >
{
public:
  gvector()
    : std::vector<T,rmmallocator<T> >() { };
  gvector(int capacity, T val)
    : std::vector<T,rmmallocator<T> >(capacity,val) { };
};

#include <set>
template<typename T>
class gset
  : public std::set<T,std::less<T>,rmmallocator<T> >
{ };

#include <string>
typedef std::basic_string<char,std::char_traits<char>,rmmallocator<char> >
        gstring;

struct ltgstring {
  inline bool operator()(const gstring* s1, const gstring* s2) {
    return strcmp(s1->c_str(), s2->c_str()) < 0;
  };
};
inline gstring& operator+=(gstring& lval, const std::string& rval) {
  lval += rval.c_str();
  return lval;
};

inline bool
operator==(const gstring& s1, const std::string& s2)
{
  return !strcmp(s1.c_str(),s2.c_str());
};

inline bool
operator==(const std::string& s1, const gstring& s2)
{
  return !strcmp(s1.c_str(),s2.c_str());
};

inline bool
operator!=(const gstring& s1, const std::string& s2)
{
  return strcmp(s1.c_str(),s2.c_str());
};

inline bool
operator!=(const std::string& s1, const gstring& s2)
{
  return strcmp(s1.c_str(),s2.c_str());
};

#else
#include <map>
template<typename S, typename T>
class gmap : public std::map<S,T,std::less<S> >
{ };
template<typename S, typename T>
class gmultimap : public std::multimap<S,T,std::less<S> >
{ };
#include <list>
template<typename T>
class glist : public std::list<T>
{ };
#include <vector>
template<typename T>
class gvector : public std::vector<T>
{ };
#include <set>
template<typename T>
class gset : public std::set<T>
{ };
#include <string>
#define gstring std::string

#endif

/* Arno: ISO C++ has a new operator for objects and one for arrays, so we must
 * define that too. In [Stroustrup97] there are 3 different variants for each
 * of the two operators, with different signatures.
 *
 * In addition, G. defines two variants: one that takes a reference
 * and one that takes a pointer to a rmmmemory.
 *
 * So here's 12 definitions, most of which are not implemented currently!
 * "Use with care"
 *
 * [WW] removing the throw declarations. This means they can throw anything.
 * See: http://www.codeproject.com/cpp/stdexceptionspec.asp
 */
void* operator new   (size_t, rmmmemory&) throw(std::bad_alloc);
void* operator new[] (size_t, rmmmemory&) throw(std::bad_alloc);
void* operator new[] (size_t, rmmmemory*) throw(std::bad_alloc);
void* operator new   (size_t, rmmmemory*) throw(std::bad_alloc);
void* operator new   (size_t, const std::nothrow_t&, rmmmemory&) throw();
void* operator new[] (size_t, const std::nothrow_t&, rmmmemory&) throw();
void* operator new[] (size_t, const std::nothrow_t&, rmmmemory*) throw();
void* operator new   (size_t, const std::nothrow_t&, rmmmemory*) throw();
void* operator new   (size_t, void*, rmmmemory&) throw();
void* operator new[] (size_t, void*, rmmmemory&) throw();
void* operator new[] (size_t, void*, rmmmemory*) throw();
void* operator new   (size_t, void*, rmmmemory*) throw();
void  operator delete   (void*, rmmmemory&) throw();
void  operator delete[] (void*, rmmmemory&) throw();
void  operator delete[] (void*, rmmmemory*) throw();
void  operator delete   (void*, rmmmemory*) throw();
void  operator delete   (void*, const std::nothrow_t&, rmmmemory&) throw();
void  operator delete[] (void*, const std::nothrow_t&, rmmmemory&) throw();
void  operator delete[] (void*, const std::nothrow_t&, rmmmemory*) throw();
void  operator delete   (void*, const std::nothrow_t&, rmmmemory*) throw();
void  operator delete   (void*, void*, rmmmemory&) throw();
void  operator delete[] (void*, void*, rmmmemory&) throw();
void  operator delete[] (void*, void*, rmmmemory*) throw();
void  operator delete   (void*, void*, rmmmemory*) throw();

// } // namespace globule

#ifdef RECORDSHMALLOC
#define allocate(ARG1)        allocate_debug(__FILE__,__LINE__,ARG1)
#define deallocate(ARG1)      deallocate_debug(__FILE__,__LINE__,ARG1)
#define reallocate(ARG1,ARG2) reallocate_debug(2,__FILE__,__LINE__,ARG1,ARG2)
#endif

#endif /* _ALLOCATOR_HPP */
