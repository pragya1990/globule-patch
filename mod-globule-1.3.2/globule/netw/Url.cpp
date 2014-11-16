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
#include <apr.h>
#include <apr_strings.h>
#include <httpd.h>
#include <string>
#include <ctype.h>
#include "utilities.h"
#include "netw/Url.hpp"

apr_uri_t
Url::construct(apr_pool_t* p, const char* scheme, const char* hostname,
               apr_port_t port, const char *rest)
  throw(UrlException)
{
  if(!rest)
    rest = "/";
  return construct(p, apr_psprintf(p, "%s://%s:%hu%s", scheme, hostname, port, rest));
}

apr_uri_t
Url::construct(apr_pool_t* p, const char* uri_str,
               const char* extension)
  throw(UrlException)
{
  const char* infix = "";
  if(uri_str[strlen(uri_str)-1]=='/') {
    if(extension[0]=='/')
      ++extension;
  } else {
    switch(extension[0]) {
    case '/':
    case '?':
    case '#':
      break;
    default:
      infix = "/";
    }
  }
  return construct(p, apr_psprintf(p, "%s%s%s", uri_str, infix, extension));
}

apr_uri_t
Url::construct(apr_pool_t* p, const char* uri_str)
  throw(UrlException)
{
  apr_uri_t rtnuri;
  if(!strstr(uri_str,"://"))
    uri_str = apr_psprintf(p, "http://%s", uri_str);
  /* apr_uri.h header file broken (0.9.5), documents wrong return
   * value for apr_uri_parse.
   * if the URL is invalid, the return status will be ok, but we will
   * see an improperly scanned URL and corrupted memory.
   */
  apr_status_t status = apr_uri_parse(p, uri_str, &rtnuri);
  if(status != APR_SUCCESS) {
    throw UrlException(mkstring() << "invalid URL " << uri_str);
  }
  if(rtnuri.port_str && strcmp(rtnuri.port_str, "")) {
    int nitems = 0;
    unsigned long toobig_portno;
    nitems = sscanf(rtnuri.port_str, "%lu", &toobig_portno );
    if(nitems == 1) {
      if (toobig_portno > 65535)
        throw "Invalid URL: port number too high!";
    }
  }
  return rtnuri;
}

Url::Url()
  throw()
  : _hostaddr(0)
{
  _uri.scheme = _uri.hostinfo = _uri.hostname = _uri.port_str = _uri.path = 0;
  _uri.user = _uri.password = 0;
}

Url::Url(apr_pool_t* pool, const apr_uri_t* u)
  throw()
  : _uri(*u), _hostaddr(0)
{
  if(_uri.scheme)   _uri.scheme   = apr_pstrdup(pool, _uri.scheme);
  if(_uri.hostinfo) _uri.hostinfo = apr_pstrdup(pool, _uri.hostinfo);
  if(_uri.hostname) _uri.hostname = apr_pstrdup(pool, _uri.hostname);
  if(_uri.port_str) _uri.port_str = apr_pstrdup(pool, _uri.port_str);
  if(_uri.user)     _uri.user     = apr_pstrdup(pool, _uri.user);
  if(_uri.password) _uri.password = apr_pstrdup(pool, _uri.password);
  if(_uri.path)     _uri.path     = apr_pstrdup(pool, _uri.path);
  if(_uri.query)    _uri.query    = apr_pstrdup(pool, _uri.query);
  if(_uri.fragment) _uri.fragment = apr_pstrdup(pool, _uri.fragment);
  _uri.hostent       = NULL;
  _uri.dns_looked_up = 0;
  _uri.dns_resolved  = 0;
}

Url::Url(apr_pool_t* pool, const Url& u, const char* extension)
  throw(UrlException)
  : _uri(construct(pool, u(pool), extension)), _hostaddr(0)
{
}

Url::Url(apr_pool_t* pool, const char* url_str)
  throw(UrlException)
  : _uri(construct(pool, url_str)), _hostaddr(0)
{
}

