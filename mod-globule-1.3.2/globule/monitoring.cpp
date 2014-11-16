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
#include <cstdio>
#include <vector>
#include <set>
#include <map>
#include <string>
#include <iostream>
#include <httpd.h>
#include <apr.h>
#include <apr_global_mutex.h>
#include <apr_shm.h>
#include <apr_lib.h>
#include <http_protocol.h>
#include <http_log.h>
#include "utilities.h"
#include "monitoring.h"
#include "alloc/Allocator.hpp"
#include "locking.hpp"

using std::vector;
using std::set;
using std::map;
using std::string;

/****************************************************************************/

Monitor Monitor::root("");
static Monitor* registeredMonitors = 0;

Monitor::Monitor(const Monitor& org)
  throw()
  : _id(org._id)
{
  ap_assert(_id >= 0);
}

Monitor::Monitor(const char* name, const char* description)
  throw()
  : _nextRegistered(registeredMonitors), _id(-1)
{
  _name = strdup(name);
  _description = description;
  registeredMonitors = this;
}

Monitor::Monitor(Context* ctx, const char* description)
  throw()
  : _nextRegistered(registeredMonitors), _id(0)
{
  _name = 0;
  _description = description; // FIXME make this allocate in shared mem
  while(_id < 0) {
    Event ev(this);
    ctx->push(ev);
  }
  ap_assert(_id >= 0);
}

Monitor::Monitor(Context* ctx, const char* name, const char* description)
  throw()
  : _nextRegistered(registeredMonitors), _id(0)
{
  _name = strdup(name);
  _description = description; // FIXME make this allocate in shared mem
  Event ev(this);
  ctx->push(ev);
  ap_assert(_id >= 0);
  ctx->_monitors[_id] = this;
}

/****************************************************************************/

Event::Event(const Event& ev)
  throw()
  : Persistent("Event"), _timestamp(ev._timestamp), _value(ev._value),
    _tokens(ev._tokens), _message(ev._message),
    _file(ev._file), _line(ev._line)
{
}

Event::Event()
  throw()
  : Persistent("Event"), _timestamp(0),
    _value(1), _message(""), _file(0), _line(0)
{
}

Event::Event(apr_int64_t v)
  throw()
  : Persistent("Event"), _timestamp(apr_time_now()),
    _value(v), _message(""), _file(0), _line(0)
{
}

Event::Event(apr_int64_t t, apr_int64_t v)
  throw()
  : Persistent("Event"), _timestamp(t),
    _value(v), _message(""), _file(0), _line(0)
{
}

Event::Event(Monitor* m1)
  throw()
  : Persistent("Event"), _timestamp(apr_time_now()),
    _value(1), _message(""), _file(0), _line(0)
{
  ap_assert(m1);
  _tokens.push_back(m1);
}

Event::Event(Monitor* m1, Monitor* m2)
  throw()
  : Persistent("Event"), _timestamp(apr_time_now()),
    _value(1), _message(""), _file(0), _line(0)
{
  ap_assert(m1);
  ap_assert(m2);
  _tokens.push_back(m1);
  _tokens.push_back(m2);
}

Event::Event(Monitor* m1, Monitor* m2, Monitor* m3)
  throw()
  : Persistent("Event"), _timestamp(apr_time_now()),
    _value(1), _message(""), _file(0), _line(0)
{
  ap_assert(m1);
  ap_assert(m2);
  ap_assert(m3);
  _tokens.push_back(m1);
  _tokens.push_back(m2);
  _tokens.push_back(m3);
}

Event::Event(Monitor* m1, Monitor* m2, Monitor* m3, Monitor* m4)
  throw()
  : Persistent("Event"), _timestamp(apr_time_now()),
    _value(1), _message(""), _file(0), _line(0)
{
  ap_assert(m1);
  ap_assert(m2);
  ap_assert(m3);
  ap_assert(m4);
  _tokens.push_back(m1);
  _tokens.push_back(m2);
  _tokens.push_back(m3);
  _tokens.push_back(m4);
}

Event::Event(Monitor* m1, Monitor* m2, Monitor* m3, Monitor* m4, Monitor* m5)
  throw()
  : Persistent("Event"), _timestamp(apr_time_now()),
    _value(1), _message(""), _file(0), _line(0)
{
  ap_assert(m1);
  ap_assert(m2);
  ap_assert(m3);
  ap_assert(m4);
  ap_assert(m5);
  _tokens.push_back(m1);
  _tokens.push_back(m2);
  _tokens.push_back(m3);
  _tokens.push_back(m4);
  _tokens.push_back(m5);
}

Event::Event(Monitor* m1, Monitor* m2, Monitor* m3, Monitor* m4, Monitor* m5, Monitor* m6)
  throw()
  : Persistent("Event"), _timestamp(apr_time_now()),
    _value(1), _message(""), _file(0), _line(0)
{
  ap_assert(m1);
  ap_assert(m2);
  ap_assert(m3);
  ap_assert(m4);
  ap_assert(m5);
  ap_assert(m6);
  _tokens.push_back(m1);
  _tokens.push_back(m2);
  _tokens.push_back(m3);
  _tokens.push_back(m4);
  _tokens.push_back(m5);
  _tokens.push_back(m6);
}

