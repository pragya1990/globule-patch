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
#include "FilterConfiguration.hpp"
#include <iostream>
#include <cstdio>
#include <deque>

#ifdef MAIN
using std::cout;
using std::cerr;
using std::endl;
#endif
using std::string;
using std::vector;
using std::deque;

/****************************************************************************/

const char* FilterConfiguration::defaultconfiguration =
  "MODULE\t%defaults\n"
  "FILTER\tpolicy\ttrace\n"
  "END\n"
  "FILTER\treplmethod\ttrace\n"
  "END\n"
  "FILTER\tservedby\ttrace\n"
  "END\n"
  "FILTER\tfatal\tlog\n"
  "VALUE\tcrit\n"
  "END\n"
  "FILTER\terror\tlog\n"
  "VALUE\terror\n"
  "END\n"
  "FILTER\twarning\tlog\n"
  "VALUE\twarn\n"
  "END\n"
  "END\n"
  "MODULE\t%normal\n"
  "FILTER\tpolicy\ttrace\n"
  "END\n"
  "FILTER\treplmethod\ttrace\n"
  "END\n"
  "FILTER\tservedby\ttrace\n"
  "END\n"
  "FILTER\tfatal\tlog\n"
  "VALUE\tcrit\n"
  "END\n"
  "FILTER\terror\tlog\n"
  "VALUE\terror\n"
  "END\n"
  "FILTER\twarning\tlog\n"
  "VALUE\twarn\n"
  "END\n"
  "END\n"
  "MODULE\t%extended\n"
  "FILTER\tpolicy\ttrace\n"
  "END\n"
  "FILTER\treplmethod\ttrace\n"
  "END\n"
  "FILTER\tservedby\ttrace\n"
  "END\n"
  "FILTER\tfatal\tlog\n"
  "VALUE\tcrit\n"
  "END\n"
  "FILTER\terror\tlog\n"
  "VALUE\terror\n"
  "END\n"
  "FILTER\twarning\tlog\n"
  "VALUE\twarn\n"
  "END\n"
  "FILTER\tinfo\tlog\n"
  "VALUE\tinfo\n"
  "END\n"
  "FILTER\tnotice\tlog\n"
  "VALUE\tnotice\n"
  "END\n"
  "END\n"
  "MODULE\t%verbose\n"
  "FILTER\tpolicy\ttrace\n"
  "END\n"
  "FILTER\treplmethod\ttrace\n"
  "END\n"
  "FILTER\tservedby\ttrace\n"
  "END\n"
  "FILTER\tfatal\tlog\n"
  "VALUE\tcrit\n"
  "END\n"
  "FILTER\terror\tlog\n"
  "VALUE\terror\n"
  "END\n"
  "FILTER\twarning\tlog\n"
  "VALUE\twarn\n"
  "END\n"
  "FILTER\tinfo\tlog\n"
  "VALUE\tinfo\n"
  "END\n"
  "FILTER\tnotice\tlog\n"
  "VALUE\tnotice\n"
  "END\n"
  "FILTER\tdetail\tlog\n"
  "VALUE\tdebug\n"
  "END\n"
  "END\n";

apr_array_header_t* FilterConfiguration::filters = 0;

FilterConfiguration*
FilterConfiguration::loaddefault(apr_pool_t* pool) throw()
{
  apr_size_t len = strlen(defaultconfiguration) + 1;
  char* s = (char*) apr_palloc(pool, len);
  memcpy(s, defaultconfiguration, len);
  return parse(pool, s);
}

/****************************************************************************/

FilterConfiguration::FilterConfiguration(apr_pool_t* p,
                                         FilterConfiguration* templ)
  throw()
  : _type(templ->_type), _key(templ->_key), _val(templ->_val)
{
  for(vector<FilterConfiguration*>::iterator iter = templ->_defs.begin();
      iter != templ->_defs.end();
      ++iter)
    _defs.push_back(new(apr_palloc(p,sizeof(FilterConfiguration)))
                       FilterConfiguration(p,*iter));
}

FilterConfiguration::FilterConfiguration(enum Type type,
                                         const char* key, const char* val)
  throw()
  : _type(type), _key(key?key:""), _val(val?val:"")
{
}

FilterConfiguration::FilterConfiguration(enum Type type,
                                         string key, string val)
  throw()
  : _type(type), _key(key), _val(val)
{
}

