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
#include "apr_file_io.h"
#include "apr_pools.h"
#include "apr_allocator.h"
#include <ctype.h>
#include <iostream>
#include <map>
#include <vector>
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#include "reportreader.h"

using namespace std;

Mapping::Mapping(bool autostore)
  : store(autostore), uniqsequence(0)
{
  *this += "";
}

void Mapping::operator+=(string name) {
  int index = uniqsequence++;
  a.insert(make_pair<string,int>(name,index));
  b.insert(make_pair<int,string>(index,name));
}

int Mapping::operator[](string name) const {
  int rtnvalue = a[name];
  if(rtnvalue == 0 && name != "") {
    if(store) {
      b[uniqsequence] = name;
      return a[name] = uniqsequence++;
    } else
      return -1;
  } else
    return rtnvalue;
}

string& Mapping::operator[](int index) const {
  return b[index];
}

void Mapping::insert(pair<string,int> item) {
  if(uniqsequence <= item.second)
    uniqsequence = item.second + 1;
  a.insert(make_pair<string,int>(item.first,item.second));
  b.insert(make_pair<int,string>(item.second,item.first));
}
