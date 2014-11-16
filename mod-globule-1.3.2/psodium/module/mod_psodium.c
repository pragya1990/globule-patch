/* ====================================================================
 * The Apache Software License, Version 1.1
 *
 * Copyright (c) 2000-2003 The Apache Software Foundation.  All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. The end-user documentation included with the redistribution,
 *    if any, must include the following acknowledgment:
 *       "This product includes software developed by the
 *        Apache Software Foundation (http://www.apache.org/)."
 *    Alternately, this acknowledgment may appear in the software itself,
 *    if and wherever such third-party acknowledgments normally appear.
 *
 * 4. The names "Apache" and "Apache Software Foundation" must
 *    not be used to endorse or promote products derived from this
 *    software without prior written permission. For written
 *    permission, please contact apache@apache.org.
 *
 * 5. Products derived from this software may not be called "Apache",
 *    nor may "Apache" appear in their name, without prior written
 *    permission of the Apache Software Foundation.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE APACHE SOFTWARE FOUNDATION OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the Apache Software Foundation.  For more
 * information on the Apache Software Foundation, please see
 * <http://www.apache.org/>.
 *
 * Portions of this software are based upon public domain software
 * originally written at the National Center for Supercomputing Applications,
 * University of Illinois, Urbana-Champaign.
 */

/* !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 * Arno: ATTENTION: This file is a modified copy of mod_proxy.c of 
 * Apache 2.0.47.
 * Please do not modify this file extensively, such that we can easily 
 * incorporate bug fixes on the original into pSodium.
 * All modifications should be in a #ifdef PSODIUM block
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 */


#define CORE_PRIVATE

#include "mod_proxy.h"
#include "mod_core.h"

#include "apr_optional.h"

#ifndef MAX
#define MAX(x,y) ((x) >= (y) ? (x) : (y))
#endif

#include "mod_psodium.h"

#ifdef PSODIUM
#include "../globule/utilities.h"
#define proxy_module    psodium_module
#define proxy_server_conf   psodium_conf
#define proxy_run_scheme_handler    psodium_run_scheme_handler
#define proxy_run_canon_handler    psodium_run_canon_handler
#endif

/*
 * A Web proxy module. Stages:
 *
 *  translate_name: set filename to proxy:<URL>
 *  map_to_storage: run proxy_walk (rather than directory_walk/file_walk)
 *                  can't trust directory_walk/file_walk since these are
 *                  not in our filesystem.  Prevents mod_http from serving
 *                  the TRACE request we will set aside to handle later.
 *  type_checker:   set type to PROXY_MAGIC_TYPE if filename begins proxy:
 *  fix_ups:        convert the URL stored in the filename to the
 *                  canonical form.
 *  handler:        handle proxy requests
 */

/* -------------------------------------------------------------- */
/* Translate the URL into a 'filename' */

static int alias_match(const char *uri, const char *alias_fakename)
{
    const char *end_fakename = alias_fakename + strlen(alias_fakename);
    const char *aliasp = alias_fakename, *urip = uri;

    while (aliasp < end_fakename) {
        if (*aliasp == '/') {
            /* any number of '/' in the alias matches any number in
             * the supplied URI, but there must be at least one...
             */
            if (*urip != '/')
                return 0;

            while (*aliasp == '/')
                ++aliasp;
            while (*urip == '/')
                ++urip;
        }
        else {
            /* Other characters are compared literally */
            if (*urip++ != *aliasp++)
                return 0;
        }
    }

    /* Check last alias path component matched all the way */

    if (aliasp[-1] != '/' && *urip != '\0' && *urip != '/')
        return 0;

    /* Return number of characters from URI which matched (may be
     * greater than length of alias, since we may have matched
     * doubled slashes)
     */

    return urip - uri;
}

/* Detect if an absoluteURI should be proxied or not.  Note that we
 * have to do this during this phase because later phases are
 * "short-circuiting"... i.e. translate_names will end when the first
 * module returns OK.  So for example, if the request is something like:
 *
 * GET http://othervhost/cgi-bin/printenv HTTP/1.0
 *
 * mod_alias will notice the /cgi-bin part and ScriptAlias it and
 * short-circuit the proxy... just because of the ordering in the
 * configuration file.
 */
static int proxy_detect(request_rec *r)
{
    void *sconf = r->server->module_config;
    proxy_server_conf *conf;
    conf = (proxy_server_conf *) ap_get_module_config(sconf, &proxy_module);


    DPRINTF( DEBUG1, ("proxy_detect called for %s, enable is %d\n", r->unparsed_uri, conf->req ));
    DPRINTF( DEBUG1, ("phost %p, pscheme %p, rscheme %s, pportstr %s\n", 
                     r->parsed_uri.hostname, r->parsed_uri.scheme, ap_http_method(r),
                     r->parsed_uri.port_str ));

#ifdef LATER
    DPRINTF( DEBUG1, ("ap_matches %d\n", 
                     ap_matches_request_vhost(r, r->parsed_uri.hostname,
                                          (apr_port_t)(r->parsed_uri.port_str ? r->parsed_uri.port 
                                                       : ap_default_port(r))) ));
#endif

    /* Ick... msvc (perhaps others) promotes ternary short results to int */

    if (conf->req && r->parsed_uri.scheme) {
        /* but it might be something vhosted */
        if (!(r->parsed_uri.hostname
              && !strcasecmp(r->parsed_uri.scheme, ap_http_method(r))
#ifndef PSODIUM
              && ap_matches_request_vhost(r, r->parsed_uri.hostname,
                                          (apr_port_t)(r->parsed_uri.port_str ? r->parsed_uri.port 
                                                       : ap_default_port(r))))) {
#else
            && FALSE)) {
            /* We need to be less strict than mod_proxy, otherwise we cannot run
               pSodium in both proxy and master/slave mode at the same time in
               one server. Less strict means that the URL in the request
               starts with http: to make it a proxy request, which is more
               or less correct according to the HTTP 1.1 spec (see p. 37)
               */
#endif
            r->proxyreq = PROXYREQ_PROXY;
            r->uri = r->unparsed_uri;
            r->filename = apr_pstrcat(r->pool, "proxy:", r->uri, NULL);
            r->handler = "proxy-server";
        }
    }
    /* We need special treatment for CONNECT proxying: it has no scheme part */
    else if (conf->req && r->method_number == M_CONNECT
             && r->parsed_uri.hostname
             && r->parsed_uri.port_str) {
        r->proxyreq = PROXYREQ_PROXY;
        r->uri = r->unparsed_uri;
        r->filename = apr_pstrcat(r->pool, "proxy:", r->uri, NULL);
        r->handler = "proxy-server";
    }

    DPRINTF( DEBUG1, ("proxy_detect result is: %d\n", r->proxyreq ));

    return DECLINED;
}

static int proxy_trans(request_rec *r)
{
    void *sconf = r->server->module_config;
    proxy_server_conf *conf =
    (proxy_server_conf *) ap_get_module_config(sconf, &proxy_module);
    int i, len;
    struct proxy_alias *ent = (struct proxy_alias *) conf->aliases->elts;

#ifdef PSODIUM
    pso_master_badslaves_to_note2globule( conf, r );
#endif
    if (r->proxyreq) {
        /* someone has already set up the proxy, it was possibly ourselves
         * in proxy_detect
         */
        return OK;
    }

    /* XXX: since r->uri has been manipulated already we're not really
     * compliant with RFC1945 at this point.  But this probably isn't
     * an issue because this is a hybrid proxy/origin server.
     */

    for (i = 0; i < conf->aliases->nelts; i++) {
        len = alias_match(r->uri, ent[i].fake);

       if (len > 0) {
           if ((ent[i].real[0] == '!' ) && ( ent[i].real[1] == 0 )) {
               return DECLINED;
           }

           r->filename = apr_pstrcat(r->pool, "proxy:", ent[i].real,
                                 (r->uri + len ), NULL);
           r->handler = "proxy-server";
           r->proxyreq = PROXYREQ_REVERSE;
           return OK;
       }
    }
    return DECLINED;
}