Event::Event(Monitor* m1, apr_int64_t v)
  throw()
  : Persistent("Event"), _timestamp(apr_time_now()),
    _value(v), _message(""), _file(0), _line(0)
{
  ap_assert(m1);
  _tokens.push_back(m1);
}

Event::Event(Monitor* m1, Monitor* m2, apr_int64_t v)
  throw()
  : Persistent("Event"), _timestamp(apr_time_now()),
    _value(v), _message(""), _file(0), _line(0)
{
  ap_assert(m1);
  ap_assert(m2);
  _tokens.push_back(m1);
  _tokens.push_back(m2);
}

Event::Event(Monitor* m1, Monitor* m2, Monitor* m3, apr_int64_t v)
  throw()
  : Persistent("Event"), _timestamp(apr_time_now()),
    _value(v), _message(""), _file(0), _line(0)
{
  ap_assert(m1);
  ap_assert(m2);
  ap_assert(m3);
  _tokens.push_back(m1);
  _tokens.push_back(m2);
  _tokens.push_back(m3);
}

Event::Event(Monitor* m1, Monitor* m2, Monitor* m3, Monitor* m4, apr_int64_t v)
  throw()
  : Persistent("Event"), _timestamp(apr_time_now()),
    _value(v), _message(""), _file(0), _line(0)
{
  ap_assert(m1);
  ap_assert(m2);
  ap_assert(m3);
  ap_assert(m4);
  _tokens.push_back(m1);
  _tokens.push_back(m2);
  _tokens.push_back(m3);
  _tokens.push_back(m4);
}

Event::Event(Monitor* m1, Monitor* m2, Monitor* m3, Monitor* m4, Monitor* m5, apr_int64_t v)
  throw()
  : Persistent("Event"), _timestamp(apr_time_now()),
    _value(v), _message(""), _file(0), _line(0)
{
  ap_assert(m1);
  ap_assert(m2);
  ap_assert(m3);
  ap_assert(m4);
  ap_assert(m5);
  _tokens.push_back(m1);
  _tokens.push_back(m2);
  _tokens.push_back(m3);
  _tokens.push_back(m4);
  _tokens.push_back(m5);
}

Event::Event(Monitor* m1, Monitor* m2, Monitor* m3, Monitor* m4, Monitor* m5, Monitor* m6, apr_int64_t v)
  throw()
  : Persistent("Event"), _timestamp(apr_time_now()),
    _value(v), _message(""), _file(0), _line(0)
{
  ap_assert(m1);
  ap_assert(m2);
  ap_assert(m3);
  ap_assert(m4);
  ap_assert(m5);
  ap_assert(m6);
  _tokens.push_back(m1);
  _tokens.push_back(m2);
  _tokens.push_back(m3);
  _tokens.push_back(m4);
  _tokens.push_back(m5);
  _tokens.push_back(m6);
}

Event::Event(const Event& base, std::vector<Monitor*> tokens,
             apr_time_t timestamp, apr_int64_t v)
  throw()
  : Persistent("Event"), _timestamp(timestamp), _value(v),
    _message(base._message), _file(base._file), _line(base._line)
{
}

Event::~Event() throw()
{
}

Event&
Event::message(const char* fmt, ...)
  throw()
{
  va_list ap;
  if(fmt) {
    va_start(ap, fmt);
    _message = mkstring::format(fmt, ap);
    va_end(ap);
  }
  return *this;
}

string
Event::format(Context* ctx) const
  throw()
{
  mkstring rtnvalue;
  apr_uint32_t i;
  // FIXME next check should be on _description being NULL probably iso \0
  if(_tokens.size() == 0 || (_tokens.size() == 1 &&
                             !_tokens[0]->_description))
    if(_message != "")
      return _message;
    else
      return mkstring()<<_value;
  for(i=0; i<_tokens.size(); i++) {
    if(i==1)
        rtnvalue << "(";
    else if(i>1)
      rtnvalue << ",";
    if(_tokens[i]->_description)
      rtnvalue << _tokens[i]->_description;
    else
      rtnvalue << _tokens[i]->_id;
  }
  if(_value != 1) {
    if(i==1)
      rtnvalue << "(";
    else if(i>1)
      rtnvalue << ",";
    ++i;
    rtnvalue << _value;
  }
  if(_message != "") {
    if(i==1)
      rtnvalue << "(";
    else if(i>1)
      rtnvalue << ",";
    ++i;
    rtnvalue << "\"" << _message << "\"";
  }
  if(i>1)
    rtnvalue << ")";
  return rtnvalue;
}

string
Event::logformat(Context* ctx) const
  throw()
{
  bool first = true;
  mkstring rtnvalue;
  apr_uint32_t i;
  for(i=0; i<_tokens.size(); i++)
    if(_tokens[i]->_description)
      if(first)
        rtnvalue << "For " << _tokens[i]->_description;
      else
        rtnvalue << ", " << _tokens[i]->_description;
  if(!first)
    rtnvalue << ": ";
  if(_message != "")
    rtnvalue << _message;
  else
    rtnvalue << _value;
  return rtnvalue;
}

Persistent*
Event::instantiateClass()
  throw()
{
  return new Event();
}

