// Copyright (c) 2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef _FRAME_CPPTOC_H
#define _FRAME_CPPTOC_H

#ifndef BUILDING_CEF_SHARED
#pragma message("Warning: "__FILE__" may be accessed DLL-side only")
#else // BUILDING_CEF_SHARED

#include "cef.h"
#include "cef_capi.h"
#include "cpptoc.h"


// Wrap a C++ frame class with a C frame structure.
// This class may be instantiated and accessed DLL-side only.
class CefFrameCppToC
    : public CefCppToC<CefFrameCppToC, CefFrame, cef_frame_t>
{
public:
  CefFrameCppToC(CefFrame* cls);
  virtual ~CefFrameCppToC() {}
};


#endif // BUILDING_CEF_SHARED
#endif // _FRAME_CPPTOC_H
