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
#include <stdarg.h>
#include <apr_lib.h>
#include "utilities.h"
#include "Storage.hpp"

using std::string;

/****************************************************************************/

FileError::FileError() throw()
  : _stacktrace(ExceptionHandler::location())
{
}

FileError::FileError(const char* message) throw()
  : _message(message), _stacktrace(ExceptionHandler::location())
{
}

FileError::FileError(std::string message) throw()
  : _message(message), _stacktrace(ExceptionHandler::location())
{
}

FileError::FileError(const char* message, string item) throw()
{
  string::size_type len;
  if((len = item.find('\n')) == string::npos)
    len = item.length();
  if(len > 160-4-strlen(message))
    len = 160-4-strlen(message);
  _message = mkstring() << message << " \"" << item.substr(0,len) << "\"";
};

FileError::~FileError() throw()
{
}

string
FileError::getMessage() const throw()
{
  return _message;
}

string
FileError::getStackTrace() const throw()
{
  return _stacktrace;
}

/****************************************************************************/

Input& InputBinaryBase::operator>>(bool& v) throw(FileError) {
  apr_byte_t t;
  operator>>(t);
  v = t;
  return *this;
}
Input& InputBinaryBase::operator>>(char& v) throw(FileError) {
  apr_byte_t t = v;
  operator>>(t);
  v = t;
  return *this;
}
Input& InputBinaryBase::operator>>(apr_byte_t& v) throw(FileError) {
  apr_size_t sz = 1;
  rd(&v, sz);
  return *this;
}
Input& InputBinaryBase::operator>>(apr_int16_t& v) throw(FileError) {
  apr_size_t sz = 2;
  apr_uint16_t buf;
  rd(&buf, sz);
  v = ntohs((apr_uint16_t)buf);
  return *this;
}
Input& InputBinaryBase::operator>>(apr_uint16_t& v) throw(FileError) {
  apr_size_t sz = 2;
  apr_uint16_t buf;
  rd(&buf, sz);
  v = ntohs(buf);
  return *this;
}
Input& InputBinaryBase::operator>>(apr_int32_t& v) throw(FileError) {
  apr_size_t sz = 4;
  apr_uint32_t buf;
  rd(&buf, sz);
  v = ntohl(buf);
  return *this;
}
Input& InputBinaryBase::operator>>(apr_uint32_t& v) throw(FileError) {
  apr_size_t sz = 4;
  apr_uint32_t buf;
  rd(&buf, sz);
  v = ntohl(buf);
  return *this;
}
Input& InputBinaryBase::operator>>(apr_int64_t& v) throw(FileError) {
  apr_size_t sz = 8;
  apr_uint32_t buf[2];
  rd(&buf, sz);
  v = (((apr_uint64_t)ntohl(buf[0]))<<32) | ntohl(buf[1]);
  return *this;
}
Input& InputBinaryBase::operator>>(apr_uint64_t& v) throw(FileError) {
  apr_size_t sz = 8;
  apr_uint32_t buf[2];
  rd(&buf, sz);
  v = (((apr_uint64_t)ntohl(buf[0]))<<32) | ntohl(buf[1]);
  return *this;
}
Input& InputBinaryBase::operator>>(string& v) throw(FileError) {
  apr_size_t sz;
  *this >> sz;
  if(sz >= 8*1024)
    throw FileError("string too large");
  std::vector<char> buf(sz+1, '\0');
  rd(&buf[0], sz);
  v = std::string(&buf[0], sz);
  return *this;
}
#ifndef STANDALONE_APR
Input& InputBinaryBase::operator>>(gstring& v) throw(FileError) {
  apr_size_t sz;
  *this >> sz;
  if(sz >= 8*1024)
    throw FileError("string too large");
  std::vector<char> buf(sz+1, '\0');
  rd(&buf[0], sz);
  v = gstring(&buf[0], sz);
  return *this;
}
#endif