Input&
Event::operator>>(Input& stream)
  throw()
{
  apr_byte_t nmonitors;
  stream >> _timestamp >> _value >> nmonitors >> _message;
  _tokens.clear();
  for(int i=0; i<nmonitors; i++) {
    apr_uint32_t id;
    stream >> id;
    _tokens.push_back(stream.context()->_monitors[id]);
  }
  return stream;
}

Output&
Event::operator<<(Output& stream) const
  throw()
{
  apr_byte_t nmonitors = _tokens.size();
  stream << _timestamp << _value << nmonitors << _message;
  for(vector<Monitor*>::const_iterator iter=_tokens.begin();
      iter != _tokens.end();
      ++iter)
    stream << (*iter)->_id;
  return stream;
}

int
Event::operator<(const Event& other) const
  throw()
{
  int cmp;
  if((cmp = _tokens.size() < other._tokens.size()) == 0)
    for(apr_uint32_t i=0; i<_tokens.size(); i++)
      if((cmp = (_tokens[i]->_id < other._tokens[i]->_id)) != 0)
        return cmp;
  return cmp;
}

/****************************************************************************/

map<string,Filter*> Filter::_filters;

Filter::Filter(const char* name)
  throw()
  : MergePersistent(name)
{
}

Output&
Filter::operator<<(Output& out) const
  throw()
{
  Persistent::operator<<(out);
  return out;
}

Input&
Filter::operator>>(Input& in)
  throw()
{
  Persistent::operator>>(in);
  return in;
}

bool
Filter::sync(SharedStorage& store)
  throw()
{
  try {
    this->operator>>(store);
  } catch(NoData) {
  }
  this->operator<<(store);
  return true;
}

void
Filter::registerFilter(const char* name, Filter* templ)
  throw()
{
  _filters[name] = templ;
}

Filter*
Filter::lookupFilter(Context* ctx, const char *name)
  throw()
{
  map<string,Filter*>::iterator iter = _filters.find(name);
  if(iter != _filters.end()) {
    Filter* f = iter->second;
    if(f)
      f = f->instantiateFilter();
    return f;
  } else
    return 0;
}

std::string
Filter::nextString(const char** arg)
  throw()
{
  if(!arg || !*arg)
    return "";
  while(**arg && apr_isspace(**arg))
    ++*arg;
  const char* s1 = *arg;
  while(**arg && !apr_isspace(**arg))
    ++*arg;
  const char* s2 = *arg;
  while(**arg && apr_isspace(**arg))
    ++*arg;  
  return string(s1, s2 - s1);
}

apr_int32_t
Filter::nextInteger(const char** arg)
  throw()
{
  char *endptr;
  apr_int32_t rtnvalue = strtol(*arg, &endptr, 0);
  if(!endptr || endptr == *arg) {
    *arg = 0;
    return -1;
  } else {
    while(apr_isspace(*endptr))
      ++endptr;
    *arg = endptr;
    return rtnvalue;
  }
}

/****************************************************************************/

class DeclareMonitorFilter : public Filter
{
private:
  int _sequenceid;
public:
  DeclareMonitorFilter() throw()
    : Filter("DeclareMonitorFilter"), _sequenceid(0)    
  { };
  virtual Filter* instantiateFilter() throw() {
    return new DeclareMonitorFilter();
  };
  virtual void initialize(Context* ctx, const char* arg) throw() {
    _sequenceid = atoi(arg);
  };
  virtual Output& operator<<(Output& out) const throw() {
    Filter::operator<<(out);
    return out << _sequenceid;
  };
  virtual Input& operator>>(Input& in) throw() {
    Filter::operator>>(in);
    return in >> _sequenceid;
  };
  virtual ~DeclareMonitorFilter() throw() { };
  virtual void push(Context* ctx, const Event& evt) throw() {
    if(ctx->update(true)) {
      evt._tokens[0]->_id = _sequenceid++;
      ctx->unlock();
    }
  }
  virtual void pull(Context* ctx, Filter* callback) throw() {
  }
  vector<string> description() throw() {
    vector<string> props;
    props.push_back("");
    return props;
  };  
};

class LogFilter : public Filter
{
private:
  bool _traditional;
  int _aplevel;
public:
  LogFilter() throw()
    : Filter("LogFilter"), _traditional(false), _aplevel(APLOG_ERR)
  { };
  virtual Filter* instantiateFilter() throw() {
    return new LogFilter();
  };
  virtual void initialize(Context* ctx, const char* arg) throw() {
    if(arg)
      if(!strcasecmp(arg,"traditional"))
        _traditional = true;
      else if(!strcasecmp(arg,"crit") || !strcasecmp(arg,"critical"))
        _aplevel = APLOG_CRIT;
      else if(!strcasecmp(arg,"error") || !strcasecmp(arg,"err"))
        _aplevel = APLOG_ERR;
      else if(!strcasecmp(arg,"warn") || !strcasecmp(arg,"warning"))
        _aplevel = APLOG_WARNING;
      else if(!strcasecmp(arg,"notice"))
        _aplevel = APLOG_NOTICE;
      else if(!strcasecmp(arg,"info"))
        _aplevel = APLOG_INFO;
      else if(!strcasecmp(arg,"debug"))
        _aplevel = APLOG_DEBUG;
  };
  virtual Output& operator<<(Output& out) const throw() {
    Filter::operator<<(out);
    return out << _traditional << _aplevel;
  };
  virtual Input& operator>>(Input& in) throw() {
    Filter::operator>>(in);
    return in >> _traditional >> _aplevel;
  };
  virtual ~LogFilter() throw() { };
  virtual void push(Context* ctx, const Event& evt) throw() {
    string msg = evt.logformat(ctx);
    diag_print(ctx, evt._file, evt._line, 0, _aplevel, msg.c_str(), APR_SUCCESS);
  }
  virtual void pull(Context* ctx, Filter* callback) throw() {
  }
  vector<string> description() throw() {
    vector<string> props;
    props.push_back("log");
    return props;
  };
};

