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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <string>
#include <apr.h>
#include <httpd.h>
#include <http_core.h>
#include <http_protocol.h>
#include <http_request.h>
#include <http_log.h>
#include <util_filter.h>
#include <apr_lib.h>
#include "utilities.h"
#include "resource/ConfigHandler.hpp"
#include "event/HttpReqEvent.hpp"
#include "event/HttpMetaEvent.hpp"
#include "event/RedirectEvent.hpp"
#include "event/ReportEvent.hpp"
#include "resource/NameBindingHandler.hpp"

using namespace std;

const char* ConfigHandler::password = 0;
const char* ConfigHandler::brokerSerial = 0;
const char* ConfigHandler::path = 0;

class MonitorFilter : public Filter
{
  friend class Filter;
  friend class Context;
  friend class ConfigHandler;
private:
  string _name;
public:
  map<string,string>* data;
  MonitorFilter(const char* name = "", map<string,string>* m = 0)
    throw()
    : Filter("MonitorFilter"), _name(name), data(m)
  { };
  void initialize(Context* ctx, const char* arg) throw() {
  };
  virtual Filter* instantiateFilter() throw() {
    return new MonitorFilter();
  };
  virtual Output& operator<<(Output& out) const throw() {
    Filter::operator<<(out);
    out << _name;
    return out;
  };
  virtual Input& operator>>(Input& in) throw() {
    Filter::operator>>(in);
    in >> _name;
    return in;
  };
  virtual ~MonitorFilter() throw() {
  };
  virtual void push(Context* ctx, const Event& evt) throw() {
    if(data) {
      if(_name != "") {
        map<string,string>::iterator iter;
        string counter = mkstring() << "n" << _name;
        iter = data->find(counter);
        if(iter == data->end()) {
          counter = mkstring() << "n" << _name << "s";
          iter = data->find(counter);
        }
        if(iter == data->end()) {
          (*data)[_name] = mkstring() << evt._value;
          (*data)[mkstring()<<_name<<"_t"] = mkstring() << evt._timestamp;
          (*data)[mkstring()<<_name<<"_m"] = evt.format(ctx);
        } else {
          (*data)[mkstring()<<_name<<iter->second] = mkstring() << evt._value;
          (*data)[mkstring()<<_name<<iter->second<<"_t"] = mkstring() << evt._timestamp;
          (*data)[mkstring()<<_name<<iter->second<<"_m"] = evt.format(ctx);
          (*data)[iter->first] = mkstring()<<(atoi(iter->second.c_str())+1);
        }
      } else
        (*data)[""] = mkstring() << evt._value;
    }
  };
  virtual void pull(Context* ctx, Filter* callback) throw() {
  };
  vector<string> description() throw() {
    vector<string> props;
    props.push_back("monitor");
    return props;
  };
};

class MemoryInfoFilter : public Filter
{
private:
  string _fname;
public:
  MemoryInfoFilter(const char* filename = "")
    throw()
    : Filter("MemoryInfoFilter"), _fname(filename)
  { };
  void initialize(Context* ctx, const char* arg) throw() {
    if(arg)
      _fname = arg;
  };
  virtual Filter* instantiateFilter() throw() {
    return new MemoryInfoFilter();
  };
  virtual Output& operator<<(Output& out) const throw() {
    Filter::operator<<(out);
    out << _fname;
    return out;
  };
  virtual Input& operator>>(Input& in) throw() {
    Filter::operator>>(in);
    in >> _fname;
    return in;
  };
  virtual ~MemoryInfoFilter() throw() {
  };
  virtual void push(Context* ctx, const Event& evt) throw() {
  }
  virtual void pull(Context* ctx, Filter* callback) throw() {
#if (defined(HAVE_MYAPR) && !defined(SINGLECHILD))
    void *dump;
    int i, nobjects;
    apr_rmm_off_t* addrs;
    apr_size_t size;
    apr_size_t* sizes;
    myapr_rmm_usage(rmmmemory::rmmhandle, &size, NULL, NULL, NULL);
    dump = myapr_rmm_dump(rmmmemory::rmmhandle, ctx->pool(), &nobjects,
                          &addrs, &sizes);
    for(i=0; i<nobjects; i++) {
      Event e(addrs[i], sizes[i]);
      callback->push(ctx, e);
    }
    {
      Event e(size, 0);
      callback->push(ctx, e);
    }
    if(_fname != "") {
      apr_status_t status;
      static apr_file_t* fp;
      status = apr_file_open(&fp, _fname.c_str(),
#if (APR_MAJOR_VERSION > 0)
                           APR_FOPEN_CREATE|APR_FOPEN_TRUNCATE|APR_FOPEN_WRITE,
#else
                           APR_CREATE|APR_TRUNCATE|APR_WRITE,
#endif
                           APR_OS_DEFAULT, ctx->pool());
      if(status == APR_SUCCESS) {
        apr_size_t bytesleft = size;
        while(bytesleft > 0) {
          size = bytesleft;
          if((status = apr_file_write(fp, dump, &size)) != APR_SUCCESS) {
            ap_log_perror(APLOG_MARK, APLOG_ERR, status, ctx->pool(),
                          "Cannot write to shared memory dump file");
            break;
          }
          dump = &((char*)dump)[size];
          bytesleft -= size;
        }
        apr_file_close(fp);
      } else
        ap_log_perror(APLOG_MARK, APLOG_ERR, status, ctx->pool(),
                      "Cannot open shared memory dump file");
    }
#endif
  };
  vector<string> description() throw() {
    vector<string> props;
    props.push_back("meminfo");
    return props;
  };
};

