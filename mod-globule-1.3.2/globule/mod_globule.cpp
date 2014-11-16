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
/* We need to know the main DocumentRoot path, therefor we need to access the
 * core Apache module, which can only be done if we pretend we are part of
 * the core software.
 */
#define CORE_PRIVATE

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/* First include the config and undef designated initialized symbol, because
 * although our compiler may be able to compile the directive command table,
 * the C++ compiler will probably not be able to do this.
 */
#include <ap_config.h>
#undef AP_HAVE_DESIGNATED_INITIALIZER

#include <httpd.h>
extern "C" {
#ifndef WIN32
#include <unixd.h>
#endif /* !WIN32 */
};
#include <http_config.h>
#include <string>
#include <map>
#include <vector>
#include <fstream>
#include <http_log.h>
#include <http_core.h>
#include <http_protocol.h>
#include <http_connection.h>
#include <http_request.h>
#include <http_core.h>
#include <http_main.h>
#include <apr_strings.h>
#include <apr_tables.h>
#include <apr_lib.h>
#include <apr_general.h>
#include "utilities.h"
#include "redirect/mod_netairt.h"
#include "redirect/dns_comm.h"
#include "redirect/dns_config.h"
#include "redirect/dns_policy.hpp"
#include "redirect/dns_policy_rr.hpp"
#include "redirect/dns_policy_as.hpp"
#include "redirect/dns_policy_wrr.hpp"
#include "redirect/dns_policy_wrand.hpp"
#include "redirect/dns_policy_balanced.hpp"
#include "alloc/Allocator.hpp"
#include "locking.hpp"
#include "event/EventMgr.hpp"
#include "event/HttpReqEvent.hpp"
#include "event/HttpMetaEvent.hpp"
#include "event/HttpLogEvent.hpp"
#include "event/RedirectEvent.hpp"
#include "event/PostRequestEvent.hpp"
#include "event/RegisterEvent.hpp"
#include "policy/ReplPolicy.hpp"
#include "resource/RedirectorHandler.hpp"
#include "resource/ConfigHandler.hpp"
#include "resource/OriginHandler.hpp"
#include "resource/ProxyHandler.hpp"
#include "resource/ImportHandler.hpp"
#include "resource/KeeperHandler.hpp"
#include "resource/BrokeredHandler.hpp"
#include "resource/DatabaseHandler.hpp"
#include "resource/NameBindingHandler.hpp"
#include "policy/SpecialPolicy.hpp"
#include "policy/GlobeCBPolicy.hpp"
#include "policy/InvalidatePolicy.hpp"
#include "policy/PureProxyPolicy.hpp"
#include "policy/MirrorNoConsPolicy.hpp"
#include "policy/AlexPolicy.hpp"
#include "policy/TtlPolicy.hpp"
#include "resources.hpp"
#include "heartbeat.hpp"
#include "filemonitor.hpp"
#include "configuration.hpp"
#include "FilterConfiguration.hpp"

using std::string;
using std::vector;

#define CFG_DEFAULT_DNS_PORT 53
#define CFG_DEFAULT_DNS_HOST "0.0.0.0"
#define CFG_DEFAULT_TTL (DNS_TTL_DAY)
#define CFG_DEFAULT_TTL_RR (30*DNS_TTL_MINUTE)
#define CFG_DEFAULT_TTL_AS (10*DNS_TTL_MINUTE)
#define CFG_DEFAULT_NS_TTL DNS_TTL_HOUR
#define CFG_DEFAULT_MAX_SITES 10         // warning! real low!
#define CFG_DEFAULT_MAX_REPLICAS 32
#define CFG_DEFAULT_IPCOUNT 3
#define CFG_DEFAULT_BGP_REFRESH DNS_TTL_DAY
#ifdef WIN32
#define CFG_DEFAULT_BGP_FILE_NAME "NUL"
#else
#define CFG_DEFAULT_BGP_FILE_NAME "/dev/null"
#endif
#define CFG_DEFAULT_RESET_ON_RESTART 1

extern "C" {
extern module AP_MODULE_DECLARE_DATA globule_module;
};

extern "C" {
struct sharedstate_struct *sharedstate;
struct childstate_struct childstate;
};
static const char* servername;
static apr_array_header_t* filtermodules;
static vector<DNSRecord> cfgnamebindings;

enum tristate { disabled=0, enabled=1, unset=-1 };
enum sectiontype {
  PLAINSECTION=0, CONFIGSECTION, BROKEREDSECTION, DATABASESECTION,
  EXPORTSECTION, PROXYSECTION, IMPORTSECTION, KEEPERSECTION, REDIRECTSECTION
};
char* sectionname[] = {
  "plain", "config", "brokered", "database", "export", "import", "keeper",
  "redirector"
};
struct section_struct {
  enum sectiontype type;
  struct section_struct *next;
  struct configuration *conf;
  server_rec* srv;
  char *servername; // as also specified by server_rec->server_hostname, but we're not allowed to copy that
  apr_port_t portnum;
  BaseHandler* handler;
  char *upstream;
  char *password;
  apr_uint16_t weight;
};
struct configuration {
  tristate exported;
  tristate imported;
  bool enabled;
  char *path;
  char *directory;
  int reevaluateinterval;
  bool reevaluateinterval_set;
  int reevaluateadaptinterval;
  bool reevaluateadaptinterval_set;
  int reevaluatecontentinterval;
  bool reevaluatecontentinterval_set;
  apr_off_t quotadiskspace;
  bool quotadiskspace_set;
  apr_off_t quotadocuments;
  bool quotadocuments_set;
  vector<Contact>* contacts;
  // redirection
  bool redirect_mode_set;
  int redirect_mode;
  bool redirect_policy_set;
  char *redirect_policy;
  bool replication_policy_set;
  const char *replication_policy;
  const char *fancyservername;
  struct section_struct *section; // sections should reside in shared memory
};
static struct section_struct *sections = 0;

static int shmem_sz = 8*1024*1024;
dns_config *globule_dns_config = 0;
static apr_uri_t configpath;
static char* configpass;

#ifdef PSODIUM
extern "C" {
  const char *intraserverauthpwd; /* password for Globule-pSodium authen. */
};
#endif

static struct workerstate_struct*
initworker(apr_pool_t* p)
{
  apr_status_t status;
  struct workerstate_struct *state;
  status = apr_threadkey_private_get((void**)&state,  childstate.threadkey);
  if(status != APR_SUCCESS) {
    ap_log_perror(APLOG_MARK, APLOG_CRIT, status, p,
                  "cannot get threadkey private data");
  }
  if(!state) {
    state = new struct workerstate_struct; // cannot be allocated from r->pool
    state->ctx = new Context(&sharedstate->monctxref, childstate.lock);
    new(&state->queuedEvents) std::queue<GlobuleEvent*>;
    state->ctx->update();
    
    status = apr_threadkey_private_set(state, childstate.threadkey);
    if(status != APR_SUCCESS) {
      ap_log_perror(APLOG_MARK, APLOG_CRIT, status, p,
                    "cannot set threadkey private data");
    }
  }
  return state;
}

struct translations { char* s; apr_int64_t v; };
static const struct translations translations_bytes[] = {
  { "kb",                 1024LL },
  { "kbyte",              1024LL },
  { "kbytes",             1024LL },
  { "kilobyte",           1024LL },
  { "kilobytes",          1024LL },
  { "kibibyte",           1024LL },
  { "kibibytes",          1024LL },
  { "mb",              1048576LL },
  { "mbyte",           1048576LL },
  { "mbytes",          1048576LL },
  { "megabyte",        1048576LL },
  { "megabytes",       1048576LL },
  { "mebibyte",        1048576LL },
  { "mebibytes",       1048576LL },
  { "gb",           1073741824LL },
  { "gbyte",        1073741824LL },
  { "gbytes",       1073741824LL },
  { "gigabyte",     1073741824LL },
  { "gigabytes",    1073741824LL },
  { "gibibyte",     1073741824LL },
  { "gibibytes",    1073741824LL },
  { "tb",        1099511627776LL },
  { "tbyte",     1099511627776LL },
  { "tbytes",    1099511627776LL },
  { "terabyte",  1099511627776LL },
  { "terabytes", 1099511627776LL },
  { "tibibyte",  1099511627776LL },
  { "tibibytes", 1099511627776LL },
  { NULL, 0 }
};
static const struct translations translations_time[] = {
  { "never",          0L },
  { "usec",           1L },
  { "usecs",          1L },
  { "microsecond",    1L },
  { "microseconds",   1L },
  { "msec",           1L },
  { "msecs",          1L },
  { "millisecond",    1L },
  { "milliseconds",   1L },
  { "s",              1L },
  { "sec",            1L },
  { "secs",           1L },
  { "second",         1L },
  { "seconds",        1L },
  { "m",             60L },
  { "min",           60L },
  { "mins",          60L },
  { "minut",         60L },
  { "minutes",       60L },
  { "h",           3600L },
  { "hour",        3600L },
  { "hours",       3600L },
  { "d",          86400L },
  { "day",        86400L },
  { "days",       86400L },
  { "w",         604800L },
  { "week",      604800L },
  { "weeks",     604800L },
  { NULL, 0 }
};

