// Copyright (c) 2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef _BROWSER_CPPTOC_H
#define _BROWSER_CPPTOC_H

#ifndef BUILDING_CEF_SHARED
#pragma message("Warning: "__FILE__" may be accessed DLL-side only")
#else // BUILDING_CEF_SHARED

#include "cef.h"
#include "cef_capi.h"
#include "cpptoc.h"


// Wrap a C++ browser class with a C browser structure.
// This class may be instantiated and accessed DLL-side only.
class CefBrowserCppToC : public CefCppToC<CefBrowser, cef_browser_t>
{
public:
  CefBrowserCppToC(CefBrowser* cls);
  virtual ~CefBrowserCppToC() {}
};


#endif // BUILDING_CEF_SHARED
#endif // _BROWSER_CPPTOC_H