class InfoFilter : public Filter
{
  friend class ConfigHandler;
private:
  static const int TIMESTAMP = -1;
  static const int VALUE     = -2;
  static const int MESSAGE   = -3;
  static const int INDEX     = -4;
  int _count, _keyfield, _valfield;
  Filter* _link;
  map<string,string>* _data;
public:
  InfoFilter(map<string,string>* m = 0)
    throw()
    : Filter("InfoFilter"), _count(0), _keyfield(TIMESTAMP), _valfield(VALUE),
      _link(0), _data(m)
  {
};
  void initialize(Context* ctx, const char* arg) throw() {
    if(arg && *arg) {
      string keyfield(nextString(&arg));
      if(!isdigit(*keyfield.c_str())) {
        switch(*keyfield.c_str()) {
        case 't':  _keyfield = TIMESTAMP;  break;
        case 'v':  _keyfield = VALUE;      break;
        case 'm':  _keyfield = MESSAGE;    break;
        case 'i':  _keyfield = INDEX;      break;
        }
      } else
        _keyfield = atoi(keyfield.c_str());
    }
    if(arg && *arg) {
      string valfield(nextString(&arg));
      if(!isdigit(*valfield.c_str())) {
        switch(*valfield.c_str()) {
        case 't':  _valfield = TIMESTAMP;  break;
        case 'v':  _valfield = VALUE;      break;
        case 'm':  _valfield = MESSAGE;    break;
        case 'i':  _valfield = INDEX;      break;
        }
      } else
        _valfield = atoi(valfield.c_str());
    }
    if(arg && *arg) {
      _link = ctx->filter(nextString(&arg).c_str());
    }
  };
  virtual Filter* instantiateFilter() throw() {
    return new InfoFilter();
  }
  virtual Output& operator<<(Output& out) const throw() {
    Filter::operator<<(out);
    return out;
  };
  virtual Input& operator>>(Input& in) throw() {
    Filter::operator>>(in);
    return in;
  };
  virtual ~InfoFilter() throw() {
  };
  virtual void push(Context* ctx, const Event& evt) throw() {
    if(_data) {
      mkstring key;
      mkstring val;
      switch(_keyfield) {
      case TIMESTAMP:  key << evt._timestamp;   break;
      case VALUE:      key << evt._value;       break;
      case MESSAGE:    key << evt.format(ctx);  break;
      case INDEX:      key << _count;           break;
      default:         key << evt._tokens[_keyfield]->_name;
      }
      switch(_valfield) {
      case TIMESTAMP:  val << evt._timestamp;   break;
      case VALUE:      val << evt._value;       break;
      case MESSAGE:    val << evt.format(ctx);  break;
      case INDEX:      val << _count;           break;
      default:         val << evt._tokens[_valfield]->_name;
      }
      (*_data)[key] = val;
    }
    ++_count;
  };
  virtual void pull(Context* ctx, Filter* callback) throw() {
    if(_link)
      _link->pull(ctx, this);
  };
  vector<string> description() throw() {
    vector<string> props;
    props.push_back("info");
    return props;
  };
};

