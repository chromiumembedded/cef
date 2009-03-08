// Copyright (c) 2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "../precompiled_libcef.h"
#include "ctocpp/stream_ctocpp.h"
#include "base/logging.h"


size_t CefStreamReaderCToCpp::Read(void *ptr, size_t size, size_t n)
{
  if(CEF_MEMBER_MISSING(struct_, read))
    return 0;

  return struct_->read(struct_, ptr, size, n);
}

int CefStreamReaderCToCpp::Seek(long offset, int whence)
{
  if(CEF_MEMBER_MISSING(struct_, seek))
    return 0;

  return struct_->seek(struct_, offset, whence);
}

long CefStreamReaderCToCpp::Tell()
{
  if(CEF_MEMBER_MISSING(struct_, tell))
    return 0;

  return struct_->tell(struct_);
}

int CefStreamReaderCToCpp::Eof()
{
  if(CEF_MEMBER_MISSING(struct_, eof))
    return 0;

  return struct_->eof(struct_);
}

#ifdef _DEBUG
long CefCToCpp<CefStreamReader, cef_stream_reader_t>::DebugObjCt = 0;
#endif


size_t CefStreamWriterCToCpp::Write(const void *ptr, size_t size, size_t n)
{
  if(CEF_MEMBER_MISSING(struct_, write))
    return 0;

  return struct_->write(struct_, ptr, size, n);
}

int CefStreamWriterCToCpp::Seek(long offset, int whence)
{
  if(CEF_MEMBER_MISSING(struct_, seek))
    return 0;

  return struct_->seek(struct_, offset, whence);
}

long CefStreamWriterCToCpp::Tell()
{
  if(CEF_MEMBER_MISSING(struct_, tell))
    return 0;

  return struct_->tell(struct_);
}

int CefStreamWriterCToCpp::Flush()
{
  if(CEF_MEMBER_MISSING(struct_, flush))
    return 0;

  return struct_->flush(struct_);
}

#ifdef _DEBUG
long CefCToCpp<CefStreamWriter, cef_stream_writer_t>::DebugObjCt = 0;
#endif
