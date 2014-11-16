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
// File:   OnDiskHashTable.h
// Author: arno
//
// Created on August 27, 2003, 9:19 AM
//

#ifndef _OnDiskHashTable_H
#define _OnDiskHashTable_H

#include <math.h>
#include "AprOpenFileVector.hpp"
#include "utillib/AprError.hpp"


#define TABLE_LENGTH           10
#define RANDOM_BLOCK_LEN       24

#define CREATE_FLAGS           (APR_CREATE|APR_READ|APR_WRITE|APR_BINARY|APR_BUFFERED|APR_DELONCLOSE|APR_TRUNCATE)

#define ELEM_LEN_MAX_STRLEN    NUMBER_MAX_STRLEN( apr_size_t )


class OnDiskHashTable
{
protected:
    AprOpenFileVector   *_vector;
    bool                _switch_files;
    int                 _curr_file_idx; // index into table
    apr_file_t          *_curr_file;
    bool                _table_done;
    const char          *_fn_prefix;
public:
    OnDiskHashTable()
    {
        _vector = new AprOpenFileVector();
        _switch_files=true;
        _curr_file_idx=0; 
        _curr_file=NULL;
        _fn_prefix="unknown prefix";
        _table_done = false;
    }
    
    // pool does cleanup when destroyed
    
    void init( const char *FullFilenamesPrefix, apr_pool_t *pool ) 
    throw( AprError )
    {
        int i=0;
        apr_file_t *file=NULL;
        apr_status_t status=APR_SUCCESS;

        _fn_prefix = apr_pstrdup( pool, FullFilenamesPrefix );

        // Open files
        for (i=0; i<TABLE_LENGTH; i++)
        {
            char *fn = genFilename( i, pool );
            status = apr_file_open( &file, fn, CREATE_FLAGS, APR_OS_DEFAULT, pool );
            if (status != APR_SUCCESS)
                throw AprError( status ); // no new, files closed by pool
            
            DPRINTF( DEBUG5, ("psodium: auditor: Table row %s is file %p\n", fn, file ));
            
            _vector->insert( _vector->end(), file );
        }
    }
    