static apr_status_t
getformitem(char *arg, map<string,string>& formdata) throw()
{
  char ch, ch1, ch2;
  char* s = ap_strchr(arg, '=');
  if(!s) {
    formdata[arg] = "";
  } else {
    string key(arg, s++ - arg);
    mkstring val;
    while(*s) {
      switch(*s) {
      case '+':
        val << ' ';
        ++s;
        break;
      case '%':
        if((ch1 = *++s) && apr_isxdigit(ch1)) {
          if((ch2 = *++s) && apr_isxdigit(ch2)) {
            ch = ((apr_isupper(ch1) ? ch1-'A'+10 :
                   apr_islower(ch1) ? ch1-'a'+10 : ch1-'0')<<4)
              | (apr_isupper(ch2) ? ch2-'A'+10 :
                 apr_islower(ch2) ? ch2-'a'+10 : ch2-'0');
            val << ch;
            ++s;
          } else {
            if(*s) {
              val << '%' << ch1 << ch2;
              ++s;
            } else
              val << '%' << ch1;
          }
        } else {
          if(*s) {
            val << '%' << ch1;
            ++s;
          } else
            val << '%';
        }
        break;
      default:
        val << *s;
        ++s;
      }
    }
    formdata[key] = val.str();
  }
  return APR_SUCCESS;
}

static apr_status_t
getform(char* args, map<string,string>& formdata)
{
  char* lastp;
  for(char* arg = apr_strtok(args, "&", &lastp);
      arg;
      arg = apr_strtok(NULL, "&", &lastp))
    getformitem(arg, formdata);
  return APR_SUCCESS;
}

static apr_status_t
getform(request_rec* r, map<string,string>& formdata)
{
  const unsigned int maxdata = 10240;
  const int maxbuffer = 10240;
  char* content;
  char* lastp;
  apr_status_t status;
  apr_off_t actual;
  apr_size_t total;
  apr_bucket_brigade *bb;
  bb = apr_brigade_create(r->pool, r->connection->bucket_alloc);
  status = ap_get_brigade(r->input_filters, bb, AP_MODE_READBYTES,
                          APR_BLOCK_READ, maxbuffer);
  apr_brigade_length(bb, 1, &actual);
  total = (apr_size_t)actual;
  if(total > maxdata)
    return HTTP_BAD_REQUEST; // max'ed out
  content = (char*) apr_palloc(r->pool, total + 1);
  status = apr_brigade_flatten(bb, content, &total);
  content[total] = '\0';
  if(status != APR_SUCCESS)
    return status;

  for(char* arg = apr_strtok(content, "&", &lastp);
      arg;
      arg = apr_strtok(NULL, "&", &lastp))
    getformitem(arg, formdata);
  return APR_SUCCESS;
}

static map<string,string>
fn_index(apr_pool_t* pool, Context* ctx,
         map<string,string>& formdata, ConfigHandler* cfg, void* data)
{
  {
    map<string,string>::iterator iter = formdata.find("test");
    if(iter != formdata.end())
      cfg->_test = iter->second.c_str();
  }
  formdata.clear();
  if(cfg->brokerSerial)
    formdata["gbs"] = cfg->brokerSerial;
  formdata["version"] = VERSION;
  formdata["test"] = cfg->_test.c_str();
  return formdata;
}

static map<string,string>
fn_resources(apr_pool_t* pool, Context* ctx,
             map<string,string>& formdata, ConfigHandler* cfg, void* data)
{
  apr_size_t usagesize, usagebytes, usagefree, usageitems;
  rmmmemory::usage(&usagesize, &usagebytes, &usagefree, &usageitems);
  formdata["shmsize"]  = mkstring()<<usagesize;
  formdata["shmbytes"] = mkstring()<<usagebytes;
  formdata["shmused"]  = mkstring()<<usagebytes;
  formdata["shmavail"] = mkstring()<<usagefree;
  formdata["shmitems"] = mkstring()<<usageitems;
  formdata["nsections"] = "0";
  ReportEvent evt(pool, ctx, formdata);
  GlobuleEvent::submit(evt);
  return evt.properties();
}

static map<string,string>
fn_gbs(apr_pool_t* pool, Context* ctx,
       map<string,string>& formdata, ConfigHandler* cfg, void* data)
{
  formdata.clear();
  if(cfg->brokerSerial)
    formdata[""] = cfg->brokerSerial;
  return formdata;
}