Url::Url(apr_pool_t* pool, const std::string urlstring)
  throw(UrlException)
  : _uri(construct(pool, urlstring.c_str())), _hostaddr(0)
{
}

Url::Url(apr_pool_t* pool, const char* scheme, const char* hostname,
         apr_port_t port, const char* rest)
  throw(UrlException)
  : _uri(construct(pool, scheme, hostname, port, rest)), _hostaddr(0)
{
}

Url::Url(const Url& u)
  throw()
  : _uri(u._uri), _hostaddr(0)
{
}

Url::Url(Url& u) throw()
  : _hostaddr(0)
{
  memcpy(&_uri, &u._uri, sizeof(apr_uri_t));
}

Url::Url(apr_pool_t* pool, const Url& org)
  throw()
  : _uri(org._uri), _hostaddr(0)
{
  if(_uri.scheme)   _uri.scheme   = apr_pstrdup(pool, _uri.scheme);
  if(_uri.hostinfo) _uri.hostinfo = apr_pstrdup(pool, _uri.hostinfo);
  if(_uri.hostname) _uri.hostname = apr_pstrdup(pool, _uri.hostname);
  if(_uri.port_str) _uri.port_str = apr_pstrdup(pool, _uri.port_str);
  if(_uri.user)     _uri.user     = apr_pstrdup(pool, _uri.user);
  if(_uri.password) _uri.password = apr_pstrdup(pool, _uri.password);
  if(_uri.path)     _uri.path     = apr_pstrdup(pool, _uri.path);
  if(_uri.query)    _uri.query    = apr_pstrdup(pool, _uri.query);
  if(_uri.fragment) _uri.fragment = apr_pstrdup(pool, _uri.fragment);
  _uri.hostent       = NULL;
  _uri.dns_looked_up = 0;
  _uri.dns_resolved  = 0;
}

apr_status_t
Url::address(apr_pool_t* pool, apr_sockaddr_t **addrptr, bool cache) throw()
{
  apr_status_t status = APR_SUCCESS;
  if(!_hostaddr) {
    apr_port_t targetport;
    if((targetport = port()) <= 0) {
      if(scheme() && !strcmp(scheme(),""))
	targetport = apr_uri_port_of_scheme(scheme());
      else
	targetport = apr_uri_port_of_scheme("http");
    }
    status = apr_sockaddr_info_get(addrptr,host(),APR_INET,targetport,0,pool);
    if(cache)
      _hostaddr = *addrptr;
  } else
    *addrptr = _hostaddr;
  return status;
}

Url&
Url::operator=(const Url& org) throw()
{
  _uri = org._uri;
  return *this;
}

char *
Url::operator()(apr_pool_t* pool) const throw()
{
  return apr_uri_unparse(pool, &_uri, 0);
}

char *
Url::pathquery(apr_pool_t *pool) const throw()
{
  // Normalize following RFC2396bis: leave out empty query
  if(!_uri.query || !strcmp(_uri.query,""))
    return _uri.path;
  else
    return apr_pstrcat(pool, _uri.path, "?", _uri.query, NULL);
}

bool
Url::operator==(const Url& u) const throw()
{
  return !(
    (_uri.scheme && _uri.scheme[0] && u._uri.scheme && u._uri.scheme[0]
     && strcasecmp(_uri.scheme,u._uri.scheme)) ||
    (_uri.hostname && _uri.hostname[0] && u._uri.hostname && u._uri.hostname[0]
     && strcasecmp(_uri.hostname,u._uri.hostname)) ||
    (_uri.path && _uri.path[0] && u._uri.path && u._uri.path[0]
     && strcmp(_uri.path,u._uri.path))
    );
}

void
Url::normalize(apr_pool_t* pool) throw()
{
  apr_uri_t* uriptr = &_uri;
  (void)urlutil_normalize((*this)(pool), &uriptr, 0, pool);
}

