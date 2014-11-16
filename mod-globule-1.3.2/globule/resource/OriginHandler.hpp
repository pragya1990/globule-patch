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
#ifndef _ORIGINHANDLER_HPP
#define _ORIGINHANDLER_HPP

#include <map>
#include <string>
#include "resource/SourceHandler.hpp"

class OriginHandler : public SourceHandler
{
private:
  int          _reevaluateadaptskipcnt;
  int          _reevaluateadaptskip;
  int          _reevaluatecontentskipcnt;
  int          _reevaluatecontentskip;
  apr_size_t   _lastevaluationsize;
  bool         _config_dirty;
  apr_time_t   _filetree_timestamp;
  unsigned int _filetree_numofdocs;
protected:
  void adapt(const GlobuleEvent& evt,
             const char *fname, size_t beginoffset, size_t endoffset) throw();
  void evallog(GlobuleEvent& evt) throw();
  void checkcontent(apr_pool_t* pool, Context*) throw();
  void checkconfig(apr_pool_t* pool, Context*) throw();
  apr_time_t traversedocs(std::map<std::string,std::string>& filelist,
                          const char* location, const char* path,
                          apr_pool_t* ptemp) throw();
  void switchpolicy(apr_pool_t*, Context* ctx,
                    const char* path, const char* repltype) throw();
public:
  OriginHandler(Handler*, apr_pool_t* p, Context* ctx, const Url& uri, 
                const char* path, const char *upstream, const char *password)
    throw(SetupException);
  virtual ~OriginHandler() throw();
  virtual bool initialize(apr_pool_t*, Context*, HandlerConfiguration*) throw(SetupException);
  virtual bool handle(GlobuleEvent& evt, const std::string& remainder) throw();
  virtual std::string description() const throw();
  inline virtual bool ismaster() const throw() { return true; };
  inline virtual bool isslave() const throw() { return false; };
};

#endif  /* _ORIGINHANDLER_HPP */