static const char *
getvalue(cmd_parms *cmd, const char *param, apr_int64_t *valueptr,
         const struct translations *table)
{
  char *endptr;
  int i, len;
  apr_int64_t value;
  value = strtol(param, &endptr, 0);
#ifdef NOTDEFINED
  if(endptr == param)
    return "unspecified number";
#else
  if(endptr == param)
    value = 1;
#endif
  while(apr_isspace(*endptr))
    ++endptr;
  for(len=0; apr_isalpha(endptr[len]); len++)
    ;
  if(len > 0) {
    if(table) {
      for(i=0; table[i].s; i++)
        if(!strncasecmp(endptr, table[i].s, len))
          break;
      if(table[i].s)
        value *= table[i].v;
      else
        return "unrecognized index for parameter";
    } else
      return "parameter cannot be indexed";
  }
  *valueptr = value;
  return NULL;
}

static int
inner_handler(Context* ctx, request_rec *r)
{
  int status;
  struct configuration *cfg;
  cfg = (struct configuration *) ap_get_module_config(r->per_dir_config,
                                                      &globule_module);
  DIAG(ctx,(MONITOR(detail)),("received %s request to %s parsed to %s://%s:%s%s%s%s\n",r->method, r->uri,r->parsed_uri.scheme,r->parsed_uri.hostname,(r->parsed_uri.port_str?r->parsed_uri.port_str:"-"),r->parsed_uri.path, (r->args?"?":""), (r->args?r->args:"")));

  if(cfg && cfg->section &&
     (!strcasecmp(r->method,"SIGNAL") || !strcasecmp(r->method,"REPORT") ||
      (cfg->section->type == CONFIGSECTION ||
       cfg->section->type == BROKEREDSECTION ||
       cfg->section->type == DATABASESECTION ||
       cfg->section->type == PROXYSECTION ||
       (cfg->section->type == EXPORTSECTION   && cfg->exported == enabled) ||
       (cfg->section->type == IMPORTSECTION   && cfg->imported == enabled) ||
       (cfg->section->type == KEEPERSECTION   && cfg->imported == enabled) ||
       (cfg->section->type == REDIRECTSECTION && cfg->imported == enabled)))) {
    apr_table_add(r->headers_out, "X-Globule-Trace", "globule");
    DIAG(ctx,(MONITOR(servedby)),("%s",cfg->fancyservername?cfg->fancyservername:servername));
    if(r->method_number == M_OPTIONS) {
      if((status = ap_discard_request_body(r)) != OK)
        return status;
      ap_allow_standard_methods(r, REPLACE_ALLOW, M_GET,M_POST,-1);
      ap_allow_methods(r, MERGE_ALLOW, "REPORT", "SIGNAL", NULL);
      return OK;
    } else if (!strcasecmp(r->method, "REPORT")) {
      HttpLogEvent ev(r->pool, ctx, r);
      return GlobuleEvent::submit(ev,cfg->section->handler) && ev.completed() ? OK : DECLINED;
    } else if (!strcasecmp(r->method, "SIGNAL")) {
      HttpMetaEvent ev(r->pool, ctx, r);
      return GlobuleEvent::submit(ev,cfg->section->handler) && ev.completed() ? OK : DECLINED;
    } else if(r->method_number == M_GET ||
              r->method_number == M_POST ||
              r->method_number == M_PUT) {
      HttpReqEvent ev(r->pool, ctx, r);
      // FIXME: if an internal error occurs, GlobuleEvent::submit should
      // throw an exception and the status should be set to
      // HTTP_INTERNAL_SERVER_ERROR
      return GlobuleEvent::submit(ev,cfg->section->handler) && ev.completed() ? (r->status >= 200 && r->status < 300 ? OK : r->status ) : DECLINED;
    } else
      return DECLINED;
  } else
    return DECLINED;
}

static int
handler(request_rec *r)
{
  int rtncode;
  if(r->proxyreq && (r->method || strcasecmp(r->method, "SIGNAL")))
    return DECLINED;

  struct workerstate_struct* state = initworker(r->pool);
  state->ctx->request(r);
  rtncode = inner_handler(state->ctx, r);
  state->ctx->request(0);
  state->ctx->update();
  return rtncode;
}

static int
logging(request_rec *r)
{
  struct configuration *cfg;
  cfg = (struct configuration*) ap_get_module_config(r->per_dir_config,
                                                     &globule_module);
#ifdef DEBUG
  ExceptionHandler exhandler(r->pool);
#endif
  struct workerstate_struct* state = initworker(r->pool);
  state->ctx->request(r);

  while(!state->queuedEvents.empty()) {
    GlobuleEvent* evt = state->queuedEvents.front();
    state->queuedEvents.pop();
    evt->dispatch();
    delete evt;
  }

  if(r->method && (!strcasecmp(r->method,"SIGNAL") ||
                   !strcasecmp(r->method,"REPORT")))
    return OK;

  PostRequestEvent ev(r->pool, state->ctx, r);
  GlobuleEvent::submit(ev, cfg->section->handler);

  /* Run the queued events sequence a second time, as the PostRequestEvent
   * may have queued more events.  We cannot handle the events all at this
   * point, as some events should be handled before runing PostRequestEvent.
   */
  while(!state->queuedEvents.empty()) {
    GlobuleEvent* evt = state->queuedEvents.front();
    state->queuedEvents.pop();
    evt->dispatch();
    delete evt;
  }

  state->ctx->request(0);
  state->ctx->update();
  if(dns_http_cleanup(r) != OK)
    return DECLINED;
  return OK;
}

static int
redirect(request_rec* r)
{
  if(!strcasecmp(r->method,"SIGNAL") || !strcasecmp(r->method,"REPORT")) {
    r->filename = "humor-apache-with-some-filename";
    return OK;
  }
  struct configuration *cfg;
  cfg = (struct configuration*) ap_get_module_config(r->per_dir_config,
                                                     &globule_module);
#ifdef DEBUG
  ExceptionHandler exhandler(r->pool);
#endif
  struct workerstate_struct* state = initworker(r->pool);
  state->ctx->request(r);
  RedirectEvent ev(r->pool, state->ctx, r);
  if(cfg->exported && GlobuleEvent::submit(ev, cfg->section->handler)) {
    if(ev.completed()) {
      return (r->status != HTTP_OK ? r->status : OK);
    } else {
      return DECLINED;
    }
  } else {
    return DECLINED;
  }
}

static int
fixup(request_rec* r)
{
  /* This forces that replica servers which access a directory index through
   * an URI which ends with a slash always get a redirection iso. the content.
   */
  if(r->main && r->main->uri[strlen(r->main->uri)-1] == '/' &&
     apr_table_get(r->main->headers_in,"X-From-Replica") &&
     r->finfo.filetype == APR_REG) {
    char* location = ap_escape_uri(r->pool,r->uri);
    /* The Location header must be an absolute URL, which is a problem, since
     * with DNS redirection, the exported URL seen be users must be based
     * on the generic URL, which is actually the hostname in the ServerAlias.
     */
    location = apr_pstrcat(r->pool, (r->main->parsed_uri.scheme?r->main->parsed_uri.scheme:"http"), "://",
                           (r->main->server->names && r->main->server->names->nelts > 0
                                                 ? ((const char**)r->main->server->names->elts)[0]
                                                 : r->main->parsed_uri.hostname),
                           (r->main->parsed_uri.port_str && *r->main->parsed_uri.port_str ? ":" : ""),
                           (r->main->parsed_uri.port_str && *r->main->parsed_uri.port_str ? r->main->parsed_uri.port_str : ""),
                           location, NULL);
    if(r->args)
      location = apr_pstrcat(r->pool,location,"?",r->args,NULL);
    apr_table_setn(r->headers_out,"Location",location);
    return r->status = HTTP_MOVED_PERMANENTLY;
  }
  return DECLINED;
}

static apr_status_t
cleanup(void *data)
{
  apr_pool_t* pool = (apr_pool_t*)data;
  GlobuleEvent::disposeMainHandler(pool);
  return APR_SUCCESS;
}

