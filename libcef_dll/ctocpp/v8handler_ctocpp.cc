// Copyright (c) 2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "../precompiled_libcef.h"
#include "ctocpp/v8handler_ctocpp.h"
#include "cpptoc/v8value_cpptoc.h"


bool CefV8HandlerCToCpp::Execute(const std::wstring& name,
                                 CefRefPtr<CefV8Value> object,
                                 CefV8ValueList& arguments,
                                 CefRefPtr<CefV8Value>& retval,
                                 std::wstring& exception)
{
  if(CEF_MEMBER_MISSING(struct_, execute))
    return RV_CONTINUE;

  cef_v8value_t** argsStructPtr = NULL;
  int argsSize = arguments.size();
  if(argsSize > 0) {
    argsStructPtr = new cef_v8value_t*[argsSize];
    for(int i = 0; i < argsSize; ++i)
      argsStructPtr[i] = CefV8ValueCppToC::Wrap(arguments[i]);
  }

  cef_v8value_t* retvalStruct = NULL;
  cef_string_t exceptionStr = NULL;

  int rv = struct_->execute(struct_, name.c_str(),
      CefV8ValueCppToC::Wrap(object), argsSize, argsStructPtr, &retvalStruct,
      &exceptionStr);
  if(retvalStruct)
    retval = CefV8ValueCppToC::Unwrap(retvalStruct);
  if(exceptionStr) {
    exception = exceptionStr;
    cef_string_free(exceptionStr);
  }

  if(argsStructPtr)
    delete [] argsStructPtr;

  return rv;
}

#ifdef _DEBUG
long CefCToCpp<CefV8HandlerCToCpp, CefV8Handler, cef_v8handler_t>::DebugObjCt
    = 0;
#endif