static int proxy_walk(request_rec *r)
{
    proxy_server_conf *sconf = ap_get_module_config(r->server->module_config,
                                                    &proxy_module);
    ap_conf_vector_t *per_dir_defaults = r->server->lookup_defaults;
    ap_conf_vector_t **sec_proxy = (ap_conf_vector_t **) sconf->sec_proxy->elts;
    ap_conf_vector_t *entry_config;
    proxy_dir_conf *entry_proxy;
    int num_sec = sconf->sec_proxy->nelts;
    /* XXX: shouldn't we use URI here?  Canonicalize it first?
     * Pass over "proxy:" prefix 
     */
    const char *proxyname = r->filename + 6;
    int j;

    for (j = 0; j < num_sec; ++j) 
    {
        entry_config = sec_proxy[j];
        entry_proxy = ap_get_module_config(entry_config, &proxy_module);

        /* XXX: What about case insensitive matching ???
         * Compare regex, fnmatch or string as appropriate
         * If the entry doesn't relate, then continue 
         */
        if (entry_proxy->r 
              ? ap_regexec(entry_proxy->r, proxyname, 0, NULL, 0)
              : (entry_proxy->p_is_fnmatch
                   ? apr_fnmatch(entry_proxy->p, proxyname, 0)
                   : strncmp(proxyname, entry_proxy->p, 
                                        strlen(entry_proxy->p)))) {
            continue;
        }
        per_dir_defaults = ap_merge_per_dir_configs(r->pool, per_dir_defaults,
                                                             entry_config);
    }

    r->per_dir_config = per_dir_defaults;

    return OK;
}

static int proxy_map_location(request_rec *r)
{
    int access_status;

    if (!r->proxyreq || !r->filename || strncmp(r->filename, "proxy:", 6) != 0)
        return DECLINED;

    /* Don't let the core or mod_http map_to_storage hooks handle this,
     * We don't need directory/file_walk, and we want to TRACE on our own.
     */
    if ((access_status = proxy_walk(r))) {
        ap_die(access_status, r);
        return access_status;
    }

    return OK;
}

/* -------------------------------------------------------------- */
/* Fixup the filename */

/*
 * Canonicalise the URL
 */
static int proxy_fixup(request_rec *r)
{
    char *url, *p;
    int access_status;

    if (!r->proxyreq || !r->filename || strncmp(r->filename, "proxy:", 6) != 0)
        return DECLINED;

    /* XXX: Shouldn't we try this before we run the proxy_walk? */
    url = &r->filename[6];

    /* canonicalise each specific scheme */
    if ((access_status = proxy_run_canon_handler(r, url))) {
        return access_status;
    }

    p = strchr(url, ':');
    if (p == NULL || p == url)
        return HTTP_BAD_REQUEST;

    return OK;          /* otherwise; we've done the best we can */
}

/* Send a redirection if the request contains a hostname which is not */
/* fully qualified, i.e. doesn't have a domain name appended. Some proxy */
/* servers like Netscape's allow this and access hosts from the local */
/* domain in this case. I think it is better to redirect to a FQDN, since */
/* these will later be found in the bookmarks files. */
/* The "ProxyDomain" directive determines what domain will be appended */
static int proxy_needsdomain(request_rec *r, const char *url, const char *domain)
{
    char *nuri;
    const char *ref;

    /* We only want to worry about GETs */
    if (!r->proxyreq || r->method_number != M_GET || !r->parsed_uri.hostname)
        return DECLINED;

    /* If host does contain a dot already, or it is "localhost", decline */
    if (strchr(r->parsed_uri.hostname, '.') != NULL
     || strcasecmp(r->parsed_uri.hostname, "localhost") == 0)
        return DECLINED;        /* host name has a dot already */

    ref = apr_table_get(r->headers_in, "Referer");

    /* Reassemble the request, but insert the domain after the host name */
    /* Note that the domain name always starts with a dot */
    r->parsed_uri.hostname = apr_pstrcat(r->pool, r->parsed_uri.hostname,
                                         domain, NULL);
    nuri = apr_uri_unparse(r->pool,
                           &r->parsed_uri,
                           APR_URI_UNP_REVEALPASSWORD);

    apr_table_set(r->headers_out, "Location", nuri);
    ap_log_rerror(APLOG_MARK, APLOG_INFO, 0, r,
                  "Domain missing: %s sent to %s%s%s", r->uri,
                  apr_uri_unparse(r->pool, &r->parsed_uri,
                                  APR_URI_UNP_OMITUSERINFO),
                  ref ? " from " : "", ref ? ref : "");

    return HTTP_MOVED_PERMANENTLY;
}

/* -------------------------------------------------------------- */
/* Invoke handler */