static int
postconfig(apr_pool_t *pconf, apr_pool_t *plog, apr_pool_t *ptemp, server_rec *s)
{
  void *flag;
  apr_status_t status;
  char *errmsg;
  const char *userdata_key = "mod_globule_init";

#ifdef WIN32
  win_open_stderr(plog);
#endif /* WIN32 */

#ifdef DEBUG
  ExceptionHandler exhandler(plog);
#endif

  ap_add_version_component(pconf, "Globule/" VERSION);

  if(configpath.path == NULL) {
    ap_log_perror(APLOG_MARK, APLOG_EMERG, APR_SUCCESS, plog,
                  "required directive GlobuleAdminURL not set");
    return DONE;
  }

  // This MUST be done both in the Windows Parent Process AND in the Child
  // process.
  dns_post_config(pconf, plog, ptemp, s);

#ifdef WIN32
  /* Do not run post_config in the Windows parent process, it is also run in
   * the child process and we don't want to do double work.  The envar
   * AP_PARENT_PID is set in the env list (by mpm_winnt) for the child
   * process.  **WARNING**: This check is not as robust as I would like
   * because the name of this envar is subject to change. Apache2 needs
   * something like ap_mpm_query(AP_MPMQ_IS_PARENT,&rc);
   * http://www.mail-archive.com/dev@perl.apache.org/msg06600.html
   */
  if(!getenv("AP_PARENT_PID"))
    return OK;
#endif /* WIN32 */

  apr_pool_userdata_get(&flag, userdata_key, s->process->pool);
  if(!flag) {
    apr_pool_userdata_set((const void *)1, userdata_key,
    apr_pool_cleanup_null, s->process->pool);
    return OK;
  }

  try {

    servername = apr_psprintf(pconf,"%s:%d", s->server_hostname, s->port);
    rmmmemory::shm(pconf, shmem_sz, sizeof(struct sharedstate_struct), (void**)&sharedstate);
    try {
      Lock::initialize(pconf);
    } catch(ResourceError ex) {
      ap_log_error(APLOG_MARK, APLOG_EMERG, APR_SUCCESS, s, "Resources problem#1%s%s",
                   (ex.message()?": ":""), (ex.message()?ex.message():""));
      return DONE;
    } catch(ResourceException ex) {
      ap_log_error(APLOG_MARK, APLOG_ERR, APR_SUCCESS, s, "Resources problem%s%s",
                   (ex.message()?": ":""), (ex.message()?ex.message():""));
    }

    childstate.lock_fname = Lock::mklockfname(pconf);
    if((status = apr_global_mutex_create(&childstate.lock, childstate.lock_fname,
                                         APR_LOCK_DEFAULT, pconf))) {
      ap_log_error(APLOG_MARK, APLOG_EMERG, status, s,
                   "Cannot create monitor global lock");
      return DONE;
    }
    new(&sharedstate->monctxref) ShmSharedStorageSpace;
    Context::initialize();
    Context* ctx = new Context(&sharedstate->monctxref, childstate.lock);

    if(apr_is_empty_array(filtermodules)) {
      char **defaultmodule = (char**) apr_array_push(filtermodules);
      defaultmodule[0] = "defaults";
      defaultmodule[1] = "";
    }

    FilterConfiguration* filtercfg = FilterConfiguration::loaddefault(ptemp);
    char** filtermodule;
    while((filtermodule = (char**)apr_array_pop(filtermodules))) {
      string modname = mkstring() << "%" << filtermodule[0];
      filtercfg->add(new(apr_palloc(ptemp,sizeof(FilterConfiguration))) FilterConfiguration(FilterConfiguration::INSTANCE, modname.c_str(), filtermodule[0]));
    }
    apr_array_header_t* pass1 = filtercfg->evaluate(ptemp);
    apr_array_header_t* pass2 = apr_array_make(ptemp, 0, sizeof(char*[2]));
    char** filterdef;
    while((filterdef = (char**)apr_array_pop(pass1))) {
      ctx->filter(filterdef[0], Filter::lookupFilter(ctx, filterdef[1]));
      char** pusheddef = (char**)apr_array_push(pass2);
      pusheddef[0] = filterdef[0];
      pusheddef[1] = filterdef[2];
    }
    while((filterdef = (char**)apr_array_pop(pass2))) {
      Filter* filterptr = ctx->filter(filterdef[0]);
      if(filterptr)
        filterptr->initialize(ctx, filterdef[1]);
    }

    ctx->update();

    string selfurl = mkstring()<< "http://" << servername << "/?heartbeat";
    Url selfcontact(pconf, selfurl.c_str());

    RootHandler *root = new(rmmmemory::shm()) RootHandler(pconf,selfcontact);
    GlobuleEvent::registerMainHandler(root);

    {
      GlobuleEvent::hbreceiver = new(rmmmemory::shm()) HeartBeat(root,pconf,selfcontact);
      RegisterEvent evt(pconf, GlobuleEvent::hbreceiver);
      GlobuleEvent::submit(evt);
    }
    {
      GlobuleEvent::fmreceiver = new(rmmmemory::shm()) FileMonitor(root,pconf,selfcontact);
      RegisterEvent evt(pconf, GlobuleEvent::fmreceiver);
      GlobuleEvent::submit(evt);
    }

    {
      core_server_config *corecfg = (core_server_config*)
        ap_get_module_config(s->module_config, &core_module);
      ap_assert(corecfg);
      char *path = configpath.path;
      if(path) {
        while(*path && *path=='/')
          ++path;
        if((status = apr_filepath_merge(&path, corecfg->ap_document_root, path,
                                        0, pconf))) {
          ap_log_error(APLOG_MARK, APLOG_ERR, status, s,
                       "Bad filepath %s appended with %s",
                       corecfg->ap_document_root, configpath.path);
          return DONE;
        }
        path = apr_pstrdup(pconf, path);
      } else
        path = apr_pstrdup(pconf, corecfg->ap_document_root);
      Url url(ptemp,&configpath);
      RegisterEvent registerevt(pconf, new(rmmmemory::shm())
                                ConfigHandler(root, ptemp, url, path, configpass));
      GlobuleEvent::submit(registerevt);
    }

#ifndef WIN32  
    if((status = unixd_set_global_mutex_perms(childstate.lock)) != APR_SUCCESS) {
      ap_log_error(APLOG_MARK, APLOG_CRIT, status, s,
                   "Error setting permissions on Monitor lock");
      return DONE;
    }
#endif /* !WIN32 */
  
    Persistent::registerClass("Event",                          new Event((apr_int64_t)0));
    Persistent::registerClass("ResourcesRecord",                new(rmmmemory::shm()) ResourcesRecord());
    Persistent::registerClass("SpecialPolicy",                  new(rmmmemory::shm()) SpecialPolicy());
    Persistent::registerClass("GlobeCBPolicy",                  new(rmmmemory::shm()) GlobeCBPolicy());
    Persistent::registerClass("InvalidatePolicy",               new(rmmmemory::shm()) InvalidatePolicy());
    Persistent::registerClass("PureProxyPolicy",                new(rmmmemory::shm()) PureProxyPolicy());
    Persistent::registerClass("MirrorNoConsPolicy",             new(rmmmemory::shm()) MirrorNoConsPolicy());
    Persistent::registerClass("AlexPolicy",                     new(rmmmemory::shm()) AlexPolicy());
    Persistent::registerClass("TtlPolicy",                      new(rmmmemory::shm()) TtlPolicy());
    Persistent::registerClass("StaticRedirectPolicy",           new(rmmmemory::shm()) StaticRedirectPolicy(globule_dns_config->ttl_none));
    Persistent::registerClass("RoundRobinRedirectPolicy",       new(rmmmemory::shm()) RoundRobinRedirectPolicy(globule_dns_config->ttl_rr));
    RedirectPolicy::registerPolicy("WeightedRoundRobinRedirectPolicy", new(rmmmemory::shm()) WeightedRoundRobinRedirectPolicy(globule_dns_config->ttl_rr));
    RedirectPolicy::registerPolicy("WeightedRandomRedirectPolicy", new(rmmmemory::shm()) WeightedRandomRedirectPolicy(globule_dns_config->ttl_rr));
    Persistent::registerClass("AutonomousSystemRedirectPolicy", new(rmmmemory::shm()) AutonomousSystemRedirectPolicy(globule_dns_config->ttl_as));
    Persistent::registerClass("BalancedRedirectPolicy",         new(rmmmemory::shm()) BalancedRedirectPolicy(globule_dns_config->ttl_as));
    ReplPolicy::registerPolicy("Special",                       new(rmmmemory::shm()) SpecialPolicy());
    ReplPolicy::registerPolicy("GlobeCB",                       new(rmmmemory::shm()) GlobeCBPolicy());
    ReplPolicy::registerPolicy("Invalidate",                    new(rmmmemory::shm()) InvalidatePolicy());
    ReplPolicy::registerPolicy("PureProxy",                     new(rmmmemory::shm()) PureProxyPolicy());
    ReplPolicy::registerPolicy("MirrorNoCons",                  new(rmmmemory::shm()) MirrorNoConsPolicy());
    ReplPolicy::registerPolicy("Alex",                          new(rmmmemory::shm()) AlexPolicy());
    ReplPolicy::registerPolicy("Ttl",                           new(rmmmemory::shm()) TtlPolicy());
    RedirectPolicy::registerPolicy("static",                    new(rmmmemory::shm()) StaticRedirectPolicy(globule_dns_config->ttl_none));
    RedirectPolicy::registerPolicy("RR",                        new(rmmmemory::shm()) RoundRobinRedirectPolicy(globule_dns_config->ttl_rr));
    RedirectPolicy::registerPolicy("WRR",                       new(rmmmemory::shm()) WeightedRoundRobinRedirectPolicy(globule_dns_config->ttl_rr));
    RedirectPolicy::registerPolicy("WRAND",                     new(rmmmemory::shm()) WeightedRandomRedirectPolicy(globule_dns_config->ttl_rr));
    RedirectPolicy::registerPolicy("AS",                        new(rmmmemory::shm()) AutonomousSystemRedirectPolicy(globule_dns_config->ttl_as));
    RedirectPolicy::registerPolicy("BAS",                       new(rmmmemory::shm()) BalancedRedirectPolicy(globule_dns_config->ttl_as));
    if((errmsg = RedirectPolicy::initializeAll(globule_dns_config, pconf))) {
      ap_log_error(APLOG_MARK, APLOG_CRIT, OK, s,
                   "Globule redirector failed initializing policies: %s",
                   errmsg);
      return DONE;
    }

    ap_log_error(APLOG_MARK, APLOG_NOTICE, APR_SUCCESS, s,
                 "Globule/" VERSION " warming up");
#ifdef DNS_REDIRECTION
    {
      NameBindingHandler::dnsdb = new(rmmmemory::shm()) gvector<gDNSRecord>;
      Url dummy(ptemp, "http://localhost/");
      RegisterEvent evt(pconf, new(rmmmemory::shm())
                        NameBindingHandler(root, ptemp, dummy, cfgnamebindings));
      GlobuleEvent::submit(evt);
    }
#endif
    for(struct section_struct* sec=sections; sec; sec=sec->next) {
      if(sec->type == PLAINSECTION)
        continue;
      if(sec->type != PLAINSECTION && sec->type != CONFIGSECTION && sec->conf->directory == NULL) {
        core_server_config *corecfg = (core_server_config*)
          ap_get_module_config(sec->srv->module_config, &core_module);
        ap_assert(corecfg);
        char *path = sec->conf->path;
        if(path) {
          while(*path && *path=='/')
            ++path;
          if((status = apr_filepath_merge(&path, corecfg->ap_document_root, path,
                                          0, pconf))) {
            ap_log_error(APLOG_MARK, APLOG_ERR, status, s,
                         "Bad filepath %s appended with %s",
                         corecfg->ap_document_root, sec->conf->path);
            return DONE;
          }
          sec->conf->directory = apr_pstrdup(pconf, path);
        } else
          sec->conf->directory = apr_pstrdup(pconf, corecfg->ap_document_root);
      }
      switch(sec->type) {
      case PLAINSECTION:
      case CONFIGSECTION:
        break;
      case REDIRECTSECTION:
      case EXPORTSECTION:
      case PROXYSECTION:
      case IMPORTSECTION:
      case KEEPERSECTION:
      case BROKEREDSECTION:
      case DATABASESECTION: {
        try {
          Url location(pconf, "http", sec->servername, sec->portnum, sec->conf->path);
          try {
            switch(sec->type) {
            case PLAINSECTION:
            case CONFIGSECTION:
              ap_assert(!"Impossible program state");
              break;
            case REDIRECTSECTION:
              ap_log_error(APLOG_MARK, APLOG_NOTICE, APR_SUCCESS, s, "Globule redirector section at url %s",location(ptemp));
              sec->handler = new(rmmmemory::shm()) RedirectorHandler(root, pconf, ctx, location, sec->conf->directory, sec->upstream, sec->password);
              break;
            case EXPORTSECTION:
              ap_log_error(APLOG_MARK, APLOG_NOTICE, APR_SUCCESS, s, "Globule export section at url %s",location(ptemp));
              sec->handler = new(rmmmemory::shm()) OriginHandler(root, pconf, ctx, location, sec->conf->directory, sec->upstream, sec->password);
              break;
            case PROXYSECTION:
              ap_log_error(APLOG_MARK, APLOG_NOTICE, APR_SUCCESS, s, "Globule proxy section at url %s",location(ptemp));
              sec->handler = new(rmmmemory::shm()) ProxyHandler(root, pconf, ctx, location, sec->conf->directory, sec->upstream);
              break;
            case IMPORTSECTION:
              ap_log_error(APLOG_MARK, APLOG_NOTICE, APR_SUCCESS, s, "Globule import section at url %s",location(ptemp));
              sec->handler = new(rmmmemory::shm()) ImportHandler(root, pconf, ctx, location, sec->conf->directory, sec->upstream, sec->password);
              break;
            case KEEPERSECTION:
              ap_log_error(APLOG_MARK, APLOG_NOTICE, APR_SUCCESS, s, "Globule keeper section at url %s",location(ptemp));
              sec->handler = new(rmmmemory::shm()) KeeperHandler(root, pconf, ctx, location, sec->conf->directory, sec->upstream, sec->password);
              break;
            case BROKEREDSECTION:
              ap_log_error(APLOG_MARK, APLOG_NOTICE, APR_SUCCESS, s, "Globule brokered section at url %s",location(ptemp));
              sec->handler = new(rmmmemory::shm()) BrokeredHandler(root, pconf, ctx, location, sec->conf->directory, sec->upstream, sec->password);
              break;
            case DATABASESECTION:
              ap_log_error(APLOG_MARK, APLOG_NOTICE, APR_SUCCESS, s, "Globule database section at url %s",location(ptemp));
              sec->handler = new(rmmmemory::shm()) DatabaseHandler(root, pconf, ctx, location, sec->conf->directory, sec->upstream, sec->password);
              break;
            }
            RegisterEvent registerevt(pconf, sec->handler);
            GlobuleEvent::submit(registerevt);
            if(sec->srv->names)
              sec->handler->initialize(pconf, sec->srv->names->nelts, (const char**)sec->srv->names->elts);
          } catch(SetupException ex) {
            ap_log_error(APLOG_MARK, APLOG_ERR, APR_SUCCESS, s,
                         "Creating Globule enabled section at url %s failed: %s",
                         location(ptemp), ex.getMessage().c_str());
          }
        } catch(UrlException ex) {
          ap_log_error(APLOG_MARK, APLOG_ERR, APR_SUCCESS, s,
                       "Bad url location http://%s:%d%s: %s",
                       sec->servername, sec->portnum,
                       sec->conf->path, ex.getMessage().c_str());
        }
      };
      }
    }

  } catch(ResourceError ex) {
    ap_log_error(APLOG_MARK, APLOG_ERR, APR_SUCCESS, s, "Resource error%s%s",
                 (ex.message()?": ":""), (ex.message()?ex.message():"3"));
    return DONE;
  }

  // FIXME: dispose of ctx.
  apr_pool_cleanup_register(pconf, pconf, cleanup, apr_pool_cleanup_null);
  return OK;
}