Input&
InputBinaryBase::operator>>(WrappedLabel& l) throw(FileError)
{
  string s;
  operator>>(s);
  l = s;
  return *this;
}
Input& InputTextBase::operator>>(WrappedLabel& l) throw(FileError)
{
  string s(rd());
  if(s[s.length()-1] == '\n')
    s = s.substr(0,s.length()-1);
  l = s;
  return *this;
}
Output&
OutputBinaryBase::operator<<(const WrappedLabel& l) throw(FileError)
{
  return operator<<(l.c_str());
}
Output&
OutputTextBase::operator<<(const WrappedLabel& l) throw(FileError)
{
  wr("%s",l.c_str());
  return *this;
}

#if (defined(DARWIN))
// these are workaround versions for darwin
Input& InputBinaryBase::operator>>(unsigned long& v) throw(FileError)
{
  return operator>>((apr_uint32_t&)v);
}
Input& InputTextBase::operator>>(unsigned long& v) throw(FileError)
{
  return operator>>((apr_uint32_t&)v);
}
Output& OutputBinaryBase::operator<<(const unsigned long& v) throw(FileError)
{
  return operator<<((const apr_uint32_t&)v);
}
Output& OutputTextBase::operator<<(const unsigned long& v) throw(FileError)
{
  return operator<<((const apr_uint32_t&)v);
}
#endif

Input& InputTextBase::operator>>(bool& v) throw(FileError) {
  string s(rd());
  if(s.find("bool true") == 0)
    v = true;
  else if(s.find("bool false") == 0)
    v = false;
  else
    throw FileError("parse error for expected bool",s);
  return *this;
}
Input& InputTextBase::operator>>(char& v) throw(FileError) {
  string s(rd());
  int ch;
  if(sscanf(s.c_str(),"char %d",&ch)==1)
    v = ch;
  else
    throw FileError("parse error for expected char",s);
  return *this;
}
Input& InputTextBase::operator>>(apr_byte_t& v) throw(FileError) {
  string s(rd());
  int ch;
  if(sscanf(s.c_str(),"byte %d",&ch)==1)
    v = ch;
  else
    throw FileError("parse error for expected byte",s);
  return *this;
}
Input& InputTextBase::operator>>(apr_int16_t& v) throw(FileError) {
  string s(rd());
  int i;
  if(sscanf(s.c_str(),"int16 %d",&i)==1)
    v = i;
  else
    throw FileError("parse error for expected int16",s);
  return *this;
}
Input& InputTextBase::operator>>(apr_uint16_t& v) throw(FileError) {
  string s(rd());
  unsigned int i;
  if(sscanf(s.c_str(),"uint16 %u",&i)==1)
    v = i;
  else
    throw FileError("parse error for expected uint16",s);
  return *this;
}
Input& InputTextBase::operator>>(apr_int32_t& v) throw(FileError) {
  string s(rd());
  int i;
  if(sscanf(s.c_str(),"int32 %d",&i)==1)
    v = i;
  else
    throw FileError("parse error for expected int32",s);
  return *this;
}
Input& InputTextBase::operator>>(apr_uint32_t& v) throw(FileError) {
  string s(rd());
  unsigned int i;
  if(sscanf(s.c_str(),"uint32 %u",&i)==1)
    v = i;
  else
    throw FileError("parse error for expected uint32",s);
  return *this;
}
Input& InputTextBase::operator>>(apr_int64_t& v) throw(FileError) {
  string s(rd());
  if(sscanf(s.c_str(),"int64 %lld",&v)!=1)
    throw FileError("parse error for expected int64",s);
  return *this;
}
Input& InputTextBase::operator>>(apr_uint64_t& v) throw(FileError) {
  string s(rd());
  if(sscanf(s.c_str(),"uint64 %llu",&v)!=1)
    throw FileError("parse error for expected uint64",s);
  return *this;
}
Input& InputTextBase::operator>>(string& v) throw(FileError) {
  string s(rd());
  unsigned int length, pos;
  if(sscanf(s.c_str(),"string %u %n",&length,&pos)>=1) {
    v = string(&(s.c_str()[pos]));
    while(v.length() < length)
      v += rdmore();
    v = v.substr(0,v.length()-1);
    if(v.length() != length)
      throw FileError(mkstring()<<"string length error (expected "
                      << length << " got " << v.length() << ")");
  } else
    throw FileError("parse error for expected string",s);
  return *this;
}
#ifndef STANDALONE_APR
Input& InputTextBase::operator>>(gstring& v) throw(FileError) {
  string s(rd());
  unsigned int length, pos;
  if(sscanf(s.c_str(),"string %u %n",&length,&pos)>=1) {
    v = gstring(&(s.c_str()[pos]));
    while(v.length() < length)
      v += rdmore();
    v = v.substr(0,v.length()-1);
    if(v.length() != length)
      throw FileError(mkstring()<<"string length error (expected "
                      << length << " got " << v.length() << ")");
  } else
    throw FileError("parse error for expected string",s);
  return *this;
}
#endif

