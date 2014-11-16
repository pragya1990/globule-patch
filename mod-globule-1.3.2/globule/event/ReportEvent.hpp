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
#ifndef _REPORTEVENT_HPP
#define _REPORTEVENT_HPP

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <map>
#include <string>
#include <httpd.h>
#include <iterator>
#include "utilities.h"
#include "GlobuleEvent.hpp"
#include "Constants.hpp"

class ReportEvent;

class ReportEvent : public GlobuleEvent
{
private:
  ReportEvent* _parent;
  std::string _prefix;
protected:
  std::map<std::string,std::string> _properties;
public:
  ReportEvent(apr_pool_t* p, Context* ctx,
              std::map<std::string,std::string> formdata)
    throw()
    : GlobuleEvent(p, ctx, REPORT_EVT, matchall(p)), _parent(0), _prefix(""),
      _properties(formdata)
  {
  };
  ReportEvent(apr_pool_t* p, Context* ctx,
              std::map<std::string,std::string> formdata,
              const apr_uri_t* u)
    throw()
    : GlobuleEvent(p, ctx, REPORT_EVT, u), _parent(0), _prefix(""),
      _properties(formdata)
  {
  };
  ReportEvent(const char* category, ReportEvent* ref, int* index = 0) throw();
  virtual ~ReportEvent() throw();

  class iterator;
  ReportEvent::iterator begin(const char* category) throw();
  ReportEvent::iterator end(const char* category) throw();

  virtual bool asynchronous() throw() { return false; };
  virtual GlobuleEvent* instantiateEvent() throw() { return 0; };
  std::map<std::string,std::string> properties() throw() {
    return _properties;
  };
  inline void setProperty(const char* key, const char* val) throw() {
    _properties[key] = val;
  }
  inline void setProperty(std::string key, const char* val) throw() {
    _properties[key] = val;
  }
  inline void setProperty(std::string key, std::string val) throw() {
    _properties[key] = val;
  }
#ifndef STANDALONE_APR
  inline void setProperty(std::string key, gstring val) throw() {
    _properties[key] = val.c_str();
  }
#endif
  const char* getProperty(const char* key) const throw();
  bool getPropertyInt(const char* key, int& val) const throw();
  bool getPropertyInt(const char* key, apr_uint16_t& val) const throw();
  const char* getProperty(const mkstring& key) const throw() {
    std::string k(key);
    return getProperty(k.c_str());
  }
};

class ReportEvent::iterator
{
  friend class ReportEvent;
private:
  std::string _category;
  int _index, _count;
  ReportEvent _curr;
protected:
  iterator(ReportEvent* parent, const char* category, int index = 0)
    throw()
    : _category(category), _index(index), _count(_index),
      _curr(category, parent, &_count)
  {
    if(_count == -1) _index = _count;
  };
public:
  friend bool operator==(const iterator& i, const iterator& j) throw() {
    return i._index == j._index && i._index == j._index;
  }
  inline bool operator!=(const iterator& rhs) const throw() {
    return _index != rhs._index;
  };
  inline ReportEvent& operator*() throw() {
    return _curr;
  };
  inline ReportEvent* operator->() throw() {
    return &_curr;
  };
  inline iterator& operator++() throw() {
    if(++_index < _count) {
      _count = _index;
      _curr = ReportEvent(_category.c_str(), _curr._parent, &_count);
    }
    return *this;
  };
  inline iterator operator++(int) throw() {
    iterator tmp = *this;
    ++*this;
    return tmp;
  };
};

#endif /* _REPORTEVENT_HPP */
