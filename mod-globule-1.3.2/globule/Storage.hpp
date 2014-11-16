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
#ifndef _STORAGE_HPP
#define _STORAGE_HPP

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <string>
#include <vector>
#include <map>
#include <apr.h>
#include <apr_file_io.h>
#include <apr_network_io.h>
#include <httpd.h>
#ifndef STANDALONE_APR
#include "alloc/Allocator.hpp"
#endif

class Serializable;
class Persistent;
class WrappedPersistent;
class WrappedLabel;
class Context;

class FileError
{
private:
  std::string _message;
  std::string _stacktrace;
public:
  FileError() throw();
  FileError(const char* message) throw();
  FileError(std::string message) throw();
  FileError(const char* message, std::string item) throw();
  virtual ~FileError() throw();
  virtual std::string getMessage() const throw();
  std::string getStackTrace() const throw();
};

class WrappedError : public FileError
{
private:
  std::string _expectedlabel, _readlabel;
public:
  WrappedError(apr_time_t expectedversion, apr_time_t readversion) throw();
  WrappedError(const char* expectedlabel, const std::string readlabel) throw()
    : _expectedlabel(expectedlabel), _readlabel(readlabel) { };
  virtual ~WrappedError() throw();
  virtual std::string getMessage() throw();
};

class NoData : public FileError
{
public:
  NoData() {};
};

class Input
{
public:
  virtual apr_pool_t* pool() throw() = 0;
  virtual Context* context() throw() = 0;
  virtual void push() throw(FileError) = 0;
  virtual void pop() throw(WrappedError,FileError) = 0;
  virtual void overrideObject(Serializable* obj) throw() = 0;
  virtual Input& operator>>(bool&) throw(FileError) = 0;
  virtual Input& operator>>(char&) throw(FileError) = 0;
  virtual Input& operator>>(apr_byte_t&) throw(FileError) = 0;
  virtual Input& operator>>(apr_int16_t&) throw(FileError) = 0;
  virtual Input& operator>>(apr_uint16_t&) throw(FileError) = 0;
  virtual Input& operator>>(apr_int32_t&) throw(FileError) = 0;
  virtual Input& operator>>(apr_uint32_t&) throw(FileError) = 0;
  virtual Input& operator>>(apr_int64_t&) throw(FileError) = 0;
  virtual Input& operator>>(apr_uint64_t&) throw(FileError) = 0;
#if (defined(DARWIN))
  virtual Input& operator>>(unsigned long&) throw(FileError) = 0;
#endif
  virtual Input& operator>>(std::string&) throw(FileError) = 0;
#ifndef STANDALONE_APR
  virtual Input& operator>>(gstring&) throw(FileError) = 0;
#endif
  virtual Input& operator>>(Serializable&) throw(FileError) = 0;
  virtual Input& operator>>(Persistent*&) throw(FileError) = 0;
  virtual Input& operator>>(WrappedPersistent&) throw(FileError) = 0;
  virtual Input& operator>>(WrappedLabel&) throw(FileError) = 0;
};

class Output
{
public:
  virtual apr_pool_t* pool() throw() = 0;
  virtual void push() throw(FileError) = 0;
  virtual void pop() throw(WrappedError,FileError) = 0;
  virtual Output& operator<<(const bool&) throw(FileError) = 0;
  virtual Output& operator<<(const char&) throw(FileError) = 0;
  virtual Output& operator<<(const apr_byte_t&) throw(FileError) = 0;
  virtual Output& operator<<(const apr_int16_t&) throw(FileError) = 0;
  virtual Output& operator<<(const apr_uint16_t&) throw(FileError) = 0;
  virtual Output& operator<<(const apr_int32_t&) throw(FileError) = 0;
  virtual Output& operator<<(const apr_uint32_t&) throw(FileError) = 0;
  virtual Output& operator<<(const apr_int64_t&) throw(FileError) = 0;
  virtual Output& operator<<(const apr_uint64_t&) throw(FileError) = 0;
#if (defined(DARWIN))
  virtual Output& operator<<(const unsigned long&) throw(FileError) = 0;
#endif
  virtual Output& operator<<(const std::string&) throw(FileError) = 0;
#ifndef STANDALONE_APR
  virtual Output& operator<<(const gstring&) throw(FileError) = 0;
#endif
  virtual Output& operator<<(const char*) throw(FileError) = 0;
  virtual Output& operator<<(const Serializable&) throw(FileError) = 0;
  virtual Output& operator<<(const Persistent*) throw(FileError) = 0;
  virtual Output& operator<<(const WrappedPersistent&) throw(FileError) = 0;
  virtual Output& operator<<(const WrappedLabel&) throw(FileError) = 0;
};