static map<string,string>
fn_filtering(apr_pool_t* pool, Context* ctx,
             map<string,string>& formdata, ConfigHandler* cfg, void* data)
{
  unsigned int i, j, nfilters;
  nfilters = atoi(formdata["nfilters"].c_str());
  ap_assert(nfilters < 256);
  if(nfilters > 0) {
    for(i=0; i<nfilters; i++)
      if(formdata.find(mkstring()<<"type"<<i) != formdata.end()) {
        Filter* f;
        string n(formdata[mkstring()<<"name"<<i]);
        f = Filter::lookupFilter(ctx, formdata[mkstring()<<"type"<<i].c_str());
        f->initialize(ctx, formdata[mkstring()<<"args"<<i].c_str());
        ctx->filter(n.c_str(), f);
      }
  }
  formdata.clear();
  nfilters = ctx->nfilters();
  formdata["nfilters"] = mkstring()<<nfilters;
  for(i=0; i<nfilters; i++) {
    formdata[mkstring()<<"name"<<i] = ctx->monitor(i)->_name;
    if(ctx->monitor(i)->_description && ctx->monitor(i)->_description[0])
      formdata[mkstring()<<"desc"<<i] = ctx->monitor(i)->_description;
    if(ctx->filter(i)) {
      vector<string> filterprops = ctx->filter(i)->description();
      formdata[mkstring()<<"type"<<i] = filterprops[0];
      for(j=1; j<filterprops.size(); j++)
        formdata[mkstring()<<"args"<<i] = filterprops[j];
    } else
      formdata[mkstring()<<"type"<<i] = "null";
  }
  return formdata;
}

static map<string,string>
fn_namebinding(apr_pool_t* p, Context* ctx,
               map<string,string>& formdata, ConfigHandler* cfg, void* data)
{
  int i = 0;
  formdata.clear();
  formdata["nbindings"] = mkstring() << (*NameBindingHandler::dnsdb).size();
  for(gvector<gDNSRecord>::iterator iter=(*NameBindingHandler::dnsdb).begin();
      iter != (*NameBindingHandler::dnsdb).end();
      ++iter, ++i)
    {
      if(iter->type() == DNSRecord::TYPE_A) {
        formdata[mkstring()<<"type"<<i] = "A";
        formdata[mkstring()<<"name"<<i] = iter->name();
        formdata[mkstring()<<"param"<<i] = mkstring()
                           << ((iter->addr()>>24)&0xff)
                           << ((iter->addr()>>16)&0xff)
                           << ((iter->addr()>>8)&0xff)
                           << ((iter->addr())&0xff);
        break;
      } else if(iter->type() == DNSRecord::TYPE_CNAME) {
        formdata[mkstring()<<"type"<<i] = "CNAME";
        formdata[mkstring()<<"name"<<i] = iter->name();
        formdata[mkstring()<<"param"<<i] = iter->cname();
        break;
      }
    }
  return formdata;
}

static map<string,string>
fn_debug(apr_pool_t* pool, Context* ctx,
         map<string,string>& formdata, ConfigHandler* cfg, void* data)
{
  unsigned int i;
  char* monnames[7] = { "debug", "debug0", "debug1", "debug2", "debug3",
                        "debug4", "debug5" };
  bool  levels[7] = { false, true, true, false, false, false, false };
  char* styles[7] = { "",    "",   "",   "",    "",    "",    "" };
  if(formdata["profile"] == "normal") {
    levels[0] = false;  styles[0] = "debug";
    levels[1] = true;   styles[1] = "crit"; 
    levels[2] = true;   styles[2] = "error";
    levels[3] = false;  styles[3] = "warn"; 
    levels[4] = false;  styles[4] = "info"; 
    levels[5] = false;  styles[5] = "debug";
    levels[6] = false;  styles[6] = "debug";
  } else if(formdata["profile"] == "extended") {
    levels[0] = false;  styles[0] = "debug";
    levels[1] = true;   styles[1] = "crit"; 
    levels[2] = true;   styles[2] = "error";
    levels[3] = true;   styles[3] = "warn"; 
    levels[4] = false;  styles[4] = "info"; 
    levels[5] = false;  styles[5] = "debug";
    levels[6] = false;  styles[6] = "debug";
  } else if(formdata["profile"] == "verbose") {
    levels[0] = true;  styles[0] = "debug";
    levels[1] = true;  styles[1] = "crit"; 
    levels[2] = true;  styles[2] = "error";
    levels[3] = true;  styles[3] = "warn"; 
    levels[4] = true;  styles[4] = "info"; 
    levels[5] = false; styles[5] = "debug";
    levels[6] = false; styles[6] = "debug";
  } else {
    formdata.clear();
    return formdata;
  }
  for(i=0; i<7; i++)
    if(levels[i]) {
      Filter* f = Filter::lookupFilter(ctx, "log");
      f->initialize(ctx, styles[i]);
      ctx->filter(monnames[i], f);
    } else
      ctx->filter(monnames[i], 0);
  return formdata;
}