Output& OutputTextBase::operator<<(const bool& v) throw(FileError) {
  if(v)
    wr("bool true");
  else
    wr("bool false");
  return *this;
}
Output& OutputTextBase::operator<<(const char& v) throw(FileError) {
  wr("char %d",v);
  return *this;
}
Output& OutputTextBase::operator<<(const apr_byte_t& v) throw(FileError) {
  wr("byte %d",v);
  return *this;
}
Output& OutputTextBase::operator<<(const apr_int16_t& v) throw(FileError) {
  wr("int16 %d",(int)v);
  return *this;
}
Output& OutputTextBase::operator<<(const apr_uint16_t& v) throw(FileError) {
  wr("uint16 %u",(unsigned int)v);
  return *this;
}
Output& OutputTextBase::operator<<(const apr_int32_t& v) throw(FileError) {
  wr("int32 %d",(int)v);
  return *this;
}
Output& OutputTextBase::operator<<(const apr_uint32_t& v) throw(FileError) {
  wr("uint32 %d",(unsigned int)v);
  return *this;
}
Output& OutputTextBase::operator<<(const apr_int64_t& v) throw(FileError) {
  wr("int64 %lld",v);
  return *this;
}
Output& OutputTextBase::operator<<(const apr_uint64_t& v) throw(FileError) {
  wr("int64 %llu",v);
  return *this;
}
Output& OutputTextBase::operator<<(const string& v) throw(FileError) {
  wr("string %d %s",v.length(),v.c_str());
  return *this;
}
#ifndef STANDALONE_APR
Output& OutputTextBase::operator<<(const gstring& v) throw(FileError) {
  wr("string %d %s",v.length(),v.c_str());
  return *this;
}
#endif
Output& OutputTextBase::operator<<(const char* v) throw(FileError) {
  wr("string %d %s",strlen(v),v);
  return *this;
}

