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
// APL
/* ========================================================================= 
 * File:   SlaveRecord.h
 * =========================================================================
 */
#ifndef _SlaveRecord_H
#define _SlaveRecord_H

#include <apr_pools.h>
#include "../globule/alloc/Gstring.hpp"


// CAREFUL: all data related to this data structure must be in shared mem, 
// that includes the string that is the index of the map!!!


class SlaveRecord
{
public:
    Gstring *master_id;
    Gstring *pubkey_pem; /* string containined PEM encoded public key */
    Gstring *auditor_addr; /* auditor for the site the slave mirrors */
    Gstring *uri_prefix;
    /* Future:
    boolean trustworthy;
    */
    

    SlaveRecord( Gstring *MasterID, Gstring *PubKeyPEM, Gstring *AuditorAddr, Gstring *SlaveURIPrefix )
    {
        master_id = MasterID;
        pubkey_pem = PubKeyPEM;
        auditor_addr = AuditorAddr;
        uri_prefix = SlaveURIPrefix;
    }
};


void print_SlaveRecord( SlaveRecord *sr, apr_pool_t *pool );


#endif  /* _SlaveRecord_H */

