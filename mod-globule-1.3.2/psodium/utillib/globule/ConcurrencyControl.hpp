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
/*============================================================================
 * ConcurrencyControl.hpp
 * 
 * Author: arno
 *============================================================================
 */
#ifndef _ConcurrencyControl_HPP
#define _ConcurrencyControl_HPP

#include "../globule/utilities.h"

#define LOCK(x)     {DOUT( DEBUG4, "Block" ); if (x == NULL) { DOUT( DEBUG0, "lock:: is NULL!"); } else { int err=x->lock(); DOUT( DEBUG4, "Alock err=" << err ); } }
#define TRYLOCK(x)  {apr_status_t XstatusX=APR_SUCCESS; DOUT( DEBUG4, "Btrylock" ); if (x == NULL) { GDEBUG( "trylock:: is NULL!"); } else { XstatusX = x->trylock(); } if (XstatusX == APR_SUCCESS) { GDEBUG( "Attempt succeeded"); } else if (APR_STATUS_IS_EBUSY( XstatusX )) { GDEBUG( "Attempt busy"); } else { GDEBUG( "Attempt failed" );} }
#define UNLOCK(x)   {DOUT( DEBUG4, "Bunlock" ); if (x == NULL) { GDEBUG( "unlock:: is NULL!"); } else { int err = x->unlock(); DOUT( DEBUG4, "A--lock err=" << err );} }

#endif  /* _ConcurrencyControl_HPP */