Output& OutputBinaryBase::operator<<(const bool& v) throw(FileError) {
  return operator<<((apr_byte_t)v);
}
Output& OutputBinaryBase::operator<<(const char& v) throw(FileError) {
  return operator<<((apr_byte_t)v);
}
Output& OutputBinaryBase::operator<<(const apr_byte_t& v) throw(FileError) {
  apr_size_t sz = 1;
  wr(&v, sz);
  return *this;
}
Output& OutputBinaryBase::operator<<(const apr_int16_t& v) throw(FileError) {
  apr_size_t sz = 2;
  apr_uint16_t buf = htons((apr_uint16_t)v);
  wr(&buf, sz);
  return *this;
}
Output& OutputBinaryBase::operator<<(const apr_uint16_t& v) throw(FileError) {
  apr_size_t sz = 2;
  apr_uint16_t buf = htons(v);
  wr(&buf, sz);
  return *this;
}
Output& OutputBinaryBase::operator<<(const apr_int32_t& v) throw(FileError) {
  apr_size_t sz = 4;
  apr_uint32_t buf = htonl((apr_uint32_t)v);
  wr(&buf, sz);
  return *this;
}
Output& OutputBinaryBase::operator<<(const apr_uint32_t& v) throw(FileError) {
  apr_size_t sz = 4;
  apr_uint32_t buf = htonl(v);
  wr(&buf, sz);
  return *this;
}
Output& OutputBinaryBase::operator<<(const apr_int64_t& v) throw(FileError) {
  apr_size_t sz = 8;
  apr_uint32_t buf[2];
  buf[0] = htonl(((apr_uint64_t)v)>>32);
  buf[1] = htonl(((apr_uint64_t)v)&0xffff);
  wr(&buf, sz);
  return *this;
}
Output& OutputBinaryBase::operator<<(const apr_uint64_t& v) throw(FileError) {
  apr_size_t sz = 8;
  apr_uint32_t buf[2];
  buf[0] = htonl(v>>32);
  buf[1] = htonl(v&0xffff);
  wr(&buf, sz);
  return *this;
}
Output& OutputBinaryBase::operator<<(const string& v) throw(FileError) {
  apr_size_t sz = v.length();
  *this << sz;
  wr(v.c_str(), sz);
  return *this;
}
#ifndef STANDALONE_APR
Output& OutputBinaryBase::operator<<(const gstring& v) throw(FileError) {
  apr_size_t sz = v.length();
  *this << sz;
  wr(v.c_str(), sz);
  return *this;
}
#endif
Output& OutputBinaryBase::operator<<(const char* v) throw(FileError) {
  apr_size_t sz = strlen(v);
  *this << sz;
  wr(v, sz);
  return *this;
}

void
InputBase::overrideObject(Serializable* obj) throw()
{
  _objectsRetrieved.back() = obj;
}

Input&
InputBase::operator>>(Serializable& obj) throw(FileError)
{
  apr_uint32_t objectid;
  *(Input*)this >> objectid;
  if(objectid > _objectsCounter) {
    if(objectid != ++_objectsCounter)
      throw FileError("next object isn't expected next");
    _objectsRetrieved.push_back(&obj);
    obj.operator>>(*this);
  } else
    throw FileError("object was already read");
  return *this;
}

Output&
OutputBase::operator<<(const Serializable& obj) throw(FileError)
{
  apr_uint32_t objectid;
  std::map<const Serializable*,apr_uint32_t>::const_iterator iter;
  iter = _objectsStored.find(&obj);
  if(iter == _objectsStored.end()) {
    _objectsStored[&obj] = (objectid = ++_objectsCounter);
    ((Output&)(*this)) << objectid;
    obj.operator<<(*this);
  } else {
    throw FileError("object was already written");
  }
  return *this;
}

Input&
InputBase::operator>>(Persistent*& obj) throw(FileError)
{
  apr_uint32_t objectid;
  *(Input*)this >> objectid;
  if(objectid > _objectsCounter) {
    string name;
    ((Input&)(*this)) >> name;
    Persistent* templ = Persistent::lookupClass(name.c_str());
    if(!templ)
      throw FileError(mkstring() << "unknown class name \"" << name << "\"");
    obj = templ->instantiateClass();
    if(objectid != ++_objectsCounter)
      throw FileError("next object isn't expected next");
    _objectsRetrieved.push_back(obj);
    obj->operator>>(*this);
  } else {
    obj = static_cast<Persistent*>(_objectsRetrieved[objectid]);
  }
  return *this;
}

