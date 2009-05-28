// Copyright (c) 2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "../precompiled_libcef.h"
#include "cpptoc/v8handler_cpptoc.h"
#include "ctocpp/v8value_ctocpp.h"


int CEF_CALLBACK v8handler_execute(struct _cef_v8handler_t* v8handler,
    const wchar_t* name, struct _cef_v8value_t* object, size_t numargs,
    struct _cef_v8value_t** args, struct _cef_v8value_t** retval,
    cef_string_t* exception)
{
  DCHECK(v8handler);
  if(!v8handler)
    return RV_CONTINUE;

  CefRefPtr<CefV8Value> objectPtr;
  if(object)
    objectPtr = CefV8ValueCToCpp::Wrap(object);

  std::wstring nameStr;
  if(name)
    nameStr = name;

  CefV8ValueList list;
  for(size_t i = 0; i < numargs; ++i)
    list.push_back(CefV8ValueCToCpp::Wrap(args[i]));

  CefRefPtr<CefV8Value> retValPtr;
  std::wstring exceptionStr;
  bool rv = CefV8HandlerCppToC::Get(v8handler)->Execute(nameStr, objectPtr,
    list, retValPtr, exceptionStr);
  if(rv) {
    if(!exceptionStr.empty() && exception)
      *exception = cef_string_alloc(exceptionStr.c_str());
    if(retValPtr.get() && retval)
      *retval = CefV8ValueCToCpp::Unwrap(retValPtr);
  }

  return rv;
}

CefV8HandlerCppToC::CefV8HandlerCppToC(CefV8Handler* cls)
    : CefCppToC<CefV8HandlerCppToC, CefV8Handler, cef_v8handler_t>(cls)
{
  struct_.struct_.execute = v8handler_execute;
}

#ifdef _DEBUG
long CefCppToC<CefV8HandlerCppToC, CefV8Handler, cef_v8handler_t>::DebugObjCt
    = 0;
#endif