class OutputBase : public Output
{
  friend class SharedStorage;
private:
  apr_uint32_t _objectsCounter;
  std::map<const Serializable*,apr_uint32_t> _objectsStored;
public:
  OutputBase() throw() : _objectsCounter(0) {
    _objectsStored[0] = 0;
  };
  virtual ~OutputBase() throw() { };
  virtual Output& operator<<(const Serializable&) throw(FileError);
  virtual Output& operator<<(const Persistent*) throw(FileError);
  virtual Output& operator<<(const WrappedPersistent& wrapper) throw(FileError);
};

class OutputBinaryBase : public OutputBase
{
protected:
  virtual void wr(const void* buffer, apr_size_t size) throw(FileError) = 0;
public:
  OutputBinaryBase() throw() { };
  virtual ~OutputBinaryBase() throw() { };
  virtual Output& operator<<(const bool&) throw(FileError);
  virtual Output& operator<<(const char&) throw(FileError);
  virtual Output& operator<<(const apr_byte_t&) throw(FileError);
  virtual Output& operator<<(const apr_int16_t&) throw(FileError);
  virtual Output& operator<<(const apr_uint16_t&) throw(FileError);
  virtual Output& operator<<(const apr_int32_t&) throw(FileError);
  virtual Output& operator<<(const apr_uint32_t&) throw(FileError);
  virtual Output& operator<<(const apr_int64_t&) throw(FileError);
  virtual Output& operator<<(const apr_uint64_t&) throw(FileError);
#if (defined(DARWIN))
  virtual Output& operator<<(const unsigned long&) throw(FileError);
#endif
  virtual Output& operator<<(const std::string&) throw(FileError);
#ifndef STANDALONE_APR
  virtual Output& operator<<(const gstring&) throw(FileError);
#endif
  virtual Output& operator<<(const char*) throw(FileError);
  virtual Output& operator<<(const WrappedLabel&) throw(FileError);
};

class OutputTextBase : public OutputBase
{
protected:
  virtual void wr(const char* fmt, ...) throw(FileError) = 0;
public:
  OutputTextBase() throw() { };
  virtual ~OutputTextBase() throw() { };
  virtual Output& operator<<(const bool&) throw(FileError);
  virtual Output& operator<<(const char&) throw(FileError);
  virtual Output& operator<<(const apr_byte_t&) throw(FileError);
  virtual Output& operator<<(const apr_int16_t&) throw(FileError);
  virtual Output& operator<<(const apr_uint16_t&) throw(FileError);
  virtual Output& operator<<(const apr_int32_t&) throw(FileError);
  virtual Output& operator<<(const apr_uint32_t&) throw(FileError);
  virtual Output& operator<<(const apr_int64_t&) throw(FileError);
  virtual Output& operator<<(const apr_uint64_t&) throw(FileError);
#if (defined(DARWIN))
  virtual Output& operator<<(const unsigned long&) throw(FileError);
#endif
  virtual Output& operator<<(const std::string&) throw(FileError);
#ifndef STANDALONE_APR
  virtual Output& operator<<(const gstring&) throw(FileError);
#endif
  virtual Output& operator<<(const char*) throw(FileError);
  virtual Output& operator<<(const WrappedLabel&) throw(FileError);
};

class InputBase : public Input
{
  friend class SharedStorage;
  friend class ShmReadCyclic;
private:
  apr_uint32_t _objectsCounter;
  std::vector<Serializable*> _objectsRetrieved;
public:
  InputBase() throw() : _objectsCounter(0) {
    _objectsRetrieved.push_back(0);
  };
  virtual ~InputBase() throw() { };
  virtual void overrideObject(Serializable* obj) throw();
  virtual Input& operator>>(Serializable&) throw(FileError);
  virtual Input& operator>>(Persistent*&) throw(FileError);
  virtual Input& operator>>(WrappedPersistent&) throw(FileError);
};

class InputBinaryBase : public InputBase
{
protected:
  virtual void rd(void* buffer, apr_size_t size) throw(NoData,FileError) = 0;
public:
  InputBinaryBase() throw() { };
  virtual ~InputBinaryBase() throw() { };
  virtual Input& operator>>(bool&) throw(FileError);
  virtual Input& operator>>(char&) throw(FileError);
  virtual Input& operator>>(apr_byte_t&) throw(FileError);
  virtual Input& operator>>(apr_int16_t&) throw(FileError);
  virtual Input& operator>>(apr_uint16_t&) throw(FileError);
  virtual Input& operator>>(apr_int32_t&) throw(FileError);
  virtual Input& operator>>(apr_uint32_t&) throw(FileError);
  virtual Input& operator>>(apr_int64_t&) throw(FileError);
  virtual Input& operator>>(apr_uint64_t&) throw(FileError);
#if (defined(DARWIN))
  virtual Input& operator>>(unsigned long&) throw(FileError);
#endif
  virtual Input& operator>>(std::string&) throw(FileError);
#ifndef STANDALONE_APR
  virtual Input& operator>>(gstring&) throw(FileError);
#endif
  virtual Input& operator>>(WrappedLabel&) throw(FileError);
};

