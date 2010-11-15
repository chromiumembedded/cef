// Copyright (c) 2010 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/cef_wrapper.h"
#include "base/logging.h"

CefByteReadHandler::CefByteReadHandler(const unsigned char* bytes, size_t size,
                                       CefRefPtr<CefBase> source)
  : bytes_(bytes), size_(size), offset_(0), source_(source)
{
}

size_t CefByteReadHandler::Read(void* ptr, size_t size, size_t n)
{
  Lock();
  size_t s = (size_ - offset_) / size;
  size_t ret = std::min(n, s);
  memcpy(ptr, bytes_ + offset_, ret * size);
  offset_ += ret * size;
  Unlock();
  return ret;
}

int CefByteReadHandler::Seek(long offset, int whence)
{
  int rv = -1L;
  Lock();
  switch(whence) {
  case SEEK_CUR:
    if(offset_ + offset > size_)
      break;
    offset_ += offset;
    rv = offset_;
    break;
  case SEEK_END:
    if(offset > static_cast<long>(size_))
      break;
    offset_ = size_ - offset;
    rv = offset_;
  case SEEK_SET:
    if(offset > static_cast<long>(size_))
      break;
    offset_ = offset;
    rv = offset_;
    break;
  }
  Unlock();

  return rv;
}

long CefByteReadHandler::Tell()
{
  Lock();
  long rv = offset_;
  Unlock();
  return rv;
}

int CefByteReadHandler::Eof()
{
  Lock();
  int rv = (offset_ >= size_);
  Unlock();
  return rv;
}
