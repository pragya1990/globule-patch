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
#ifndef _versions_HXX
#define _versions_HXX

/* Originally this map mapped master url path (/export/index.html) to
 * a slave (slave hostname:slave port) to expiry times and digests.
 * 
 * With the new virtual host stuff in Globule this no longer works.
 * My quick hack is to remove the master url path and replace slave id with
 * a full slave url. Concretely, there is one master URL "" that maps to
 * a list of full slave URLs
 * 
 * "" -> http://slave1.fulldomain/slave/path/file.xml -> time+digest
 * "" -> http://slave2.fulldomain/slave/path2/file.xhtml -> time+digest
 * 
 * This is inefficient memory wise, so will need to be fixed someday.
 * Look for the keyword QUICK in the code to find all places affected
 * by this quick hack.
 *
 * Arno, 12-1-2005.
 */

#include "../globule/alloc/Gstring.hpp"
#include "../globule/alloc/Allocator.hpp"
#include "Slave2DigestsMap.hpp"

// CAREFUL: all data related to this data structure must be in shared mem, 
// that includes the string that is the index of the map!!!

//typedef Gmap(const Gstring *,Slave2DigestsMap *)   Versions;
typedef std::map<const Gstring *,Slave2DigestsMap *,ltGstring, rmmallocator<Slave2DigestsMap *> >   Versions;

void print_Versions( Versions *v, apr_pool_t *pool );

#endif  /* _versions_HXX */