static void
initchild(apr_pool_t *child_pool, server_rec *s)
{
  apr_status_t status;

  if((status = apr_threadkey_private_create(&childstate.threadkey, 0, child_pool))) {
    ap_log_perror(APLOG_MARK, APLOG_CRIT, status, child_pool,
                  "cannot initialize thread private key");
  }
  initworker(child_pool);
  if((status = apr_global_mutex_child_init(&childstate.lock, childstate.lock_fname, child_pool))) {
    ap_log_perror(APLOG_MARK, APLOG_CRIT, status, child_pool,
                  "cannot reinitialize monitors global lock");
  }

  rmmmemory::childinit(child_pool);
  Lock::initialize(child_pool);
}

const char *
cmdRedirectionMode(cmd_parms *cmd, void *mconfig, const char *param)
{
  struct configuration *cfg = (struct configuration *) mconfig;
#ifdef DNS_REDIRECTION
  apr_pool_t * ppool = cmd->server->process->pool;
  ap_listen_rec * lr;
#endif

  cfg->redirect_mode_set = true;
  if(!strcasecmp(param,"OFF")) {
    cfg->redirect_mode = CFG_MODE_OFF;
    return NULL;
  } else if(!strcasecmp(param,"HTTP")) {
    cfg->redirect_mode = CFG_MODE_HTTP;
  } else if(!strcasecmp(param,"DNS")) {
    cfg->redirect_mode = CFG_MODE_DNS;
  } else if(!strcasecmp(param,"BOTH")) {
    cfg->redirect_mode = CFG_MODE_BOTH;
  } else
    return "DNS_Mode must be either `DNS', `HTTP', `BOTH' or `OFF'.";

#ifndef DNS_REDIRECTION    
  if(cfg->redirect_mode & CFG_MODE_DNS)
    return "DNS mode can only be used when a modified Apache is used and "
      "globule is compiled with DNS_REDIRECTION defined (see config.h)";
  return NULL;
#else
  if(!(cfg->redirect_mode & CFG_MODE_DNS) || cmd->server->is_virtual)
    /* Code below is not meant for you */
    return NULL;
  globule_dns_config->dns_redirection = 1;

  // Remember: no actual work here, do in dns_endconf!
  // Berry: WRONG! this needs to be done here, before end_conf/post_conf,
  //   otherwise Apache doesn't know about the right port to listen to, see
  //   Michal's msc thesis.
  if(!dns_comm_alloc_listener(ppool, globule_dns_config->host,
                              globule_dns_config->port, SOCK_STREAM))
    return "Cannot allocate DNS/TCP listener";
  if(!(lr = dns_comm_alloc_listener(ppool, globule_dns_config->host,
                                    globule_dns_config->port, SOCK_DGRAM)))
    return "Cannot allocate DNS/UDP listener";
  /* assigments done here, and not in the dns_comm module, due to the
   * problems with function-from-table calls on some un*xes
   */
  lr->accept_func = dns_comm_accept_udp;
  lr->process_func = dns_comm_process_udp;
  return NULL;
#endif
}

const char *
cmdRedirectionAddress(cmd_parms *cmd, void *mconfig,
                      char *param1, char *param2)
{
#ifdef DNS_REDIRECTION
  apr_port_t port = 0;
  if(param2 == NULL || param2[0] == '\0') {
    while(apr_isspace(*param1))
      ++param1;
    if(!apr_isdigit(*param1)) {
      if(strchr(param1,':')) {
        char *last = NULL;
        // FIXME globule_dns_config->host = apr_pstrdup(cmd->pool, strtok(param1,":"));
        char *s = apr_strtok(param1,":",&last);
        globule_dns_config->host = (char*) malloc(strlen(s)+1);
        strcpy(globule_dns_config->host, s);

        port = atoi(apr_strtok(NULL,":",&last));
      } else {
        // FIXME globule_dns_config->host = apr_pstrdup(cmd->pool, param1);
        globule_dns_config->host = (char*) malloc(strlen(param1)+1);
        strcpy(globule_dns_config->host, param1);

      }
    } else
      port = atoi(param1);
  } else {
    // FIXME globule_dns_config->host = apr_pstrdup(cmd->pool, param1);
    globule_dns_config->host = (char*) malloc(strlen(param1)+1);
    strcpy(globule_dns_config->host, param1);

    port = atoi(param2);
  }
  if(port == 0)
    port = CFG_DEFAULT_DNS_PORT;
  globule_dns_config->port = port;
  return NULL;
#else
  return "DNS mode can only be used when a modified Apache is used and "
    "globule is compiled with DNS_REDIRECTION defined (see config.h)";
#endif
}