static int proxy_handler(request_rec *r)
{
    char *url, *scheme, *p;
    const char *p2;
    void *sconf = r->server->module_config;
    proxy_server_conf *conf = (proxy_server_conf *)
        ap_get_module_config(sconf, &proxy_module);
    apr_array_header_t *proxies = conf->proxies;
    struct proxy_remote *ents = (struct proxy_remote *) proxies->elts;
    int i, rc, access_status;
    int direct_connect = 0;
    const char *str;
    long maxfwd;

#ifdef PSODIUM    
    const char *want_slave = NULL, *want_pledge=NULL, *Host_header=NULL;
    const char *Refererer_header = NULL;

    char *type_str = (r->proxyreq) ? "proxy" : "regular";
DPRINTF( DEBUG1, ("\n\n\npsodium: Receiving %s request: %s, pr=%d, fn=%s$\n", type_str, r->unparsed_uri, r->proxyreq, r->filename )); 
    Host_header = apr_table_get( r->headers_in, "Host" );
    if (Host_header == NULL)
        Host_header="Lacking.";
DPRINTF( DEBUG1, ("psodium: any: Host-Header: %s\n", Host_header )); 
    want_slave = apr_table_get( r->headers_in, WANT_SLAVE_HEADER );
    if (want_slave == NULL)
        want_slave="Lacking.";
DPRINTF( DEBUG1, ("psodium: any: Want-Slave: %s\n", want_slave )); 

    want_pledge = apr_table_get( r->headers_in, WANT_PLEDGE_HEADER );
    if (want_pledge == NULL)
        want_pledge="Lacking.";
DPRINTF( DEBUG1, ("psodium: any: Want-Pledge: %s\n", want_pledge )); 

    if (Refererer_header == NULL)
        Refererer_header="Lacking.";
DPRINTF( DEBUG1, ("psodium: any: Refererer-Header: %s\n", Refererer_header )); 

#ifdef OLD
    /*
     * Arno: Hack to get at Globule's export path. Don't matter that it is
     * set in each process' copy of the module config, Globule sets the note
     * on each request.
     */
    conf->master_uri_prefix = apr_table_get( r->notes, GLOBULE_EXPORT_PATH_NOTE );
    if (conf->master_uri_prefix == NULL && !(conf->role & PSODIUM_CLIENT_ROLE))
    {
        DPRINTF( DEBUG1, ("G2P: Master export path is NULL\n" ));
        return HTTP_INTERNAL_SERVER_ERROR;
    }
    DPRINTF( DEBUG1, ("G2P: Master export path is %s\n", (conf->master_uri_prefix == NULL) ? "NULL" : conf->master_uri_prefix ));
#endif
    if (!r->proxyreq && (conf->role & PSODIUM_MASTER_ROLE))
    {
        access_status = pso_master_process_request( conf, r );
        if (access_status ==  OK)
        {
            DPRINTF( DEBUG1, ("psodium: master: Done.\n" )); 
            return OK;
        }
    }

    /* is this for us? */
    if (!r->proxyreq || !r->filename || strncmp(r->filename, "proxy:", 6) != 0)
    {
        // For this we should be called AFTER Globule!
        return DECLINED;
    }

DPRINTF( DEBUG1, ("psodium: client: Request is proxy request\n" )); 
#endif

    /* handle max-forwards / OPTIONS / TRACE */
    if ((str = apr_table_get(r->headers_in, "Max-Forwards"))) {
        maxfwd = strtol(str, NULL, 10);
        if (maxfwd < 1) {
            switch (r->method_number) {
            case M_TRACE: {
                int access_status;
                r->proxyreq = PROXYREQ_NONE;
                if ((access_status = ap_send_http_trace(r)))
                    ap_die(access_status, r);
                else
                    ap_finalize_request_protocol(r);
                return OK;
            }
            case M_OPTIONS: {
                int access_status;
                r->proxyreq = PROXYREQ_NONE;
                if ((access_status = ap_send_http_options(r)))
                    ap_die(access_status, r);
                else
                    ap_finalize_request_protocol(r);
                return OK;
            }
            default: {
                return ap_proxyerror(r, HTTP_BAD_GATEWAY,
                                     "Max-Forwards has reached zero - proxy loop?");
            }
            }
        }
        maxfwd = (maxfwd > 0) ? maxfwd - 1 : 0;
    }
    else {
        /* set configured max-forwards */
        maxfwd = conf->maxfwd;
    }
    apr_table_set(r->headers_in, "Max-Forwards", 
                  apr_psprintf(r->pool, "%ld", (maxfwd > 0) ? maxfwd : 0));

    url = r->filename + 6;
    p = strchr(url, ':');
    if (p == NULL)
        return HTTP_BAD_REQUEST;

    /* If the host doesn't have a domain name, add one and redirect. */
    if (conf->domain != NULL) {
        rc = proxy_needsdomain(r, url, conf->domain);
        if (ap_is_HTTP_REDIRECT(rc))
            return HTTP_MOVED_PERMANENTLY;
    }

    *p = '\0';
    scheme = apr_pstrdup(r->pool, url);
    *p = ':';

    /* Check URI's destination host against NoProxy hosts */
    /* Bypass ProxyRemote server lookup if configured as NoProxy */
    /* we only know how to handle communication to a proxy via http */
    /*if (strcasecmp(scheme, "http") == 0) */
    {
        int ii;
        struct dirconn_entry *list = (struct dirconn_entry *) conf->dirconn->elts;

        for (direct_connect = ii = 0; ii < conf->dirconn->nelts && !direct_connect; ii++) {
            direct_connect = list[ii].matcher(&list[ii], r);
        }
#if DEBUGGING
        ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r,
                      (direct_connect) ? "NoProxy for %s" : "UseProxy for %s",
                      r->uri);
#endif
    }

    /* firstly, try a proxy, unless a NoProxy directive is active */
    if (!direct_connect) {
        for (i = 0; i < proxies->nelts; i++) {
            p2 = ap_strchr_c(ents[i].scheme, ':');  /* is it a partial URL? */
            if (strcmp(ents[i].scheme, "*") == 0 ||
                (ents[i].use_regex && ap_regexec(ents[i].regexp, url, 0,NULL, 0)) ||
                (p2 == NULL && strcasecmp(scheme, ents[i].scheme) == 0) ||
                (p2 != NULL &&
                 strncasecmp(url, ents[i].scheme, strlen(ents[i].scheme)) == 0)) {

                /* handle the scheme */
                ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server,
                             "Trying to run scheme_handler against proxy");
                access_status = proxy_run_scheme_handler(r, conf, url, ents[i].hostname, ents[i].port);

                /* an error or success */
                if (access_status != DECLINED && access_status != HTTP_BAD_GATEWAY) {
                    return access_status;
                }
                /* we failed to talk to the upstream proxy */
            }
        }
    }

    /* otherwise, try it direct */
    /* N.B. what if we're behind a firewall, where we must use a proxy or
     * give up??
     */

    /* handle the scheme */
    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server,
                 "Trying to run scheme_handler");
    access_status = proxy_run_scheme_handler(r, conf, url, NULL, 0);
    if (DECLINED == access_status) {
        ap_log_error(APLOG_MARK, APLOG_WARNING, 0, r->server,
                    "proxy: No protocol handler was valid for the URL %s. "
                    "If you are using a DSO version of mod_proxy, make sure "
                    "the proxy submodules are included in the configuration "
                    "using LoadModule.", r->uri);
        return HTTP_FORBIDDEN;
    }
    return access_status;
}

/* -------------------------------------------------------------- */
/* Setup configurable data */

static void * create_proxy_config(apr_pool_t *p, server_rec *s)
{
    proxy_server_conf *ps = apr_pcalloc(p, sizeof(proxy_server_conf));

    DPRINTF( DEBUG1, ("create_proxy_config: pool is %p\n", p )); 
    
    ps->sec_proxy = apr_array_make(p, 10, sizeof(ap_conf_vector_t *));
    ps->proxies = apr_array_make(p, 10, sizeof(struct proxy_remote));
    ps->aliases = apr_array_make(p, 10, sizeof(struct proxy_alias));
    ps->raliases = apr_array_make(p, 10, sizeof(struct proxy_alias));
    ps->noproxies = apr_array_make(p, 10, sizeof(struct noproxy_entry));
    ps->dirconn = apr_array_make(p, 10, sizeof(struct dirconn_entry));
    ps->allowed_connect_ports = apr_array_make(p, 10, sizeof(int));
    ps->domain = NULL;
    ps->viaopt = via_off; /* initially backward compatible with 1.3.1 */
    ps->viaopt_set = 0; /* 0 means default */
    ps->req = 0;
    ps->req_set = 0;
    ps->recv_buffer_size = 0; /* this default was left unset for some reason */
    ps->recv_buffer_size_set = 0;
    ps->io_buffer_size = AP_IOBUFSIZE;
    ps->io_buffer_size_set = 0;
    ps->maxfwd = DEFAULT_MAX_FORWARDS;
    ps->maxfwd_set = 0;
    ps->error_override = 0; 
    ps->error_override_set = 0; 
    ps->preserve_host_set = 0;
    ps->preserve_host = 0;    
    ps->timeout = 0;
    ps->timeout_set = 0;
    ps->badopt = bad_error;
    ps->badopt_set = 0;
#ifdef PSODIUM
    ps->role = 0;
    ps->slavekeysfilename="no default slave key file";
    ps->slavekeysfilename_set=0;
    ps->auditor_hostname="localhost";
    ps->auditor_port = 38000;
    ps->auditor_passwd="";
    ps->auditoraddr_set=0;
    ps->tempstoragedirname="/tmp";
    ps->tempstoragedirname_set=0;
    ps->slaveid="slavehost:slaveport";
    ps->slaveid_set=0;
    ps->slaveprivatekeyfilename="no default slave private key file";
    ps->slaveprivatekeyfilename_set=0;
    ps->doublecheckprob_str="100";
    ps->doublecheckprob=1.0;
    ps->doublecheckprob_set=0;
    ps->slavekeysfilename_set=0;
    ps->badcontent_img_set=0;
    ps->badslave_img_set=0;
    ps->clientlied_img_set=0;
    ps->badslavesfilename_set=0;
    ps->lyingclientsfilename_set=0;    
    ps->versionsfilename_set=0;    
    ps->intraserverauthpwd_set=0;

    ps->versions=NULL;
    ps->lyingclients=NULL;
    ps->badslaves=NULL;
    // See PsodiumConfig.cc for initialization of other data structures.
#endif
    return ps;
}