/* The canonical form we use is based on the URI comparison rules for HTTP 
 * as defined in RFC2616: HTTP/1.1, $3.2.3., which refer to RFC2396. 
 * When compatible with the RFC2616 rules, we use the additional rules laid
 * down in a draft revision of RFC2396, 2396bis, $6.
 * See http://gbiv.com/protocols/uri/rev-2002/rfc2396bis.html
 *
 * 1. Case normalization: Scheme and host are case-insensitive (2616) and are
 *    normalized to lower case (suggestion of 2396bis). In addition, in 
 *    the path component, hex digits in percentage-encoded triplets are 
 *    normalized to uppercase (2396bis, compatible with 2616, which says
 *    http://ABC.com.%7Esmith/home.html == http://ABC.com:/%7esmith/home.html)
 *
 * 2. Encoding normalization: RFC2616 does not prescribe any. To normalize we 
 *    decode any percent-encoded octets in the path component that correspond 
 *    to an unreserved character, as described in RFC2396bis, $2.3. This 
 *    procedure is compatible with RFC2616, which says that  "Characters
 *    other than those in the "reserved" and "unsafe" sets (see RFC 2396 [..])
 *    are equivalent to their ""% HEX HEX" encoding." This is a conservative
 *    conversion, as the reserved set of RFC2396 is larger than the reserved 
 *    set of RFC2396bis.
 *
 * 3. Empty component normalization: RFC2616 prescribes the 
 *    normalization of empty or unspecified ports, and an empty path. 
 *    This is handled in Step 5. In addition, we follow RFC2396bis: 
 *    superfluous separators are removed, with the exception of 
 *    an double-slash delimiter indicating an authority component, when the 
 *    authority is empty.
 *
 * 4. Path segment normalization: I.e., how to handle "." and "..". In this
 *    area there is a conflict between RFC2396, used by HTTP/1.1 and the 
 *    proposed RFC2396bis. In 2396, "." and ".." are normalized only when
 *    resolving relative-path URIs (i.e., without a scheme and a path that 
 *    does not begin with /) to absolute form. In 2396bis, one can also 
 *    normalize in absolute paths. We must stick with RFC2396 and 2616 in 
 *    this case, until the new stuff becomes standard, which means we 
 *    don't do anything.
 *
 * 5. (HTTP) Scheme-based normalization: As prescribed by both RFC2616 and 
 *    2396bis, an explicit :port where port is empty or 80 is normalized to 
 *    one where the port and its ":" delimiter are elided. An empty path is 
 *    normalized to "/"
 *
 * Param: *dest_url_inout may be NULL, in which case it is allocated from the
 *        pool.
 */

/*
 * The set of unreserved characters in RFC2396 is larger than in 2396bis.
 * I will only normalize the 2396bis ones from their percent-encoded form 
 * back to their character, which should be safe, as they are a subset of 2396.
 */
static char rfc2396bis_unreserved[] = {
  'a' , 'b' , 'c' , 'd' , 'e' , 'f' , 'g' , 'h' , 'i' ,
  'j' , 'k' , 'l' , 'm' , 'n' , 'o' , 'p' , 'q' , 'r' ,
  's' , 't' , 'u' , 'v' , 'w' , 'x' , 'y' , 'z' ,
  'A' , 'B' , 'C' , 'D' , 'E' , 'F' , 'G' , 'H' , 'I' ,
  'J' , 'K' , 'L' , 'M' , 'N' , 'O' , 'P' , 'Q' , 'R' ,
  'S' , 'T' , 'U' , 'V' , 'W' , 'X' , 'Y' , 'Z' ,
  '0' , '1' , '2' , '3' , '4' , '5' , '6' , '7' ,
  '8' , '9' , '-' , '.' , '_' , '~', '\0' }; // must end with '\0'!

static void strtolower( char *s )
{
  size_t i=0;
  if (s != NULL)
    for (i=0; i<strlen( s ); i++)
      s[i] = tolower( (int)s[i] );
}