class TraceFilter : public Filter
{
  friend class Filter;
  friend class Context;
public:
  TraceFilter() throw() : Filter("TraceFilter") { };
  virtual Filter* instantiateFilter() throw() {
    return new TraceFilter();
  };
  virtual void initialize(Context* ctx, const char* arg) throw() {
  };
  virtual Output& operator<<(Output& out) const throw() {
    Filter::operator<<(out);
    return out;
  };
  virtual Input& operator>>(Input& in) throw() {
    Filter::operator>>(in);
    return in;
  };
  virtual ~TraceFilter() throw() { };
  virtual void push(Context* ctx, const Event& evt) throw() {
    request_rec* r = ctx->request();
    if(r)
      apr_table_merge(r->headers_out, "X-Globule-Trace",
                      evt.format(ctx).c_str());
  };
  virtual void pull(Context* ctx, Filter* callback) throw() {
  };
  vector<string> description() throw() {
    vector<string> props;
    props.push_back("trace");
    return props;
  };
};

class CounterFilter : public Filter
{
  friend class Filter;
  friend class Context;
private:
  int _currentcount, _initialcount;
public:
  CounterFilter() throw()
    : Filter("CounterFilter"),
      _currentcount(0), _initialcount(0)
  { };
  virtual Filter* instantiateFilter() throw() {
    return new CounterFilter();
  };
  virtual void initialize(Context* ctx, const char* arg) throw() {
  };
  virtual Output& operator<<(Output& out) const throw() {
    Filter::operator<<(out);
    out << _currentcount;
    return out;
  };
  virtual Input& operator>>(Input& in) throw() {
    Filter::operator>>(in);
    in >> _currentcount;
    _initialcount = _currentcount;
    return in;
  };
  virtual bool sync(SharedStorage& store) throw() {
    _currentcount = _currentcount - _initialcount; // only difference
    try {
      Filter::operator>>(store);
      store >> _initialcount;
    } catch(NoData) {
    }
    _currentcount += _initialcount;
    _initialcount  = _currentcount;
    Filter::operator<<(store);
    store << _currentcount;
    return true;
  };
  virtual ~CounterFilter() throw() { };
  virtual void push(Context* ctx, const Event& evt) throw() {
    ++_currentcount;
  };
  virtual void pull(Context* ctx, Filter* callback) throw() {
    Event e(_currentcount);
    callback->push(ctx, e);
  };
  vector<string> description() throw() {
    vector<string> props;
    props.push_back("counter");
    return props;
  };
};

class SelectFilter : public Filter
{
private:
  Filter* _link;
  vector<apr_int32_t> _matches;
public:
  SelectFilter() throw()
    : Filter("SelectFilter"), _link(0)
  { };
  virtual Filter* instantiateFilter() throw() {
    return new SelectFilter();
  };
  virtual void initialize(Context* ctx, const char* arg) throw() {
    string token;
    if(arg && *arg) {
      string link(nextString(&arg));
      _link = ctx->filter(arg);
    }
    while(arg && *arg) {
      string token(nextString(&arg));
      _matches.push_back(ctx->monitorid(token.c_str()));
    }
  };
  virtual Output& operator<<(Output& out) const throw() {
    Filter::operator<<(out);
    return out;
  };
  virtual Input& operator>>(Input& in) throw() {
    Filter::operator>>(in);
    return in;
  };
  virtual bool sync(SharedStorage& store) throw() {
    try {
      Filter::operator>>(store);
    } catch(NoData) {
    }
    Filter::operator<<(store);
    return true;
  };
  virtual ~SelectFilter() throw() { };
  virtual void push(Context* ctx, const Event& evt) throw() {
    if(_link) {
      for(vector<apr_int32_t>::iterator iter = _matches.begin();
          iter != _matches.end();
          ++iter)
        _link->push(ctx, evt);
    }
  };
  virtual void pull(Context* ctx, Filter* callback) throw() {
  };
  vector<string> description() throw() {
    vector<string> props;
    props.push_back("select");
    return props;
  };
};

