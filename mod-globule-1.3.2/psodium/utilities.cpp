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
#include <stdio.h>
#include <vector>
#include <iostream>
#include <string>
#include <httpd.h>
#include <http_log.h>
#include <apr.h>
#include <apr_strings.h>
#include <apr_time.h>
#ifndef WIN32
#include <sys/types.h>
#include <unistd.h>
#endif
#include "utilities.h"

#undef MONITOR
#define MONITOR(name,num,desc) \
  static Monitor monitor_##name(num,desc?desc:#name);
MONITOR(debug,             0,"");
MONITOR(debug0,            1,"");
MONITOR(debug1,            2,"");
MONITOR(debug2,            3,"");
MONITOR(debug3,            4,"");
MONITOR(debug4,            5,"");
MONITOR(debug5,            6,"");
MONITOR(policy,            7,"policy");
MONITOR(policynone,        8,"none");
MONITOR(policymirror,      9,"mirror");
MONITOR(policyproxy,      10,"proxy");
MONITOR(policyttl,        11,"ttl");
MONITOR(policyalex,       12,"alex");
MONITOR(policyinvalidate, 13,"invalidate");
MONITOR(replmethod,       14,"method");
MONITOR(dofetch,          15,"fetch");
MONITOR(docache,          16,"cache");
MONITOR(donative,         17,"native");
MONITOR(doerror,          18,"error");
MONITOR(internal,         19,"internal");
MONITOR(error,            20,"");
MONITOR(warning,          21,"");
MONITOR(notice,           22,"");
MONITOR(info,             23,"");
MONITOR(servedby,         24,"servedby");

struct monitors_struct monitors = {
  &monitor_debug,
  &monitor_debug0,
  &monitor_debug1,
  &monitor_debug2,
  &monitor_debug3,
  &monitor_debug4,
  &monitor_debug5,
  &monitor_policy,
  &monitor_policynone,
  &monitor_policymirror,
  &monitor_policyproxy,
  &monitor_policyttl,
  &monitor_policyalex,
  &monitor_policyinvalidate,
  &monitor_replmethod,
  &monitor_dofetch,
  &monitor_docache,
  &monitor_donative,
  &monitor_doerror,
  &monitor_internal,
  &monitor_error,
  &monitor_warning,
  &monitor_notice,
  &monitor_info,
  &monitor_servedby
};

Monitor::Monitor(int num, const char *description)
{
  strcpy(_description, description);
}

#ifdef WIN32
static apr_file_t* win_stderr = NULL;
#endif

#ifdef WIN32
void win_open_stderr(apr_pool_t *p) {
  if (win_stderr == NULL) {
    int status = apr_file_open_stderr(&win_stderr, p);
    if (!APR_STATUS_IS_SUCCESS(status)) {
      // Something for the debugger then..
    }
  }
}
#endif /* WIN32 */

std::string mkstring::format(const char *fmt, va_list ap)
{
  va_list ap2;
#ifdef WIN32
  ap2 = ap;
#else
  va_copy(ap2, ap);
#endif /* WIN32 */
  // vsnprintf is a common but non-standard extension.
  // I am assuming C99 behaviour when the first two args
  // are zero: not all implementations have this behaviour.
#ifdef MANDRAKE10_PATCH
  size_t sz = vsnprintf(NULL, 0, fmt, ap);
#else
  size_t sz = apr_vsnprintf(NULL, 0, fmt, ap);
#endif
  // If your string implementation guarantees contiguity
  // of storage, you can use that.  AFAIK, the standard
  // doesn't guarantee this for strings but does for vectors
  std::vector<char> v(sz+1, '\0');
#ifdef MANDRAKE10_PATCH
  vsnprintf(&v[0], sz+1, fmt, ap2);
#else
  apr_vsnprintf(&v[0], sz+1, fmt, ap2);
#endif
  return std::string(&v[0], sz);
}

std::string mkstring::format(const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  // vsnprintf is a common but non-standard extension.
  // I am assuming C99 behaviour when the first two args
  // are zero: not all implementations have this behaviour.
#ifdef MANDRAKE10_PATCH
  size_t sz = vsnprintf( NULL, 0, fmt, ap );
#else
  size_t sz = apr_vsnprintf( NULL, 0, fmt, ap );
#endif
#ifdef MANDRAKE10_PATCH
  va_end(ap);
  va_start(ap, fmt);
#endif
  // If your string implementation guarantees contiguity
  // of storage, you can use that.  AFAIK, the standard
  // doesn't guarantee this for strings but does for vectors
  std::vector<char> v(sz+1, '\0');
#ifdef MANDRAKE10_PATCH
  vsnprintf(&v[0], sz+1, fmt, ap);
#else
  apr_vsnprintf(&v[0], sz+1, fmt, ap);
#endif
  va_end(ap);
  return std::string(&v[0], sz);
}

#ifndef HAVE_LRINT
long int
lrint(double x)
{
  return (int)(x+0.5);
}
#endif

static struct workerstate_struct standalonestate = { 0, "" };

struct workerstate_struct*
diag_message(const char *fmt, ...)
{
  apr_status_t status;
  struct workerstate_struct *state = 0;
    
  state = &standalonestate;

  if(fmt) {
    va_list ap;
    va_start(ap, fmt);
    apr_vsnprintf(state->auxstring, sizeof(state->auxstring)-1, fmt, ap);
    va_end(ap);
  }
  return state;
}

void
diag_log(context_t ctx, const char* file, int line, Monitor* mon,
         int style, const char* errmessage)
{
  diag_print(ctx, file, line, mon, style, 0, errmessage);
}

void
diag_print(context_t ctx, const char* file, int line, Monitor* mon,
           int style, int aplevel, const char* errmessage)
{ 
  int len = strlen(errmessage);
  while(len>0 && errmessage[len-1] == '\n')
    --len;
  switch(style) {
  case diag_DPRINTF: {
#ifdef WIN32
    int id = GetCurrentThreadId();
    apr_file_printf(win_stderr,
#else
    int id = getpid();
    fprintf(stderr,
#endif
            "\t\t\t\t(%s:%d_T%" APR_UINT64_T_FMT "_P%u)\n%.*s\n",
            file, line, apr_time_now()%1000000000, id, len, errmessage);
#ifdef WIN32
    apr_file_flush(win_stderr);
#else
    fflush(stderr);
#endif
    break;
  }
  case diag_GDEBUG: {
#ifdef WIN32
    int id = GetCurrentThreadId();
    std::string s = mkstring()
#else
    int id = getpid();
    std::cerr
#endif
      << "\t\t\t\t(" << file << ':' << line << "_T"
      << apr_time_now()%1000000000 << "_P" << id << ")\n"
      << errmessage
#ifndef WIN32
      << std::endl
#endif
      ;
#ifdef WIN32
    apr_file_printf(win_stderr,"%s\n",s.c_str());
    apr_file_flush(win_stderr);
#else
    std::cerr.flush();
#endif
    break;
  }
  case diag_GDEBUG2: {
#ifdef WIN32
    std::string s = mkstring()
#else
    std::cerr
#endif
      << '[' << __FILE__ << ':' << __LINE__ << "[ "
      << errmessage
#ifndef WIN32
      << std::endl
#endif
      ;
#ifdef WIN32
    apr_file_printf(win_stderr,"%s\n",s.c_str());
    apr_file_flush(win_stderr);
#else
    std::cerr.flush();
#endif
    break;
  }
  default: {
    abort();
  }
  }
}
