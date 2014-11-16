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
#ifndef _DATABASEHANDLER_HPP
#define _DATABASEHANDLER_HPP
                                                                               
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <string>
#include <set>
#include <vector>
#include <map>
#include <apr.h>
#include "resource/BaseHandler.hpp"
#include "alloc/Allocator.hpp"
#include "resource/BaseHandler.hpp"
#include "netw/Url.hpp"
#include "documents.hpp"

class DatabaseHandler : public BaseHandler
{
private:
  gUrl _upstream;
protected:
  virtual Element* newtarget(apr_pool_t*, const Url&, Context*,
                             const char* path, const char* policy) throw();
public:
  DatabaseHandler(Handler*, apr_pool_t*, Context*, const Url& uri,
		  const char* path, const char* upstream, const char *secret) throw();
  virtual ~DatabaseHandler() throw();
  virtual bool initialize(apr_pool_t*, Context*, HandlerConfiguration*) throw(SetupException);
  virtual bool handle(GlobuleEvent& evt, const std::string& remainder) throw();
  virtual std::string description() const throw();
  virtual bool ismaster() const throw();
  virtual bool isslave() const throw();
  virtual Fetch* fetch(apr_pool_t*, const char* path, const Url& replica, apr_time_t ims, const char* arguments = 0, const char* method = 0) throw();
};

#endif /* _DATABASEHANDLER_HPP */
