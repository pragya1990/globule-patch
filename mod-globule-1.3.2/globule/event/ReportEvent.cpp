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
#include <apr_lib.h>
#include "ReportEvent.hpp"

ReportEvent::ReportEvent(const char* category, ReportEvent* ref, int* indexptr)
  throw()
  : GlobuleEvent(ref->pool, ref->context, REPORT_EVT, (Handler*)0),
    _parent(ref)
{
  std::string counter = mkstring() << "n" << category;
  const char *s = ref->getProperty(counter.c_str());
  if(!s) {
    counter += "s";
    s = ref->getProperty(counter.c_str());
  }
  if(s) {
    int numof = atoi(s);
    if(indexptr) {
      int index = *indexptr;
      if(index >= numof) {
        numof = index + 1;
        ref->setProperty(counter, mkstring() << numof);
      } else
        *indexptr = numof;
      numof = index;
    } else
      ref->setProperty(counter, mkstring() << (numof+1));
    _prefix = mkstring() << category << numof << "_";
  } else {
    _prefix = mkstring() << category << "_";
    if(indexptr)
      *indexptr = -1;
  }
}

ReportEvent::~ReportEvent() throw()
{
  if(_parent)
    for(std::map<std::string,std::string>::iterator iter=_properties.begin();
        iter != _properties.end();
        ++iter)
      _parent->_properties[mkstring()<<_prefix<<iter->first] = iter->second;
}

const char*
ReportEvent::getProperty(const char* key) const throw()
{
  std::map<std::string,std::string>::const_iterator iter =
    _properties.find(key);
  if(iter != _properties.end())
    return iter->second.c_str();
  else if(_parent) {
    return _parent->getProperty(mkstring()<<_prefix<<key);
  } else
    return 0;
}

bool
ReportEvent::getPropertyInt(const char* key, int& val) const throw()
{
  const char* str = getProperty(key);
  char* end;
  int value;
  if(str) {
    while(apr_isspace(*str))
      ++str;
    if(apr_isdigit(*str)) {
      value = strtol(str, &end, 0);
      if(end != str) {
        while(apr_isspace(*str))
          ++str;
        if(!*str) {
          val = value;
          return true;
        }
      }
    }
  }
  return false;
}

bool
ReportEvent::getPropertyInt(const char* key, apr_uint16_t& val) const throw()
{
  int aux;
  if(getPropertyInt(key,aux)) {
    val = aux;
    return true;
  } else
    return false;
}

ReportEvent::iterator
ReportEvent::begin(const char* category) throw()
{
  return ReportEvent::iterator(this, category);
}

// FIXME: this end() method is way too expensive at the current implementation
ReportEvent::iterator
ReportEvent::end(const char* category) throw()
{
  ReportEvent::iterator iter = ReportEvent::iterator(this, category, -1);
  iter._index = iter._count;
  return iter;
}
