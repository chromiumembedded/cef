// Copyright (c) 2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "../precompiled_libcef.h"
#include "cpptoc/browser_cpptoc.h"
#include "cpptoc/variant_cpptoc.h"
#include "ctocpp/jshandler_ctocpp.h"


bool CefJSHandlerCToCpp::HasMethod(CefRefPtr<CefBrowser> browser,
                                   const std::wstring& name)
{
  if(CEF_MEMBER_MISSING(struct_, has_method))
    return RV_CONTINUE;

  CefBrowserCppToC* browserPtr = new CefBrowserCppToC(browser);
  browserPtr->AddRef();

  return struct_->has_method(struct_, browserPtr->GetStruct(), name.c_str());
}

bool CefJSHandlerCToCpp::HasProperty(CefRefPtr<CefBrowser> browser,
                                     const std::wstring& name)
{
  if(CEF_MEMBER_MISSING(struct_, has_method))
    return RV_CONTINUE;

  CefBrowserCppToC* browserPtr = new CefBrowserCppToC(browser);
  browserPtr->AddRef();

  return struct_->has_property(struct_, browserPtr->GetStruct(), name.c_str());
}

bool CefJSHandlerCToCpp::SetProperty(CefRefPtr<CefBrowser> browser,
                                     const std::wstring& name,
                                     const CefRefPtr<CefVariant> value)
{
  if(CEF_MEMBER_MISSING(struct_, has_method))
    return RV_CONTINUE;

  CefBrowserCppToC* browserPtr = new CefBrowserCppToC(browser);
  browserPtr->AddRef();

  CefVariantCppToC* valuePtr = new CefVariantCppToC(value);
  valuePtr->AddRef();

  return struct_->set_property(struct_, browserPtr->GetStruct(), name.c_str(),
      valuePtr->GetStruct());
}

bool CefJSHandlerCToCpp::GetProperty(CefRefPtr<CefBrowser> browser,
                                     const std::wstring& name,
                                     CefRefPtr<CefVariant> value)
{
  if(CEF_MEMBER_MISSING(struct_, has_method))
    return RV_CONTINUE;

  CefBrowserCppToC* browserPtr = new CefBrowserCppToC(browser);
  browserPtr->AddRef();

  CefVariantCppToC* valuePtr = new CefVariantCppToC(value);
  valuePtr->AddRef();

  return struct_->get_property(struct_, browserPtr->GetStruct(), name.c_str(),
      valuePtr->GetStruct());
}

bool CefJSHandlerCToCpp::ExecuteMethod(CefRefPtr<CefBrowser> browser,
                                       const std::wstring& name,
                                       const VariantVector& args,
                                       CefRefPtr<CefVariant> retval)
{
  if(CEF_MEMBER_MISSING(struct_, has_method))
    return RV_CONTINUE;

  CefBrowserCppToC* browserPtr = new CefBrowserCppToC(browser);
  browserPtr->AddRef();

  CefVariantCppToC* retvalPtr = new CefVariantCppToC(retval);
  retvalPtr->AddRef();

  cef_variant_t** argsStructPtr = NULL;
  int argsSize = args.size();
  if(argsSize > 0) {
    CefVariantCppToC* vPtr;
    argsStructPtr = new cef_variant_t*[argsSize];
    for(int i = 0; i < argsSize; ++i) {
      vPtr = new CefVariantCppToC(args[i]);
      vPtr->AddRef();
      argsStructPtr[i] = vPtr->GetStruct();
    }
  }

  int rv = struct_->execute_method(struct_, browserPtr->GetStruct(),
      name.c_str(), argsSize, argsStructPtr, retvalPtr->GetStruct());

  if(argsStructPtr)
    delete [] argsStructPtr;

  return rv;
}

#ifdef _DEBUG
long CefCToCpp<CefJSHandler, cef_jshandler_t>::DebugObjCt = 0;
#endif