class InputTextBase : public InputBase
{
protected:
  virtual std::string rd() throw(NoData,FileError) = 0;
  virtual std::string rdmore() throw(FileError) = 0;
public:
  InputTextBase() throw() { };
  virtual ~InputTextBase() throw() { };
  virtual Input& operator>>(bool&) throw(FileError);
  virtual Input& operator>>(char&) throw(FileError);
  virtual Input& operator>>(apr_byte_t&) throw(FileError);
  virtual Input& operator>>(apr_int16_t&) throw(FileError);
  virtual Input& operator>>(apr_uint16_t&) throw(FileError);
  virtual Input& operator>>(apr_int32_t&) throw(FileError);
  virtual Input& operator>>(apr_uint32_t&) throw(FileError);
  virtual Input& operator>>(apr_int64_t&) throw(FileError);
  virtual Input& operator>>(apr_uint64_t&) throw(FileError);
#if (defined(DARWIN))
  virtual Input& operator>>(unsigned long&) throw(FileError);
#endif
  virtual Input& operator>>(std::string&) throw(FileError);
#ifndef STANDALONE_APR
  virtual Input& operator>>(gstring&) throw(FileError);
#endif
  virtual Input& operator>>(WrappedLabel&) throw(FileError);
};

class Serializable
{
  friend class InputBase;
  friend class OutputBase;
  friend class SharedStorage;
protected:
  virtual Input& operator>>(Input&) throw(FileError) = 0;
  virtual Output& operator<<(Output&) const throw(FileError) = 0;
  Serializable() throw();
public:
  virtual ~Serializable() throw();
};

class Persistent : public Serializable
{
  friend class InputBase;
  friend class OutputBase;
  friend class SharedStorage;
private:
  const char* _name;
  static std::map<std::string,Persistent*> _templates;
protected:
  static Persistent* lookupClass(const char* name) throw();
  virtual Persistent* instantiateClass() throw() = 0;
  Persistent(const char* name) throw();
  Persistent(Persistent& base) throw();
public:
  virtual ~Persistent() throw();
  static void registerClass(const char* name, Persistent*) throw();
};

class WrappedLabel : std::string
{
public:
  inline WrappedLabel() throw() { };
  inline WrappedLabel(const char* s) throw() : std::string(s) { };
  inline const char* c_str() const throw() { return std::string::c_str(); };
  WrappedLabel& operator=(const std::string& s) throw() { std::string::operator=(s); return *this; };
};

class WrappedPersistent : public Persistent
{
protected:
  const char* _label;
public:
  WrappedPersistent(const char* label) throw()
    : Persistent("WrappedPersistent"), _label(label) { }
  Persistent* instantiateClass() throw() { return 0; };
  virtual Input& operator>>(Input& in) throw(WrappedError) {
    WrappedLabel label;
    in >> label;
    if(strcmp(label.c_str(),_label))
      throw WrappedError(_label, label.c_str());
    return in;
  }
  virtual Input& operator>>(Input& in) const throw(FileError,WrappedError) {
    WrappedLabel label;
    in >> label;
    if(strcmp(label.c_str(),_label))
      throw WrappedError(_label, label.c_str());
    return in;
  };
  virtual Output& operator<<(Output& out) const throw(FileError) {
    return out << WrappedLabel(_label);
  };
};

template<typename T>
class InputWrappedPersistent : public WrappedPersistent
{
private:
  T& _target;
protected:
  virtual Persistent* instantiateClass() throw() {
    return this;
  };
public:
  InputWrappedPersistent(const char* label, T& target) throw()
    : WrappedPersistent(label), _target(target) { };
  InputWrappedPersistent(const InputWrappedPersistent<T>& ref) throw()
    : WrappedPersistent(ref._label), _target(ref._target) { };
  virtual Input& operator>>(Input& in) const throw(WrappedError,FileError) {
    WrappedPersistent::operator>>(in);
    in.push();
    in >> _target;
    in.pop();
    return in;
  };
  virtual Output& operator<<(Output& out) const throw() {
    ap_assert(!"Programmer error: InputWrappedPersistent used for output");
    return out;
  };
};