static void * merge_proxy_config(apr_pool_t *p, void *basev, void *overridesv)
{
    proxy_server_conf *ps = apr_pcalloc(p, sizeof(proxy_server_conf));
    proxy_server_conf *base = (proxy_server_conf *) basev;
    proxy_server_conf *overrides = (proxy_server_conf *) overridesv;

    ps->proxies = apr_array_append(p, base->proxies, overrides->proxies);
    ps->sec_proxy = apr_array_append(p, base->sec_proxy, overrides->sec_proxy);
    ps->aliases = apr_array_append(p, base->aliases, overrides->aliases);
    ps->raliases = apr_array_append(p, base->raliases, overrides->raliases);
    ps->noproxies = apr_array_append(p, base->noproxies, overrides->noproxies);
    ps->dirconn = apr_array_append(p, base->dirconn, overrides->dirconn);
    ps->allowed_connect_ports = apr_array_append(p, base->allowed_connect_ports, overrides->allowed_connect_ports);

    ps->domain = (overrides->domain == NULL) ? base->domain : overrides->domain;
    ps->viaopt = (overrides->viaopt_set == 0) ? base->viaopt : overrides->viaopt;
    ps->req = (overrides->req_set == 0) ? base->req : overrides->req;
    ps->recv_buffer_size = (overrides->recv_buffer_size_set == 0) ? base->recv_buffer_size : overrides->recv_buffer_size;
    ps->io_buffer_size = (overrides->io_buffer_size_set == 0) ? base->io_buffer_size : overrides->io_buffer_size;
    ps->maxfwd = (overrides->maxfwd_set == 0) ? base->maxfwd : overrides->maxfwd;
    ps->error_override = (overrides->error_override_set == 0) ? base->error_override : overrides->error_override;
    ps->preserve_host = (overrides->preserve_host_set == 0) ? base->preserve_host : overrides->preserve_host;
    ps->timeout= (overrides->timeout_set == 0) ? base->timeout : overrides->timeout;
    ps->badopt = (overrides->badopt_set == 0) ? base->badopt : overrides->badopt;
#ifdef PSODIUM
    ps->role = overrides->role | base->role;
    ps->slavekeysfilename=(overrides->slavekeysfilename_set==0) ? base->slavekeysfilename : overrides->slavekeysfilename;
    ps->auditor_hostname=(overrides->auditoraddr_set==0) ? base->auditor_hostname : overrides->auditor_hostname;
    ps->auditor_passwd=(overrides->auditoraddr_set==0) ? base->auditor_passwd : overrides->auditor_passwd;
    ps->auditor_port=(overrides->auditoraddr_set==0) ? base->auditor_port : overrides->auditor_port;
    ps->tempstoragedirname=(overrides->tempstoragedirname_set==0) ? base->tempstoragedirname: overrides->tempstoragedirname;
    ps->slaveid=(overrides->slaveid_set==0) ? base->slaveid: overrides->slaveid;
    ps->slaveprivatekeyfilename=(overrides->slaveprivatekeyfilename_set==0) ? base->slaveprivatekeyfilename: overrides->slaveprivatekeyfilename;
    ps->doublecheckprob_str=(overrides->doublecheckprob_set==0) ? base->doublecheckprob_str: overrides->doublecheckprob_str;
    ps->badcontent_img_filename=(overrides->badcontent_img_set==0) ? base->badcontent_img_filename: overrides->badcontent_img_filename;
    ps->badcontent_img_mimetype=(overrides->badcontent_img_set==0) ? base->badcontent_img_mimetype: overrides->badcontent_img_mimetype;
    ps->badslave_img_filename=(overrides->badslave_img_set==0) ? base->badslave_img_filename: overrides->badslave_img_filename;
    ps->badslave_img_mimetype=(overrides->badslave_img_set==0) ? base->badslave_img_mimetype: overrides->badslave_img_mimetype;

    ps->clientlied_img_filename=(overrides->clientlied_img_set==0) ? base->clientlied_img_filename: overrides->clientlied_img_filename;
    ps->clientlied_img_mimetype=(overrides->clientlied_img_set==0) ? base->clientlied_img_mimetype: overrides->clientlied_img_mimetype;

    
ps->intraserverauthpwd=(overrides->intraserverauthpwd_set==0) ? base->intraserverauthpwd: overrides->intraserverauthpwd;
    ps->badslavesfilename=(overrides->badslavesfilename_set==0) ? base->badslavesfilename: overrides->badslavesfilename;
    ps->lyingclientsfilename=(overrides->lyingclientsfilename_set==0) ? base->lyingclientsfilename: overrides->lyingclientsfilename;
    ps->versionsfilename=(overrides->versionsfilename_set==0) ? base->versionsfilename: overrides->versionsfilename;
#endif
    return ps;
}

static void *create_proxy_dir_config(apr_pool_t *p, char *dummy)
{
    proxy_dir_conf *new =
        (proxy_dir_conf *) apr_pcalloc(p, sizeof(proxy_dir_conf));

    /* Filled in by proxysection, when applicable */

    return (void *) new;
}

static void *merge_proxy_dir_config(apr_pool_t *p, void *basev, void *addv)
{
    proxy_dir_conf *new = (proxy_dir_conf *) apr_pcalloc(p, sizeof(proxy_dir_conf));
    proxy_dir_conf *add = (proxy_dir_conf *) addv;

    new->p = add->p;
    new->p_is_fnmatch = add->p_is_fnmatch;
    new->r = add->r;
    return new;
}


static const char *
    add_proxy(cmd_parms *cmd, void *dummy, const char *f1, const char *r1, int regex)
{
    server_rec *s = cmd->server;
    proxy_server_conf *conf =
    (proxy_server_conf *) ap_get_module_config(s->module_config, &proxy_module);
    struct proxy_remote *new;
    char *p, *q;
    char *r, *f, *scheme;
    regex_t *reg = NULL;
    int port;

    r = apr_pstrdup(cmd->pool, r1);
    scheme = apr_pstrdup(cmd->pool, r1);
    f = apr_pstrdup(cmd->pool, f1);
    p = strchr(r, ':');
    if (p == NULL || p[1] != '/' || p[2] != '/' || p[3] == '\0') {
        if (regex)
            return "ProxyRemoteMatch: Bad syntax for a remote proxy server";
        else
            return "ProxyRemote: Bad syntax for a remote proxy server";
    }
    else {
        scheme[p-r] = 0;
    }
    q = strchr(p + 3, ':');
    if (q != NULL) {
        if (sscanf(q + 1, "%u", &port) != 1 || port > 65535) {
            if (regex)
                return "ProxyRemoteMatch: Bad syntax for a remote proxy server (bad port number)";
            else
                return "ProxyRemote: Bad syntax for a remote proxy server (bad port number)";
        }
        *q = '\0';
    }
    else
        port = -1;
    *p = '\0';
    if (regex) {
        reg = ap_pregcomp(cmd->pool, f, REG_EXTENDED);
        if (!reg)
            return "Regular expression for ProxyRemoteMatch could not be compiled.";
    }
    else
        if (strchr(f, ':') == NULL)
            ap_str_tolower(f);          /* lowercase scheme */
    ap_str_tolower(p + 3);              /* lowercase hostname */

    if (port == -1) {
        port = apr_uri_port_of_scheme(scheme);
    }

    new = apr_array_push(conf->proxies);
    new->scheme = f;
    new->protocol = r;
    new->hostname = p + 3;
    new->port = port;
    new->regexp = reg;
    new->use_regex = regex;
    return NULL;
}

