// Copyright (c) 2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef _REQUEST_CPPTOC_H
#define _REQUEST_CPPTOC_H

#ifndef BUILDING_CEF_SHARED
#pragma message("Warning: "__FILE__" may be accessed DLL-side only")
#else // BUILDING_CEF_SHARED

#include "cef.h"
#include "cef_capi.h"
#include "cpptoc.h"


// Wrap a C++ request class with a C request structure.
// This class may be instantiated and accessed DLL-side only.
class CefRequestCppToC : public CefCppToC<CefRequest, cef_request_t>
{
public:
  CefRequestCppToC(CefRequest* cls);
  virtual ~CefRequestCppToC() {}
};


// Wrap a C++ post data class with a C post data structure.
// This class may be instantiated and accessed DLL-side only.
class CefPostDataCppToC : public CefCppToC<CefPostData, cef_post_data_t>
{
public:
  CefPostDataCppToC(CefPostData* cls);
  virtual ~CefPostDataCppToC() {}
};

class CefPostDataElementCppToC;


// Wrap a C++ post data element class with a C post data element structure.
// This class may be instantiated and accessed DLL-side only.
class CefPostDataElementCppToC :
    public CefCppToC<CefPostDataElement, cef_post_data_element_t>
{
public:
  CefPostDataElementCppToC(CefPostDataElement* cls);
  virtual ~CefPostDataElementCppToC() {}
};


#endif // BUILDING_CEF_SHARED
#endif // _REQUEST_CPPTOC_H