const char *
cmdStaticResolv(cmd_parms *cmd, void *mconfig,
                char *param1, char *param2, char *param3)
{
  param1 = apr_pstrdup(cmd->pool, param1);
  if(!strcmp(param2,"A")) {
    apr_status_t status;
    apr_sockaddr_t *sa = NULL;
    status = apr_sockaddr_info_get(&sa, param3, AF_INET, 0, 0, cmd->pool);
    if(status != APR_SUCCESS)
      return "Cannot resolve A record to IP address";
    apr_uint32_t addr = sa->sa.sin.sin_addr.s_addr;
    cfgnamebindings.push_back(DNSRecord(cmd->pool, param1, addr));
  } else if(!strcmp(param2,"CNAME")) {
    param3 = apr_pstrdup(cmd->pool, param3);
    cfgnamebindings.push_back(DNSRecord(cmd->pool, param1, param3));
  } else
    return "Second parameter for static DNS binding should be A or CNAME";
  return NULL;
}

const char *
dns_config_ttl(cmd_parms *cmd, void *mconfig, const char *param)
{ 
  const char *errmsg;
  apr_int64_t value;
  if((errmsg = getvalue(cmd, param, &value, translations_time)))
    return errmsg;
  globule_dns_config->ttl_none = value;
  return NULL;
}

const char *
dns_config_ttl_as(cmd_parms *cmd, void *mconfig, const char *param)
{ 
  const char *errmsg;
  apr_int64_t value;
  if((errmsg = getvalue(cmd, param, &value, translations_time)))
    return errmsg;
  globule_dns_config->ttl_as = value;
  return NULL;
}

const char *
dns_config_ttl_rr(cmd_parms *cmd, void *mconfig, const char *param)
{
  const char *errmsg;
  apr_int64_t value;
  if((errmsg = getvalue(cmd, param, &value, translations_time)))
    return errmsg;
  globule_dns_config->ttl_rr = value;
  return NULL;
}

const char *
dns_config_ipcount(cmd_parms *cmd, void *mconfig, const char *param)
{
  const char *errmsg;
  apr_int64_t value;
  if((errmsg = getvalue(cmd, param, &value, NULL)))
    return errmsg;
  globule_dns_config->ipcount = value;
  return NULL;
}

const char *
dns_config_bgprefresh(cmd_parms *cmd, void *mconfig, const char *param)
{
  const char *errmsg;
  apr_int64_t value;
  if((errmsg = getvalue(cmd, param, &value, translations_time)))
    return errmsg;
  globule_dns_config->bgp_refresh = value;
  return NULL;
}

const char *
dns_config_bgpfile(cmd_parms * cmd, void * mconfig, const char * param)
{ 
  dns_config *cfg = globule_dns_config;
  // FIXME cfg->bgp_file_name = apr_pstrdup(cmd->pool, param);
  cfg->bgp_file_name = (char*) malloc(strlen(param)+1);
  strcpy((char*)cfg->bgp_file_name, param);
  return NULL;
}

const char *
dns_config_ror(cmd_parms *cmd, void *mconfig, int reset)
{
  dns_config *cfg = globule_dns_config;
  cfg->ror = reset;
  return NULL;    
}

static const char *
cmdRedirectPolicy(cmd_parms *cmd, void *mconfig, const char *param)
{
  struct configuration *cfg = (struct configuration *) mconfig;

  cfg->redirect_policy_set = true;
  if (!strcasecmp( param, "static" ))
    cfg->redirect_policy = "static";
  else if (!strcasecmp( param, "RR" ))
    cfg->redirect_policy = "RR";
  else if(!strcasecmp( param, "AS" ))
    cfg->redirect_policy = "AS";
  else if(!strcasecmp( param, "WRR" ))
    cfg->redirect_policy = "WRR";
  else if(!strcasecmp( param, "WRAND" ))
    cfg->redirect_policy = "WRAND";
  else if(!strcasecmp( param, "BAS" ))
    cfg->redirect_policy = "BAS";
  else
    return "Unknown redirection policy given as default!";

  return NULL;
}

static const char *
cmdHeartBeatInterval(cmd_parms *cmd, void *mconfig,
                     const char *param1)
{
  struct configuration *cfg = (struct configuration *) mconfig;
  const char *errmsg;
  apr_int64_t value;
  if((errmsg = getvalue(cmd, param1, &value, translations_time)))
    return errmsg;
  cfg->reevaluateinterval = value;
  cfg->reevaluateinterval_set = true;
  return NULL;
}

static const char *
cmdPolicyAdaptationInterval(cmd_parms *cmd, void *mconfig,
                            const char *param1)
{
  struct configuration *cfg = (struct configuration *) mconfig;
  const char *errmsg;
  apr_int64_t value;
  if((errmsg = getvalue(cmd, param1, &value, translations_time)))
    return errmsg;
  cfg->reevaluateadaptinterval = value;
  cfg->reevaluateadaptinterval_set = true;
  return NULL;
}

static const char *
cmdContentTraversalInterval(cmd_parms *cmd, void *mconfig,
                            const char *param1)
{
  struct configuration *cfg = (struct configuration *) mconfig;
  const char *errmsg;
  apr_int64_t value;
  if((errmsg = getvalue(cmd, param1, &value, translations_time)))
    return errmsg;
  cfg->reevaluatecontentinterval = value;
  cfg->reevaluatecontentinterval_set = true;
  return NULL;
}

static const char *
cmdNumofLocks(cmd_parms *cmd, void *mconfig,
              const char *param1)
{
  static bool notifiedUser = false;
  if(!notifiedUser) {
    notifiedUser = true;
    ap_log_perror(APLOG_MARK, APLOG_WARNING, APR_SUCCESS, cmd->pool, "The usage of GlobuleLockCount is deprecated and no replacement is needed.");
  }
  return NULL;
}

static bool
makesection(cmd_parms* cmd, struct configuration *cfg, enum sectiontype type)
{
  if(!cfg->section) {
    /* FIXME:
     * Why can't we use the cmd pool here? apparently the pool is (partially) cleaned before
     * the postconfig phase as data is gone..
     * cfg->section = (struct section_struct *) apr_palloc(pool, sizeof(struct section_struct));
     */
    cfg->section = (struct section_struct *) malloc(sizeof(struct section_struct));
    cfg->section->type = type;
    cfg->section->conf = cfg;
    ap_assert(cmd->server);
    cfg->section->srv  = cmd->server;
    // FIXME
    cfg->section->servername = (char*) malloc(strlen(cmd->server->server_hostname)+1);
    strcpy(cfg->section->servername, cmd->server->server_hostname);
    cfg->section->portnum = cmd->server->port;
    struct section_struct **pptr = &sections;
    if(type != CONFIGSECTION)
      while(*pptr)
        pptr = &(*pptr)->next;
    cfg->section->next = *pptr;
    *pptr = cfg->section;
    return true;
  } else {
    ap_assert(cfg->section->srv == cmd->server);
    if(cfg->section->type != type && cfg->section->type == PLAINSECTION) {
      cfg->section->type = type;
      cfg->section->srv  = cmd->server;
      ap_assert(cfg == cfg->section->conf);
      return true;
    }
    return false;
  }
}

static const char *
cmdImport(cmd_parms *cmd, void *mconfig,
          const char *param1, const char *param2)
{
  struct configuration *cfg = (struct configuration *) mconfig;
  enum sectiontype type;
  cfg->imported = enabled;
  if(!strcasecmp(cmd->cmd->name, "GlobuleAnythingFor")) {
    type = BROKEREDSECTION;
  } else if(!strcasecmp(cmd->cmd->name, "GlobuleReplicaFor")) {
    type = IMPORTSECTION;
  } else if(!strcasecmp(cmd->cmd->name, "GlobuleRedirectorFor")) {
    type = REDIRECTSECTION;
  } else if(!strcasecmp(cmd->cmd->name, "GlobuleBackupFor")) {
    type = KEEPERSECTION;
  } else {
    return "Unknown type of import";
  }
  if(makesection(cmd, cfg, type)) {
    // FIXME cfg->section->upstream = apr_pstrdup(cmd->pool, param1);
    // FIXME cfg->section->password = apr_pstrdup(cmd->pool, param2);

    cfg->section->upstream = (char*) malloc(strlen(param1)+1);
    cfg->section->password = (char*) malloc(strlen(param2)+1);
    cfg->section->weight = 0;
    strcpy(cfg->section->upstream, param1);
    strcpy(cfg->section->password, param2);
  }
  return NULL;
}

static const char *
cmdServer(cmd_parms *cmd, void *mconfig,
          const char *param1, const char *param2)
{
  struct configuration *cfg = (struct configuration *) mconfig;
  const char *server_uri_str;
  if(cfg->imported != enabled)
    return "Servers can only be specified at a top level import";

  if(param2[ strlen(param2)-1 ] != '/')
    server_uri_str = (char *)apr_pstrcat(cmd->pool, param2, "/", NULL);
  else
    server_uri_str = (char *)param2;
  Url server_uri(cmd->pool, server_uri_str);
  server_uri.normalize(cmd->pool);

  if(!cfg->contacts)
    cfg->contacts = new vector<Contact>;
  cfg->contacts->push_back(Contact(cmd->pool, Peer::KEEPER, server_uri(cmd->pool), ""));
  return NULL;
}

static const char *
cmdDatabase(cmd_parms *cmd, void *mconfig,
            char *param1, char *param2)
{
  struct configuration *cfg = (struct configuration *) mconfig;
  if(makesection(cmd, cfg, DATABASESECTION)) {
    cfg->section->upstream = apr_pstrdup(cmd->pool, param1);
    cfg->section->password = apr_pstrdup(cmd->pool, param2);
    cfg->section->weight = 0;
  }
  return NULL;
}

static const char *
cmdExport(cmd_parms *cmd, void *mconfig,
          int on)
{
  struct configuration *cfg = (struct configuration *) mconfig;
  cfg->exported = (on ? enabled : disabled);
  return NULL;
}