static void strntoupper( char *s, size_t len )
{
  size_t i=0;
  if (s != NULL)
    for (i=0; i<len; i++)
      s[i] = toupper( (int)s[i] );
}
      
apr_status_t
Url::urlutil_normalize(const char *src_url_str, apr_uri_t **dest_url_inout, 
                       char **dest_url_str_out, apr_pool_t *_pool)
  throw(UrlException)
{
    apr_status_t status;
    apr_port_t default_port;
    apr_uri_t *_uri=NULL;
    char *_uri_str=NULL;
    char *new_hostinfo="";
    char *new_ptr=NULL;
    int octet_code;
    int nitems;
    
    if (*dest_url_inout == NULL)
    {    
        *dest_url_inout = (apr_uri_t *)apr_palloc( _pool, sizeof( apr_uri_t ) );
    }
    _uri = *dest_url_inout;
    
    // apr_uri.h header file broken (0.9.5), documents wrong return
    // value for apr_uri_parse.
    status = apr_uri_parse( _pool, src_url_str, _uri );
    if(status != APR_SUCCESS)
        throw UrlException(mkstring() << "invalid URL " << src_url_str);

    /*
     * Numbers in front of comments indicate the type of normalization
     * done in the code following it. See Url.hpp for an explanation
     * of each type.
     */
    // 1. Case normalization:
    strtolower( _uri->scheme );
    
    // Do NOT look at trailing dot : i.e. "example.com" != "example.com."
    // for example, the first may refer to example.com.apache.org. in
    // the context of apache.org.
    strtolower( _uri->hostname );

    // 2. Encoding normalization:
    // Any percent-encoded octets that correspond to
    // an unreserved character, as described in RFC2396bis, $2.3 are decoded.
    //
    // I'll only do this for the path, and leave query and fragment alone.
    if (!(_uri->path == NULL || !strcmp( _uri->path, "" )))
    {
        char *ptr = _uri->path;
        do
        {
            ptr = strchr( ptr, '%' );
            if (ptr != NULL)
            {
                if (strlen( ptr ) >=3 )
                {
                    // Copy candidate bytes, so we can convert to upper case
                    char dup[3+1];
                    dup[3]='\0';
                    strncpy( dup, ptr, 3 );
                    strntoupper( dup, 3 );
                    
                    // See if candidate bytes are a triplet (% HEX HEX)
                    octet_code = 0;
                    nitems = sscanf( dup, "%%%02X", &octet_code );
                    if (nitems == 1)
                    {
                        // We have a triplet.
                        size_t i=0;
                        // 1. Case normalization, also for the hex digits ($6.2.2.1)
                        strntoupper( ptr, 3 ); // yes, in original path
                        
                        for (i=0; i<strlen( rfc2396bis_unreserved ); i++)
                        {
                            if (octet_code == rfc2396bis_unreserved[i])
                            {
                                // We have an encoded rfc2396bis_unreserved octet, replace
                                char *new_path = (char *)apr_palloc( _pool, (-2+strlen( _uri->path )+1)*sizeof( char ) ); // +1 for '\0'
                                
                                // copy prefix
                                size_t prefix_len = ptr-_uri->path;
                                strncpy( new_path, _uri->path, prefix_len );
                                // replace triplet
                                new_path[ prefix_len ] = rfc2396bis_unreserved[i];
                                // copy postfix
                                strncpy( &new_path[prefix_len + 1], ptr+3, strlen( ptr )-3 );
                                new_path[ strlen( _uri->path )-2 ] = '\0';
                                
                                // Switch ptr over to new path string
                                new_ptr = new_path + (ptr-_uri->path);
                                _uri->path = new_path;
                                ptr = new_ptr;
                                break;
                            }
                        }
                    }
                }
                ptr++;
            }
        }
        while( ptr != NULL );
    }
        

    /*    
     * Generate URL string and do further normalization:
     */
    _uri_str = "";
    // 3. Empty component normalization:
    if (_uri->scheme != NULL && strcmp( _uri->scheme, "" ))
    {
        _uri_str = apr_pstrcat( _pool, _uri_str, _uri->scheme, ":", NULL );
    }
    
    // ASSUMPTION: only allow authority part when scheme is set
    if (_uri->scheme != NULL && strcmp( _uri->scheme, "" ))
    {
        // The conversion /// -> //localhost/ is not assumed for HTTP, 
        // See RFC2396bis $6.2.3
        if (_uri->hostinfo == NULL || !strcmp( _uri->hostinfo, "" ))
        {
            // Store normalized
            _uri->hostinfo = "";
            _uri_str = apr_pstrcat( _pool, _uri_str, "//" , NULL );
        }
        else
        {
            _uri_str = apr_pstrcat( _pool, _uri_str, "//", NULL );

            // 3. Empty component normalization:
            if (_uri->user != NULL && strcmp( _uri->user, "" ))
            {
                new_hostinfo = apr_pstrcat( _pool, new_hostinfo, _uri->user, NULL );
            }
            if (_uri->password != NULL && strcmp( _uri->password, "" ))
            {
                // I assume an empty user is also legal
                new_hostinfo = apr_pstrcat( _pool, new_hostinfo, ":", _uri->password, NULL );
            }

            if ((_uri->user != NULL && strcmp( _uri->user, "" )) || (_uri->password != NULL && strcmp( _uri->password, "" )))
            {
                // Yes, there was some user info 
                new_hostinfo = apr_pstrcat( _pool, new_hostinfo, "@", NULL );
            }

            if (_uri->hostname != NULL && strcmp( _uri->hostname, "" ))
            {
                new_hostinfo = apr_pstrcat( _pool, new_hostinfo, _uri->hostname, NULL );
            }

            // 5. (HTTP) Scheme-based normalization
            // Default port for scheme (e.g. 80 for HTTP) is never shown in the URL.
            default_port = APR_URI_HTTP_DEFAULT_PORT;
            if (_uri->scheme != NULL && strcmp( _uri->scheme, ""))
            {
                default_port = apr_uri_port_of_scheme( _uri->scheme );
            }
            //Store normalized
            if (_uri->port_str == NULL || !strcmp( _uri->port_str, ""))
            {
                _uri->port = default_port;
            }
            if (_uri->port != default_port)
            {
                new_hostinfo = apr_psprintf( _pool, "%s:%hu", new_hostinfo, _uri->port );
            }
            _uri_str = apr_pstrcat( _pool, _uri_str, new_hostinfo, NULL );
           _uri->hostinfo = new_hostinfo;
        }
    }
    
    // 5. (HTTP) Scheme-based normalization: An empty path is normalized to "/"
    if (_uri->path == NULL || !strcmp( _uri->path, "" ))
    {
        // Store normalized
        _uri->path = "/";
    }
    
    // 4. No path segment normalization.
    _uri_str = apr_pstrcat( _pool, _uri_str, _uri->path, NULL );
    
    // 3. Empty component normalization:
    if (_uri->query != NULL && strcmp( _uri->query, ""))
    {
        _uri_str = apr_pstrcat( _pool, _uri_str, "?", _uri->query, NULL );
    }
    // I assume fragment identifier is part of a URI (not clear)
    if (_uri->fragment != NULL && strcmp( _uri->fragment, ""))
    {
        _uri_str = apr_pstrcat( _pool, _uri_str, "#", _uri->fragment, NULL );
    }
    
    if(dest_url_str_out)
      *dest_url_str_out = _uri_str;
    return APR_SUCCESS;
}

