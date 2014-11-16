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

// TODO: Handle APR_HAS_LARGE_FILE issue, as described in server/core.c


#include "mod_psodium.h"
#include "../globule/utilities.h"

#define TEMPFILE_NOTE   "pSodium-Temp-File"

/* create does "delete on close" only when not double checking */
#if APR_HAS_SENDFILE
#define CREATE_FLAGS    (APR_CREATE|APR_READ|APR_WRITE|APR_BINARY|APR_BUFFERED|APR_SENDFILE_ENABLED)
#else
#define CREATE_FLAGS    (APR_CREATE|APR_READ|APR_WRITE|APR_BINARY|APR_BUFFERED)
#endif

/* Reopen does "delete on close" always */
#if APR_HAS_SENDFILE
#define REOPEN_FLAGS    (APR_READ|APR_WRITE|APR_BINARY|APR_BUFFERED|APR_DELONCLOSE|APR_SENDFILE_ENABLED)
#else
#define REOPEN_FLAGS    (APR_READ|APR_WRITE|APR_BINARY|APR_BUFFERED|APR_DELONCLOSE)
#endif


#define TEMPFILE_PREFIX         "temp-storage-"
#define REQUIRED_MKTEMP_POSTFIX "XXXXXX"

#define DELREC_POOL_KEY         "pSodium-Delrec"
struct delete_record
{
    char *filename;
    apr_file_t* file;
    apr_pool_t *pool; //needed by apr_file_remove(), hope that still works at cleanup time
};
apr_status_t parent_cleanup_callback( void *data );
apr_status_t child_cleanup_callback( void *data );



apr_status_t pso_bb_to_temp_storage( psodium_conf *conf, 
                                     request_rec *r, 
                                     apr_file_t **temp_file_out, 
                                     apr_bucket_brigade *bb,
                                     apr_bool_t delonclose )
{
    apr_status_t status=APR_SUCCESS;
    char *pathtempl=NULL;
    struct iovec *vecarray;
    int nvec=0;
    apr_size_t nbytes=0;
    apr_bucket *e=NULL;

    //DPRINTF( DEBUG1, ("psodium: client: %s %d: Storing bb %p on temp storage\n", bb )); 
    DPRINTF( DEBUG4, ("psodium: client: Storing bb %p on temp storage\n", bb ));

    if (*temp_file_out == NULL)
    {
        // No file created yet
        DPRINTF( DEBUG5, ("psodium: client: Creating new temp file\n" )); 
        
        pathtempl = apr_pstrcat( r->pool, conf->tempstoragedirname, TEMPFILE_PREFIX, REQUIRED_MKTEMP_POSTFIX, NULL );

        DPRINTF( DEBUG5, ("psodium: client: Creating new temp file: pathtempl=%s\n", pathtempl )); 

        if (delonclose)
        {
            status = apr_file_mktemp( temp_file_out, pathtempl, 
                                      CREATE_FLAGS|APR_DELONCLOSE, 
                                      r->pool );
        }
        else
        {
            status = apr_file_mktemp( temp_file_out, pathtempl, 
                                      CREATE_FLAGS, 
                                      r->pool );
        }
        if (status != APR_SUCCESS)
        {
            DPRINTF( DEBUG0, ("Cannot create temporary file from template %s\n", pathtempl ));
            return status;
        }

        // Record filename, so we can find it back if we do a delonclose
        apr_table_set( r->notes, TEMP_STORAGE_FILENAME_NOTE, pathtempl );

        
        // Setup cleanup
        if (!delonclose)
        {
            // With delonclose the pool cleanup will automatically remove
            // the temp storage. If not, we have to take measures that
            // the file will be removed in case of an error. We have to 
            // support this option because delonclose cannot always be used
            // in pSodium, see client_http.c
            //
            struct delete_record *delrec=NULL;
            
            delrec = (struct delete_record *)apr_palloc( r->pool, sizeof( struct delete_record ) );
            delrec->filename = pathtempl; //also allocated from r->pool, so no problem
            delrec->file = *temp_file_out; //let's hope this is too
            delrec->pool = r->pool; // Hmmm... recursion alert!
            apr_pool_cleanup_register( r->pool, (void *)delrec, parent_cleanup_callback, child_cleanup_callback );
            
            // We need the delrec to deregister this cleanup, so keep it
            status = apr_pool_userdata_set( delrec, DELREC_POOL_KEY, NULL, r->pool );
            if (status != APR_SUCCESS)
            {
                DPRINTF( DEBUG0, ("Cannot register file for removal when request done!\n" ));
                return status;
            }
        }
        
        
        DPRINTF( DEBUG5, ("psodium: client: Creating new temp file: path=%s\n", pathtempl )); 
    }

    DPRINTF( DEBUG5, ("psodium: client: Temp storage: before length, bb is %p\n", bb )); 
#ifdef DEBUG
   if (1)
   {
        apr_off_t readbytes;
        apr_brigade_length( bb, 0, &readbytes);
        DPRINTF( DEBUG5, ("psodium: client: Moving brigade total len %d to temp storage\n", readbytes )); 
    } 
#endif
  
    // Write brigade as I/O vector
    // Braindead brigade_to_iovec function!
    for (e = APR_BRIGADE_FIRST(bb); e != APR_BRIGADE_SENTINEL(bb); e = APR_BUCKET_NEXT(e)) 
    {   
        DPRINTF( DEBUG5, ("psodium: client: Temp storage: bucket to iovec\n" )); 
        nvec++;
    }
    vecarray = apr_palloc( r->pool, nvec*sizeof( struct iovec ) );
    if (vecarray == NULL)
        return APR_ENOMEM;
    status = apr_brigade_to_iovec( bb, vecarray, &nvec );
    if (status != APR_SUCCESS)
    {
        DPRINTF( DEBUG0, ("Cannot batch write to temporary storage!\n" ));
        return status;
    }

    DPRINTF( DEBUG5, ("psodium: client: Moving iovec length %d to temp storage\n", nvec )); 

    status = apr_file_writev( *temp_file_out, vecarray, nvec, &nbytes );
    if (status != APR_SUCCESS)
    {
        DPRINTF( DEBUG0, ("Write to temporary storage failed!\n" ));
        return status;
    }

    DPRINTF( DEBUG4, ("psodium: client: Wrote %d bytes to temp storage\n", nbytes )); 
    
    return APR_SUCCESS;
}


