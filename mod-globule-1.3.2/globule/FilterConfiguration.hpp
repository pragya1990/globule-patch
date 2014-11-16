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
#ifndef _FILTERCONFIGURATION_HPP
#define _FILTERCONFIGURATION_HPP

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <apr.h>
#include <apr_pools.h>
#include <apr_strings.h>
#include <apr_tables.h>
#include <string>
#include <vector>

class FilterConfiguration
{
public:
  enum Type { VALUE, FILTER, MODULE, SET, SEQUENCE, INSTANCE };
private:
  enum Type _type;
  std::string _key, _val;
  std::vector<FilterConfiguration*> _defs;
  static const char* defaultconfiguration;
  static apr_array_header_t* filters;
protected:
  FilterConfiguration* evaluate(apr_pool_t*,std::string& str) throw();
  bool evaluate(apr_pool_t*,FilterConfiguration* env) throw();
  void deffilter(apr_pool_t*, const char*, const char*, const char*) throw();
public:
  FilterConfiguration(apr_pool_t*, FilterConfiguration* templ) throw();
  FilterConfiguration(enum Type, const char* key = 0, const char* val = 0) throw();
  FilterConfiguration(enum Type, std::string key, std::string val = "") throw();
  apr_array_header_t* evaluate(apr_pool_t*) throw();
  void traverse(int indent=0) throw();
  void add(FilterConfiguration* def) throw() { _defs.push_back(def); };
  char* parse(apr_pool_t* p, char** entries) throw();
  static FilterConfiguration* parse(apr_pool_t* p, char* input) throw();
  static FilterConfiguration* loaddefault(apr_pool_t* p) throw();
};

#endif