    /*
     * TODO: make thread safe
     */
    void insert( const char *elem, apr_size_t elem_len ) 
    throw( AprError )
    {
        apr_status_t status = APR_SUCCESS;
        int idx=0;

        if (_table_done)
        {
            throw AprError( APR_EINVAL ); //no new
        }
        
        // Pick random table entry. 
        // CAREFUL: If we start using a hash, it should not depend on client 
        // input. Using client input would enable denial of service attacks on 
        // the table.
    
        idx = calcRandomIndex();
        
        _curr_file = (*_vector)[ idx ];
        
        //DPRINTF( DEBUG1, ("psodium: auditor: Prefixed size str is %d\n", ELEM_LEN_MAX_STRLEN ));
        
        // Write length of element as string first, NICE: binary not string
        char *len_str = new char[ ELEM_LEN_MAX_STRLEN+1 ];  // +1 for string
        sprintf( len_str, "%0*d", ELEM_LEN_MAX_STRLEN, elem_len );
        
        apr_size_t bytes_2write_written = strlen( len_str );
        status = apr_file_write( _curr_file, len_str, &bytes_2write_written ); 
        if (status != APR_SUCCESS)
            throw AprError( status ); // contents of file inconsistent, fix is TODO
        
        delete[] len_str; // We're not in Kansas anymore, Toto

        // Write element
        bytes_2write_written = elem_len;
        status = apr_file_write( _curr_file, elem, &bytes_2write_written ); 
        if (status != APR_SUCCESS)
            throw AprError( status ); // contents of file inconsistent, fix is TODO

        // TEST
        apr_file_flush( _curr_file );
    }

    
    /*
     * Once you started retrieving using this method, you cannot insert()
     * anymore.
     *
     * Thread synchronization is done using external semaphore
     */
    bool getNextElement( char **elem_out, apr_size_t *elem_len_out ) 
    throw( AprError )
    {
        char len_str[ ELEM_LEN_MAX_STRLEN+1 ]; //+1 for string
        apr_size_t bytes_read = ELEM_LEN_MAX_STRLEN;
        apr_status_t status=APR_SUCCESS;
        
        if (_table_done)
        {
            return false; //success
        }
        
        if (_switch_files == true)
        {
            apr_off_t   soffset=0;
            // Let's go
            _curr_file = (*_vector)[ _curr_file_idx ];
            
            // Seek to begin of file
            status = apr_file_seek( _curr_file, APR_SET, &soffset );
            if (status != APR_SUCCESS)
                throw AprError( status );
            
            _switch_files = false;
        }
        
        DPRINTF( DEBUG5, ("psodium: auditor: getNext: curr file is %p (idx %d)\n", _curr_file, _curr_file_idx ));
        
        status = apr_file_read( _curr_file, len_str, &bytes_read );
        if (status == APR_EOF)
        {
            // End of file reached, time to switch
            _curr_file_idx++;
            if (_curr_file_idx == TABLE_LENGTH)
            {
                // That's all folks
                _curr_file_idx %= TABLE_LENGTH; // for subsequent calls
                _table_done = true;
                return false; // success
            }
            _switch_files = true;
            
            // Let's recurse
            return getNextElement( elem_out, elem_len_out );
        }
        else if (status != APR_SUCCESS)
            throw AprError( status );
        
        len_str[ bytes_read ] = '\0';
        int ret = pso_atoN( len_str, elem_len_out, sizeof( *elem_len_out ) );
        if (ret == -1)
        {
            DPRINTF( DEBUG1, ("OnDiskHashTable: element len not number!\n" ));
            return APR_EINVAL;
        }
        
            DPRINTF( DEBUG5, ("OnDiskHashTable: element len is %d!\n", *elem_len_out ));
        
        *elem_out = new char[ *elem_len_out ]; // delete done by caller
        
        bytes_read = *elem_len_out;
        status = apr_file_read( _curr_file, *elem_out, &bytes_read );
        if (status != APR_SUCCESS)
            throw AprError( status );
        
        return true;
    }
    
    
    void clear( apr_pool_t *pool ) 
    throw( AprError )
    {
        apr_status_t status=APR_SUCCESS;
        int i=0;
        for (i=0; i<TABLE_LENGTH; i++)
        {
            char *fn = genFilename( i, pool );
    
            DPRINTF( DEBUG3, ("Clearing on-disk hashtable slot %d, file %s\n", i, fn ));

            /*
             * Clear file radically, truncating file don't work
             */
            status = apr_file_close( (*_vector)[i] );
            if (status != APR_SUCCESS)
                throw AprError( status );
                
            // Closing the file should also delete it, as we are using
            // DELONCLOSE semantics. However, to be sure:
            (void)apr_file_remove( fn, pool );
        }
        _vector->clear();
        // Recreate files
        init( _fn_prefix, pool );
        _table_done = false;
    }
    
    
    
protected:
    char *genFilename( int i, apr_pool_t *pool )
    {
        int postfix_len = ((int)log10( TABLE_LENGTH ))+1;
        return apr_psprintf( pool, "%s%0*d", _fn_prefix, postfix_len, i );
    }
    

    int calcRandomIndex()
    throw( AprError )
    {
        apr_status_t status=APR_SUCCESS;
        unsigned char block[RANDOM_BLOCK_LEN];
        apr_size_t i=0;
        long sum=0L;

        status = apr_generate_random_bytes( block, RANDOM_BLOCK_LEN );
        if (status != APR_SUCCESS)
            throw AprError( status );
        
        // Poor man's random
        for (i=0; i<RANDOM_BLOCK_LEN; i++)
        {
            sum += block[i];
        }
        return sum % TABLE_LENGTH;
    }



};


#endif  /* _OnDiskHashTable_H */