Output&
OutputBase::operator<<(const Persistent* obj) throw(FileError)
{
  apr_uint32_t objectid = 0;
  if(obj) {
    std::map<const Serializable*,apr_uint32_t>::const_iterator iter;
    iter = _objectsStored.find(obj);
    if(iter == _objectsStored.end()) {
      _objectsStored[obj] = (objectid = ++_objectsCounter);
      ((Output&)(*this)) << objectid << obj->_name;
      obj->operator<<(*this);
    } else {
      objectid = iter->second;
      ((Output&)(*this)) << objectid;
    }
  } else
    ((Output&)(*this)) << objectid;
  return *this;
}

Input&
InputBase::operator>>(WrappedPersistent& wrapper) throw(FileError)
{
  wrapper.operator>>(*this);
  return *this;
}

Output&
OutputBase::operator<<(const WrappedPersistent& wrapper) throw(FileError)
{
  wrapper.operator<<(*this);
  return *this;
}

WrappedError::WrappedError(apr_time_t expectedversion, apr_time_t readversion)
  throw()
  : _expectedlabel(mkstring() << expectedversion),
    _readlabel(mkstring() << readversion)
{
}

WrappedError::~WrappedError() throw()
{
}

string
WrappedError::getMessage() throw()
{
  return mkstring() << "got " << _expectedlabel << " expected " << _readlabel;
}

/****************************************************************************/

std::map<string,Persistent*> Persistent::_templates;

Persistent::Persistent(const char* name) throw()
{
  _name = name;
}

Persistent::Persistent(Persistent& base) throw()
{
  _name = base._name;
}

Persistent::~Persistent() throw()
{
}

void
Persistent::registerClass(const char* name, Persistent* templ) throw()
{
  _templates[name] = templ;
}

Persistent*
Persistent::lookupClass(const char* name) throw()
{
  return _templates[name];
}

Serializable::Serializable() throw()
{
}

Serializable::~Serializable() throw()
{
}

Input&
Serializable::operator>>(Input& in) throw(FileError)
{
  return in;
}

Output&
Serializable::operator<<(Output& out) const throw(FileError)
{
  return out;
}

/****************************************************************************/

void
FileStore::rd(void* buf, apr_size_t sz) throw(NoData,FileError)
{
  if(apr_file_read(_fp, buf, &sz))
    throw FileError("unexpected end of file");
}

void
FileStore::wr(const void* buf, apr_size_t sz) throw(FileError)
{
  if(apr_file_write(_fp, buf, &sz))
    throw FileError("out of storage space");
}

FileStore::FileStore(apr_pool_t* pool, Context* ctx,
                     std::string fname, bool input)
  throw(FileError)
  : _pool(pool), _context(ctx)
{
  const char magic[11] = "GLOBULE A\n";
  apr_int32_t flags = (input?(APR_READ):(APR_WRITE|APR_CREATE))|APR_BUFFERED;
  if(apr_file_open(&_fp, fname.c_str(), flags, APR_OS_DEFAULT, pool) !=
     APR_SUCCESS)
    throw FileError("could not open file");
  if(input) {
    char buf[11];
    rd(buf, 10);
    buf[10] = '\0';
    if(strcmp(buf,magic))
      throw FileError("incompatible file content type");
  } else
    wr(magic, 10);
}

FileStore::~FileStore() throw()
{
  apr_file_close(_fp);
}

apr_pool_t*
FileStore::pool() throw()
{
  return _pool;
}

Context*
FileStore::context() throw()
{
  return _context;
}

void
FileStore::push() throw(FileError)
{
}

void
FileStore::pop() throw(WrappedError,FileError)
{
}

string
FileTextStore::rd() throw(NoData,FileError)
{
  int pos;
  char s[8*1024];
  apr_file_gets(s, sizeof(s), _fp);
  s[sizeof(s)-1] = '\0';
  // skip any leading spaces on line
  for(pos=0; apr_isspace(s[pos]); pos++)
    ;
  return string(&s[pos]);
}