static const char *
cmdWeight(cmd_parms *cmd, void *mconfig, const char *param)
{
  const char *errmsg;
  struct configuration *cfg = (struct configuration *) mconfig;
  if(cfg->exported == unset)
    return "Weight can only be specified at top of exported tree";
  if(makesection(cmd, cfg, EXPORTSECTION)) {
    char *s;
    s = ap_server_root_relative(cmd->pool, ".");
    cfg->section->upstream = "";
    cfg->section->password = "";
  }
  apr_int64_t weight;
  if((errmsg = getvalue(cmd, param, &weight, NULL)))
    return errmsg;
  cfg->section->weight = weight;
  return NULL;
}

static const char *
cmdManaged(cmd_parms *cmd, void *mconfig, const char *param)
{
  struct configuration *cfg = (struct configuration *) mconfig;
  if(cfg->exported == unset)
    return "Manager site can only be specified at top of exported tree, you need to specify a GlobuleReplicate first";
  if(makesection(cmd, cfg, EXPORTSECTION)) {
    char *s;
    s = ap_server_root_relative(cmd->pool, ".");
    cfg->section->upstream = "";
    cfg->section->password = "";
    cfg->section->weight = 1;
  }

  if(param && strcmp(param,"") && strcmp(param,"-")) {
    Url uri(cmd->pool, param);
    uri.normalize(cmd->pool);
    if(!cfg->contacts)
      cfg->contacts = new vector<Contact>;
    cfg->contacts->push_back(Contact(cmd->pool, Peer::MANAGER,
                                     uri(cmd->pool), ""));
  }

  return NULL;
}

static const char *
cmdReplica(cmd_parms *cmd, void *mconfig,
           const char *param1, const char *param2, const char *param3)
{
  struct configuration *cfg = (struct configuration *) mconfig;
  const char *errmsg;
  const char *slave_uri_str;
  if(cfg->exported == unset)
    return "Replica sites can only be specified at top of exported tree, you need to specify a GlobuleReplate first";
  if(makesection(cmd, cfg, EXPORTSECTION)) {
    char *s;
    s = ap_server_root_relative(cmd->pool, ".");
    cfg->section->upstream = "";
    cfg->section->password = "";
    cfg->section->weight = 1;
  }

  if(param1[strlen(param1)-1] != '/')
    slave_uri_str = (char *)apr_pstrcat(cmd->pool, param1, "/", NULL);
  else
    slave_uri_str = (char *)param1;
  Url slave_uri(cmd->pool, slave_uri_str);
  slave_uri.normalize(cmd->pool);

  if(!strcasecmp(cmd->cmd->name,"GlobuleProxyFor")) {
    cfg->section->type = PROXYSECTION;
    cfg->section->upstream = slave_uri(cmd->pool);
    cfg->section->password = "";
    cfg->section->weight = 1;
  } else {
    if(!cfg->contacts)
      cfg->contacts = new vector<Contact>;
    if(!strcasecmp(cmd->cmd->name,"GlobuleReplicaIs")) {
      if(param3) {
        apr_int64_t weight;
        if((errmsg = getvalue(cmd, param3, &weight, NULL)))
          return errmsg;
        cfg->contacts->push_back(Contact(cmd->pool, Peer::REPLICA,
                                         slave_uri(cmd->pool),
                                         (param2 ? apr_pstrdup(cmd->pool,param2) : ""),
                                         (apr_uint16_t)weight));
      } else {
        cfg->contacts->push_back(Contact(cmd->pool, Peer::REPLICA,
                                         slave_uri(cmd->pool),
                                         (param2 ? apr_pstrdup(cmd->pool,param2) : "")));
      }
    } else if(!strcasecmp(cmd->cmd->name,"GlobuleMirrorIs")) {
      apr_int64_t weight;
      if((errmsg = getvalue(cmd, param2, &weight, NULL)))
        return errmsg;
      cfg->contacts->push_back(Contact(cmd->pool, Peer::MIRROR, slave_uri(cmd->pool), "",
                                       (apr_uint16_t)weight));
    } else if(!strcasecmp(cmd->cmd->name,"GlobuleDisabledReplicaIs")) {
      cfg->contacts->push_back(Contact(cmd->pool, Peer::REPLICA,
                                       slave_uri(cmd->pool),
                                       (param2 ? apr_pstrdup(cmd->pool,param2) : ""),
                                       true));
    } else if(!strcasecmp(cmd->cmd->name,"GlobuleRedirectorIs")) {
      cfg->contacts->push_back(Contact(cmd->pool, Peer::REDIRECTOR,
                                       slave_uri(cmd->pool),
                                       (param2 ? apr_pstrdup(cmd->pool,param2) : "")));
    } else if(!strcasecmp(cmd->cmd->name,"GlobuleBackupIs")) {
      cfg->contacts->push_back(Contact(cmd->pool, Peer::KEEPER,
                                       slave_uri(cmd->pool),
                                       (param2 ? apr_pstrdup(cmd->pool,param2) : "")));
    } else
      ap_assert(!"Internal error - impossible state");
  }
  return NULL;
}

static const char *
cmdMemorySize(cmd_parms *cmd, void *mconfig, const char *param1)
{
  apr_int64_t value;
  const char *errmsg;
  if((errmsg = ap_check_cmd_context(cmd, GLOBAL_ONLY)))
    return errmsg;
  if((errmsg = getvalue(cmd, param1, &value, translations_bytes)))
    return errmsg;
  shmem_sz = (int) value;
  return NULL;
}

static const char *
cmdMaxDiskSpace(cmd_parms *cmd, void *mconfig, const char *param1)
{
  struct configuration *cfg = (struct configuration *) mconfig;
  apr_int64_t value;
  const char *errmsg;
  if((errmsg = getvalue(cmd, param1, &value, translations_bytes)))
    return errmsg;
  cfg->quotadiskspace = (int) value;
  cfg->quotadiskspace_set = true;
  makesection(cmd, cfg, PLAINSECTION);
  return NULL;
}

static const char *
cmdMaxNumofDocs(cmd_parms *cmd, void *mconfig, const char *param1)
{
  struct configuration *cfg = (struct configuration *) mconfig;
  apr_int64_t value;
  const char *errmsg;
  if((errmsg = getvalue(cmd, param1, &value, translations_bytes)))
    return errmsg;
  cfg->quotadocuments = (int) value;
  cfg->quotadocuments_set = true;
  makesection(cmd, cfg, PLAINSECTION);
  return NULL;
}

static const char *
cmdDirectory(cmd_parms *cmd, void *mconfig, const char *param1)
{
  struct configuration *cfg = (struct configuration *) mconfig;
  cfg->directory = apr_pstrdup(cmd->pool, param1);
  return NULL;
}

static const char *
cmdDefaultReplicationPolicy(cmd_parms *cmd, void *mconfig,
                            const char *param1)
{
  struct configuration *cfg = (struct configuration *) mconfig;
  cfg->replication_policy = param1; // FIXME: check upon validity of name
  cfg->replication_policy_set = true;
  return NULL;
}

static const char *
cmdConfigPath(cmd_parms *cmd, void *mconfig,
              const char *param1, const char *param2)
{
  struct configuration *cfg = (struct configuration *) mconfig;
  if(makesection(cmd, cfg, CONFIGSECTION)) {
    cfg->section->upstream = "";
    cfg->section->password = "";
    cfg->section->weight = 0;
  }
  if(apr_uri_parse(cmd->pool, param1, &configpath))
    return "Invalid configuration path";
  if(!param2) {
    const char* passchars = "34ACEFHKLMNPRTWY";
    unsigned char password[17];
    apr_generate_random_bytes(password, sizeof(password)-1);
    password[sizeof(password)] = '\0';
    for(unsigned int i=0; i<sizeof(password); i++)
      password[i] = passchars[password[i] % strlen(passchars)];
    configpass = apr_pstrdup(cmd->pool, (char*)password);
    DIAG(0,(MONITOR(warning)),("Using auto-generated password %s for internal requests",configpass));
  } else
    configpass = cfg->section->password = apr_pstrdup(cmd->pool, param2);
  return NULL;
}

static const char *
cmdBrokerSerial(cmd_parms *cmd, void *mconfig,
                const char *param1)
{
  ConfigHandler::brokerSerial = apr_pstrdup(cmd->pool, param1);
  return NULL;
}

static const char *
cmdFancyServerName(cmd_parms *cmd, void *mconfig, const char *param1)
{
  struct configuration *cfg = (struct configuration *) mconfig;
  if(ap_strchr(param1,'\"'))
    return "Double quotes (\") not allowed in GlobuleFancyServerName argument";
  else if(ap_strstr(param1,"%22"))
    return "Escaped double quotes (%22) are also not allowed in "
      "GlobuleFancyServerName argument";
  cfg->fancyservername = apr_pstrdup(cmd->pool, param1);
  return NULL;
}

static const char *
cmdMonitor(cmd_parms *cmd, void *mconfig,
           const char *param1, const char *param2, const char *param3)
{
  static bool notifiedUser = false;
  if(!notifiedUser) {
    notifiedUser = true;
    ap_log_perror(APLOG_MARK, APLOG_WARNING, APR_SUCCESS, cmd->pool, "The usage of GlobuleMonitor is deprecated in favor of GlobuleDebugProfile and should no longer be used at all");
  }
  return NULL;
}