FilterConfiguration*
FilterConfiguration::evaluate(apr_pool_t* p, string& str) throw()
{
  vector<FilterConfiguration*>::iterator iter = _defs.begin();
  switch(_type) {
  case VALUE:
    break;
  case FILTER:
    break;
  case MODULE:
    if(str == _key) {
      FilterConfiguration* instance;
      instance = new(apr_palloc(p,sizeof(FilterConfiguration)))
                    FilterConfiguration(p, this);
      instance->_type = SEQUENCE;
      return instance;
    }
    break;
  case SET:
    if(_key == str)
      str = _val;
    break;
  case SEQUENCE:
    for(; iter!=_defs.end(); iter++) {
      FilterConfiguration* d = (*iter)->evaluate(p, str);
      if(d)
	return d;
    }
    break;
  case INSTANCE:
    break;
  }
  if(str.find("$") == string::npos && str.find("%") == string::npos)
    return new(apr_palloc(p,sizeof(FilterConfiguration)))
              FilterConfiguration(VALUE, str);
  else
    return 0;
}

bool
FilterConfiguration::evaluate(apr_pool_t* p, FilterConfiguration* env) throw()
{
  bool change = false;
  vector<FilterConfiguration*>::iterator iter = _defs.begin();
  switch(_type) {
  case VALUE:
    break;
  case FILTER:
    if(env->evaluate(p, _key) && env->evaluate(p, _val)) {
      vector<FilterConfiguration*>::iterator subiter = _defs.begin();
      string args("");
      while(iter != _defs.end()) {
        (*iter)->evaluate(p, env);
        if((*iter)->_type == VALUE) {
          if(args != "")
            args += " ";
          args += (*iter)->_key;
        } else
          break;
	++iter;
      }
      if(iter == _defs.end())
        env->deffilter(p, _key.c_str(), _val.c_str(), args.c_str());
    }
    break;
  case SEQUENCE:
    for(; iter!=_defs.end(); iter++)
      if(*iter)
	if((*iter)->evaluate(p, env))
	  change = true;
    break;
  case INSTANCE: {
    FilterConfiguration* def = env->evaluate(p, _key);
    FilterConfiguration instantiation(SET,_key,_val);
    def->evaluate(p, &instantiation);
    *this = *def;
    change = true;
  }
  case SET:
    env->evaluate(p, _val);
    env->add(this);
    break;
  case MODULE:
    env->add(this);
    break;
  }
  return change;
}

apr_array_header_t*
FilterConfiguration::evaluate(apr_pool_t* p) throw()
{
  FilterConfiguration env(FilterConfiguration::SEQUENCE);
  while(evaluate(p, &env))
    ;
  filters = apr_array_make(p, 0, sizeof(char*[3]));
  evaluate(p, &env);
  return filters;
}

void
FilterConfiguration::deffilter(apr_pool_t* p, const char* position,
                               const char* type, const char* args) throw()
{
  if(filters) {
    char** entry = (char**) apr_array_push(filters);
    entry[0] = apr_pstrdup(p, position);
    entry[1] = apr_pstrdup(p, type);
    entry[2] = apr_pstrdup(p, args);
  }
}

/****************************************************************************/

void
FilterConfiguration::traverse(int indent) throw()
{
  int spaces = (indent > 0 ? indent*2 : 0);
  vector<FilterConfiguration*>::iterator iter = _defs.begin();
  switch(_type) {
  case VALUE:
    if(indent>=0)
      printf("%*.*sVALUE %s\n",spaces,spaces,"",_key.c_str());
    else
      printf("%s",_key.c_str());
    break;
  case FILTER:
    printf("%*.*sFILTER %s = %s",spaces,spaces,"",_key.c_str(),_val.c_str());
    switch(_defs.size()) {
    case 0:
      printf("\n");
      break;
    case 1:
      printf("(");
      _defs[0]->traverse(-1);
      printf(")");
      break;
    default:
      printf("{\n");
      for(; iter!=_defs.end(); iter++)
	(*iter)->traverse(indent+1);
      printf("%*.*s}\n",spaces,spaces,"");
      break;
    }
    break;
  case MODULE:
    printf("%*.*sMODULE %s {\n",spaces,spaces,"",_key.c_str());
    for(; iter!=_defs.end(); iter++)
      (*iter)->traverse(indent+1);
    printf("%*.*s}\n",spaces,spaces,"");
    break;
  case SET:
    printf("%*.*sSET %s = %s\n",spaces,spaces,"",_key.c_str(),_val.c_str());
    break;
  case SEQUENCE:
    printf("%*.*sSEQUENCE {\n",spaces,spaces,"");
    for(; iter!=_defs.end(); iter++)
      (*iter)->traverse(indent+1);
    printf("%*.*s}\n",spaces,spaces,"");
    break;
  case INSTANCE:
    printf("%*.*sINSTANCE %s = %s\n",spaces,spaces,"",_key.c_str(),
           _val.c_str());
    break;
  }
}