static const char *
    add_proxy_noregex(cmd_parms *cmd, void *dummy, const char *f1, const char *r1)
{
    return add_proxy(cmd, dummy, f1, r1, 0);
}

static const char *
    add_proxy_regex(cmd_parms *cmd, void *dummy, const char *f1, const char *r1)
{
    return add_proxy(cmd, dummy, f1, r1, 1);
}

static const char *
    add_pass(cmd_parms *cmd, void *dummy, const char *f, const char *r)
{
    server_rec *s = cmd->server;
    proxy_server_conf *conf =
    (proxy_server_conf *) ap_get_module_config(s->module_config, &proxy_module);
    struct proxy_alias *new;
    if (r!=NULL && cmd->path == NULL ) {
        new = apr_array_push(conf->aliases);
        new->fake = f;
        new->real = r;
    } else if (r==NULL && cmd->path != NULL) {
        new = apr_array_push(conf->aliases);
        new->fake = cmd->path;
        new->real = f;
    } else {
        if ( r== NULL)
            return "ProxyPass needs a path when not defined in a location";
        else 
            return "ProxyPass can not have a path when defined in a location";
    }

     return NULL;
}

static const char *
    add_pass_reverse(cmd_parms *cmd, void *dummy, const char *f, const char *r)
{
    server_rec *s = cmd->server;
    proxy_server_conf *conf;
    struct proxy_alias *new;

    conf = (proxy_server_conf *)ap_get_module_config(s->module_config, 
                                                     &proxy_module);
    if (r!=NULL && cmd->path == NULL ) {
        new = apr_array_push(conf->raliases);
        new->fake = f;
        new->real = r;
    } else if (r==NULL && cmd->path != NULL) {
        new = apr_array_push(conf->raliases);
        new->fake = cmd->path;
        new->real = f;
    } else {
        if ( r == NULL)
            return "ProxyPassReverse needs a path when not defined in a location";
        else 
            return "ProxyPassReverse can not have a path when defined in a location";
    }

    return NULL;
}

static const char *
    set_proxy_exclude(cmd_parms *parms, void *dummy, const char *arg)
{
    server_rec *s = parms->server;
    proxy_server_conf *conf =
    ap_get_module_config(s->module_config, &proxy_module);
    struct noproxy_entry *new;
    struct noproxy_entry *list = (struct noproxy_entry *) conf->noproxies->elts;
    struct apr_sockaddr_t *addr;
    int found = 0;
    int i;

    /* Don't duplicate entries */
    for (i = 0; i < conf->noproxies->nelts; i++) {
        if (apr_strnatcasecmp(arg, list[i].name) == 0) { /* ignore case for host names */
            found = 1;
        }
    }

    if (!found) {
        new = apr_array_push(conf->noproxies);
        new->name = arg;
        if (APR_SUCCESS == apr_sockaddr_info_get(&addr, new->name, APR_UNSPEC, 0, 0, parms->pool)) {
            new->addr = addr;
        }
        else {
            new->addr = NULL;
        }
    }
    return NULL;
}

/*
 * Set the ports CONNECT can use
 */
static const char *
    set_allowed_ports(cmd_parms *parms, void *dummy, const char *arg)
{
    server_rec *s = parms->server;
    proxy_server_conf *conf =
        ap_get_module_config(s->module_config, &proxy_module);
    int *New;

    if (!apr_isdigit(arg[0]))
        return "AllowCONNECT: port number must be numeric";

    New = apr_array_push(conf->allowed_connect_ports);
    *New = atoi(arg);
    return NULL;
}

/* Similar to set_proxy_exclude(), but defining directly connected hosts,
 * which should never be accessed via the configured ProxyRemote servers
 */
static const char *
    set_proxy_dirconn(cmd_parms *parms, void *dummy, const char *arg)
{
    server_rec *s = parms->server;
    proxy_server_conf *conf =
    ap_get_module_config(s->module_config, &proxy_module);
    struct dirconn_entry *New;
    struct dirconn_entry *list = (struct dirconn_entry *) conf->dirconn->elts;
    int found = 0;
    int i;

    /* Don't duplicate entries */
    for (i = 0; i < conf->dirconn->nelts; i++) {
        if (strcasecmp(arg, list[i].name) == 0)
            found = 1;
    }

    if (!found) {
        New = apr_array_push(conf->dirconn);
        New->name = apr_pstrdup(parms->pool, arg);
        New->hostaddr = NULL;

        if (renamed_ap_proxy_is_ipaddr(New, parms->pool)) {
#if DEBUGGING
            ap_log_error(APLOG_MARK, APLOG_STARTUP, 0, NULL,
                         "Parsed addr %s", inet_ntoa(New->addr));
            ap_log_error(APLOG_MARK, APLOG_STARTUP, 0, NULL,
                         "Parsed mask %s", inet_ntoa(New->mask));
#endif
        }
        else if (renamed_ap_proxy_is_domainname(New, parms->pool)) {
            ap_str_tolower(New->name);
#if DEBUGGING
            ap_log_error(APLOG_MARK, APLOG_STARTUP, 0, NULL,
                         "Parsed domain %s", New->name);
#endif
        }
        else if (renamed_ap_proxy_is_hostname(New, parms->pool)) {
            ap_str_tolower(New->name);
#if DEBUGGING
            ap_log_error(APLOG_MARK, APLOG_STARTUP, 0, NULL,
                         "Parsed host %s", New->name);
#endif
        }
        else {
            renamed_ap_proxy_is_word(New, parms->pool);
#if DEBUGGING
            fprintf(stderr, "Parsed word %s\n", New->name);
#endif
        }
    }
    return NULL;
}

static const char *
    set_proxy_domain(cmd_parms *parms, void *dummy, const char *arg)
{
    proxy_server_conf *psf =
    ap_get_module_config(parms->server->module_config, &proxy_module);

    if (arg[0] != '.')
        return "ProxyDomain: domain name must start with a dot.";

    psf->domain = arg;
    return NULL;
}

static const char *
    set_proxy_req(cmd_parms *parms, void *dummy, int flag)
{
    proxy_server_conf *psf =
    ap_get_module_config(parms->server->module_config, &proxy_module);

    psf->req = flag;
    psf->req_set = 1;
    return NULL;
}
static const char *
    set_proxy_error_override(cmd_parms *parms, void *dummy, int flag)
{
    proxy_server_conf *psf =
    ap_get_module_config(parms->server->module_config, &proxy_module);

    psf->error_override = flag;
    psf->error_override_set = 1;
    return NULL;
}
static const char *
    set_preserve_host(cmd_parms *parms, void *dummy, int flag)
{
    proxy_server_conf *psf =
    ap_get_module_config(parms->server->module_config, &proxy_module);

    psf->preserve_host = flag;
    psf->preserve_host_set = 1;
    return NULL;
}

static const char *
    set_recv_buffer_size(cmd_parms *parms, void *dummy, const char *arg)
{
    proxy_server_conf *psf =
    ap_get_module_config(parms->server->module_config, &proxy_module);
    int s = atoi(arg);
    if (s < 512 && s != 0) {
        return "ProxyReceiveBufferSize must be >= 512 bytes, or 0 for system default.";
    }

    psf->recv_buffer_size = s;
    psf->recv_buffer_size_set = 1;
    return NULL;
}

