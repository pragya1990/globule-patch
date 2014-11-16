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
#define __USE_GNU
#include <stdio.h>
#ifdef __GNUC__
#include <dlfcn.h>
#endif
#include <errno.h>
#include <string.h>
#include <cstdio>
#include <apr.h>
#include <httpd.h>
#include <http_log.h>
#include "utilities.h"
#include "netw/Url.hpp"
#include "netw/HttpRequest.hpp"
#include "Storage.hpp"
#ifndef STANDALONE_APR
#include "locking.hpp"
#include "resource/BaseHandler.hpp"
#endif

using std::exception;
using std::bad_exception;
using std::set_unexpected;
using std::set_terminate;
using std::string;

extern "C" {

#define functionstacksize 1024
static const char *functionstack[functionstacksize];
static int functionstackindex = 0;

#ifdef __GNUC__

extern void __wrap___cyg_profile_func_enter(void *this_fn, void *call_site)
  __attribute__((no_instrument_function));
extern void __wrap___cyg_profile_func_exit(void *this_fn, void *call_site)
  __attribute__((no_instrument_function));
extern void __cyg_profile_func_enter(void *this_fn, void *call_site)
  __attribute__((no_instrument_function));
extern void __cyg_profile_func_exit(void *this_fn, void *call_site)
  __attribute__((no_instrument_function));

void
__wrap___cyg_profile_func_enter(void *this_fn, void *call_site)
{
  Dl_info dli;    
  int index = functionstackindex++;
  if(index >= functionstacksize)
    index = functionstacksize - 1;
  functionstack[index] = NULL;
  if(dladdr(this_fn, &dli) && dli.dli_sname)
    functionstack[index] = dli.dli_sname;
}

void
__wrap___cyg_profile_func_exit(void *this_fn, void *call_site)
{
  if(functionstackindex > 0)
    if(--functionstackindex >= functionstacksize - 1)
      functionstack[functionstacksize - 1] = NULL;
}

#endif

};

ExceptionHandler* ExceptionHandler::_current = 0;

ExceptionHandler::ExceptionHandler(apr_pool_t* pool)
  throw()
  : _pool(pool)
{
  _oldUnexpected = set_unexpected(unexpectedHandler);
  _oldTerminate  = set_terminate(terminateHandler);
  _previous = _current;
  _current  = this;
}

ExceptionHandler::~ExceptionHandler()
  throw()
{
  set_unexpected(_oldUnexpected);
  set_terminate(_oldTerminate);
  _current = _previous;
}

inline const char*
ExceptionHandler::getExceptionType(string& message)
{
  try {
    throw;
  } catch(UrlException ex) {
    message = ex.getMessage();
    return "UrlException";
  } catch(globule::netw::HttpException ex) {    
    message = ex.getMessage();
    return "HttpException";
#ifndef STANDALONE_APR
  } catch(ResourceException ex) {
    message = (ex.message()?ex.message():"");
    return "ResourceException";
  } catch(ResourceError ex) {
    message = (ex.message()?ex.message():"");
    return "ResourceError";
  } catch(SetupException ex) {
    message   = ex.getMessage();
    return "SetupException";
#endif
  } catch(WrappedError ex) {
    message   = ex.getMessage();
    return "WrappedError";
  } catch(FileError ex) {
    message   = ex.getMessage();
    return "FileError";
  } catch(std::bad_alloc) {
    return "bad_alloc";
  } catch(bad_exception) {
    return "";
  } catch(...) {
    return 0;
  }
}

string
ExceptionHandler::location()
{
  mkstring s;
  int i = functionstackindex - 1;
  if(functionstackindex >= functionstacksize) {
    s << "\t" << functionstack[functionstacksize-1] << "\n"
      << "\t(" << (functionstacksize-functionstackindex+1)
      << " more stackframes)" << "\n";
    i = functionstackindex - 2;
  }
  for(; i>0; i--)
    s << "\t" << functionstack[i] << "\n";
  return s;
}

void
ExceptionHandler::unexpectedHandler()
  throw(bad_exception)
{
  string message("");
  const char* exception = 0;
  const char* currentfunction = (functionstackindex>0 ?
                                 functionstack[functionstackindex-1] : NULL);
  try {
    throw;
  } catch(UrlException ex) {
    message = ex.getMessage();
    exception = "UrlException";
  } catch(globule::netw::HttpException ex) {    
    message = ex.getMessage();
     exception = "HttpException";
#ifndef STANDALONE_APR
  } catch(ResourceException ex) {
    message = (ex.message()?ex.message():"");
     exception = "ResourceException";
  } catch(ResourceError ex) {
    message = (ex.message()?ex.message():"");
    exception ="ResourceError";
  } catch(SetupException ex) {
    message   = ex.getMessage();
    exception = "SetupException";
#endif
  } catch(WrappedError ex) {
    message   = ex.getMessage();
     exception = "WrappedError";
  } catch(FileError ex) {
    message   = ex.getMessage();
     exception = "FileError";
  } catch(std::bad_alloc) {
     exception = "bad_alloc";
  } catch(bad_exception) {
     exception = "";
  } catch(...) {
  }
  if(!exception)
    exception = "unknown";
  else if(!*exception)
    exception = "bad_exception";
  if(!_current->_pool) {
    fprintf(stderr, "%s%s%sUnexpected exception %s%s%s\n",
            (currentfunction?"In ":""),
            (currentfunction?currentfunction:""),
            (currentfunction?": ":""),
            exception,
            (message!=""?": ":""), message.c_str());
    fflush(stderr);
  } else
    ap_log_perror(APLOG_MARK, APLOG_CRIT, OK, _current->_pool,
                  "%s%s%sUnexpected exception %s%s%s",
                  (currentfunction?"In ":""),
                  (currentfunction?currentfunction:""),
                  (currentfunction?": ":""),
                  exception,
                  (message!=""?": ":""), message.c_str());
  throw bad_exception();
}

void
ExceptionHandler::terminateHandler()
{
  string message("");
  const char* exception = getExceptionType(message);
  const char* currentfunction = (functionstackindex>0 ?
                                 functionstack[functionstackindex-1] : NULL);
  if(!exception)
    exception = "unknown";
  if(!_current->_pool) {
    if(!*exception)
      fprintf(stderr,"Chaining unexpected exception to termination"
                     " (this indicates a programming error)\n");
    else
      fprintf(stderr,"%s%s%sUncaught exception %s%s%s",
                     (currentfunction?"In ":""),
                     (currentfunction?currentfunction:""),
                     (currentfunction?": ":""),
                     exception,
                     (message!=""?": ":""), message.c_str());
    fflush(stderr);
  } else
    if(!*exception)
      ap_log_perror(APLOG_MARK, APLOG_EMERG, OK, _current->_pool,
                    "Chaining unexpected exception to termination"
                    " (this indicates a programming error)");
    else
      ap_log_perror(APLOG_MARK, APLOG_EMERG, OK, _current->_pool,
                    "%s%s%sUncaught exception %s%s%s",
                    (currentfunction?"In ":""),
                    (currentfunction?currentfunction:""),
                    (currentfunction?": ":""),
                    exception,
                    (message!=""?": ":""), message.c_str());
  _current->_oldTerminate();
}
