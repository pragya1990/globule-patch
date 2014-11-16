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
#ifndef _DOCUMENTS_HPP
#define _DOCUMENTS_HPP
#include "resource/Handler.hpp"
#include "policy/ReplPolicy.hpp"
#include "event/SwitchEvent.hpp"
#include "resources.hpp"

class Fetch;
class ContainerHandler;

class Element : public Handler, public ResourcesUser, public Serializable
{
  friend class ReplPolicy;
protected:
  Lock              _lock;
  ContainerHandler* _parent; // FIXME overload from Handler::_parent
  gstring           _path;
  ReplPolicy*       _policy;
  Monitor           _monitor;
  ResourcesRecord*  _rsrcs;
protected:
  virtual Input& operator>>(Input&) throw(WrappedError);
  virtual Output& operator<<(Output&) const throw();
  void invalidate(Context*, apr_pool_t* ptemp, ReplSet *repl) throw();
  void switchpolicy(Context*, apr_pool_t*, const char* policy, bool sticky=false) throw();
  virtual void execute(Context* ctx, apr_pool_t*, ReplAction) throw() = 0;
private:
  Element() throw();
public:
  Element(ContainerHandler* parent, apr_pool_t*, const Url& url, Context* ctz) throw();
  Element(ContainerHandler* parent, apr_pool_t*, const Url& url, Context* ctx,
          const char* path, const char* policy) throw();
  virtual ~Element() throw();
  virtual void flush(apr_pool_t*) throw();
  void purge(Context*, apr_pool_t*) throw();
  virtual EvictionEvent evict(Context*, apr_pool_t*);
  virtual Lock* getlock() throw();
  virtual void log(apr_pool_t*, std::string, const std::string) throw();
  virtual bool handle(GlobuleEvent& evt, const std::string& remainder) throw();
  virtual bool handle(GlobuleEvent& evt) throw() = 0;
};

class Document : public Element
{
private:
  gstring      _master_fname; // at master, the name of the file of this doc.
  gstring      _content_type; // at slave, as returned by master
  apr_time_t   _lastmod;      // at slave, as returned by master
  apr_size_t   _docsize;      // at slave, as on disk
  apr_int16_t           _http_status;
  gmap<const gstring,gstring> _http_headers;
private:
  int fetch(Context*, apr_pool_t*, const Url&, request_rec*, ReplAction&,
            ResourceDeclaration&) throw();
protected:
  virtual Input& operator>>(Input&) throw();
  virtual Output& operator<<(Output&) const throw();
  virtual void execute(Context* ctx, apr_pool_t*, ReplAction) throw();
private:
  Document() throw();
public:
  Document(const Document&) throw();
  Document(ContainerHandler* parent, apr_pool_t*, const Url& url, Context* ctx) throw();
  Document(ContainerHandler* parent, apr_pool_t*, const Url& url, Context* ctx,
           const char* path, const char* policy) throw();
  virtual ~Document() throw();
  virtual bool handle(GlobuleEvent& evt) throw();
  virtual std::string description() const throw();
};

#endif