#ifndef STANDALONE_APR

gUrl::gUrl() throw()
  : Url()
{
}

gUrl::gUrl(apr_pool_t* pool, const std::string url_str)
  throw (UrlException)
  : Url(pool, url_str)
{
  apr_uri_t org = _uri;
  if(_uri.scheme) {
    _uri.scheme = (char*) rmmmemory::allocate(strlen(_uri.scheme)+1);
    strcpy(_uri.scheme, org.scheme);
  }
  if(_uri.hostinfo) {
    _uri.hostinfo = (char*) rmmmemory::allocate(strlen(_uri.hostinfo)+1);
    strcpy(_uri.hostinfo, org.hostinfo);
  }
  if(_uri.hostname) {
    _uri.hostname = (char*) rmmmemory::allocate(strlen(_uri.hostname)+1);
    strcpy(_uri.hostname, org.hostname);
  }
  if(_uri.port_str) {
    _uri.port_str = (char*) rmmmemory::allocate(strlen(_uri.port_str)+1);
    strcpy(_uri.port_str, org.port_str);
  }
  if(_uri.user) {
    _uri.user = (char*) rmmmemory::allocate(strlen(_uri.user)+1);
    strcpy(_uri.user, org.user);
  }
  if(_uri.password) {
    _uri.password = (char*) rmmmemory::allocate(strlen(_uri.password)+1);
    strcpy(_uri.password, org.password);
  }
  if(_uri.path) {
    _uri.path = (char*) rmmmemory::allocate(strlen(_uri.path)+1);
    strcpy(_uri.path, org.path);
  }
  _uri.query    = 0;
  _uri.fragment = 0;
  _uri.hostent  = NULL;
  _uri.dns_looked_up = 0;
  _uri.dns_resolved  = 0;
}

