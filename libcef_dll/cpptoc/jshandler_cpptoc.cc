// Copyright (c) 2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "../precompiled_libcef.h"
#include "cpptoc/jshandler_cpptoc.h"
#include "ctocpp/browser_ctocpp.h"
#include "ctocpp/variant_ctocpp.h"
#include "base/logging.h"


bool CEF_CALLBACK jshandler_has_method(struct _cef_jshandler_t* jshandler,
    cef_browser_t* browser, const wchar_t* name)
{
  DCHECK(jshandler);
  DCHECK(browser);
  if(!jshandler || !browser)
    return RV_CONTINUE;

  CefJSHandlerCppToC::Struct* impl =
      reinterpret_cast<CefJSHandlerCppToC::Struct*>(jshandler);
  
  CefBrowserCToCpp* bp = new CefBrowserCToCpp(browser);
  CefRefPtr<CefBrowser> browserPtr(bp);
  bp->UnderlyingRelease();
  
  std::wstring nameStr;
  if(name)
    nameStr = name;

  return impl->class_->GetClass()->HasMethod(browserPtr, nameStr);
}

bool CEF_CALLBACK jshandler_has_property(struct _cef_jshandler_t* jshandler,
    cef_browser_t* browser, const wchar_t* name)
{
  DCHECK(jshandler);
  DCHECK(browser);
  if(!jshandler || !browser)
    return RV_CONTINUE;

  CefJSHandlerCppToC::Struct* impl =
      reinterpret_cast<CefJSHandlerCppToC::Struct*>(jshandler);
  
  CefBrowserCToCpp* bp = new CefBrowserCToCpp(browser);
  CefRefPtr<CefBrowser> browserPtr(bp);
  bp->UnderlyingRelease();
  
  std::wstring nameStr;
  if(name)
    nameStr = name;

  return impl->class_->GetClass()->HasProperty(browserPtr, nameStr);
}

bool CEF_CALLBACK jshandler_set_property(struct _cef_jshandler_t* jshandler,
    cef_browser_t* browser, const wchar_t* name,
    struct _cef_variant_t* value)
{
  DCHECK(jshandler);
  DCHECK(browser);
  if(!jshandler || !browser)
    return RV_CONTINUE;

  CefJSHandlerCppToC::Struct* impl =
      reinterpret_cast<CefJSHandlerCppToC::Struct*>(jshandler);
  
  CefBrowserCToCpp* bp = new CefBrowserCToCpp(browser);
  CefRefPtr<CefBrowser> browserPtr(bp);
  bp->UnderlyingRelease();

  std::wstring nameStr;
  if(name)
    nameStr = name;

  CefVariantCToCpp* vp = new CefVariantCToCpp(value);
  CefRefPtr<CefVariant> valuePtr(vp);
  vp->UnderlyingRelease();
  
  return impl->class_->GetClass()->SetProperty(browserPtr, nameStr, valuePtr);
}

bool CEF_CALLBACK jshandler_get_property(struct _cef_jshandler_t* jshandler,
    cef_browser_t* browser, const wchar_t* name,
    struct _cef_variant_t* value)
{
  DCHECK(jshandler);
  DCHECK(browser);
  if(!jshandler || !browser)
    return RV_CONTINUE;

  CefJSHandlerCppToC::Struct* impl =
      reinterpret_cast<CefJSHandlerCppToC::Struct*>(jshandler);
  
  CefBrowserCToCpp* bp = new CefBrowserCToCpp(browser);
  CefRefPtr<CefBrowser> browserPtr(bp);
  bp->UnderlyingRelease();
  
  std::wstring nameStr;
  if(name)
    nameStr = name;

  CefVariantCToCpp* vp = new CefVariantCToCpp(value);
  CefRefPtr<CefVariant> valuePtr(vp);
  vp->UnderlyingRelease();
  
  return impl->class_->GetClass()->GetProperty(browserPtr, nameStr, valuePtr);
}

bool CEF_CALLBACK jshandler_execute_method(struct _cef_jshandler_t* jshandler,
    cef_browser_t* browser, const wchar_t* name, size_t numargs,
    struct _cef_variant_t** args, struct _cef_variant_t* retval)
{
  DCHECK(jshandler);
  DCHECK(browser);
  if(!jshandler || !browser)
    return RV_CONTINUE;

  CefJSHandlerCppToC::Struct* impl =
      reinterpret_cast<CefJSHandlerCppToC::Struct*>(jshandler);
  
  CefBrowserCToCpp* bp = new CefBrowserCToCpp(browser);
  CefRefPtr<CefBrowser> browserPtr(bp);
  bp->UnderlyingRelease();
  
  std::wstring nameStr;
  if(name)
    nameStr = name;

  CefVariantCToCpp* vp = new CefVariantCToCpp(retval);
  CefRefPtr<CefVariant> retvalPtr(vp);
  vp->UnderlyingRelease();
  
  CefJSHandler::VariantVector vec;
  for(int i = 0; i < (int)numargs; ++i) {
    vp = new CefVariantCToCpp(args[i]);
    CefRefPtr<CefVariant> argPtr(vp);
    vp->UnderlyingRelease();
    vec.push_back(argPtr);
  }

  return impl->class_->GetClass()->ExecuteMethod(browserPtr, nameStr, vec,
    retvalPtr);
}


CefJSHandlerCppToC::CefJSHandlerCppToC(CefRefPtr<CefJSHandler> cls)
    : CefCppToC<CefJSHandler, cef_jshandler_t>(cls)
{
  struct_.struct_.has_method = jshandler_has_method;
  struct_.struct_.has_property = jshandler_has_property;
  struct_.struct_.set_property = jshandler_set_property;
  struct_.struct_.get_property = jshandler_get_property;
  struct_.struct_.execute_method = jshandler_execute_method;
}
