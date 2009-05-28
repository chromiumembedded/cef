// Copyright (c) 2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef _V8VALUE_CPPTOC_H
#define _V8VALUE_CPPTOC_H

#ifndef BUILDING_CEF_SHARED
#pragma message("Warning: "__FILE__" may be accessed DLL-side only")
#else // BUILDING_CEF_SHARED

#include "cef.h"
#include "cef_capi.h"
#include "cpptoc.h"


// Wrap a C++ v8value class with a C v8value structure.
// This class may be instantiated and accessed wrapper-side only.
class CefV8ValueCppToC
    : public CefCppToC<CefV8ValueCppToC, CefV8Value, cef_v8value_t>
{
public:
  CefV8ValueCppToC(CefV8Value* cls);
  virtual ~CefV8ValueCppToC() {}
};


#endif // BUILDING_CEF_SHARED
#endif // _V8VALUE_CPPTOC_H