static const char *
cmdDebugProfile(cmd_parms *cmd, void *mconfig,
                const char *param1)
{
  char *module;
  char *option = 0;
  if(!strcmp(param1,"default") || !strcmp(param1,"defaults")) {
    module = "defaults";
  } else if(!strcmp(param1,"normal")) {
    module = "normal";
  } else if(!strcmp(param1,"extended")) {
    module = "extended";
  } else if(!strcmp(param1,"verbose")) {
    module = "verbose";
  } else
    return "Unrecognized debug profile";
  char **filtermodule = (char **) apr_array_push(filtermodules);
  filtermodule[0] = apr_pstrdup(cmd->pool, module);
  filtermodule[1] = apr_pstrdup(cmd->pool, (option?option:""));
  return NULL;
}

#ifdef PSODIUM
static const char *
cmdPsodium(cmd_parms *cmd, void *mconfig, const char *param)
{
  intraserverauthpwd = param;
  return NULL;
}
#endif

const int
HandlerConfiguration::get_reevaluateadaptskip() const throw()
{
  return (_reevaluateadaptinterval + _reevaluateinterval - 1) /
                                                 _reevaluateinterval;
}

const int
HandlerConfiguration::get_reevaluatecontentskip() const throw()
{
  return (_reevaluatecontentinterval + _reevaluateinterval - 1) /
                                                 _reevaluateinterval;
}

static void *
createconf(apr_pool_t *p, char *path)
{
  struct configuration *cfg;
  cfg = (struct configuration *)apr_pcalloc(p, sizeof(struct configuration));
  cfg->section  = NULL;
  cfg->exported                = unset;
  cfg->imported                = unset;
  cfg->path                    = (path ? apr_pstrdup(p, path) : NULL);
  cfg->directory               = NULL;
  cfg->quotadiskspace          = 50000000;
  cfg->quotadiskspace_set      = false;
  cfg->quotadocuments          = 500;
  cfg->quotadocuments_set      = false;
  cfg->redirect_mode_set       = false;
  cfg->redirect_mode           = CFG_MODE_HTTP;
  cfg->redirect_policy_set     = false;
  cfg->redirect_policy         = "RR";
  cfg->replication_policy_set  = false;
  cfg->replication_policy      = "Invalidate";
  cfg->contacts                = 0;
  cfg->fancyservername         = 0;
  cfg->reevaluateinterval      = 120;
  cfg->reevaluateinterval_set  = false;
#ifdef DEBUG
  cfg->reevaluateadaptinterval = 1200; // 20 mins eval replication policy
#else
  cfg->reevaluateadaptinterval = 86400; // 1 day eval replication policy
#endif
  cfg->reevaluateadaptinterval_set = false;
  cfg->reevaluatecontentinterval = cfg->reevaluateinterval;
  cfg->reevaluatecontentinterval_set = false;
  return cfg;
}

static void *
mergeconf(apr_pool_t *p, void *base, void *over)
{
  struct configuration *basecfg = (struct configuration *) base;
  struct configuration *overcfg = (struct configuration *) over;

  if(overcfg->exported == unset)      overcfg->exported = basecfg->exported;
  if(overcfg->imported == unset)      overcfg->imported = basecfg->exported;
  if(!overcfg->contacts)              overcfg->contacts = basecfg->contacts;

  if(!overcfg->reevaluateinterval_set) {
    overcfg->reevaluateinterval     = basecfg->reevaluateinterval;
    overcfg->reevaluateinterval_set = basecfg->reevaluateinterval_set;
  }
  if(!overcfg->reevaluateadaptinterval_set) {
    overcfg->reevaluateadaptinterval     =basecfg->reevaluateadaptinterval;
    overcfg->reevaluateadaptinterval_set =basecfg->reevaluateadaptinterval_set;
  }
  if(!overcfg->reevaluatecontentinterval_set) {
    overcfg->reevaluatecontentinterval     =basecfg->reevaluatecontentinterval;
    overcfg->reevaluatecontentinterval_set =basecfg->reevaluatecontentinterval_set;
  }
  if(!overcfg->redirect_mode_set) {
    overcfg->redirect_mode     = basecfg->redirect_mode;
    overcfg->redirect_mode_set = basecfg->redirect_mode_set;
  }
  if(!overcfg->redirect_policy_set) {
    overcfg->redirect_policy     = basecfg->redirect_policy;
    overcfg->redirect_policy_set = basecfg->redirect_policy_set;
  }
  if(!overcfg->replication_policy_set) {
    overcfg->replication_policy     = basecfg->replication_policy;
    overcfg->replication_policy_set = basecfg->replication_policy_set;
  }
  if(!overcfg->fancyservername)
    overcfg->fancyservername = basecfg->fancyservername;
  if(overcfg->section && basecfg->section &&
     overcfg->section->type != CONFIGSECTION &&
     basecfg->section->type != CONFIGSECTION &&
     basecfg->section->type != PLAINSECTION &&
     !(basecfg->section->type == EXPORTSECTION && overcfg->section->type == DATABASESECTION)
) {
    ap_log_perror(APLOG_MARK, APLOG_ERR, OK, p, "re-exporting/importing site is not allowed: "
                  "%s section at %s overridden by %s section at %s",
                  sectionname[basecfg->section->type], basecfg->section->conf->path,
                  sectionname[overcfg->section->type], overcfg->section->conf->path);
  } else if(overcfg->section && overcfg->section->type != PLAINSECTION) {
    Context* ctx = initworker(p)->ctx;
    ctx->request(0);
    bool initialized = true; /* some sections don't need initialization
                              * otherwise this flag indicates if section was
                              * already previously initialized.
                              */
    if(overcfg->section->handler) {
      HandlerConfiguration cfg(overcfg->redirect_mode,
                               overcfg->redirect_policy,
                               overcfg->replication_policy,
                               overcfg->reevaluateinterval,
                               overcfg->reevaluateadaptinterval,
                               overcfg->reevaluatecontentinterval,
			       overcfg->section->weight,
                               overcfg->contacts);
      try {
        initialized = overcfg->section->handler->initialize(p, ctx, &cfg);
      } catch(SetupException ex) {
        initialized  = false;
        ap_log_perror(APLOG_MARK, APLOG_ERR, APR_SUCCESS, p,
                     "Initializing Globule enabled section at url %s failed: %s",
                     overcfg->section->handler->location(p), ex.getMessage().c_str());
      }
    } else if(overcfg->section->type == EXPORTSECTION) {
      ap_log_perror(APLOG_MARK, APLOG_WARNING, OK, p, "Section with GlobuleReplicate on specified without any replica's will be ignored");
    }
    if(overcfg->section->type == DATABASESECTION && basecfg->section->type == EXPORTSECTION) {
      RegisterEvent registerevt(p, overcfg->section->handler);
      GlobuleEvent::submit(registerevt);
    }
    if(!initialized) {
      ResourceDeclaration resources;
      resources.set(ResourceDeclaration::QUOTA_DISKSPACE, overcfg->quotadiskspace);
      resources.set(ResourceDeclaration::QUOTA_NUMDOCS,   overcfg->quotadocuments);
      overcfg->section->handler->accounting(new(rmmmemory::shm()) ResourceAccounting(resources));
      struct workerstate_struct* workerstate = initworker(p);;
      ((BaseHandler*)overcfg->section->handler)->restore(p, workerstate->ctx);
    }
  } else if(basecfg->section) {
    overcfg->section = basecfg->section;
  }
  return over;
}

static void *
createserv(apr_pool_t *p, server_rec *s)
{
  if(!s->is_virtual)
    servername = apr_psprintf(p,"%s:%d", s->server_hostname, s->port);
  return NULL;
}

static void *
mergeserv(apr_pool_t *p, void *base, void *over)
{
  return base;
}