static const char *
    set_io_buffer_size(cmd_parms *parms, void *dummy, const char *arg)
{
    proxy_server_conf *psf =
    ap_get_module_config(parms->server->module_config, &proxy_module);
    long s = atol(arg);

    psf->io_buffer_size = ((s > AP_IOBUFSIZE) ? s : AP_IOBUFSIZE);
    psf->io_buffer_size_set = 1;
    return NULL;
}

static const char *
    set_max_forwards(cmd_parms *parms, void *dummy, const char *arg)
{
    proxy_server_conf *psf =
    ap_get_module_config(parms->server->module_config, &proxy_module);
    long s = atol(arg);
    if (s < 0) {
        return "ProxyMaxForwards must be greater or equal to zero..";
    }

    psf->maxfwd = s;
    psf->maxfwd_set = 1;
    return NULL;
}
static const char*
    set_proxy_timeout(cmd_parms *parms, void *dummy, const char *arg)
{
    proxy_server_conf *psf =
    ap_get_module_config(parms->server->module_config, &proxy_module);
    int timeout;

    timeout=atoi(arg);
    if (timeout<1) {
        return "Proxy Timeout must be at least 1 second.";
    }
    psf->timeout_set=1;
    psf->timeout=apr_time_from_sec(timeout);

    return NULL;    
}

static const char*
    set_via_opt(cmd_parms *parms, void *dummy, const char *arg)
{
    proxy_server_conf *psf =
    ap_get_module_config(parms->server->module_config, &proxy_module);

    if (strcasecmp(arg, "Off") == 0)
        psf->viaopt = via_off;
    else if (strcasecmp(arg, "On") == 0)
        psf->viaopt = via_on;
    else if (strcasecmp(arg, "Block") == 0)
        psf->viaopt = via_block;
    else if (strcasecmp(arg, "Full") == 0)
        psf->viaopt = via_full;
    else {
        return "ProxyVia must be one of: "
            "off | on | full | block";
    }

    psf->viaopt_set = 1;
    return NULL;    
}

static const char*
    set_bad_opt(cmd_parms *parms, void *dummy, const char *arg)
{
    proxy_server_conf *psf =
    ap_get_module_config(parms->server->module_config, &proxy_module);

    if (strcasecmp(arg, "IsError") == 0)
        psf->badopt = bad_error;
    else if (strcasecmp(arg, "Ignore") == 0)
        psf->badopt = bad_ignore;
    else if (strcasecmp(arg, "StartBody") == 0)
        psf->badopt = bad_body;
    else {
        return "ProxyBadHeader must be one of: "
            "IsError | Ignore | StartBody";
    }

    psf->badopt_set = 1;
    return NULL;    
}

static void ap_add_per_proxy_conf(server_rec *s, ap_conf_vector_t *dir_config)
{
    proxy_server_conf *sconf = ap_get_module_config(s->module_config,
                                                    &proxy_module);
    void **new_space = (void **)apr_array_push(sconf->sec_proxy);
    
    *new_space = dir_config;
}

static const char *proxysection(cmd_parms *cmd, void *mconfig, const char *arg)
{
    const char *errmsg;
    const char *endp = ap_strrchr_c(arg, '>');
    int old_overrides = cmd->override;
    char *old_path = cmd->path;
    proxy_dir_conf *conf;
    ap_conf_vector_t *new_dir_conf = ap_create_per_dir_config(cmd->pool);
    regex_t *r = NULL;
    const command_rec *thiscmd = cmd->cmd;

    const char *err = ap_check_cmd_context(cmd,
                                           NOT_IN_DIR_LOC_FILE|NOT_IN_LIMIT);
    if (err != NULL) {
        return err;
    }

    if (endp == NULL) {
        return apr_pstrcat(cmd->pool, cmd->cmd->name,
                           "> directive missing closing '>'", NULL);
    }

    arg=apr_pstrndup(cmd->pool, arg, endp-arg);

    if (!arg) {
        if (thiscmd->cmd_data)
            return "<ProxyMatch > block must specify a path";
        else
            return "<Proxy > block must specify a path";
    }

    cmd->path = ap_getword_conf(cmd->pool, &arg);
    cmd->override = OR_ALL|ACCESS_CONF;

    if (!strncasecmp(cmd->path, "proxy:", 6))
        cmd->path += 6;

    /* XXX Ignore case?  What if we proxy a case-insensitive server?!? 
     * While we are at it, shouldn't we also canonicalize the entire
     * scheme?  See proxy_fixup()
     */
    if (thiscmd->cmd_data) { /* <ProxyMatch> */
        r = ap_pregcomp(cmd->pool, cmd->path, REG_EXTENDED);
        if (!r) {
             return "Regex could not be compiled";
        }
    }
    else if (!strcmp(cmd->path, "~")) {
        cmd->path = ap_getword_conf(cmd->pool, &arg);
        if (!cmd->path)
            return "<Proxy ~ > block must specify a path";
        if (strncasecmp(cmd->path, "proxy:", 6))
            cmd->path += 6;
        r = ap_pregcomp(cmd->pool, cmd->path, REG_EXTENDED);
        if (!r) {
             return "Regex could not be compiled";
        }
    }

    /* initialize our config and fetch it */
    conf = ap_set_config_vectors(cmd->server, new_dir_conf, cmd->path,
                                 &proxy_module, cmd->pool);

    errmsg = ap_walk_config(cmd->directive->first_child, cmd, new_dir_conf);
    if (errmsg != NULL)
        return errmsg;

    conf->r = r;
    conf->p = cmd->path;
    conf->p_is_fnmatch = apr_fnmatch_test(conf->p);

    ap_add_per_proxy_conf(cmd->server, new_dir_conf);

    if (*arg != '\0') {
        return apr_pstrcat(cmd->pool, "Multiple ", thiscmd->name,
                           "> arguments not (yet) supported.", NULL);
    }

    cmd->path = old_path;
    cmd->override = old_overrides;

    return NULL;
}