static map<string,string>
fn_getinfo(apr_pool_t* pool, Context* ctx,
         map<string,string>& formdata, ConfigHandler* cfg, void* data)
{
  for(map<string,string>::iterator iter = formdata.begin();
      iter != formdata.end();
      ++iter)
    {
      Filter *source;
      if((source = ctx->filter(iter->second.c_str()))) {
        MonitorFilter sink(iter->first.c_str(), &formdata);
        source->pull(ctx, &sink);
      }
    }
  return formdata;
}

static map<string,string>
fn_info(apr_pool_t* pool, Context* ctx,
        map<string,string>& formdata, ConfigHandler* cfg, void* data)
{
  vector<Filter*> sources;
  OverlayContext overctx(ctx);
  for(map<string,string>::iterator iter = formdata.begin();
      iter != formdata.end();
      ++iter)
    {
      const char *s = iter->second.c_str();
      string filtertype(Filter::nextString(&s));
      if(filtertype == "info") {
        InfoFilter* filter = new InfoFilter(&formdata);
        sources.push_back(filter);
        overctx.filter(iter->first.c_str(), filter);
      } else
        overctx.filter(iter->first.c_str(),
                       Filter::lookupFilter(&overctx, filtertype.c_str()));
    }
  for(map<string,string>::iterator iter = formdata.begin();
      iter != formdata.end();
      ++iter)
    {
      const char *s = iter->second.c_str();
      Filter::nextString(&s); // skip type argument of previous cycle
      overctx.filter(iter->first.c_str())->initialize(&overctx,s);
    }
  formdata.clear();
  for(vector<Filter*>::iterator iter = sources.begin();
      iter != sources.end();
      ++iter)
    (*iter)->pull(&overctx, 0);
  return formdata;
}

static map<string,string>
fn_sections(apr_pool_t* pool, Context* ctx,
            map<string,string>& formdata, ConfigHandler* cfg, void* data)
{
  formdata["nsections"] = "0";
  ReportEvent evt(pool, ctx, formdata);
  GlobuleEvent::submit(evt);
  return evt.properties();
}

static map<string,string>
fn_section(apr_pool_t* pool, Context* ctx,
           map<string,string>& formdata, ConfigHandler* cfg, void* data)
{
  Url url(pool, formdata["url"]);
  ReportEvent evt(pool, ctx, formdata, url.uri());
  GlobuleEvent::submit(evt);
  return evt.properties();
}

static map<string,string>
fn_checkup(apr_pool_t* pool, Context* ctx,
           map<string,string>& formdata, ConfigHandler* cfg, void* data)
{
  apr_uri_t uri;
  uri.scheme   = 0;
  uri.hostname = 0;
  uri.path     = "";
  uri.query    = "check";
  uri.port     = 0;
  HttpMetaEvent evt(pool, ctx, &uri);
  GlobuleEvent::submit(evt);
  if(evt.getRequestStatusPurging())
    formdata["check"] = "0";
  else
    formdata["check"] = "1";
  return formdata;
}

/* FIXME:
 * The filterfunc is called multiple times (3) for some reason, and I have
 * now no idea why, how to prevent it, and if it is necessary.  For the
 * moment this has no serious consequences, as the current actions triggered
 * by the dochandler may be executed multiple times.  However, in future
 * this may change.
 * One effect we have to counteract, namely the problem that the first element
 * normally should not be prefixed with a ampersand.  Because we are now
 * called multiple times, we do not know it is the first element generated
 * so it is deliberate that the first bool variable is set to false.
 */