gUrl::gUrl(const Url& org) throw()
  : Url(org)
{
  if(_uri.scheme) {
    _uri.scheme = (char*) rmmmemory::allocate(strlen(_uri.scheme)+1);
    strcpy(_uri.scheme, org._uri.scheme);
  }
  if(_uri.hostinfo) {
    _uri.hostinfo = (char*) rmmmemory::allocate(strlen(_uri.hostinfo)+1);
    strcpy(_uri.hostinfo, org._uri.hostinfo);
  }
  if(_uri.hostname) {
    _uri.hostname = (char*) rmmmemory::allocate(strlen(_uri.hostname)+1);
    strcpy(_uri.hostname, org._uri.hostname);
  }
  if(_uri.port_str) {
    _uri.port_str = (char*) rmmmemory::allocate(strlen(_uri.port_str)+1);
    strcpy(_uri.port_str, org._uri.port_str);
  }
  if(_uri.user) {
    _uri.user = (char*) rmmmemory::allocate(strlen(_uri.user)+1);
    strcpy(_uri.user, org._uri.user);
  }
  if(_uri.password) {
    _uri.password = (char*) rmmmemory::allocate(strlen(_uri.password)+1);
    strcpy(_uri.password, org._uri.password);
  }
  if(_uri.path) {
    _uri.path = (char*) rmmmemory::allocate(strlen(_uri.path)+1);
    strcpy(_uri.path, org._uri.path);
  }
  _uri.user     = 0;
  _uri.password = 0;
  _uri.query    = 0;
  _uri.fragment = 0;
  _uri.hostent  = NULL;
  _uri.dns_looked_up = 0;
  _uri.dns_resolved  = 0;
}

gUrl::gUrl(const gUrl& org) throw()
  : Url(org)
{
  if(_uri.scheme) {
    _uri.scheme = (char*) rmmmemory::allocate(strlen(_uri.scheme)+1);
    strcpy(_uri.scheme, org._uri.scheme);
  }
  if(_uri.hostinfo) {
    _uri.hostinfo = (char*) rmmmemory::allocate(strlen(_uri.hostinfo)+1);
    strcpy(_uri.hostinfo, org._uri.hostinfo);
  }
  if(_uri.hostname) {
    _uri.hostname = (char*) rmmmemory::allocate(strlen(_uri.hostname)+1);
    strcpy(_uri.hostname, org._uri.hostname);
  }
  if(_uri.port_str) {
    _uri.port_str = (char*) rmmmemory::allocate(strlen(_uri.port_str)+1);
    strcpy(_uri.port_str, org._uri.port_str);
  }
  if(_uri.user) {
    _uri.user = (char*) rmmmemory::allocate(strlen(_uri.user)+1);
    strcpy(_uri.user, org._uri.user);
  }
  if(_uri.password) {
    _uri.password = (char*) rmmmemory::allocate(strlen(_uri.password)+1);
    strcpy(_uri.password, org._uri.password);
  }
  if(_uri.path) {
    _uri.path = (char*) rmmmemory::allocate(strlen(_uri.path)+1);
    strcpy(_uri.path, org._uri.path);
  }
  _uri.user     = 0;
  _uri.password = 0;
  _uri.query    = 0;
  _uri.fragment = 0;
  _uri.hostent  = NULL;
  _uri.dns_looked_up = 0;
  _uri.dns_resolved  = 0;
}