class ViewFilter : public Filter
{
private:
  static const int TIMESTAMP = -1;
  static const int VALUE = -2;
  int _timestampfield, _valuefield;
  Filter* _link;
  vector<int> _fields;
public:
  ViewFilter() throw()
    : Filter("ViewFilter")
  { };
  virtual Filter* instantiateFilter() throw() {
    return new ViewFilter();
  };
  virtual void initialize(Context* ctx, const char* arg) throw() {
  };
  virtual Output& operator<<(Output& out) const throw() {
    Filter::operator<<(out);
    return out;
  };
  virtual Input& operator>>(Input& in) throw() {
    Filter::operator>>(in);
    return in;
  };
  virtual bool sync(SharedStorage& store) throw() {
    try {
      Filter::operator>>(store);
    } catch(NoData) {
    }
    Filter::operator<<(store);
    return true;
  };
  virtual ~ViewFilter() throw() { };
  virtual void push(Context* ctx, const Event& evt) throw() {
    apr_int64_t value;
    apr_time_t timestamp;
    vector<Monitor*> tokens;
    switch(_timestampfield) {
    case TIMESTAMP:
      timestamp = evt._timestamp;
      break;
    case VALUE:
      timestamp = evt._value;
      break;
    default:
      timestamp = evt._tokens[_timestampfield]->_id;
      break;
    }
    switch(_valuefield) {
    case TIMESTAMP:
      value = evt._timestamp;
      break;
    case VALUE:
      value = evt._value;
      break;
    default:
      value = evt._tokens[_timestampfield]->_id;
      break;
    }
    for(vector<int>::iterator iter = _fields.begin();
        iter != _fields.end();
        ++iter)
      tokens.push_back(evt._tokens[*iter]);
    Event ev(evt, tokens, timestamp, value);
    _link->push(ctx, ev);
  };
  virtual void pull(Context* ctx, Filter* callback) throw() {
    Filter* savedfwdlink = _link;
    _link = callback;
    _link->pull(ctx, this);
    _link = savedfwdlink;
  };
  vector<string> description() throw() {
    vector<string> props;
    props.push_back("view");
    return props;
  };
};

class GroupFilter : public Filter
{
  friend class Filter;
  friend class Context;
private:
public:
  GroupFilter() throw()
    : Filter("GroupFilter")
  { };
  virtual Filter* instantiateFilter() throw() {
    return new GroupFilter();
  };
  virtual void initialize(Context* ctx, const char* arg) throw() {
  }
  virtual Output& operator<<(Output& out) const throw() {
    Filter::operator<<(out);
    return out;
  };
  virtual Input& operator>>(Input& in) throw() {
    Filter::operator>>(in);
    return in;
  };
  virtual bool sync(SharedStorage& store) throw() {
    try {
      Filter::operator>>(store);
    } catch(NoData) {
    }
    Filter::operator<<(store);
    return true;
  };
  virtual ~GroupFilter() throw() { };
  virtual void push(Context* ctx, const Event& evt) throw() {
  };
  virtual void pull(Context* ctx, Filter* callback) throw() {
  };
  vector<string> description() throw() {
    vector<string> props;
    props.push_back("group");
    return props;
  };
};

class DuplicateFilter : public Filter
{
private:
  vector<Filter*> _links;
public:
  DuplicateFilter() throw()
    : Filter("DuplicateFilter")
  { };
  virtual Filter* instantiateFilter() throw() {
    return new DuplicateFilter();
  };
  virtual void initialize(Context* ctx, const char* arg) throw() {
    while(arg && *arg) {
      string link(nextString(&arg));
      _links.push_back(ctx->filter(link.c_str()));
    }
  };
  virtual Output& operator<<(Output& out) const throw() {
    Filter::operator<<(out);
    int size = _links.size();
    out << size;
    for(vector<Filter*>::const_iterator iter = _links.begin();
        iter != _links.end();
        ++iter)
      out << *iter;
    return out;
  };
  virtual Input& operator>>(Input& in) throw() {
    Filter::operator>>(in);
    int size;
    in >> size;
    _links.clear();
    while(size--) {
      Filter* f;
      in >> (Persistent*&)f;
      _links.push_back(f);
    }
    return in;
  };
  virtual ~DuplicateFilter() throw() { };
  virtual void push(Context* ctx, const Event& evt) throw() {
    for(vector<Filter*>::iterator iter = _links.begin();
        iter != _links.end();
        ++iter)
      (*iter)->push(ctx, evt);
  };
  virtual void pull(Context* ctx, Filter* callback) throw() {
  };
  vector<string> description() throw() {
    vector<string> props;
    props.push_back("duplicate");
    return props;
  };
};

