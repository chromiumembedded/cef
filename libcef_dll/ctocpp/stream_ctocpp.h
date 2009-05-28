// Copyright (c) 2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef _STREAM_CTOCPP_H
#define _STREAM_CTOCPP_H

#ifndef USING_CEF_SHARED
#pragma message("Warning: "__FILE__" may be accessed wrapper-side only")
#else // USING_CEF_SHARED

#include "cef.h"
#include "cef_capi.h"
#include "ctocpp.h"


// Wrap a C stream reader structure with a C++ stream reader class.
// This class may be instantiated and accessed wrapper-side only.
class CefStreamReaderCToCpp
    : public CefCToCpp<CefStreamReaderCToCpp, CefStreamReader,
                       cef_stream_reader_t>
{
public:
  CefStreamReaderCToCpp(cef_stream_reader_t* str)
    : CefCToCpp<CefStreamReaderCToCpp, CefStreamReader,
                cef_stream_reader_t>(str) {}
  virtual ~CefStreamReaderCToCpp() {}

  // CefStreamReader methods
  virtual size_t Read(void *ptr, size_t size, size_t n);
  virtual int Seek(long offset, int whence);
  virtual long Tell();
  virtual int Eof();
};


// Wrap a C stream writer structure with a C++ stream writer class.
// This class may be instantiated and accessed wrapper-side only.
class CefStreamWriterCToCpp
    : public CefCToCpp<CefStreamWriterCToCpp, CefStreamWriter,
                       cef_stream_writer_t>
{
public:
  CefStreamWriterCToCpp(cef_stream_writer_t* str)
    : CefCToCpp<CefStreamWriterCToCpp, CefStreamWriter,
                cef_stream_writer_t>(str) {}
  virtual ~CefStreamWriterCToCpp() {}

  // CefStreamWriter methods
  virtual size_t Write(const void *ptr, size_t size, size_t n);
  virtual int Seek(long offset, int whence);
  virtual long Tell();
  virtual int Flush();
};


#endif // USING_CEF_SHARED
#endif // _STREAM_CTOCPP_H