gUrl::~gUrl() throw()
{
  if(_uri.scheme)   rmmmemory::deallocate(_uri.scheme);
  if(_uri.hostinfo) rmmmemory::deallocate(_uri.hostinfo);
  if(_uri.hostname) rmmmemory::deallocate(_uri.hostname);
  if(_uri.port_str) rmmmemory::deallocate(_uri.port_str);
  if(_uri.user)     rmmmemory::deallocate(_uri.user);
  if(_uri.password) rmmmemory::deallocate(_uri.password);
  if(_uri.path)     rmmmemory::deallocate(_uri.path);
  _uri.scheme = _uri.hostinfo = _uri.hostname = _uri.port_str = _uri.path = 0;
}

gUrl&
gUrl::operator=(const gUrl& org) throw()
{
  if(_uri.scheme)   rmmmemory::deallocate(_uri.scheme);
  if(_uri.hostinfo) rmmmemory::deallocate(_uri.hostinfo);
  if(_uri.hostname) rmmmemory::deallocate(_uri.hostname);
  if(_uri.port_str) rmmmemory::deallocate(_uri.port_str);
  if(_uri.user)     rmmmemory::deallocate(_uri.user);
  if(_uri.password) rmmmemory::deallocate(_uri.password);
  if(_uri.path)     rmmmemory::deallocate(_uri.path);
  _uri = org._uri;
  if(_uri.scheme) {
    _uri.scheme = (char*) rmmmemory::allocate(strlen(_uri.scheme)+1);
    strcpy(_uri.scheme, org._uri.scheme);
  }
  if(_uri.hostinfo) {
    _uri.hostinfo = (char*) rmmmemory::allocate(strlen(_uri.hostinfo)+1);
    strcpy(_uri.hostinfo, org._uri.hostinfo);
  }
  if(_uri.hostname) {
    _uri.hostname = (char*) rmmmemory::allocate(strlen(_uri.hostname)+1);
    strcpy(_uri.hostname, org._uri.hostname);
  }
  if(_uri.port_str) {
    _uri.port_str = (char*) rmmmemory::allocate(strlen(_uri.port_str)+1);
    strcpy(_uri.port_str, org._uri.port_str);
  }
  if(_uri.user) {
    _uri.user = (char*) rmmmemory::allocate(strlen(_uri.user)+1);
    strcpy(_uri.user, org._uri.user);
  }
  if(_uri.password) {
    _uri.password = (char*) rmmmemory::allocate(strlen(_uri.password)+1);
    strcpy(_uri.password, org._uri.password);
  }
  if(_uri.path) {
    _uri.path = (char*) rmmmemory::allocate(strlen(_uri.path)+1);
    strcpy(_uri.path, org._uri.path);
  }
  _uri.query    = 0;
  _uri.fragment = 0;
  _uri.hostent  = NULL;
  _uri.dns_looked_up = 0;
  _uri.dns_resolved  = 0;
  return *this;
}

Input&
operator>>(Input& in, gUrl& u) throw()
{
  std::string s;
  in >> s;
  u = gUrl(in.pool(), s);
  return in;
}

Output&
operator<<(Output& out, const gUrl& u) throw()
{
  return out << u(out.pool());
}

#endif