class RateFilter : public Filter
{
private:
  apr_time_t  _timestamp;
  apr_time_t  _interval;
  apr_int64_t _rate;
  Filter* _source;
  Filter* _forward;
public:
  RateFilter() throw()
    : Filter("RateFilter"), _timestamp(0), _interval(0), _rate(0),
      _source(0), _forward(0)
  { };
  virtual Filter* instantiateFilter() throw() {
    return new RateFilter();
  };
  virtual void initialize(Context* ctx, const char* arg) throw() {
    _interval = nextInteger(&arg);
    if(arg && *arg) {
      string link(nextString(&arg));
      _source = ctx->filter(arg);
      if(arg && *arg) {
        link = nextString(&arg);
        _forward = ctx->filter(link.c_str());
      }
    }
  };
  virtual Output& operator<<(Output& out) const throw() {
    Filter::operator<<(out);
    return out << _timestamp << _interval << _rate << _source << _forward;
  };
  virtual Input& operator>>(Input& in) throw() {
    Filter::operator>>(in);
    return in >> _timestamp >> _interval >> _rate
              >> (Persistent*&)_source >> (Persistent*&)_forward;
  };
  virtual bool sync(SharedStorage& store) throw() {
    bool rtnvalue = true;
    try {
      Filter::operator>>(store);
      apr_time_t storedtime;
      store >> storedtime;
      if(storedtime > _timestamp) {
        _timestamp = storedtime;
        store >> _interval >> _rate
              >> (Persistent*&)_source >> (Persistent*&)_forward;
        rtnvalue = false;
      }
    } catch(NoData) {
    }
    this->operator<<(store);
    return rtnvalue;
  };
  virtual ~RateFilter() throw() { };
  virtual void push(Context* ctx, const Event& evt) throw() {
    if(_rate >= 0) {
      apr_time_t now = apr_time_now();
      if(_timestamp + _interval < now) {
        _timestamp = now;
        _rate = -1;
        _source->pull(ctx, this);
        Event evt(_rate);
        if(_forward)
          _forward->push(ctx, evt);
      }
    } else
      if((_rate = evt._value) < 0)
        _rate = 0;
  };
  virtual void pull(Context* ctx, Filter* callback) throw() {
    Event evt(_rate);
    callback->push(ctx, _rate);
  };
  vector<string> description() throw() {
    vector<string> props;
    props.push_back("rate");
    return props;
  };
};

/****************************************************************************/

class PeriodsStoreFilter : public Filter
{
private:
  gvector<Event> *_storage;
  vector<Event> _newevents;
  int _nperiods;
public:
  PeriodsStoreFilter() throw()
    : Filter("PeriodsStoreFilter"), _storage(0)
  { };
  virtual Filter* instantiateFilter() throw() {
    return new PeriodsStoreFilter();
  };
  virtual void initialize(Context* ctx, const char* arg) throw() {
    _nperiods = 1;
    if(_storage)
      rmmmemory::destroy(_storage); // operator delete(_storage,rmmmemory::shm());
    if(arg && *arg)
      _nperiods = atoi(arg);
    _storage = new(rmmmemory::shm()) gvector<Event>;
  };
  virtual Output& operator<<(Output& out) const throw() {
    Filter::operator<<(out);
    return ((Output&)out) << _storage;
  };
  virtual Input& operator>>(Input& in) throw() {
    Filter::operator>>(in);
    return in >> (Persistent*&)_storage;
  };
  virtual bool sync(SharedStorage& store) throw() {
    Filter::operator>>(store);
    ((Input&)store) >> (Persistent*&)_storage;
    if(_storage) {
      for(vector<Event>::const_iterator iter = _newevents.begin();
          iter != _newevents.end();
          ++iter)
        _storage->push_back(*iter);
      _newevents.clear();
    }
    Filter::operator<<(store);
    ((Output&)store) << _storage;
    return true;
  };
  virtual ~PeriodsStoreFilter() throw() { };
  virtual void push(Context* ctx, const Event& evt) throw() {
    _newevents.push_back(evt);
  };
  virtual void pull(Context* ctx, Filter* callback) throw() {
    if(ctx->update(true)) {
      for(gvector<Event>::iterator iter = _storage->begin();
          iter != _storage->end(); ++iter)
        callback->push(ctx, *iter);
      ctx->unlock();
    }
  };
  vector<string> description() throw() {
    vector<string> props;
    props.push_back("periodsstorage");
    return props;
  };
};

/****************************************************************************/

class TriggerFilter : public Filter
{
private:
  Filter* _bcklink;
  Filter* _fwdlink;
public:
  TriggerFilter() throw()
    : Filter("TriggerFilter"), _bcklink(0), _fwdlink(0)
  { };
  virtual Filter* instantiateFilter() throw() {
    return new TriggerFilter();
  };
  virtual void initialize(Context* ctx, const char* arg) throw() {
    if(arg && *arg) {
      string link(nextString(&arg));
      _bcklink = ctx->filter(link.c_str());
      if(*arg) {
        link = nextString(&arg);
        _fwdlink = ctx->filter(link.c_str());
      }
    }
  };
  virtual Output& operator<<(Output& out) const throw() {
    Filter::operator<<(out);
    return out << _bcklink << _fwdlink;
  };
  virtual Input& operator>>(Input& in) throw() {
    Filter::operator>>(in);
    return in >> (Persistent*&)_bcklink >> (Persistent*&)_fwdlink;
  };
  virtual ~TriggerFilter() throw() { };
  virtual void push(Context* ctx, const Event& evt) throw() {
    if(_fwdlink)
      if(_bcklink)
        _bcklink->pull(ctx, _fwdlink);
      else
        _fwdlink->push(ctx, evt);
  };
  virtual void pull(Context* ctx, Filter* callback) throw() {
    _bcklink->pull(ctx, callback);
  };
  vector<string> description() throw() {
    vector<string> props;
    props.push_back("trigger");
    return props;
  };
};

