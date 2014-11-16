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
#ifndef _REPORTREADER_H
#define _REPORTREADER_H

#include <apr.h>
#include <apr_file_io.h>
#include <ctype.h>
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#include <string>
#include "mapping.h"

using std::string;

class reportreader {
private:
  apr_int64_t *storage;
  Mappings& m;
  int nlines, idx, nfields;
  Mapping& indexes;
  void setvalue(Mappings &m, apr_int64_t *values, char *k, char *v,
                bool tryasnumber=true);
  void load(apr_pool_t* p, const char *fname, apr_off_t fstart, apr_off_t fend,
            apr_int64_t **rtnstorage, int *rtnnlines);
public:
  reportreader(apr_pool_t* ptemp, Mappings& maps,
               const char *fname = "report.log",
               apr_off_t fstart = 0,
               apr_off_t fend = -1);
  ~reportreader();
  void reset() { idx = 0; };
  bool more() { return idx < nlines; };
  void next() { idx++; }
  int count() { return nlines; };
  int length() { return nfields; };
  int count(char* fieldname);
  int count(string& fieldname);
  apr_int64_t* field() const;
  apr_int64_t& field(int* fieldid, char* fieldname) const;
  apr_int64_t& field(int* fieldid, string& fieldname) const;
};

#endif /* _REPORTREADER_H */