string
FileTextStore::rdmore() throw(FileError)
{
  char s[8*1024];
  apr_file_gets(s, sizeof(s), _fp);
  s[sizeof(s)-1] = '\0';
  return string(s);
}

void
FileTextStore::wr(const char* fmt, ...) throw(FileError)
{
  va_list ap;
  va_start(ap, fmt);
  for(int i=0; i<_indent; i++)
    apr_file_puts("  ", _fp);
  apr_file_puts(mkstring::format(fmt,ap).c_str(), _fp);
  apr_file_puts("\n",_fp);
  va_end(ap);
}

FileTextStore::FileTextStore(apr_pool_t* pool, Context* ctx, 
                             string fname, bool input)
  throw(FileError)
  : _pool(pool), _context(ctx), _indent(0)
{
  const char magic[11] = "GLOBULE B\n";
  apr_int32_t flags = (input?(APR_READ):(APR_WRITE|APR_CREATE))|APR_BUFFERED;
  if(apr_file_open(&_fp, fname.c_str(), flags, APR_OS_DEFAULT, pool) !=
     APR_SUCCESS)
    throw FileError(mkstring()<<"could not open file \""<<fname<<"\"");
  if(input) {
    char buf[11];
    apr_size_t sz = 10;
    if(apr_file_read(_fp, buf, &sz)) {
      apr_file_close(_fp);
      throw FileError("unexpected end of file");
    }
    buf[10] = '\0';
    if(strcmp(buf,magic))
      throw FileError("incompatible file content type");
  } else
    wr(magic, 10);
}

FileTextStore::~FileTextStore() throw()
{
  apr_file_close(_fp);
}

apr_pool_t*
FileTextStore::pool() throw()
{
  return _pool;
}

Context*
FileTextStore::context() throw()
{
  return _context;
}

void
FileTextStore::push() throw(FileError)
{
  ++_indent;
}

void
FileTextStore::pop() throw(WrappedError,FileError)
{
  if(_indent-- == 0)
    throw WrappedError("}","");
}

/****************************************************************************/

SharedStorageSpace::SharedStorageSpace()
  throw()
  : _mem1(0), _mem2(0)
{
}

SharedStorageSpace::~SharedStorageSpace()
  throw()
{
}

void
SharedStorageSpace::access(apr_byte_t** inputmem, apr_byte_t** outputmem)
{
  apr_byte_t* aux;
  if(inputmem)
    *inputmem  = _mem1;
  if(outputmem)
    *outputmem = _mem2;
  aux = _mem1;
  _mem1 = _mem2;
  _mem2 = aux;
}

#ifndef STANDALONE_APR

ShmSharedStorageSpace::ShmSharedStorageSpace()
  throw()
{
  apr_size_t length = 0;
  _mem1 = (apr_byte_t*)rmmmemory::allocate(65536);
  memcpy(_mem1, &length, sizeof(apr_size_t));
  _mem2 = (apr_byte_t*)rmmmemory::allocate(65536);
  memcpy(_mem2, &length, sizeof(apr_size_t));
}

ShmSharedStorageSpace::~ShmSharedStorageSpace() throw()
{
  rmmmemory::deallocate(_mem1);
  rmmmemory::deallocate(_mem2);
}

#endif

void
SharedStorage::wr(const void* buffer, apr_size_t size) throw()
{
  memcpy(&_outputmem[outputidx], buffer, size);
  outputidx += size;
}

void
SharedStorage::rd(void* buffer, apr_size_t size) throw(NoData,FileError)
{
  if(inputidx + size > inputlen)
    throw NoData();
  memcpy(buffer, &_inputmem[inputidx], size);
  inputidx += size;
}

SharedStorage::SharedStorage(SharedStorageSpace* storage)
  throw()
  : _storage(storage), inputidx(0), outputidx(0)
{
  storage->access(&_outputmem, &_inputmem);
  memcpy(&inputlen, _inputmem, sizeof(apr_size_t));
  outputidx = inputidx = sizeof(apr_size_t);
}