static const command_rec proxy_cmds[] =
{
#ifdef PSODIUM
    // client
    AP_INIT_FLAG("PsodiumProxyRequests", set_proxy_req, NULL, RSRC_CONF,
     "on if the true proxy requests should be accepted."),
    AP_INIT_TAKE1("PsodiumClientDoubleCheckProbability", pso_set_doublecheckprob, NULL, RSRC_CONF,
     "The probability of the proxy doing a double check (a percentage 0-100)."),

    //master
    AP_INIT_TAKE1("PsodiumMasterSlaveKeys", pso_set_slavekeysfile, NULL, RSRC_CONF,
     "The file containing the public keys of the slaves."),
    AP_INIT_TAKE2("PsodiumMasterAuditor", pso_set_auditoraddr, NULL, RSRC_CONF,
     "an IP address:port number and a password."),
    AP_INIT_TAKE1("PsodiumMasterVersionsFile", pso_set_versionsfilename, NULL, RSRC_CONF,
     "The file for persistently storing the VERSIONS administration."),
    AP_INIT_TAKE1("PsodiumMasterLyingClientsFile", pso_set_lyingclientsfilename, NULL, RSRC_CONF,
     "The file for persistently storing the list of lying clients."),

    //slave
    AP_INIT_TAKE1("PsodiumSlaveID", pso_set_slaveid, NULL, RSRC_CONF,
     "This slave's global ID."),
    AP_INIT_TAKE1("PsodiumSlavePrivateKey", pso_set_slaveprivatekeyfilename, NULL, RSRC_CONF,
     "This slave's private key."),

    //general
    AP_INIT_TAKE2("PsodiumBadContentImage", pso_set_badcontent_img, NULL, RSRC_CONF,
     "Filename and MIME type of image pSodium will return in response to BADPLEDGE request (master) or bad digests (proxy)."),
    AP_INIT_TAKE2("PsodiumBadSlaveImage", pso_set_badslave_img, NULL, RSRC_CONF,
     "Filename and MIME type of image pSodium (proxy) will return when reattempting to contact a bad slave" ),
    AP_INIT_TAKE2("PsodiumClientLiedImage", pso_set_clientlied_img, NULL, RSRC_CONF,
     "Filename and MIME type of image pSodium (master) will return when a proxy falsely reported a bad pledge." ),
    AP_INIT_TAKE1("PsodiumTempStorageDir", pso_set_tempstoragedir, NULL, RSRC_CONF,
     "The directory for temporarily storing slave replies."),
    AP_INIT_TAKE1("PsodiumBadSlavesFile", pso_set_badslavesfilename, NULL, RSRC_CONF,
     "The file for persistently storing the list of bad slaves."),
    AP_INIT_TAKE1("PsodiumIntraServerAuthPassword", pso_set_intraserverauthpwd, NULL, RSRC_CONF,
     "The password to use for intra-server authentication."),

/* Be a full proxy replacement */
    AP_INIT_RAW_ARGS("<PsodiumProxy", proxysection, NULL, RSRC_CONF, 
    "Container for directives affecting resources located in the proxied "
    "location"),
    AP_INIT_RAW_ARGS("<PsodiumProxyMatch", proxysection, (void*)1, RSRC_CONF,
    "Container for directives affecting resources located in the proxied "
    "location, in regular expression syntax"),
    AP_INIT_TAKE2("PsodiumProxyRemote", add_proxy_noregex, NULL, RSRC_CONF,
     "a scheme, partial URL or '*' and a proxy server"),
    AP_INIT_TAKE2("PsodiumProxyRemoteMatch", add_proxy_regex, NULL, RSRC_CONF,
     "a regex pattern and a proxy server"),
    AP_INIT_TAKE12("PsodiumProxyPass", add_pass, NULL, RSRC_CONF|ACCESS_CONF,
     "a virtual path and a URL"),
    AP_INIT_TAKE12("PsodiumProxyPassReverse", add_pass_reverse, NULL, RSRC_CONF|ACCESS_CONF,
     "a virtual path and a URL for reverse proxy behaviour"),
    AP_INIT_ITERATE("PsodiumProxyBlock", set_proxy_exclude, NULL, RSRC_CONF,
     "A list of names, hosts or domains to which the proxy will not connect"),
    AP_INIT_TAKE1("PsodiumProxyReceiveBufferSize", set_recv_buffer_size, NULL, RSRC_CONF,
     "Receive buffer size for outgoing HTTP and FTP connections in bytes"),
    AP_INIT_TAKE1("PsodiumProxyIOBufferSize", set_io_buffer_size, NULL, RSRC_CONF,
     "IO buffer size for outgoing HTTP and FTP connections in bytes"),
    AP_INIT_TAKE1("PsodiumProxyMaxForwards", set_max_forwards, NULL, RSRC_CONF,
     "The maximum number of proxies a request may be forwarded through."),
    AP_INIT_ITERATE("PsodiumNoProxy", set_proxy_dirconn, NULL, RSRC_CONF,
     "A list of domains, hosts, or subnets to which the proxy will connect directly"),
    AP_INIT_TAKE1("PsodiumProxyDomain", set_proxy_domain, NULL, RSRC_CONF,
     "The default intranet domain name (in absence of a domain in the URL)"),
    AP_INIT_ITERATE("PsodiumAllowCONNECT", set_allowed_ports, NULL, RSRC_CONF,
     "A list of ports which CONNECT may connect to"),
    AP_INIT_TAKE1("PsodiumProxyVia", set_via_opt, NULL, RSRC_CONF,
     "Configure Via: proxy header header to one of: on | off | block | full"),
    AP_INIT_FLAG("PsodiumProxyErrorOverride", set_proxy_error_override, NULL, RSRC_CONF,
     "use our error handling pages instead of the servers' we are proxying"),
    AP_INIT_FLAG("PsodiumProxyPreserveHost", set_preserve_host, NULL, RSRC_CONF,
     "on if we should preserve host header while proxying"),
    AP_INIT_TAKE1("PsodiumProxyTimeout", set_proxy_timeout, NULL, RSRC_CONF,
     "Set the timeout (in seconds) for a proxied connection. "
     "This overrides the server timeout"),
    AP_INIT_TAKE1("PsodiumProxyBadHeader", set_bad_opt, NULL, RSRC_CONF,
     "How to handle bad header line in response: IsError | Ignore | StartBody"),

#else
    AP_INIT_RAW_ARGS("<Proxy", proxysection, NULL, RSRC_CONF, 
    "Container for directives affecting resources located in the proxied "
    "location"),
    AP_INIT_RAW_ARGS("<ProxyMatch", proxysection, (void*)1, RSRC_CONF,
    "Container for directives affecting resources located in the proxied "
    "location, in regular expression syntax"),
    AP_INIT_FLAG("ProxyRequests", set_proxy_req, NULL, RSRC_CONF,
     "on if the true proxy requests should be accepted"),
    AP_INIT_TAKE2("ProxyRemote", add_proxy_noregex, NULL, RSRC_CONF,
     "a scheme, partial URL or '*' and a proxy server"),
    AP_INIT_TAKE2("ProxyRemoteMatch", add_proxy_regex, NULL, RSRC_CONF,
     "a regex pattern and a proxy server"),
    AP_INIT_TAKE12("ProxyPass", add_pass, NULL, RSRC_CONF|ACCESS_CONF,
     "a virtual path and a URL"),
    AP_INIT_TAKE12("ProxyPassReverse", add_pass_reverse, NULL, RSRC_CONF|ACCESS_CONF,
     "a virtual path and a URL for reverse proxy behaviour"),
    AP_INIT_ITERATE("ProxyBlock", set_proxy_exclude, NULL, RSRC_CONF,
     "A list of names, hosts or domains to which the proxy will not connect"),
    AP_INIT_TAKE1("ProxyReceiveBufferSize", set_recv_buffer_size, NULL, RSRC_CONF,
     "Receive buffer size for outgoing HTTP and FTP connections in bytes"),
    AP_INIT_TAKE1("ProxyIOBufferSize", set_io_buffer_size, NULL, RSRC_CONF,
     "IO buffer size for outgoing HTTP and FTP connections in bytes"),
    AP_INIT_TAKE1("ProxyMaxForwards", set_max_forwards, NULL, RSRC_CONF,
     "The maximum number of proxies a request may be forwarded through."),
    AP_INIT_ITERATE("NoProxy", set_proxy_dirconn, NULL, RSRC_CONF,
     "A list of domains, hosts, or subnets to which the proxy will connect directly"),
    AP_INIT_TAKE1("ProxyDomain", set_proxy_domain, NULL, RSRC_CONF,
     "The default intranet domain name (in absence of a domain in the URL)"),
    AP_INIT_ITERATE("AllowCONNECT", set_allowed_ports, NULL, RSRC_CONF,
     "A list of ports which CONNECT may connect to"),
    AP_INIT_TAKE1("ProxyVia", set_via_opt, NULL, RSRC_CONF,
     "Configure Via: proxy header header to one of: on | off | block | full"),
    AP_INIT_FLAG("ProxyErrorOverride", set_proxy_error_override, NULL, RSRC_CONF,
     "use our error handling pages instead of the servers' we are proxying"),
    AP_INIT_FLAG("ProxyPreserveHost", set_preserve_host, NULL, RSRC_CONF,
     "on if we should preserve host header while proxying"),
    AP_INIT_TAKE1("ProxyTimeout", set_proxy_timeout, NULL, RSRC_CONF,
     "Set the timeout (in seconds) for a proxied connection. "
     "This overrides the server timeout"),
    AP_INIT_TAKE1("ProxyBadHeader", set_bad_opt, NULL, RSRC_CONF,
     "How to handle bad header line in response: IsError | Ignore | StartBody"),
#endif 
    {NULL}
};
APR_DECLARE_OPTIONAL_FN(int, ssl_proxy_enable, (conn_rec *));
APR_DECLARE_OPTIONAL_FN(int, ssl_engine_disable, (conn_rec *));

