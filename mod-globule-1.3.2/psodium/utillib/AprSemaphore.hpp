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
// 
// File:   AprSemaphore.h
// Author: arno
//
// Created on August 28, 2003, 4:32 PM
//

#ifndef _AprSemaphore_H
#define _AprSemaphore_H

#ifndef APR_HAS_THREADS
#warning "Auditor won't work unless we have threads!"
#define APR_HAS_THREADS
#endif

#include <apr_thread_mutex.h>
#include <apr_thread_cond.h>
#include <apr_thread_proc.h>

#include "utillib/AprError.hpp"
#include "../../globule/utilities.h"

class AprSemaphore
{
protected:
    int _count;
    apr_thread_cond_t   *_cond;
    apr_thread_mutex_t   *_mutex;
    
public:
    AprSemaphore( apr_pool_t *pool ) throw( AprError )
    {
        _count=0;
         
        //apr_status_t status = apr_thread_mutex_create( &_mutex, APR_THREAD_MUTEX_NESTED, pool );
        //apr_status_t status = apr_thread_mutex_create( &_mutex, APR_THREAD_MUTEX_DEFAULT, pool );
        // Apache's srclib/apr-util/misc/apr_queue.c says: "nested doesn't work ;("
        apr_status_t status = apr_thread_mutex_create( &_mutex, APR_THREAD_MUTEX_UNNESTED, pool );
        if (status != APR_SUCCESS)
            throw AprError( status ); // no new

        status = apr_thread_cond_create( &_cond, pool );
        if (status != APR_SUCCESS)
            throw AprError( status ); // no new
    }

    ~AprSemaphore() throw( AprError )
    {
        apr_status_t status = apr_thread_mutex_destroy( _mutex );
        if (status != APR_SUCCESS)
            throw AprError( status ); // no new

        status = apr_thread_cond_destroy( _cond );
        if (status != APR_SUCCESS)
            throw AprError( status ); // no new
        
    }
    
    void down() throw( AprError )
    {
        apr_status_t status = apr_thread_mutex_lock( _mutex );
        if (status != APR_SUCCESS)
        {
            DPRINTF( DEBUG0, ("Lock failed err=%d!\n", status ));
            throw AprError( status ); // no new
        }

        while( _count == 0 )
        {
            status = apr_thread_cond_wait( _cond, _mutex );
            if (status != APR_SUCCESS)
            {
                DPRINTF( DEBUG0, ("Thread wait failed! err=%d!\n", status ));
                (void)apr_thread_mutex_unlock( _mutex );
                throw AprError( status ); // no new
            }
        }
        _count--;

        status = apr_thread_mutex_unlock( _mutex );
        if (status != APR_SUCCESS)
        {
            DPRINTF( DEBUG0, ("Unlock failed! err=%d!\n", status ));
            throw AprError( status ); // no new
        }
    }
    
    
    void up() throw( AprError )
    {
        apr_status_t status = apr_thread_mutex_lock( _mutex );
        if (status != APR_SUCCESS)
        {
            DPRINTF( DEBUG0, ("Lock failed! err=%d!\n", status ));
            throw AprError( status ); // no new
        }

        _count++;
        if (_count == 1)
        {
            // Awaken!
            status = apr_thread_cond_broadcast( _cond );
            if (status != APR_SUCCESS)
            {
                DPRINTF( DEBUG0, ("Broadcast thread wakeup failed! err=%d!\n", status ));
                (void)apr_thread_mutex_unlock( _mutex );
                throw AprError( status ); // no new    
            }
        }

        status = apr_thread_mutex_unlock( _mutex );
        // Arno: this unlock used to work, don't know why it fails now...
        if (status != APR_SUCCESS)
        {
            DPRINTF( DEBUG0, ("Unlock failed! err=%d!\n", status ));
            throw AprError( status ); // no new
        }
    }
};

#endif  /* _AprSemaphore_H */