SharedStorage::~SharedStorage() throw()
{
  memcpy(_outputmem, &outputidx, sizeof(apr_size_t));
}

void
SharedStorage::truncate() throw()
{
  inputlen = inputidx = 0;
}

bool
SharedStorage::sync(MergePersistent*& obj) throw(FileError)
{
  apr_uint32_t objectid = 0;
  std::map<const Serializable*,apr_uint32_t>::const_iterator iter;
  try {
    *(Input*)this >> objectid;
    if(objectid > InputBase::_objectsCounter) {   
      std::string name;
      ((Input&)(*this)) >> name;
      ++InputBase::_objectsCounter;
      ++OutputBase::_objectsCounter;
      if(objectid != InputBase::_objectsCounter)
        throw FileError("next object isn't expected next");
      ((Output&)(*this)) << objectid << name;
      if(obj) {
        _objectsRetrieved.push_back((Persistent*&)obj);
        _objectsStored[(Persistent*&)obj] = objectid;
        obj->sync(*this);
      } else {
        obj = (MergePersistent*) Persistent::lookupClass(name.c_str());
        if(obj) {
          obj = (MergePersistent*) obj->instantiateClass();
          _objectsRetrieved.push_back(obj);
          _objectsStored[obj] = OutputBase::_objectsCounter;
          obj->operator>>(*this);
        }
        return false;
      }
    } else {
      ((Output&)(*this)) << objectid;
      if(!obj)
        obj = (MergePersistent*) _objectsRetrieved[objectid];
      ap_assert(obj == _objectsRetrieved[objectid]);
    }
  } catch(NoData) {
    if(obj) {
      iter = _objectsStored.find(obj);
      if(iter == _objectsStored.end()) {
        ++InputBase::_objectsCounter;
        ++OutputBase::_objectsCounter;
        _objectsStored[obj] = (objectid = OutputBase::_objectsCounter);
        ((Output&)(*this)) << objectid << obj->_name;
        obj->operator<<(*this);
      } else {
        objectid = iter->second;
        ((Output&)(*this)) << objectid;
      }
    } else
      ((Output&)(*this)) << objectid;
  }
  return true;
}

bool
SharedStorage::sync(MergePersistent& obj) throw(FileError)
{
  apr_uint32_t objectid = 0;
  std::map<const Serializable*,apr_uint32_t>::const_iterator iter;
  ++InputBase::_objectsCounter;
  ++OutputBase::_objectsCounter;
  try {
    *(Input*)this >> objectid;
    if(objectid < InputBase::_objectsCounter)
      throw FileError("object was already read");
    if(objectid != InputBase::_objectsCounter)
      throw FileError("next object isn't expected next");
    _objectsRetrieved.push_back(&obj);
    iter = _objectsStored.find(&obj);
    if(iter != _objectsStored.end())
      throw FileError("object was already written");
    _objectsStored[&obj] = OutputBase::_objectsCounter;
    ((Output&)(*this)) << objectid;
    obj.sync(*this);
  } catch(NoData) {
    iter = _objectsStored.find(&obj);
    if(iter != _objectsStored.end())
      throw FileError("object was already written");
    _objectsStored[&obj] = objectid = OutputBase::_objectsCounter;
    ((Output&)(*this)) << objectid;
    obj.operator<<(*this);
  }
  return true;
}

apr_pool_t*
SharedStorage::pool() throw()
{
  return 0;
}

Context*
SharedStorage::context() throw()
{
  return 0;
}

void
SharedStorage::push() throw(FileError)
{
}

void
SharedStorage::pop() throw(WrappedError,FileError)
{
}

/****************************************************************************/

bool
MergePersistent::sync(SharedStorage& store) throw(FileError)
{
  try {
    this->operator>>(store);
  } catch(NoData) {
  }
  this->operator<<(store);
  return true;
};

/****************************************************************************/