static APR_OPTIONAL_FN_TYPE(ssl_proxy_enable) *proxy_ssl_enable = NULL;
static APR_OPTIONAL_FN_TYPE(ssl_engine_disable) *proxy_ssl_disable = NULL;

PROXY_DECLARE(int) renamed_ap_proxy_ssl_enable(conn_rec *c)
{
    /* 
     * if c == NULL just check if the optional function was imported
     * else run the optional function so ssl filters are inserted
     */
    if (proxy_ssl_enable) {
        return c ? proxy_ssl_enable(c) : 1;
    }

    return 0;
}

PROXY_DECLARE(int) renamed_ap_proxy_ssl_disable(conn_rec *c)
{
    if (proxy_ssl_disable) {
        return proxy_ssl_disable(c);
    }

    return 0;
}

#ifndef PSODIUM
static int proxy_post_config(apr_pool_t *pconf, apr_pool_t *plog,
                             apr_pool_t *ptemp, server_rec *s)
{
    proxy_ssl_enable = APR_RETRIEVE_OPTIONAL_FN(ssl_proxy_enable);
    proxy_ssl_disable = APR_RETRIEVE_OPTIONAL_FN(ssl_engine_disable);

    return OK;
}
#endif


static void register_hooks(apr_pool_t *p)
{
    /* fixup before mod_rewrite, so that the proxied url will not
     * escaped accidentally by our fixup.
     */
#ifdef PSODIUM
    /* pSodium was initially designed to run parallel to mod_proxy. We should,
     * however, have the upper hand so we can handle pSodium enabled sites before
     * mod_proxy has a go. That is, mod_psodium must go before mod_proxy.
     *
     * At present, mod_psodium replaces mod_proxy for HTTP proxying. It should
     * be possible to use mod_proxy for FTP proxying, as the set of directives
     * is disjunct. In other words, we have prefixes all mod_proxy directives
     * with "Psodium" so there are no clashes.
     *
     * For the translate_name hook, mod_psodium must run also before Globule, 
     * such that it can tell the Globule redirector (part of master currently) 
     * to which slaves it should NOT redirect, cause they're bad. In other 
     * cases, mod_psodium should run after (e.g. to receive instructions from
     * replication policies on how long a reply is valid).
     */
    
    static const char * const predecessorModsGeneral[]={ "mod_globule.cpp", NULL };
    static const char * const successorModsGeneral[]={ "mod_proxy.c", NULL };
    static const char * const predecessorModsBeforeGlobule = NULL;
    static const char * const successorModsBeforeGlobule[]={ "mod_globule.cpp", "mod_proxy.c", NULL };
    static const char * const successorModsOriginalException[]={ "mod_proxy.c", "mod_rewrite.c", NULL };
    
    /* post read_request handling */
    ap_hook_post_read_request(proxy_detect, predecessorModsGeneral, successorModsGeneral, APR_HOOK_FIRST);
    /* handler */
    ap_hook_handler( proxy_handler, predecessorModsGeneral, successorModsGeneral, APR_HOOK_FIRST);
    /* filename-to-URI translation */
    ap_hook_translate_name( proxy_trans, predecessorModsBeforeGlobule, successorModsBeforeGlobule, APR_HOOK_FIRST);
    /* walk <Proxy > entries and suppress default TRACE behavior */
    ap_hook_map_to_storage(proxy_map_location, predecessorModsGeneral, successorModsGeneral, APR_HOOK_FIRST);
    /* fixups */
    ap_hook_fixups(proxy_fixup, predecessorModsGeneral, successorModsOriginalException, APR_HOOK_FIRST);
    /* register output filter */
#ifdef OLD
    ap_register_output_filter( OUTPUT_FILTER_NAME, pso_output_filter, NULL,
                               AP_FTYPE_CONTENT_SET );
#endif
    pso_output_filter_handle =
        ap_register_output_filter( OUTPUT_FILTER_NAME, pso_output_filter, NULL,
                               AP_FTYPE_CONTENT_SET );

    ap_hook_insert_filter( pso_add_output_filter_to_request, predecessorModsGeneral, successorModsGeneral, APR_HOOK_FIRST );
    /* post config handling */
    ap_hook_post_config( pso_post_config, predecessorModsGeneral, successorModsGeneral, APR_HOOK_MIDDLE);
    //ap_hook_child_init( pso_child_init, NULL, NULL, APR_HOOK_MIDDLE);
#else
    ap_hook_post_config(proxy_post_config, NULL, successorModsGeneral, APR_HOOK_MIDDLE);
#endif
}

module AP_MODULE_DECLARE_DATA psodium_module =
{
    STANDARD20_MODULE_STUFF,
    create_proxy_dir_config,    /* create per-directory config structure */
    merge_proxy_dir_config,     /* merge per-directory config structures */
    create_proxy_config,        /* create per-server config structure */
    merge_proxy_config,         /* merge per-server config structures */
    proxy_cmds,                 /* command table */
    register_hooks
};

APR_HOOK_STRUCT(
        APR_HOOK_LINK(scheme_handler)
        APR_HOOK_LINK(canon_handler)
)

#ifdef PSODIUM
APR_IMPLEMENT_EXTERNAL_HOOK_RUN_FIRST(psodium, PSO, int, scheme_handler, 
                                     (request_rec *r, proxy_server_conf *conf, 
                                     char *url, const char *psodiumhost, 
                                     apr_port_t psodiumport),(r,conf,url,
                                     psodiumhost,psodiumport),DECLINED)
APR_IMPLEMENT_EXTERNAL_HOOK_RUN_FIRST(psodium, PSO, int, canon_handler, 
                                     (request_rec *r, char *url),(r,
                                     url),DECLINED)
APR_IMPLEMENT_OPTIONAL_HOOK_RUN_ALL(psodium, PSO, int, fixups,
                                    (request_rec *r), (r),
                                    OK, DECLINED)
#else
APR_IMPLEMENT_EXTERNAL_HOOK_RUN_FIRST(proxy, PROXY, int, scheme_handler, 
                                     (request_rec *r, proxy_server_conf *conf, 
                                     char *url, const char *proxyhost, 
                                     apr_port_t proxyport),(r,conf,url,
                                     proxyhost,proxyport),DECLINED)
APR_IMPLEMENT_EXTERNAL_HOOK_RUN_FIRST(proxy, PROXY, int, canon_handler, 
                                     (request_rec *r, char *url),(r,
                                     url),DECLINED)
APR_IMPLEMENT_OPTIONAL_HOOK_RUN_ALL(proxy, PROXY, int, fixups,
                                    (request_rec *r), (r),
                                    OK, DECLINED)
#endif