class VariableFilter : public Filter
{
private:
  enum { NONE, SHMUSED, SHMAVAIL, SHMITEMS } _type;
  string _varname;
public:
  VariableFilter() throw()
    : Filter("VariableFilter"), _varname("")
  { };
  virtual Filter* instantiateFilter() throw() {
    return new VariableFilter();
  };
  virtual void initialize(Context* ctx, const char* arg) throw() {
    if(arg && *arg)
      _varname = arg;
    if(_varname == "shmused" || _varname == "shmbytes")
      _type = SHMUSED;
    else if(_varname == "shmavail")
      _type = SHMAVAIL;
    else if(_varname == "shmitems")
      _type = SHMITEMS;
    else
      _type = NONE;
  };
  virtual Output& operator<<(Output& out) const throw() {
    Filter::operator<<(out);
    return out << (int&)_type << _varname;
  };
  virtual Input& operator>>(Input& in) throw() {
    Filter::operator>>(in);
    return in >> (int&)_type >> _varname;
  };
  virtual bool sync(SharedStorage& store) throw() {
    Filter::operator>>(store);
    store >> (int&)_type >> _varname;
    Filter::operator<<(store);
    store << (int&)_type << _varname;
    return true;
  };
  virtual ~VariableFilter() throw() { };
  virtual void push(Context* ctx, const Event& evt) throw() {
  };
  virtual void pull(Context* ctx, Filter* callback) throw() {
    apr_size_t size;
    apr_int64_t value = 0;
    switch(_type) {
    case SHMUSED:
      rmmmemory::usage(0, &size, 0, 0);
      value = size;
      break;
    case SHMAVAIL:
      rmmmemory::usage(0, 0, &size, 0);
      value = size;
      break;
    case SHMITEMS:
      rmmmemory::usage(0, 0, 0, &size);
      value = size;
      break;
    case NONE:
      break;
    }
    Event event(value);
    callback->push(ctx, event);
  };
  vector<string> description() throw() {
    vector<string> props;
    props.push_back("variable");
    return props;
  };
};

/****************************************************************************/

std::map<apr_int32_t,Monitor*> Context::_monitors;

#ifndef STANDALONE_APR

Context::Context(SharedStorageSpace* storage, apr_global_mutex_t* mutex)
  throw()
  : MergePersistent("Context"), _storage(storage), _mutex(mutex),
    _cfgtimestamp(0), _request(0), _pool(0)
{
  int i = 0;
  _monitors[i++] = &Monitor::root;
  Monitor::root._id = 0;
  for(Monitor* run=registeredMonitors; run; run=run->_nextRegistered) {
    if(run != &Monitor::root) {
      run->_id = i;
      _monitors[i++] = run;
    }
  }
  _nfilters = i;
  _filters = new Filter*[_nfilters];
  _filters[0] = Filter::lookupFilter(this, "");
  string arg = mkstring() << i;
  _filters[0]->initialize(this, arg.c_str());
  for(unsigned int i=1; i<_nfilters; i++)
    _filters[i] = 0;
}

#else

Context::Context() throw()
  : MergePersistent("Context"), _storage(0),
    _cfgtimestamp(0), _request(0), _pool(0)
{
}

#endif

void
Context::initialize() throw()
{
  Filter::registerFilter("null",      0);
#define DEFFILTER(CLASSNAME,SYMBOLICNAME) { Filter *f = new CLASSNAME; Persistent::registerClass(#CLASSNAME,f); Filter::registerFilter(SYMBOLICNAME, f); }
  DEFFILTER(DeclareMonitorFilter, "");
  DEFFILTER(LogFilter,         "log");
  DEFFILTER(TraceFilter,       "trace");
  DEFFILTER(CounterFilter,     "counter");
  DEFFILTER(DuplicateFilter,   "duplicate");
  DEFFILTER(RateFilter,        "rate");
  DEFFILTER(SelectFilter,      "select");
  DEFFILTER(ViewFilter,        "view");
  DEFFILTER(GroupFilter,       "group");
  DEFFILTER(TriggerFilter,     "trigger");
  DEFFILTER(VariableFilter,    "variable");
#undef DEFFILTER
}

Persistent*
Context::instantiateClass() throw()
{
  return 0;
}

Output&
Context::operator<<(Output& out) const throw()
{
  Persistent::operator<<(out);
  out << _cfgtimestamp << _nfilters;
  for(int filteridx=_nfilters-1; filteridx>=0; filteridx--)
    out << _filters[filteridx];
  return out;
}

Input&
Context::operator>>(Input& in) throw()
{
  Persistent::operator>>(in);
  if(_filters) {
    for(_filteridx=_nfilters-1; _filteridx>=0; _filteridx--)
      if(_filters[_filteridx])
        delete _filters[_filteridx];
    delete[] _filters;
  }
  in >> _cfgtimestamp >> _nfilters;
  _filters = new Filter*[_nfilters];
  for(_filteridx=_nfilters-1; _filteridx>=0; _filteridx--)
    in >> (Persistent*&)_filters[_filteridx];
  return in;
}