static const command_rec cmdtable[] = {
  AP_INIT_TAKE12("GlobuleAdminURL", (cmd_func)cmdConfigPath, NULL, RSRC_CONF, "Set the internal reference and configuration path."),
  AP_INIT_TAKE12("GlobuleAdmURL", (cmd_func)cmdConfigPath, NULL, RSRC_CONF, "Alias for GlobuleAdminUrl"),
  AP_INIT_TAKE1( "GlobuleBrokerConfigurationSerial", (cmd_func)cmdBrokerSerial, NULL, RSRC_CONF, "Opaque string used by the GBS, not for manual use."),
  AP_INIT_TAKE1( "GlobuleFancyServerName", (cmd_func)cmdFancyServerName, NULL, RSRC_CONF|ACCESS_CONF, "Set the fancy human readable name of the server."),
  AP_INIT_TAKE23("GlobuleMonitor", (cmd_func)cmdMonitor, NULL, RSRC_CONF, "Install a monitor."),
  AP_INIT_TAKE1( "GlobuleDebugProfile", (cmd_func)cmdDebugProfile, NULL, RSRC_CONF, "Set the debug messages verbosity (0-6[+])"),
  AP_INIT_TAKE1( "GlobuleMemSize", (cmd_func)cmdMemorySize, NULL, RSRC_CONF, "Size of Globule shared memory in mebibytes."),
  AP_INIT_TAKE1( "GlobuleDirectory", (cmd_func)cmdDirectory, NULL, RSRC_CONF|ACCESS_CONF, "Directory that should be used by globule for this export or import."),
  AP_INIT_TAKE12("GlobuleAnythingFor", (cmd_func)cmdImport, NULL, RSRC_CONF|ACCESS_CONF, "URI prefix of brokering server and the unique password for mutual authentication"),
  AP_INIT_TAKE2( "GlobuleReplicaFor", (cmd_func)cmdImport, NULL, RSRC_CONF|ACCESS_CONF, "URI prefix of an exporting server and the unique password for mutual authentication"),
  AP_INIT_TAKE2( "GlobuleBackupFor", (cmd_func)cmdImport, NULL, RSRC_CONF|ACCESS_CONF, "URI prefix of an exporting server and the unique password for mutual authentication"),
  AP_INIT_TAKE2( "GlobuleRedirectorFor", (cmd_func)cmdImport, NULL, RSRC_CONF|ACCESS_CONF, "URI prefix of an exporting server and the unique password for mutual authentication"),
  AP_INIT_TAKE2( "GlobuleBackupForIs", (cmd_func)cmdServer, NULL, RSRC_CONF|ACCESS_CONF, ""),
  AP_INIT_TAKE12("GlobuleDatabase", (cmd_func)cmdDatabase, NULL, RSRC_CONF|ACCESS_CONF, "Database interface" ),
  AP_INIT_FLAG(  "GlobuleReplicate", (cmd_func)cmdExport, NULL, RSRC_CONF|ACCESS_CONF, "Enable or disable exported." ),
  AP_INIT_TAKE1( "GlobuleOriginWeight", (cmd_func)cmdWeight, NULL, RSRC_CONF|ACCESS_CONF, ""),
  AP_INIT_TAKE1( "GlobuleProxyFor", (cmd_func)cmdReplica, NULL, RSRC_CONF|ACCESS_CONF, "URI prefix of an non-Globule upstream server"),
  AP_INIT_TAKE1(  "GlobuleManagedBy", (cmd_func)cmdManaged, NULL, RSRC_CONF|ACCESS_CONF, "URI of managing server providing replicas or hyphen if none"),
  AP_INIT_TAKE23( "GlobuleReplicaIs", (cmd_func)cmdReplica, NULL, RSRC_CONF|ACCESS_CONF, "URI prefix of an importing server and the unique password for mutual authentication"),
  AP_INIT_TAKE2( "GlobuleMirrorIs",   (cmd_func)cmdReplica, NULL, RSRC_CONF|ACCESS_CONF, "URI prefix of an mirroring server and the assigned weight"),
  AP_INIT_TAKE2( "GlobuleDisabledReplicaIs", (cmd_func)cmdReplica, NULL, RSRC_CONF|ACCESS_CONF, "URI prefix of an importing server and the unique password for mutual authentication"),
  AP_INIT_TAKE2( "GlobuleBackupIs", (cmd_func)cmdReplica, NULL, RSRC_CONF|ACCESS_CONF, "URI prefix of an exporting server and the unique password for mutual authentication"),
  AP_INIT_TAKE2( "GlobuleRedirectorIs", (cmd_func)cmdReplica, NULL, RSRC_CONF|ACCESS_CONF, "URI prefix of an exporting server and the unique password for mutual authentication"),
  AP_INIT_TAKE1( "GlobuleHeartBeatInterval", (cmd_func)cmdHeartBeatInterval, NULL, RSRC_CONF|ACCESS_CONF, "Set the interval on which to master-slave evaluate their availability"),
  AP_INIT_TAKE1( "GlobulePolicyAdaptationInterval", (cmd_func)cmdPolicyAdaptationInterval, NULL, RSRC_CONF|ACCESS_CONF, "Set the interval at which the master reevaluates the replication policies (must be multiple of HeartBeatInterval)"),
  AP_INIT_TAKE1( "GlobuleContentTraversalInterval", (cmd_func)cmdContentTraversalInterval, NULL, RSRC_CONF|ACCESS_CONF, "Set the interval at which the master traverses all content for a site (must be multiple of HeartBeatInterval)"),
  AP_INIT_TAKE1( "GlobuleLockCount", (cmd_func)cmdNumofLocks, NULL, RSRC_CONF, "Number of locks (OS mutexes) to use for Globule." ),
  AP_INIT_TAKE1( "GlobuleDefaultReplicationPolicy", (cmd_func)cmdDefaultReplicationPolicy, NULL, RSRC_CONF|ACCESS_CONF, "Default replication policy, e.g. Invalidate."),
  AP_INIT_TAKE1( "GlobuleDefaultRedirectPolicy", (cmd_func)cmdRedirectPolicy, NULL, RSRC_CONF|ACCESS_CONF, "Default redirection policy, one of: RR, AS, static"),

  AP_INIT_TAKE1( "GlobuleMaxMetaDocsInMemory", (cmd_func)cmdMaxNumofDocs, NULL, RSRC_CONF|ACCESS_CONF, "Maximum number of documents per site kept in memory"),
  AP_INIT_TAKE1( "GlobuleMaxDiskSpace", (cmd_func)cmdMaxDiskSpace, NULL, RSRC_CONF|ACCESS_CONF, "Maximum number of documents per site kept in memory"),
  AP_INIT_TAKE1( "GlobuleRedirectionMode", (cmd_func)cmdRedirectionMode, NULL, RSRC_CONF|ACCESS_CONF, "Redirector mode: DNS|HTTP|BOTH|OFF which must at least be specified at a global scope."),
#ifdef DNS_REDIRECTION
  AP_INIT_TAKE12("GlobuleDNSRedirectionAddress", (cmd_func)cmdRedirectionAddress, NULL, RSRC_CONF, "Must be specified BEFORE RedirectionMode"),
  AP_INIT_TAKE3( "GlobuleStaticResolv", (cmd_func)cmdStaticResolv, NULL, RSRC_CONF, "Defines a static DNS resolvement from hostname to CNAME or IP address."),
  AP_INIT_TAKE1( "GlobuleMaxIPCount", (cmd_func)dns_config_ipcount, NULL, RSRC_CONF,
                 "Maximum number of IP addresses returned in a single DNS response, default = 4"),
  AP_INIT_TAKE1( "GlobuleTTL", (cmd_func)dns_config_ttl, NULL, RSRC_CONF,
                 "A default TTL value (any non-negative number of seconds) for static DNS responses, default = 86400 (1 day)"),
  AP_INIT_TAKE1( "GlobuleTTL_AS", (cmd_func)dns_config_ttl_as, NULL, RSRC_CONF,
                 "A default TTL value (any non-negative number of seconds) for round-robin DNS responses, default = 1800 (30 minutes)"),
  AP_INIT_TAKE1( "GlobuleTTL_RR", (cmd_func)dns_config_ttl_rr, NULL, RSRC_CONF,
                 "A default TTL value (any non-negative number of seconds) for AS-based DNS responses, default = 600 (10 minutes)"),
#endif
  AP_INIT_TAKE1( "GlobuleBGPDataFile", (cmd_func)dns_config_bgpfile, NULL, RSRC_CONF,
                 "A filename of the file with dumped BGP data, default = /dev/null"),
  AP_INIT_TAKE1( "GlobuleBGPReloadAfter", (cmd_func)dns_config_bgprefresh, NULL, RSRC_CONF,
                 "The time in seconds after which the BGP data file is re-read, default = 86400 (1 day)"),
#ifdef PSODIUM
  AP_INIT_TAKE1("GlobuleIntraServerAuthPassword", (cmd_func)cmdPsodium, NULL, RSRC_CONF, "Set the password between Globule and pSodium."),
#endif
  {NULL}
};

static void
registerhooks(apr_pool_t *p) {
  static const char *const a[] = { "globule_module", NULL };
  ap_hook_post_config(postconfig, NULL, NULL, APR_HOOK_MIDDLE);
  ap_hook_child_init(initchild, NULL, NULL, APR_HOOK_MIDDLE);
#ifdef DNS_REDIRECTION
  ap_hook_process_connection(dns_process_connection, NULL, NULL, APR_HOOK_FIRST);
#endif
  ap_hook_translate_name(redirect, NULL, NULL, APR_HOOK_FIRST);
  ap_hook_log_transaction(logging, NULL, a, APR_HOOK_MIDDLE);
  ap_hook_log_transaction(dns_http_cleanup, NULL, NULL, APR_HOOK_MIDDLE);
  ap_hook_handler(handler, NULL, NULL, APR_HOOK_FIRST);
  ap_hook_fixups(fixup, NULL, NULL, APR_HOOK_FIRST);
  apr_status_t status;
  if((status = apr_threadkey_private_create(&childstate.threadkey, 0, p))) {
    ap_log_perror(APLOG_MARK, APLOG_EMERG, status, p,
                  "Cannot initialize thread private key");
  }
  configpath.path = 0;
  filtermodules = apr_array_make(p, 0, sizeof(char*[2]));
  globule_dns_config = (dns_config*) apr_palloc(p, sizeof(dns_config));
  globule_dns_config->port                 = CFG_DEFAULT_DNS_PORT;
  globule_dns_config->host                 = CFG_DEFAULT_DNS_HOST;
  globule_dns_config->ipcount              = CFG_DEFAULT_IPCOUNT;
  globule_dns_config->max_sites            = CFG_DEFAULT_MAX_SITES;
  globule_dns_config->max_replicas         = CFG_DEFAULT_MAX_REPLICAS;
  globule_dns_config->bgp_refresh          = CFG_DEFAULT_BGP_REFRESH;
  globule_dns_config->bgp_file_name        = CFG_DEFAULT_BGP_FILE_NAME;
  globule_dns_config->ror                  = CFG_DEFAULT_RESET_ON_RESTART;
  globule_dns_config->ttl_none             = CFG_DEFAULT_TTL;
  globule_dns_config->ttl_rr               = CFG_DEFAULT_TTL_RR;
  globule_dns_config->ttl_as               = CFG_DEFAULT_TTL_AS;
  globule_dns_config->dns_redirection = 0;
}


extern "C" {
module AP_MODULE_DECLARE_DATA globule_module =
{
  STANDARD20_MODULE_STUFF,
  createconf, mergeconf,
  createserv, mergeserv,
  cmdtable,
  registerhooks
};
};
