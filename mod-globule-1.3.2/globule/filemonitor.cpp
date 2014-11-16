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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "utilities.h"
#include "filemonitor.hpp"
#include "event/FileMonitorEvent.hpp"
#include "event/RegisterEvent.hpp"

FileMonitor::FileMonitor(Handler* parent, apr_pool_t* p,
                         const Url& dummylocation)
  throw()
  : Handler(parent,p,dummylocation)
{
}

void
FileMonitor::flush(apr_pool_t* p) throw()
{
}

bool
FileMonitor::handle(GlobuleEvent& evt) throw()
{
  if(evt.target() == this) {
    gstring file;
    DEBUGACTION(FileMonitor,handle,begin);
    _lock.lock();
    switch(evt.type()) {
    case GlobuleEvent::FILE_MONITOR_REG: {
      RegisterEvent& ev = (RegisterEvent&)evt;
      if(_filemap.empty()) {
        RegisterEvent ev(evt.pool,GlobuleEvent::fmreceiver,RegisterEvent::FILE_MONITOR_PERIOD);
        if(!GlobuleEvent::submit(ev))
          DIAG(evt.context,(MONITOR(internal)),("FileMonitor cannot register to HeartBeat"));
      }
      gmap<const gstring,FileDesc>::iterator iter;
      iter = _filemap.find(ev.filename().c_str());
      if(iter == _filemap.end())
        iter = _filemap.insert(std::pair<const gstring,FileDesc>(ev.filename().c_str(),FileDesc(evt.pool,ev.filename().c_str()))).first;
      iter->second.addListener(ev.destination());
      break;
    }
    case GlobuleEvent::FILE_MONITOR_UNREG: {
      gmap<const gstring,FileDesc>::iterator iter;
      RegisterEvent& ev = (RegisterEvent&) evt;
      iter = _filemap.find(ev.filename().c_str());
      if(iter == _filemap.end()) {
        /* FIXME: Skip the following warning.  The warning itself should
         * normally be valid, but because now we always need to unregister the
         * file (as normally all documents would be registered).  However for
         * non-existent files this is not valid, so we get this warning.
         */
        // DIAG(evt.context,(MONITOR(warning)),("Unregistering non-monitored file"));
      } else {
        iter->second.removeListener(ev.destination());
        if(iter->second.listeners() == iter->second.end())
          _filemap.erase(iter);
      }
      break;
    }
    case GlobuleEvent::HEARTBEAT_EVT: {
      check_files(evt.pool, evt.context);
      break;
    }
    default:
      DIAG(evt.context,(MONITOR(internal)),("FileMonitor received unknown event type"));
      break;
    }
    _lock.unlock();
    DEBUGACTION(FileMonitor,handle,end);
    return true;
  } else
    return false;
}

void
FileMonitor::check_files(apr_pool_t* p, Context* ctx) throw()
{
  gmap<const gstring,FileDesc>::iterator iter;
  gset<const Handler*>::const_iterator iter2;
  
  DIAG(ctx,(MONITOR(detail)),("File monitor run\n"));
  for(iter=_filemap.begin(); iter!=_filemap.end(); iter++) {
    if(iter->second.updated(p)) {
      if(iter->second.deleted())
        DIAG(ctx,(MONITOR(notice)),("File %s was deleted",iter->first.c_str()));
      else
        DIAG(ctx,(MONITOR(notice)),("File %s was modified",iter->first.c_str()));
      for(iter2=iter->second.listeners();
          iter2 != iter->second.end();
          iter2++)
        {
          FileMonitorEvent ev(p, ctx, *iter2, iter->first,
                              iter->second._lastmod, iter->second._filesiz);
          GlobuleEvent::submit(ev);
        }
      /* FIXME: workaround, see also Document.cpp::FILE_MONITOR_EVT.
       * because documents need to unregister their file monitor when the
       * file is deleted they normally would send an FileUnregEvent.  However
       * this is not possible because this object is locked (as the file
       * monitor is traversing its list at this time.
       * Therefor we will check at this time if the file is deleted and
       * AUTOmatically remove the file motoring.  We have to cut short
       * checking the file monitor loop, and and this point we should not make
       * an assumption about what the document wants to do.
       */
      if(iter->second.deleted()) {
        _filemap.erase(iter);
        return;
      }
    }
  }
}

std::string
FileMonitor::description() const throw()
{
  return "FileMonitor";
}