// returns whether current storage still valid
bool
Context::sync(SharedStorage& store)  throw()
{
  apr_time_t storecfgtimestamp;
  try {
    Persistent::operator>>(store);
    store >> storecfgtimestamp;
  } catch(NoData) {
    storecfgtimestamp = 0;
  }
  Persistent::operator<<(store);

  if(storecfgtimestamp < _cfgtimestamp) {
    store.truncate(); // do not update local from store
    store << _cfgtimestamp << _nfilters;
    for(_filteridx=_nfilters-1; _filteridx>=0; _filteridx--)
      store.sync((MergePersistent*&)_filters[_filteridx]);
    return true;
  } else if(storecfgtimestamp > _cfgtimestamp) {
    if(_filters) {
      for(_filteridx=_nfilters-1; _filteridx>=0; _filteridx--)
        if(_filters[_filteridx])
          delete _filters[_filteridx];
      delete[] _filters;
    }
    _cfgtimestamp = storecfgtimestamp;
    store >> _nfilters;
    store << _cfgtimestamp << _nfilters;
    _filters = new Filter*[_nfilters];
    for(_filteridx=_nfilters-1; _filteridx>=0; _filteridx--) {
      ((Input&)store) >> (Persistent*&)_filters[_filteridx];
      ((Output&)store) << (Persistent*&)_filters[_filteridx];
    }
    return false;
  } else {
    unsigned int dummy; // should be same data type as Context::_nfilters
    try {
      store >> dummy; // skip the nfilters field
      ap_assert(dummy == _nfilters);
    } catch(NoData) {
    }
    store << _cfgtimestamp << _nfilters;

    for(_filteridx=_nfilters-1; _filteridx>=0; _filteridx--)
      store.sync((MergePersistent*&)_filters[_filteridx]);
    return true;
  }
}

void
Context::filter(const char* monitorname, Filter* f) throw()
{
  int filterid = -1;
  _cfgtimestamp = apr_time_now();
  for(map<apr_int32_t,Monitor*>::iterator iter = _monitors.begin();
      iter != _monitors.end();
      ++iter)
    if(!strcmp(monitorname,iter->second->_name)) {
      filterid = iter->first;
      break;
    }
  if(filterid < 0) {
    Monitor* mon = new Monitor(this, monitorname, monitorname);
    filterid = mon->_id;
  }
  if((unsigned)filterid >= _nfilters) {
    // C++ could do with a realloc/re-new.
    Filter** newfilters = new Filter*[filterid+1];
    memcpy(newfilters, _filters, sizeof(Filter*)*_nfilters);
    delete[] _filters;
    _filters = newfilters;
    for(int i=_nfilters; i<filterid+1; i++)
      _filters[i] = 0;
    _nfilters = (int) filterid + 1;
  }
  if(_filters[filterid])
    delete _filters[filterid];
  _filters[filterid] = f;
}

Filter*
Context::filter(const char* monitorname) throw()
{
  int filterid = -1;
  for(map<apr_int32_t,Monitor*>::iterator iter = _monitors.begin();
      iter != _monitors.end();
      ++iter)
    if(!strcmp(monitorname,iter->second->_name)) {
      filterid = iter->first;
      break;
    }
  if(filterid < 0)
    return 0;
  return _filters[filterid];
}

apr_int32_t
Context::monitorid(const char* monitorname) throw()
{
  for(map<apr_int32_t,Monitor*>::iterator iter = _monitors.begin();
      iter != _monitors.end();
      ++iter)
    if(!strcmp(monitorname,iter->second->_name))
      return iter->first;
  return -1;
}

void
Context::lock() throw()
{
  apr_global_mutex_lock(_mutex);
}

void
Context::unlock() throw()
{
  apr_global_mutex_unlock(_mutex);
}

bool
Context::update(bool retainlock) throw()
{
  DEBUGACTION(Context,update,begin);
  lock();
  try {
    if(SharedStorage(_storage).sync(*this)) {
      if(!retainlock) {
        unlock();
        DEBUGACTION(Context,update,end);
      }
      return true;
    } else {
      unlock();
      DEBUGACTION(Context,update,end);
      return false;
    }
  } catch(FileError ex) {
    unlock();
    DEBUGACTION(Context,update,end);
    DIAG(this,(MONITOR(error)),("Internal error handling internal management data: %s",ex.getMessage().c_str()));
    return false;
  }
}

void
Context::push(const Event& ev) throw()
{
  if(ev._tokens.size()>0 && _filters[ev._tokens[0]->_id])
    _filters[ev._tokens[0]->_id]->push(this, ev);
}

OverlayContext::OverlayContext(Context* ctx)
  throw()
  : Context(*ctx), _parent(ctx)
{
  if(_parent->pool()) {
    apr_pool_create(&_pool, _parent->pool());
  }
};

OverlayContext::~OverlayContext() throw()
{
  for(map<string,Filter*>::iterator iter = _moreFilters.begin();
      iter != _moreFilters.end();
      ++iter)
    delete iter->second;
  apr_pool_destroy(_pool);
}

void
OverlayContext::filter(const char* monitorname, Filter* f) throw()
{
  _moreFilters[monitorname] = f;
}

Filter*
OverlayContext::filter(const char* name) throw()
{
  map<string,Filter*>::iterator iter = _moreFilters.find(name);
  if(iter != _moreFilters.end())
      return iter->second;
  else
    return Context::filter(name);
}

/****************************************************************************/