char*
FilterConfiguration::parse(apr_pool_t* p, char** entries) throw()
{
  char* errmsg = 0;
  deque<FilterConfiguration*> stack;
  stack.push_back(this);
  for(int i=0; entries[i*3] && entries[i*3+0]; i++) {
    if(!strcasecmp(entries[i*3+0],"end")) {
      // if(!stack.pop_back()) return "stack empty";
      stack.pop_back();
    } else {
      bool push = false;
      FilterConfiguration* def = 0;
      if(!strcasecmp(entries[i*3+0],"value")) {
	def = new(apr_palloc(p,sizeof(FilterConfiguration)))
                 FilterConfiguration(VALUE, entries[i*3+1]);
      } else if(!strcasecmp(entries[i*3+0],"set")) {
	def = new(apr_palloc(p,sizeof(FilterConfiguration)))
                 FilterConfiguration(SET, entries[i*3+1], entries[i*3+2]);
      } else if(!strcasecmp(entries[i*3+0],"instance")) {
	def = new(apr_palloc(p,sizeof(FilterConfiguration)))
                 FilterConfiguration(INSTANCE, entries[i*3+1], entries[i*3+2]);
      } else {
	push = true;
	if(!strcasecmp(entries[i*3+0],"module")) {
	  def = new(apr_palloc(p,sizeof(FilterConfiguration)))
                   FilterConfiguration(MODULE, entries[i*3+1]);
	} else if(!strcasecmp(entries[i*3+0],"filter")) {
	  def = new(apr_palloc(p,sizeof(FilterConfiguration)))
                   FilterConfiguration(FILTER, entries[i*3+1], entries[i*3+2]);
	} else if(!strcasecmp(entries[i*3+0],"sequence")) {
	  def = new(apr_palloc(p,sizeof(FilterConfiguration)))
                   FilterConfiguration(SEQUENCE);
	} else {
	  push = false;
	  if(!errmsg)
	    errmsg = "unrecognized input";
	}
      }
      if(def) {
	stack.back()->add(def);
	if(push)
	  stack.push_back(def);
      }
    }
  }
  if(stack.back() != this)
    if(!errmsg) 
      errmsg = "unexpected end";
  return errmsg;
}

FilterConfiguration*
FilterConfiguration::parse(apr_pool_t* p, char* input) throw()
{
  char* s;
  char* last = 0;
  vector<char*> lines;
  while((s = apr_strtok(input,"\n",&last))) {
    input = NULL;
    lines.push_back(apr_pstrdup(p,s));
  }
  char **inputset = (char**) apr_palloc(p, sizeof(char*)*(lines.size()+1)*3);
  int i=0;
  for(vector<char*>::iterator iter=lines.begin();
      iter!=lines.end();
      ++iter,++i)
    {
      last = NULL;
      inputset[i*3+0] = apr_pstrdup(p, apr_strtok(*iter, "\t", &last));
      inputset[i*3+1] = apr_pstrdup(p, apr_strtok(NULL,  "\t", &last));
      inputset[i*3+2] = apr_pstrdup(p, apr_strtok(NULL,  "\t", &last));
    }
  inputset[i*3+0] = 0;
  FilterConfiguration* top;
  top = new(apr_palloc(p,sizeof(FilterConfiguration)))
           FilterConfiguration(FilterConfiguration::SEQUENCE);
  if((s = top->parse(p, inputset))) {
#ifdef MAIN
    cerr << "input error: " << s << endl;
#endif
    return 0;
  } else
    return top;
}

/****************************************************************************/

#ifdef MAIN

#ifndef CHECK
#define CHECK(ARG) \
  do{if(ARG){fprintf(stderr,"call " #ARG " failed\n");exit(1);}}while(0)
#endif

int
main(int argc, char *argv[])
{
  int i;
  apr_allocator_t *allocer;
  apr_pool_t* pool;
  char** entry;

  CHECK(apr_allocator_create(&allocer));
  CHECK(apr_pool_create_ex(&pool, NULL, NULL, allocer));

  FilterConfiguration* cfg = FilterConfiguration::loaddefault(pool);
  for(i=1; i+1<argc; i++) {
    if(argv[i][0] == '%') {
      cfg->add(new(apr_palloc(pool,sizeof(FilterConfiguration)))
        FilterConfiguration(FilterConfiguration::INSTANCE,argv[i],argv[i+1]));
    } else if(argv[i][1] == '$') {
      cfg->add(new(apr_palloc(pool,sizeof(FilterConfiguration)))
        FilterConfiguration(FilterConfiguration::SET,argv[i],argv[i+1]));
    }
  }
  apr_array_header_t* filters;
  filters = cfg->evaluate(pool);
  while((entry = (char**)apr_array_pop(filters)))
    cout << entry[0] << "=" << entry[1] << "(" << entry[2] << ")" << endl;
  
  apr_pool_destroy(pool);
  apr_allocator_destroy(allocer);
  fprintf(stderr,"done.\n");
  exit(0);
}

#endif

/****************************************************************************/
