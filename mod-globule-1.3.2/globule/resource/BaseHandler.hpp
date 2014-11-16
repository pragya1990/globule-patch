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
#ifndef _BASEHANDLER_HPP
#define _BASEHANDLER_HPP

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <string>
#include <iterator>
#include <apr.h>
#include "utilities.h"
#include "event/GlobuleEvent.hpp"
#include "event/HttpMetaEvent.hpp"
#include "locking.hpp"
#include "resource/Handler.hpp"
#include "netw/HttpRequest.hpp"
#include "Storage.hpp"
#include "resources.hpp"

class Element;
class Document;
class ConfigHandler;

#include "Peer.hpp"

extern "C" {
#include <apr_buckets.h>
extern apr_status_t
globule_rgetline(char **s, apr_size_t n,
                 apr_size_t *read, request_rec *r,
                 int fold, apr_bucket_brigade *bb);
}

extern "C" {
#include <apr_buckets.h>
}

class Fetch : public globule::netw::HttpResponseCodes
{
public:
  virtual const std::string getMessage() throw() = 0;
  virtual int getStatus() throw() = 0;
  virtual const std::string getHeader(const std::string &key) throw() = 0;
  virtual void getHeaders(apr_table_t* table) throw() = 0;
  virtual apr_ssize_t getContentLength() throw() = 0; // <0 for not indicated
  const apr_time_t getDateHeader(const std::string &key) throw() {
    return apr_date_parse_http(getHeader(key).c_str());
  };
  virtual apr_ssize_t read(char** buf) throw(globule::netw::HttpException) = 0;
  virtual std::string encode(const char* buf, int nbytes = -1) throw() = 0;
};

class ExceptionFetch
  : public Fetch
{
private:
  apr_status_t _status;
  std::string _errmessage;
public:
  ExceptionFetch(apr_status_t status, const char* message) throw()
    : _status(status), _errmessage(message?message:"")
  {
  };
  ExceptionFetch(apr_status_t status, const std::string message) throw()
    : _status(status), _errmessage(message)
  {
  };
  virtual ~ExceptionFetch() throw() {
  };
  virtual const std::string getMessage() throw() {
    return _errmessage;
  };
  virtual int getStatus() throw() {
    return _status;
  };
  virtual apr_ssize_t getContentLength() throw() {
    return -1;
  };
  virtual const std::string getHeader(const std::string &key) throw() {
    return "";
  };
  virtual void getHeaders(apr_table_t* table) throw() {
  };
  virtual apr_ssize_t read(char** buf) throw() {
    return 0;
  };
  virtual std::string encode(const char* buf, int nbytes = -1) throw() {
    return "";
  };
};

class HttpFetch
  : public Fetch
{
private:
  globule::netw::HttpRequest* _req;
  globule::netw::HttpResponse* _res;
public:
  HttpFetch(apr_pool_t* pool, globule::netw::HttpRequest* req) throw() : _req(req) { _res = req->response(pool); };
  virtual const std::string getMessage() throw() { return _res->getMessage(); };
  virtual int getStatus() throw() { return (_res->getStatus()!=HTTPRES_OK ? _res->getStatus() : 0); };
  virtual apr_ssize_t getContentLength() throw() { return _res->getContentLength(); };
  virtual void getHeaders(apr_table_t* table) throw() { _res->getHeaders(table); };
  virtual const std::string getHeader(const std::string &key) throw() { return _res->getHeader(key); };
  virtual apr_ssize_t read(char** buf) throw(globule::netw::HttpException) { return _res->read(buf); };
  virtual std::string encode(const char* buf, int nbytes = -1) throw() { return buf; };
};

class SetupException
{
private:
  apr_status_t _status;
  std::string _message;
public:
  inline SetupException(std::string message) throw()
    : _status(APR_SUCCESS), _message(message) { };
  inline SetupException(apr_status_t status, std::string message) throw()
    : _status(status), _message(message) { };
  std::string getMessage() throw() { return _message; };
};

