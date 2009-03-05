// Copyright (c) 2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef _JSHANDLER_CPPTOC_H
#define _JSHANDLER_CPPTOC_H

#ifndef USING_CEF_SHARED
#pragma message("Warning: "__FILE__" may be accessed wrapper-side only")
#else // USING_CEF_SHARED

#include "cef.h"
#include "cef_capi.h"
#include "cpptoc.h"


// Wrap a C++ jshandler class with a C jshandler structure.
// This class may be instantiated and accessed wrapper-side only.
class CefJSHandlerCppToC : public CefCppToC<CefJSHandler, cef_jshandler_t>
{
public:
  CefJSHandlerCppToC(CefRefPtr<CefJSHandler> cls);
  virtual ~CefJSHandlerCppToC() {}
};


#endif // USING_CEF_SHARED
#endif // _JSHANDLER_CPPTOC_H
