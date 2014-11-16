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
#ifndef _UTILITIES_H
#define _UTILITIES_H

#ifdef __cplusplus
#include <cstdio>
#include <string>
#include <queue>
#include <sstream>
#include <exception>
#endif
#include <stdarg.h>
#include <apr.h>
#ifdef HAVE_MYAPR
#include "myapr_rmm.h"
#else
#include <apr_rmm.h>
#endif
#include <apr_strings.h>
#include <apr_thread_proc.h>
#include "monitoring.h"

#if !defined(__GNUC__) || __GNUC__ < 2 || \
    (__GNUC__ == 2 && __GNUC_MINOR__ < 7) ||\
    defined(NEXT)
#ifndef __attribute__
#define __attribute__(__x)
#endif
#endif

#ifdef __cplusplus

class mkstring
{
private:
  std::ostringstream os;
public:
  template <class T> mkstring &operator<<(const T &t) throw() {
    os << t;
    return *this;
  };
  inline mkstring& operator<<(const char* m) throw() {
    os << m;
    return *this;
  };
  inline mkstring& operator<<(const std::string& s) throw() {
    os << s;
    return *this;
  };
  inline mkstring& operator<<(const mkstring& s) throw() {
    os << s.os;
    return *this;
  };
  void append(const char* m, int len) throw();
  static std::string format(const char *fmt, va_list ap) throw();
  static std::string format(const char *fmt, ...) throw()
     __attribute__ ((__format__ (__printf__, 1, 2)));
  static std::string urlencode(const char* s) throw();
  operator std::string() const throw() { return os.str(); };
  const std::string str() const throw() { return os.str(); };
};

#include <exception>
class ExceptionHandler
{
private:
  static ExceptionHandler* _current;
  ExceptionHandler* _previous;
  apr_pool_t* _pool;
  std::unexpected_handler _oldUnexpected;
  std::terminate_handler _oldTerminate;
private:
  static const char* getExceptionType(std::string&);
  static void unexpectedHandler() throw(std::bad_exception);
  static void terminateHandler();
public:
  ExceptionHandler(apr_pool_t* pool = 0) throw();
  ~ExceptionHandler() throw();
  static std::string location();
};

#endif

#ifndef HAVE_LRINT
extern long int lrint(double x);
#endif

#ifndef HAVE_LLRINT
extern long long llrint(double x);
#endif

#ifndef BUG
#ifdef DEBUG
#define BUG(ARG) ARG
#else
#define BUG(ARG)
#endif
#endif

#ifndef CHECK
#define CHECK(EX) do { if(EX) { int err = errno; fprintf(stderr, "operation" \
 " \"%s\" failed on line %d: %s (%d)\n", #EX, __LINE__, strerror(err), err); \
 abort(); }} while(0)
#endif

#ifdef WIN32
void win_open_stderr(apr_pool_t *p);
#endif /* WIN32 */

/* The structure sharedstate is shared amongst all child processes and
 * threads by storing it in shared memory.  It is initialized in the
 * post-config hook.
 */
#ifdef __cplusplus
extern "C" {
#endif 

extern struct sharedstate_struct {
#ifndef STANDALONE_APR
  ShmSharedStorageSpace monctxref;
#else
  //FIXME requires replacement for ShmStorage
#endif
} *sharedstate;

#ifdef __cplusplus
};
#endif

/* The structure childstate is a per-child structure, shared amonst all
 * threads, but every child has a local copy, because it is stored in
 * normal main memory.  It is initialized in the child-init hook and
 * in the post-config.
 */
#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
#ifndef MONITOR
#define MONITOR(name) extern Monitor mon_##name
#endif
#else
#define MONITOR(name) extern Monitor mon_##name
#endif
MONITOR(fatal);
MONITOR(error);
MONITOR(warning);
MONITOR(notice);
MONITOR(info);
MONITOR(detail);
MONITOR(policy);
MONITOR(policynone);
MONITOR(policymirror);
MONITOR(policyproxy);
MONITOR(policyttl);
MONITOR(policyalex);
MONITOR(policyinvalidate);
MONITOR(policyglobecb);
MONITOR(replmethod);
MONITOR(dofetch);
MONITOR(docache);
MONITOR(donative);
MONITOR(doerror);
MONITOR(internal);
MONITOR(servedby);
MONITOR(heartbeat);
MONITOR(retrieval);
MONITOR(rsrcevict);
MONITOR(change);
MONITOR(disabled);
MONITOR(enabled);
MONITOR(protocol);
MONITOR(response);
MONITOR(timeout);
MONITOR(availability);
#undef MONITOR
#define MONITOR(name) &mon_##name

extern struct childstate_struct {
  apr_threadkey_t* threadkey;
  apr_global_mutex_t* lock;
  char *lock_fname;
} childstate;
#ifdef __cplusplus
};
#endif

#ifdef __cplusplus
typedef Context* context_t;
#else
typedef struct Context *context_t;
#endif

/* The structure workerstate is a per-thread structure, individual to
 * every worker thread or other kind of thread which needs access to
 * the Context (including debugging facilities).
 */
class GlobuleEvent;
struct workerstate_struct {
  context_t ctx;
  char auxstring[1024];
  std::queue<GlobuleEvent*> queuedEvents; // FIXME put into context
};

#ifdef __cplusplus
extern "C" {
#endif
extern struct workerstate_struct* diag_message(const char *fmt, ...);
extern void diag_log(context_t ctx, const char* file, int line, Monitor* mon, int format, const char *message);
extern void diag_print(context_t ctx, const char* file, int line, Monitor* mon, int aplevel, const char *message, int errcode);

#ifdef __cplusplus
static Monitor* diag_first(Monitor* m,...);
inline Monitor* diag_first(Monitor* m,...) { return m; }
#endif /* __cplusplus */

#ifdef __cplusplus
};
#endif

#ifdef __cplusplus
extern std::string dumpmem(apr_byte_t *mem, apr_size_t memsize);
#endif

/* DO NOT split up these macros up into multiple lines, as the line number
 * will then be wrong in compiler messages.
 */
#ifndef STANDALONE_APR
#define DIAG(CTX,MON,MSG) do { if(!CTX) { struct workerstate_struct* diag_state = diag_message(NULL); if(!diag_state->ctx) { diag_state = diag_message MSG; diag_log(diag_state->ctx,__FILE__,__LINE__,diag_first MON,-1,diag_state->auxstring); } else if(diag_state->ctx->filter MON) { Event diag_evt MON; diag_evt.message MSG; diag_evt.location(__FILE__,__LINE__); diag_state->ctx->push(diag_evt); }} else if(((context_t)CTX)->filter MON) { Event diag_evt MON; diag_evt.message MSG; diag_evt.location(__FILE__,__LINE__); ((context_t)CTX)->push(diag_evt); }} while(0)
#else
#define DIAG(CTX,MON,MSG) do { struct workerstate_struct* diag_state = diag_message MSG; diag_log(diag_state->ctx,__FILE__,__LINE__,diag_first MON,-1,diag_state->auxstring); } while(0)
#endif

#endif /* _UTILITIES_H */
