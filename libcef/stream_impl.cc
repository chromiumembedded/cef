// Copyright (c) 2008 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "stream_impl.h"

#include "base/logging.h"


// Static functions

CefRefPtr<CefStreamReader> CefStreamReader::CreateForFile(
    const std::wstring& fileName)
{
  CefRefPtr<CefStreamReader> reader;
  FILE *f = _wfopen(fileName.c_str(), L"rb");
  if(f)
    reader = new CefFileReader(f, true);
  return reader;
}

CefRefPtr<CefStreamReader> CefStreamReader::CreateForData(void* data,
                                                          size_t size)
{
  DCHECK(data != NULL);
  DCHECK(size > 0);
  CefRefPtr<CefStreamReader> reader;
  if(data && size > 0)
    reader = new CefBytesReader(data, size, true);
  return reader;
}

CefRefPtr<CefStreamReader> CefStreamReader::CreateForHandler(
    CefRefPtr<CefReadHandler> handler)
{
  DCHECK(handler.get());
  CefRefPtr<CefStreamReader> reader;
  if(handler.get())
    reader = new CefHandlerReader(handler);
  return reader;
}

CefRefPtr<CefStreamWriter> CefStreamWriter::CreateForFile(
    const std::wstring& fileName)
{
  DCHECK(!fileName.empty());
  CefRefPtr<CefStreamWriter> writer;
  FILE* file = _wfopen(fileName.c_str(), L"wb");
  if(file)
    writer = new CefFileWriter(file, true);
  return writer;
}

CefRefPtr<CefStreamWriter> CefStreamWriter::CreateForHandler(
    CefRefPtr<CefWriteHandler> handler)
{
  DCHECK(handler.get());
  CefRefPtr<CefStreamWriter> writer;
  if(handler.get())
    writer = new CefHandlerWriter(handler);
  return writer;
}


// CefFileReader

CefFileReader::CefFileReader(FILE* file, bool close)
  : file_(file), close_(close)
{
}

CefFileReader::~CefFileReader()
{
  Lock();
  if(close_)
    fclose(file_);
  Unlock();
}

size_t CefFileReader::Read(void* ptr, size_t size, size_t n)
{
  Lock();
  size_t rv = fread(ptr, size, n, file_);
  Unlock();
  return rv;
}

int CefFileReader::Seek(long offset, int whence)
{
  Lock();
  int rv = fseek(file_, offset, whence);
  Unlock();
  return rv;
}

long CefFileReader::Tell()
{
  Lock();
  long rv = ftell(file_);
  Unlock();
  return rv;
}

int CefFileReader::Eof()
{
  Lock();
  int rv = feof(file_);
  Unlock();
  return rv;
}


// CefFileWriter

CefFileWriter::CefFileWriter(FILE* file, bool close)
  : file_(file), close_(close)
{
}

CefFileWriter::~CefFileWriter()
{
  Lock();
  if(close_)
    fclose(file_);
  Unlock();
}

size_t CefFileWriter::Write(const void* ptr, size_t size, size_t n)
{
  Lock();
  size_t rv = (size_t)fwrite(ptr, size, n, file_);
  Unlock();
  return rv;
}

int CefFileWriter::Seek(long offset, int whence)
{
  Lock();
  int rv = fseek(file_, offset, whence);
  Unlock();
  return rv;
}

long CefFileWriter::Tell()
{
  Lock();
  long rv = ftell(file_);
  Unlock();
  return rv;
}

int CefFileWriter::Flush()
{
  Lock();
  int rv = fflush(file_);
  Unlock();
  return rv;
}


// CefBytesReader

CefBytesReader::CefBytesReader(void* data, long datasize, bool copy)
  : data_(NULL), datasize_(0), copy_(false), offset_(0)
{
  SetData(data, datasize, copy);
}

CefBytesReader::~CefBytesReader()
{
  SetData(NULL, 0, false);
}

size_t CefBytesReader::Read(void* ptr, size_t size, size_t n)
{
  Lock();
  size_t s = (datasize_ - offset_) / size;
  size_t ret = (n < s ? n : s);
  memcpy(ptr, ((char*)data_) + offset_, ret * size);
  offset_ += ret * size;
  Unlock();
	return ret;
}

int CefBytesReader::Seek(long offset, int whence)
{
  int rv = -1L;
  Lock();
  switch(whence) {
  case SEEK_CUR:
    if(offset_ + offset > datasize_) {
      break;	
    }
    offset_ += offset;
    rv = 0;
    break;
  case SEEK_END:
    if(offset > (int)datasize_) {
      break;
    }
    offset_ = datasize_ - offset;
    rv = 0;
    break;
  case SEEK_SET:
    if(offset > (int)datasize_) {
      break;	
    }
    offset_ = offset;
    rv = 0;
    break;
  }
  Unlock();
	
	return rv;
}

long CefBytesReader::Tell()
{
  Lock();
  long rv = offset_;
  Unlock();
  return rv;
}

int CefBytesReader::Eof()
{
  Lock();
  int rv = (offset_ >= datasize_);
  Unlock();
  return rv;
}
	
void CefBytesReader::SetData(void* data, long datasize, bool copy)
{
  Lock();
  if(copy_)
    free(data_);
  
  copy_ = copy;
  offset_ = 0;
  datasize_ = datasize;

  if(copy) {
    data_ = malloc(datasize);
    DCHECK(data_ != NULL);
    if(data_)
      memcpy(data_, data, datasize);
  } else {
    data_ = data;
  }
  Unlock();
}


// CefBytesWriter

CefBytesWriter::CefBytesWriter(size_t grow)
  : grow_(grow), offset_(0), datasize_(grow)
{
  DCHECK(grow > 0);
  data_ = malloc(grow);
  DCHECK(data_ != NULL);
}

CefBytesWriter::~CefBytesWriter()
{
  Lock();
  if(data_)
    free(data_);
  Unlock();
}

size_t CefBytesWriter::Write(const void* ptr, size_t size, size_t n)
{
  Lock();
  size_t rv;
  if(offset_ + size * n >= datasize_ && Grow(size * n) == 0) {
    rv = 0;
  } else {
    memcpy(((char*)data_) + offset_, ptr, size * n);
    offset_ += size * n;
    rv = n;
  }
  Unlock();
  return rv;
}

int CefBytesWriter::Seek(long offset, int whence)
{
  int rv = -1L;
  Lock();
  switch(whence) {
  case SEEK_CUR:
    if(offset_ + offset > datasize_) {
      break;	
    }
    offset_ += offset;
    rv = offset_;
    break;
  case SEEK_END:
    if(offset > (int)datasize_) {
      break;
    }
    offset_ = datasize_ - offset;
    rv = offset_;
  case SEEK_SET:
    if(offset > (int)datasize_) {
      break;	
    }
    offset_ = offset;
    rv = offset_;
    break;
  }
  Unlock();
	
	return rv;
}

long CefBytesWriter::Tell()
{
  Lock();
  long rv = offset_;
  Unlock();
  return rv;
}

int CefBytesWriter::Flush()
{
  return 0;
}

std::string CefBytesWriter::GetDataString()
{
  Lock();
  std::string str((char*)data_, offset_);
  Unlock();
  return str;
}

size_t CefBytesWriter::Grow(size_t size)
{
  Lock();
  size_t rv;
  size_t s = (size > grow_ ? size : grow_);
	void* tmp = realloc(data_, datasize_ + s);
  DCHECK(tmp != NULL);
	if(tmp) {
	  data_ = tmp;
	  datasize_ += s;
    rv = datasize_;
  } else {
    rv = 0;
  }
  Unlock();
	return rv;
}
