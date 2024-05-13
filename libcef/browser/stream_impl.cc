// Copyright (c) 2008 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "cef/libcef/browser/stream_impl.h"

#include <stdlib.h>

#include <algorithm>

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/threading/thread_restrictions.h"

// Static functions

CefRefPtr<CefStreamReader> CefStreamReader::CreateForFile(
    const CefString& fileName) {
  DCHECK(!fileName.empty());

  // TODO(cef): Do not allow file IO on all threads (issue #1187).
  base::ScopedAllowBlockingForTesting allow_blocking;

  CefRefPtr<CefStreamReader> reader;
  FILE* file = base::OpenFile(base::FilePath(fileName), "rb");
  if (file) {
    reader = new CefFileReader(file, true);
  }
  return reader;
}

CefRefPtr<CefStreamReader> CefStreamReader::CreateForData(void* data,
                                                          size_t size) {
  DCHECK(data != nullptr);
  DCHECK(size > 0);
  CefRefPtr<CefStreamReader> reader;
  if (data && size > 0) {
    reader = new CefBytesReader(data, size);
  }
  return reader;
}

CefRefPtr<CefStreamReader> CefStreamReader::CreateForHandler(
    CefRefPtr<CefReadHandler> handler) {
  DCHECK(handler.get());
  CefRefPtr<CefStreamReader> reader;
  if (handler.get()) {
    reader = new CefHandlerReader(handler);
  }
  return reader;
}

CefRefPtr<CefStreamWriter> CefStreamWriter::CreateForFile(
    const CefString& fileName) {
  DCHECK(!fileName.empty());

  // TODO(cef): Do not allow file IO on all threads (issue #1187).
  base::ScopedAllowBlockingForTesting allow_blocking;

  CefRefPtr<CefStreamWriter> writer;
  FILE* file = base::OpenFile(base::FilePath(fileName), "wb");
  if (file) {
    writer = new CefFileWriter(file, true);
  }
  return writer;
}

CefRefPtr<CefStreamWriter> CefStreamWriter::CreateForHandler(
    CefRefPtr<CefWriteHandler> handler) {
  DCHECK(handler.get());
  CefRefPtr<CefStreamWriter> writer;
  if (handler.get()) {
    writer = new CefHandlerWriter(handler);
  }
  return writer;
}

// CefFileReader

CefFileReader::CefFileReader(FILE* file, bool close)
    : close_(close), file_(file) {}

CefFileReader::~CefFileReader() {
  base::AutoLock lock_scope(lock_);
  if (close_) {
    base::CloseFile(file_);
  }
}

size_t CefFileReader::Read(void* ptr, size_t size, size_t n) {
  base::AutoLock lock_scope(lock_);
  return fread(ptr, size, n, file_);
}

int CefFileReader::Seek(int64_t offset, int whence) {
  base::AutoLock lock_scope(lock_);
#if BUILDFLAG(IS_WIN)
  return _fseeki64(file_, offset, whence);
#else
  return fseek(file_, offset, whence);
#endif
}

int64_t CefFileReader::Tell() {
  base::AutoLock lock_scope(lock_);
#if BUILDFLAG(IS_WIN)
  return _ftelli64(file_);
#else
  return ftell(file_);
#endif
}

int CefFileReader::Eof() {
  base::AutoLock lock_scope(lock_);
  return feof(file_);
}

// CefFileWriter

CefFileWriter::CefFileWriter(FILE* file, bool close)
    : file_(file), close_(close) {}

CefFileWriter::~CefFileWriter() {
  base::AutoLock lock_scope(lock_);
  if (close_) {
    base::CloseFile(file_);
  }
}

size_t CefFileWriter::Write(const void* ptr, size_t size, size_t n) {
  base::AutoLock lock_scope(lock_);
  return (size_t)fwrite(ptr, size, n, file_);
}

int CefFileWriter::Seek(int64_t offset, int whence) {
  base::AutoLock lock_scope(lock_);
  return fseek(file_, offset, whence);
}

int64_t CefFileWriter::Tell() {
  base::AutoLock lock_scope(lock_);
  return ftell(file_);
}

int CefFileWriter::Flush() {
  base::AutoLock lock_scope(lock_);
  return fflush(file_);
}

// CefBytesReader

CefBytesReader::CefBytesReader(void* data, int64_t datasize) {
  SetData(data, datasize);
}

size_t CefBytesReader::Read(void* ptr, size_t size, size_t n) {
  base::AutoLock lock_scope(lock_);
  size_t s = (data_.size() - offset_) / size;
  size_t ret = std::min(n, s);
  memcpy(ptr, data_.data() + offset_, ret * size);
  offset_ += ret * size;
  return ret;
}

int CefBytesReader::Seek(int64_t offset, int whence) {
  int rv = -1L;
  base::AutoLock lock_scope(lock_);

  const int64_t size = base::checked_cast<int64_t>(data_.size());

  switch (whence) {
    case SEEK_CUR:
      if (offset_ + offset > size || offset_ + offset < 0) {
        break;
      }
      offset_ += offset;
      rv = 0;
      break;
    case SEEK_END: {
      int64_t offset_abs = std::abs(offset);
      if (offset_abs > size) {
        break;
      }
      offset_ = size - offset_abs;
      rv = 0;
      break;
    }
    case SEEK_SET:
      if (offset > size || offset < 0) {
        break;
      }
      offset_ = offset;
      rv = 0;
      break;
  }

  return rv;
}

int64_t CefBytesReader::Tell() {
  base::AutoLock lock_scope(lock_);
  return offset_;
}

int CefBytesReader::Eof() {
  base::AutoLock lock_scope(lock_);
  return (offset_ >= base::checked_cast<int64_t>(data_.size()));
}

void CefBytesReader::SetData(void* data, int64_t datasize) {
  base::AutoLock lock_scope(lock_);

  offset_ = 0;

  if (data && datasize > 0) {
    data_.reserve(datasize);
    std::copy(static_cast<unsigned char*>(data),
              static_cast<unsigned char*>(data) + datasize,
              std::back_inserter(data_));
  } else {
    data_.clear();
  }
}
