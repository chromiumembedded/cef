// Copyright (c) 2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef _HANDLER_CPPTOC_H
#define _HANDLER_CPPTOC_H

#ifndef USING_CEF_SHARED
#pragma message("Warning: "__FILE__" may be accessed wrapper-side only")
#else // USING_CEF_SHARED

#include "cef.h"
#include "cef_capi.h"
#include "cpptoc.h"


// Wrap a C++ handler class with a C handler structure.
// This class may be instantiated and accessed wrapper-side only.
class CefHandlerCppToC : public CefCppToC<CefHandler, cef_handler_t>
{
public:
  CefHandlerCppToC(CefHandler* cls);
  virtual ~CefHandlerCppToC() {}
};


#endif // USING_CEF_SHARED
#endif // _HANDLER_CPPTOC_H