static apr_status_t
filterfunc(ap_filter_t *f, apr_bucket_brigade *bb, ap_input_mode_t mode,
           apr_read_type_e block, apr_off_t readbytes)
{
  struct workerstate_struct *state;
  apr_threadkey_private_get((void**)&state,  childstate.threadkey);
  ap_remove_input_filter(f);
  struct ConfigHandler::action* dochandler;
  dochandler = (struct ConfigHandler::action*) f->ctx;
  request_rec* r = f->r;
  map<string,string> formdata;
  if(r->method_number == M_POST || r->method_number == M_PUT) {
    const char* encoding = apr_table_get(r->headers_in,"Content-Type");
    if(encoding && !strcmp(encoding, "application/x-www-form-urlencoded"))
      getform(r, formdata);
  }
  formdata = dochandler->func(f->r->pool, state->ctx, formdata,
                              dochandler->conf, dochandler->data);
  ap_remove_input_filter(f);
  if(formdata.size() > 0) {
    r->method_number = M_POST;
    r->method = "POST";
    apr_table_unset(r->headers_in,"Content-Length");
    bool first = false; // deliberate
    for(map<string,string>::iterator iter=formdata.begin();
        iter != formdata.end();
        ++iter)
      ap_fprintf(r->input_filters, bb, "%s%s=%s",
                 (first?(first=false,""):"&"),
                 iter->first.c_str(),
                 mkstring::urlencode(iter->second.c_str()).c_str());
  }
  ap_filter_flush(bb, r->input_filters);

  return OK;
}

static int
filterinit(ap_filter_t *f)
{
  return OK;
}

ConfigHandler::ConfigHandler(Handler* parent, apr_pool_t* p, const Url& uri,
                             const char* directory, const char* secret)
  throw()
  : Handler(parent, p, uri)
{
  _password = apr_pstrdup(p, secret);
  if(!password)
    password = secret;
  if(!path)
    path = directory;

  ap_register_input_filter("GlobuleConfigFilter", filterfunc,
                           filterinit, AP_FTYPE_RESOURCE);
#define DEFFILTER(CLASSNAME,SYMBOLICNAME) { Filter *f = new CLASSNAME; Persistent::registerClass(#CLASSNAME,f); Filter::registerFilter(SYMBOLICNAME, f); }
  DEFFILTER(MonitorFilter,    "monitor");
  DEFFILTER(MemoryInfoFilter, "meminfo");
  DEFFILTER(InfoFilter,       "info");
#undef DEFFILTER

  struct {
    const char* name;
    map<string,string> (*func)(apr_pool_t*, Context*, map<string,string>&, ConfigHandler*, void*);
    void *data;
  } actions[] = {
    { "general",     fn_index,       NULL },
    { "resources",   fn_resources,   NULL },
    { "debug",       fn_debug,       NULL },
    { "filtering",   fn_filtering,   NULL },
    { "getinfo",     fn_getinfo,     NULL },
    { "info",        fn_info,        NULL },
    { "sections",    fn_sections,    NULL },
    { "section",     fn_section,     NULL },
    { "peers",       fn_section,     NULL },
    { "database",    fn_section,     NULL },
    { "namebinding", fn_namebinding, NULL },
    { "checkup",     fn_checkup,     NULL },
    { NULL, NULL, NULL }
  };
  for(unsigned int i=0; actions[i].name; i++) {
    struct action act = { this, actions[i].func, actions[i].data };
    _documents[actions[i].name] = act;
  }
  if(brokerSerial) {
    struct action gbsaction = { this, fn_gbs, 0 };
    _documents["gbs"] = gbsaction;
  }
  initialize(p, (Context*)0, (HandlerConfiguration*)0);
}

ConfigHandler::~ConfigHandler() throw()
{
}

bool
ConfigHandler::initialize(apr_pool_t* p, Context* ctx,
			  HandlerConfiguration* cfg)
  throw(SetupException)
{
  if(_initialized)
    return true;
  _initialized = true;
  return false;
}

void
ConfigHandler::flush(apr_pool_t* p) throw()
{
}


