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
#include <httpd.h>
#include <apr.h>
#include <apr_lib.h>
#include <apr_strings.h>
#include <apr_file_io.h>
#include <ctype.h>
#include <iostream>
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#include "reportreader.h"

void
reportreader::setvalue(Mappings &m, apr_int64_t *values,
                       char *k, char *v, bool tryasnumber)
{
  char *s;
  int column;
  column = indexes[k];
  if(column < 0)
    return;
  //ap_assert(column != 0);
  if(v) {
    if(tryasnumber) {
      values[column] = apr_strtoi64(v,&s,0);
    }
    if(!tryasnumber || *s != '\0' || s == v)
      values[column] = m[k][v];
  } else
    values[column] = m[k][""];
}
  
static int cmpfn(const void *a, const void *b)
{
  return (int)(  ((apr_int64_t*)a)[1] - ((apr_int64_t*)b)[1]  );
}

void
reportreader::load(apr_pool_t* pool,
                   const char *fname, apr_off_t fstart, apr_off_t fend,
                   apr_int64_t **rtnstorage, int *rtnnlines)
{
  apr_off_t fcurrent;
  char *s, *k, *v;
  apr_file_t *fdes;
  apr_int64_t *storage, *aux;
  int totlines, nlines;
  char line[1024];
  bool numberic = false; // may actually be uninitialized
  char ch;
  
  nfields = indexes.count();
  storage = (apr_int64_t*) malloc(nfields*sizeof(apr_int64_t)*(totlines=1000));
  nlines = 0;
  apr_file_open(&fdes, fname, APR_READ|APR_BUFFERED, APR_OS_DEFAULT, pool);
  if(fend < 0) {
    fend = 0;
    apr_file_seek(fdes, APR_END, &fend);
  }
  apr_file_seek(fdes, APR_SET, &fstart);
  while((fcurrent=0, apr_file_seek(fdes,APR_CUR,&fcurrent), fcurrent < fend) &&
        !apr_file_gets(line, sizeof(line), fdes))
  {
    if(nlines == totlines) {
      aux = (apr_int64_t*) realloc(storage, nfields*sizeof(apr_int64_t)*(totlines*=2));
      if(aux == NULL) {
        free(storage);
        apr_file_close(fdes);
        return;
      } else
        storage = aux;
    }
    for(s=k=line,v=NULL,ch='\0'; apr_isspace(*s); s++)
      ;
    if(*s && *s != '#') {
      for(int i=0; i<nfields; i++)
        storage[nlines*nfields+i] = 0;
      for(s=k=line,v=NULL,ch='\0'; *s; s++)
        if(ch=='\0' || *s==ch || (apr_isspace(*s) && ch==' '))
          switch(*s) {
          case ' ':
          case '\t':
          case '\n':
          case '\r':
            *s = '\0';
            if(*k)
              if(k[1] == '\0' && !v) {
                storage[nlines*nfields] = *k;
              } else
                setvalue(m, &storage[nlines*nfields], k, v, numberic);
            k = &s[1];
            v = NULL;
            ch = '\0';
            break;
          case ':':
          case ';':
          case '=':
            numberic = (*s == '=');
            *s = '\0';
            v = &s[1];
            ch = (*s == '=' ? 0xff : ' ');
            break;
          }
      if(*k)
        if(k[1] == '\0' && !v) {
          storage[nlines*nfields] = *k;
        } else
          setvalue(m, &storage[nlines*nfields], k, v, numberic);
      ++nlines;
    }
  }
  apr_file_close(fdes);

  qsort(storage, nlines, nfields*sizeof(apr_int64_t), cmpfn);

  *rtnstorage = storage;
  *rtnnlines = nlines;
}

reportreader::reportreader(apr_pool_t* ptemp,
                           Mappings& maps,
                           const char *fname,
                           apr_off_t fstart,
                           apr_off_t fend)
  : m(maps), indexes(m[""])
{
  load(ptemp, fname, fstart, fend, &storage, &nlines);
  idx = 0;
  nfields = m[""].count();
};

reportreader::~reportreader() {
  free(storage);
};

int
reportreader::count(char* fieldname)
{
  return m[fieldname].count();
}

int
reportreader::count(string& fieldname)
{
  return m[fieldname].count();
}

apr_int64_t *
reportreader::field() const
{
  return &storage[idx*nfields];
}

apr_int64_t&
reportreader::field(int* fieldid, char* fieldname) const
{
  if(fieldid)
    if(*fieldid > 0)
      return storage[idx*nfields + *fieldid];
    else
      return storage[idx*nfields + (*fieldid = m[""][fieldname])];
  else
    return storage[idx*nfields + m[""][fieldname]];
}