/*
 * Two callbacks for deleteing the file when an error occurs outside
 * our control (e.g. connection close in final network output filter)
 */
apr_status_t parent_cleanup_callback( void *data )
{
    struct delete_record *delrec=data;
    
    DPRINTF( DEBUG1, ("psodium: client: Cleaning up temp storage after error\n" )); 
    
    if (delrec->file != NULL)
    {
        apr_file_close( delrec->file );
    }
    
    if (delrec->filename == NULL)
    {
        DPRINTF( DEBUG1, ("psodium: client: Temp storage cleanup: filename NULL???\n" )); 
    }
    else
    {
        apr_file_remove( delrec->filename, delrec->pool );
    }
    
    return APR_SUCCESS; // what else?
}


apr_status_t child_cleanup_callback( void *data )
{
    return APR_SUCCESS;
}



apr_status_t pso_temp_storage_to_filebb( request_rec *r, apr_file_t *temp_file, apr_bucket_brigade **newbb_out )
{
    apr_status_t status=APR_SUCCESS;
    apr_off_t   offset=0;
    apr_bucket  *file_buck, *eos=NULL;

    DPRINTF( DEBUG1, ("psodium: client: Reading data from temp storage for transmission to client\n" )); 

    if (temp_file == NULL)
    {
        DPRINTF( DEBUG0, ("psodium: client: Reading data from temp storage: Filename lost!\n" )); 
        return APR_EINVAL;
    }

    /* Turn temporary file into brigade containing file bucket */
    offset=0;
    status = apr_file_seek( temp_file, APR_END, &offset );
    if (status != APR_SUCCESS)
    {
        char *temp_filename = NULL;
        (void)apr_file_name_get( &temp_filename, temp_file );
        if (temp_filename == NULL)
            temp_filename = "Unknown filename!";
        DPRINTF( DEBUG0, ("Cannot seek in temporary file %s\n", temp_filename ));
        return status;
    }

    DPRINTF( DEBUG5, ("psodium: client: Reading data from temp storage: File size is %d\n", offset )); 

    *newbb_out = apr_brigade_create(r->pool, r->connection->bucket_alloc );
    if (*newbb_out == NULL)
    {
        DPRINTF( DEBUG0, ("Cannot create memory structure for temporary file!\n" ));
        return APR_ENOMEM;
    }

    file_buck = apr_bucket_file_create( temp_file, 0, offset, r->pool, (*newbb_out)->bucket_alloc );
    if (status != APR_SUCCESS)
    {
        DPRINTF( DEBUG0, ("Cannot fill memory structure for temporary file!\n" ));
        return status;
    }

    APR_BRIGADE_INSERT_TAIL( *newbb_out, file_buck );

    // It appears the next filter stage can't deal with EOS buckets that are not empty,
    // although other code suggests such buckets are valid. So, we use an extra empty EOS.
    eos = apr_bucket_eos_create( (*newbb_out)->bucket_alloc );
    if (eos == NULL)
    {
        DPRINTF( DEBUG0, ("Cannot fill final memory structure for temporary file!\n" ));
        return APR_ENOMEM;
    }

    APR_BRIGADE_INSERT_TAIL( *newbb_out, eos ); 

    /* {
        apr_off_t readbytes;
        apr_brigade_length( *newbb_out, 0, &readbytes);
        DPRINTF( DEBUG1, ("psodium: client: Temp storage turned into filebb sized %d\n", readbytes )); 
    } */


    return APR_SUCCESS;
}



apr_status_t pso_reopen_temp_storage( psodium_conf *conf, 
                                     request_rec *r, 
                                     apr_file_t **temp_file_out )
{
    apr_status_t status=APR_SUCCESS;
    const char *fn = NULL;
    struct delete_record *delrec=NULL;

    fn = apr_table_get( r->notes, TEMP_STORAGE_FILENAME_NOTE );

    /* Cleanup
     * If we're reopening the previous open was without delonclose.
     * In that case we registered our own cleanup, which we now
     * have to deregister, because from now on the file will be
     * cleared up via the delonclose mechanism.
     */
    // We need the data pointer, otherwise cleanup_kill won't find
    // the callback :-(
    status = apr_pool_userdata_get( &delrec, DELREC_POOL_KEY, r->pool );
    if (status != APR_SUCCESS)
    {
        DPRINTF( DEBUG0, ("Cannot access administration on temporary storage !\n" ));
        return status;
    }
    
    apr_pool_cleanup_kill( r->pool, delrec, parent_cleanup_callback );
    // NICE: cleanup child cleanup as well
    
    DPRINTF( DEBUG1, ("psodium: client: Reopening temp file: path=%s\n", fn )); 

    status = apr_file_open( temp_file_out, fn, REOPEN_FLAGS, 
                                APR_OS_DEFAULT, r->pool );
    if (status != APR_SUCCESS)
    {
        DPRINTF( DEBUG0, ("Cannot reopen temporary storage file %s!\n", fn ));
    }
    return status;
}
