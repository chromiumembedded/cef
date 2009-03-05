// Copyright (c) 2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef _JSHANDLER_CTOCPP_H
#define _JSHANDLER_CTOCPP_H

#ifndef BUILDING_CEF_SHARED
#pragma message("Warning: "__FILE__" may be accessed DLL-side only")
#else // BUILDING_CEF_SHARED

#include "cef.h"
#include "cef_capi.h"
#include "ctocpp.h"


// Wrap a C jshandler structure with a C++ jshandler class.
// This class may be instantiated and accessed DLL-side only.
class CefJSHandlerCToCpp : public CefCToCpp<CefJSHandler, cef_jshandler_t>
{
public:
  CefJSHandlerCToCpp(cef_jshandler_t* str)
    : CefCToCpp<CefJSHandler, cef_jshandler_t>(str) {}
  virtual ~CefJSHandlerCToCpp() {}

  // CefJSHandler methods
  virtual bool HasMethod(CefRefPtr<CefBrowser> browser,
                         const std::wstring& name);
  virtual bool HasProperty(CefRefPtr<CefBrowser> browser,
                           const std::wstring& name);
  virtual bool SetProperty(CefRefPtr<CefBrowser> browser,
                           const std::wstring& name,
                           const CefRefPtr<CefVariant> value);
  virtual bool GetProperty(CefRefPtr<CefBrowser> browser,
                           const std::wstring& name,
                           CefRefPtr<CefVariant> value);
  virtual bool ExecuteMethod(CefRefPtr<CefBrowser> browser,
                             const std::wstring& name,
                             const VariantVector& args,
                             CefRefPtr<CefVariant> retval);
};


#endif // BUILDING_CEF_SHARED
#endif // _JSHANDLER_CTOCPP_H
