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
#include <apr_lib.h>
#include <apr_strings.h>
#include <apr_time.h>
#ifndef WIN32
extern "C" {
#include <sys/types.h>
#include <unistd.h>
};
#endif
#include "utilities.h"

#ifndef STANDALONE_APR

#undef MONITOR
#define MONITOR(name,desc) \
  Monitor mon_##name(#name,desc);
MONITOR(fatal,            0);
MONITOR(error,            0);
MONITOR(warning,          0);
MONITOR(notice,           0);
MONITOR(info,             0);
MONITOR(detail,           0);
MONITOR(policy,           "policy");
MONITOR(policynone,       "none");
MONITOR(policymirror,     "mirror");
MONITOR(policyproxy,      "proxy");
MONITOR(policyttl,        "ttl");
MONITOR(policyalex,       "alex");
MONITOR(policyinvalidate, "invalidate");
MONITOR(policyglobecb,    "globecb");
MONITOR(replmethod,       "method");
MONITOR(dofetch,          "fetch");
MONITOR(docache,          "cache");
MONITOR(donative,         "native");
MONITOR(doerror,          "error");
MONITOR(internal,         "internal");
MONITOR(servedby,         "servedby");
MONITOR(heartbeat,        "heartbeat");
MONITOR(retrieval,        "retrievals");
MONITOR(rsrcevict,        "evictions");
MONITOR(change,           "state change");
MONITOR(disabled,         "disabled");
MONITOR(enabled,          "enabled");
MONITOR(protocol,         "protocol mismatch");
MONITOR(response,         "bad response");
MONITOR(timeout,          "waited too long");
MONITOR(availability,     "availability check");

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

#endif

void
mkstring::append(const char* m, int len)
  throw()
{
  os.write(m, len);
}

std::string
mkstring::format(const char *fmt, va_list ap)
  throw()
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

std::string
mkstring::format(const char *fmt, ...)
  throw()
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

std::string
mkstring::urlencode(const char* s)
  throw()
{
  mkstring enc;
  for(;*s; s++)
    if(apr_isalnum(*s))
      enc << *s;
    else if(*s == ' ')
      enc << '+';
    else if(*s == '\n')
      enc << "\013\010";
    else
      enc << '%'
          << "0123456789abcdef"[*s/16]
          << "0123456789abcdef"[*s%16];
  return enc.str();
}

#ifndef HAVE_LRINT
long int
lrint(double x)
{
  return (int)(x+0.5);
}
#endif

#ifndef HAVE_LLRINT
long long
llrint(double x)
{
  return (long long)(x+0.5);
}
#endif

#ifndef STANDALONE_APR

static struct workerstate_struct standalonestate = { 0, "" };

struct workerstate_struct*
diag_message(const char *fmt, ...)
{
  apr_status_t status;
  struct workerstate_struct *state = 0;
    
  status = apr_threadkey_private_get((void**)&state, childstate.threadkey);
  if(!state)
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
  if(ctx && ctx->filter(mon)) {
    Event evt(mon);
    evt.message("%s",errmessage);
    evt.location(file, line);
    ctx->push(evt);
  }
}

void
diag_print(context_t ctx, const char* file, int line, Monitor* mon,
           int aplevel, const char* errmessage, int errcode)
{ 
  int len = strlen(errmessage);
  while(len>0 && errmessage[len-1] == '\n')
    --len;
  request_rec* r = (ctx ? ctx->request() : NULL);
  if(!r) {
    apr_allocator_t* allocer;
    apr_pool_t* pool;
    if(!apr_allocator_create(&allocer)) {
      if(!apr_pool_create_ex(&pool, NULL, NULL, allocer)) {
        ap_log_perror(file,line,aplevel,errcode,pool,"%.*s",len,errmessage);
        apr_pool_destroy(pool);
      }
      apr_allocator_destroy(allocer);
    }
  } else
    ap_log_rerror(file,line,aplevel,errcode,r,"%.*s",len,errmessage);
}

#endif

std::string
dumpmem(apr_byte_t *mem, apr_size_t memsize)
{
  const int siz = 8192;
  std::vector<char> v(siz);
  char *buf = &v[0];
  int pad, len = 0;
  for(unsigned int i=0; i<memsize; i+=16) {
    len += apr_snprintf(&buf[len],siz-len,"%04x:%04x",i/0x10000,i%0x10000);
    for(unsigned int j=0; j<16 && i+j<memsize; j+=4) {
      len += apr_snprintf(&buf[len],siz-len," ");
      for(unsigned int k=0; k<4 && i+j+k<memsize; k++)
        len += apr_snprintf(&buf[len],siz-len,"%02x",mem[i+j+k]);
    }
    if(i+16 > memsize) {
      pad = 16 - (memsize - i);
      pad = pad*2 + pad/4;
      len += apr_snprintf(&buf[len],siz-len,"%*.*s",pad,pad,"");
    }
    len += apr_snprintf(&buf[len],siz-len,"  ");
    for(unsigned int j=0; j<16 && i+j<memsize; j++)
      len += apr_snprintf(&buf[len],siz-len,"%c",(isgraph(mem[i+j]) ?
                                              mem[i+j] : '.'));
    len += apr_snprintf(&buf[len],siz-len,"\n");
  }
  return std::string(buf);
}