template<typename T>
Input& operator>>(Input& in, InputWrappedPersistent<T> obj) throw(WrappedError) {
  obj.operator>>(in);
  return in;
};

template<typename T>
class OutputWrappedPersistent : public WrappedPersistent
{
private:
  const T& _target;
protected:
  virtual Persistent* instantiateClass() throw() {
    return this;
  };
public:
  OutputWrappedPersistent(const OutputWrappedPersistent& r) throw()
    : WrappedPersistent(r._label), _target(r._target) { };
  OutputWrappedPersistent(const char* label, const T& target) throw()
    : WrappedPersistent(label), _target(target) { };
  virtual Input& operator>>(Input& in) throw() {
    ap_assert(!"Programmer error: OutputWrappedPersistent used for input");
    return in;
  };
  virtual Output& operator<<(Output& out) const throw(WrappedError,FileError) {
    WrappedPersistent::operator<<(out);
    out.push();
    out << _target;
    out.pop();
    return out;
  };
};

template<typename T>
Output& operator<<(Output& out, const OutputWrappedPersistent<T>& obj) throw() {
  obj.operator<<(out);
  return out;
};

class FileStore : public InputBinaryBase, public OutputBinaryBase
{
private:
  apr_file_t* _fp;
  apr_pool_t* _pool;
  Context* _context;
protected:
  virtual void rd(void* buffer, apr_size_t size) throw(NoData,FileError);
  virtual void wr(const void* buffer, apr_size_t size) throw(FileError);
public:
  FileStore(apr_pool_t* pool, Context*, std::string fname, bool input) throw(FileError);
  virtual ~FileStore() throw();
  virtual apr_pool_t* pool() throw();
  virtual Context* context() throw();
  virtual void push() throw(FileError);
  virtual void pop() throw(WrappedError,FileError);
};

class FileTextStore : public InputTextBase, public OutputTextBase
{
private:
  apr_file_t* _fp;
  apr_pool_t* _pool;
  Context* _context;
  int _indent;
protected:
  virtual std::string rd() throw(NoData,FileError);
  virtual std::string rdmore() throw(FileError);
  virtual void wr(const char* fmt, ...) throw(FileError);
public:
  FileTextStore(apr_pool_t* pool, Context*, std::string fname, bool input) throw(FileError);
  virtual ~FileTextStore() throw();
  virtual apr_pool_t* pool() throw();
  virtual Context* context() throw();
  virtual void push() throw(FileError);
  virtual void pop() throw(WrappedError,FileError);
};

/****************************************************************************/

class MergePersistent;

class SharedStorageSpace
{
protected:
  apr_byte_t* _mem1;
  apr_byte_t* _mem2;
  SharedStorageSpace(SharedStorageSpace& org) throw();
public:
  SharedStorageSpace() throw();
  virtual ~SharedStorageSpace() throw();
  void access(apr_byte_t** inputmem, apr_byte_t** outputmem);
};

#ifndef STANDALONE_APR
class ShmSharedStorageSpace : public SharedStorageSpace
{
  friend class ShmReadCyclic;
private:
  ShmSharedStorageSpace(ShmSharedStorageSpace& org) throw();
public:
  ShmSharedStorageSpace() throw();
  virtual ~ShmSharedStorageSpace() throw();
};
#endif

class SharedStorage : public OutputBinaryBase, public InputBinaryBase
{
private:
  SharedStorageSpace* _storage;
  apr_size_t inputidx, outputidx, inputlen;
  apr_byte_t *_outputmem;
  apr_byte_t *_inputmem;
protected:
  virtual void wr(const void* buffer, apr_size_t size) throw();
  virtual void rd(void* buffer, apr_size_t size) throw(NoData,FileError);
public:
  SharedStorage(SharedStorageSpace* storage) throw();
  ~SharedStorage() throw();
  void truncate() throw();
  bool sync(MergePersistent*& obj) throw(FileError);
  bool sync(MergePersistent& obj) throw(FileError);
  virtual apr_pool_t* pool() throw();
  virtual Context* context() throw();
  virtual void push() throw(FileError);
  virtual void pop() throw(WrappedError,FileError);
};

class MergePersistent : public Persistent
{
  friend class SharedStorage;
protected:
  virtual bool sync(SharedStorage& store) throw(FileError);
  MergePersistent(const char* name) throw() : Persistent(name) { };    
  MergePersistent(MergePersistent& base) throw() : Persistent(base) { };
  virtual Persistent* instantiateClass() throw() = 0;
};

/****************************************************************************/

#endif