bool
ConfigHandler::handle(GlobuleEvent& evt, const string& remainder) throw()
{
  int i;
  apr_status_t status;
  switch(evt.type()) {
  case GlobuleEvent::INVALID_EVT: {
    HttpMetaEvent& ev = (HttpMetaEvent&) evt;
    char* secret;
    if(getCredentials(evt.context, ev.getRequest(), NULL, NULL, &secret) &&
       !strcmp(secret, ConfigHandler::password))
      ev.setStatus(HTTP_NO_CONTENT);
    else
      ev.setStatus(HTTP_UNAUTHORIZED);
    return true;
  }
  case GlobuleEvent::REDIRECT_EVT: {
    //RedirectEvent& ev = (RedirectEvent&)evt;
    //ev.getRequest()->filename = "/globule.html";
    //ev.setStatus(HTTP_OK);
    return true;
  };
  case GlobuleEvent::HEARTBEAT_EVT: {
    return true;
  };
  case GlobuleEvent::REQUEST_EVT: {
    HttpReqEvent& ev = (HttpReqEvent&) evt;
    request_rec* r = ev.getRequest();
    string document(remainder);
    char *suffixes[] = { "", ".php", ".cgi", ".shtml", ".html", ".htm", 0 };
    if(r && r->filename) {
      for(i=0; suffixes[i]; i++) {
        apr_finfo_t f;
        string fname = mkstring() << r->filename << suffixes[i];
        status = apr_stat(&f, fname.c_str(), APR_FINFO_TYPE, r->pool);
        if(status == APR_SUCCESS && f.filetype == APR_REG) {
          r->finfo = f;
          string::size_type pfound = string::npos, psuffix;
          if(i == 0) {
            for(i=1; suffixes[i]; i++)
              if((pfound = remainder.rfind(suffixes[i])) != string::npos)
                break;
          } else {
            pfound = remainder.rfind(suffixes[i]);
            r->filename = apr_pstrdup(r->pool, fname.c_str());
          }
          if(pfound != string::npos) {
            psuffix = remainder.length() - strlen(suffixes[i]);
            if(pfound == psuffix)
              document = remainder.substr(0, psuffix);
          }
          break;
        }
      }
      map<string,struct action>::iterator dochandler=_documents.find(document);
      while(dochandler == _documents.end() && document.rfind("-") != string::npos) {
        document = document.substr(0, document.rfind("-"));
        dochandler = _documents.find(document);
      }
      if(dochandler != _documents.end()) {
        if(suffixes[i]) { // document was found
          if(r->method_number == M_POST || r->method_number == M_PUT) {
            struct action *act = &dochandler->second;
            ap_add_input_filter("GlobuleConfigFilter", act, r, r->connection);
            /* Let apache deliver the file we had detected earlier, so do not
             * return a status through ev.setStatus()
             */
          } else {
            map<string,string> formdata;
            if(r->args)
              getform(apr_pstrdup(r->pool, r->args), formdata);
            formdata = dochandler->second.func(r->pool, evt.context, formdata,
                                               dochandler->second.conf,
                                               dochandler->second.data);
            mkstring args;
            bool first = true;
            for(map<string,string>::iterator iter=formdata.begin();
                iter != formdata.end();
                ++iter)
              args << (first?first=false,"":"&")
                   << iter->first.c_str()
                   << "="
                   << mkstring::urlencode(iter->second.c_str());
            string argsstr(args.str());
            r->args = apr_pstrdup(evt.pool, argsstr.c_str());
          }
        } else {
          /* Auto-generate result and return data. */
          map<string,string> formdata;
          if(r->method_number == M_POST || r->method_number == M_PUT) {
            const char* encoding = apr_table_get(r->headers_in,"Content-Type");
            if(encoding &&
               !strcmp(encoding,"application/x-www-form-urlencoded"))
              getform(r, formdata);
          } else
            if(r->args)
              getform(apr_pstrdup(r->pool, r->args), formdata);
          formdata = dochandler->second.func(r->pool, evt.context, formdata,
                                             dochandler->second.conf,
                                             dochandler->second.data);
          ev.setStatus(HTTP_OK);
          if(formdata.size() != 1 || formdata.begin()->first != "") {
            bool first = true;
            for(map<string,string>::iterator iter=formdata.begin();
                iter != formdata.end();
                ++iter)
              ap_rprintf(r, "%s%s=%s", (first?first=false,"":"&"),
                         iter->first.c_str(), iter->second.c_str());
          } else {
            ap_set_content_type(r, "text/plain");
            ap_rprintf(r, "%s\n", formdata.begin()->second.c_str());
          }
          return true;
        }
      }
      return true;
    } else
      return false;
  };
  default:
    ; // log some error
  }
  return false;
}

string
ConfigHandler::description() const throw()
{
  return "ConfigHandler()";
}
