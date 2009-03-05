// Copyright (c) 2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef _STREAM_CPPTOC_H
#define _STREAM_CPPTOC_H

#ifndef BUILDING_CEF_SHARED
#pragma message("Warning: "__FILE__" may be accessed DLL-side only")
#else // BUILDING_CEF_SHARED

#include "cef.h"
#include "cef_capi.h"
#include "cpptoc.h"


// Wrap a C++ stream reader class with a C stream reader structure.
// This class may be instantiated and accessed DLL-side only.
class CefStreamReaderCppToC :
    public CefCppToC<CefStreamReader, cef_stream_reader_t>
{
public:
  CefStreamReaderCppToC(CefRefPtr<CefStreamReader> cls);
  virtual ~CefStreamReaderCppToC() {}
};


// Wrap a C++ stream writer class with a C stream writer structure.
// This class may be instantiated and accessed DLL-side only.
class CefStreamWriterCppToC :
    public CefCppToC<CefStreamWriter, cef_stream_writer_t>
{
public:
  CefStreamWriterCppToC(CefRefPtr<CefStreamWriter> cls);
  virtual ~CefStreamWriterCppToC() {}
};


#endif // BUILDING_CEF_SHARED
#endif // _STREAM_CPPTOC_H