class ContainerHandler : public Handler, public Serializable
{
  friend class ConfigHandler;
private:
  gmap<const gstring,Element*> _documents;
protected:
  Lock                   _lock;
  const char*            _path;
  Monitor                _monitor;
private:
  ContainerHandler(const ContainerHandler&) throw();
  ContainerHandler& operator=(const ContainerHandler&) throw();
protected:
  virtual Element* newtarget(apr_pool_t*, const Url&, Context*,
                             const char* path, const char* policy) throw() = 0;
  inline Handler* gettarget(apr_pool_t* p, Context* ctx,
                            const std::string remainder,
                            const Handler* target = 0) throw() {
    return gettarget(p, ctx, remainder, 0, target);
  };
  Handler* gettarget(apr_pool_t*, Context* ctx, const std::string,
                     const char* policy, const Handler* = 0) throw();
  void delalltargets(Context*, apr_pool_t*) throw();
  void alltargets(GlobuleEvent& evt) throw();
public:
  std::string docfilename(const char* path) throw();
  std::string docdirectory(const char* path) throw();
protected:
  void deltarget(Context*, apr_pool_t*, const std::string) throw();
public:
  ContainerHandler(Handler* parent, apr_pool_t*, Context*,
                   const Url& uri) throw (SetupException);
  ContainerHandler(Handler* parent, apr_pool_t*, Context*,
                   const Url& uri, const char* path) throw (SetupException);
  bool initialize(apr_pool_t*, Context*, unsigned int nlocks) throw(SetupException);
  ~ContainerHandler() throw();
  virtual Input& operator>>(Input&) throw(FileError,WrappedError);
  virtual Output& operator<<(Output&) const throw(FileError);
  virtual ResourceAccounting* accounting() throw();
  virtual void flush(apr_pool_t*) throw();
  virtual Lock* getlock() throw();
  virtual std::string description() const throw() = 0;
  /* The method chownToCurrentUser should be private, but pSodium
   * doesn't use the same Handler structure.
   */
  static apr_status_t chownToCurrentUser(apr_pool_t* pool, const char* fname) throw();
  virtual bool ismaster() const throw() = 0;
  virtual bool isslave() const throw() = 0;
  virtual Fetch* fetch(apr_pool_t*, const char* path, const Url& replica, apr_time_t ims, const char* arguments = 0, const char* method = 0) throw() = 0;
};

class BaseHandler : public ContainerHandler
{
  friend class ConfigHandler;
private:
  static const int FILEVERSION = 6;
public:
  static const int PROTVERSION = 2;
private:
  static std::map<BaseHandler*,apr_file_t*> _logfp;
  bool                   _initialized;
  ResourceAccounting*    _accounting;
protected:
  gUrl                   _base_uri;
  const char*            _servername;
  Peers                  _peers;
private:
  BaseHandler(const BaseHandler&) throw();
  BaseHandler& operator=(const BaseHandler&) throw();
protected:
  apr_file_t *logfile(apr_pool_t*,bool drop=false) throw();
  apr_size_t logsize(apr_pool_t*) throw();
  std::string logname() throw();
  virtual Peer* addReplicaFast(Context* ctx, apr_pool_t* p, const char* location, const char* secret) throw();
  virtual void changeReplica(Peer* p) throw();
  virtual Element* newtarget(apr_pool_t*, const Url&, Context*,
                             const char* path, const char* policy) throw();
  virtual bool handle(GlobuleEvent&, const std::string&) throw();
  bool fetchcfg(apr_pool_t*, Context*,
		const Url& upstream, const char* secret,
		const char* metadata) throw();
  void shiplog(Context* ctx, apr_pool_t* p, apr_size_t& offset) throw();
  void reload(apr_pool_t* ptemp, Context*,
	      const Url& upstream, const char* secret,
	      std::string& redirectionPolicy) throw();
public:
  BaseHandler(Handler* parent, apr_pool_t*, Context*,
              const Url& uri) throw (SetupException);
  BaseHandler(Handler* parent, apr_pool_t*, Context*,
              const Url& uri, const char* path) throw (SetupException);
  virtual void initialize(apr_pool_t*, Context*, const std::vector<Contact>*) throw(SetupException);
  virtual void initialize(apr_pool_t*, int naliases, const char** aliases) throw(SetupException);
  virtual bool initialize(apr_pool_t*, Context*, HandlerConfiguration*) throw(SetupException);
  virtual ResourceAccounting* accounting() throw();
  virtual void accounting(ResourceAccounting*) throw();
  ~BaseHandler() throw();
  virtual Input& operator>>(Input&) throw(FileError,WrappedError);
  virtual Output& operator<<(Output&) const throw(FileError);
  virtual void flush(apr_pool_t*) throw();
  void restore(apr_pool_t*, Context*) throw();
  void setserial(apr_time_t) throw();
  virtual void log(apr_pool_t*, std::string, const std::string) throw();
  const char* servername() throw();
  const Url& siteaddress() const throw();
  const char* siteaddress(apr_pool_t*) const throw();
  virtual std::string description() const throw() = 0;
  virtual Fetch* fetch(apr_pool_t*, const char* path, const Url& replica, apr_time_t ims, const char* arguments = 0, const char* method = 0) throw();
};

#endif /* _BASEHANDLER_HPP */
