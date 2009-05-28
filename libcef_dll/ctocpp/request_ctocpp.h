// Copyright (c) 2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef _REQUEST_CTOCPP_H
#define _REQUEST_CTOCPP_H

#ifndef USING_CEF_SHARED
#pragma message("Warning: "__FILE__" may be accessed wrapper-side only")
#else // USING_CEF_SHARED

#include "cef.h"
#include "cef_capi.h"
#include "ctocpp.h"


// Wrap a C request structure with a C++ request class.
// This class may be instantiated and accessed wrapper-side only.
class CefRequestCToCpp
    : public CefCToCpp<CefRequestCToCpp, CefRequest, cef_request_t>
{
public:
  CefRequestCToCpp(cef_request_t* str)
    : CefCToCpp<CefRequestCToCpp, CefRequest, cef_request_t>(str) {}
  virtual ~CefRequestCToCpp() {}

   // CefRequest methods
  virtual std::wstring GetURL();
  virtual void SetURL(const std::wstring& url);
  virtual std::wstring GetMethod();
  virtual void SetMethod(const std::wstring& method);
  virtual CefRefPtr<CefPostData> GetPostData();
  virtual void SetPostData(CefRefPtr<CefPostData> postData);
  virtual void GetHeaderMap(HeaderMap& headerMap);
  virtual void SetHeaderMap(const HeaderMap& headerMap);
  virtual void Set(const std::wstring& url,
                   const std::wstring& method,
                   CefRefPtr<CefPostData> postData,
                   const HeaderMap& headerMap);
};


// Wrap a C post data structure with a C++ post data class.
// This class may be instantiated and accessed wrapper-side only.
class CefPostDataCToCpp
    : public CefCToCpp<CefPostDataCToCpp, CefPostData, cef_post_data_t>
{
public:
  CefPostDataCToCpp(cef_post_data_t* str)
    : CefCToCpp<CefPostDataCToCpp, CefPostData, cef_post_data_t>(str) {}
  virtual ~CefPostDataCToCpp() {}

  // CefPostData methods
  virtual size_t GetElementCount();
  virtual void GetElements(ElementVector& elements);
  virtual bool RemoveElement(CefRefPtr<CefPostDataElement> element);
  virtual bool AddElement(CefRefPtr<CefPostDataElement> element);
  virtual void RemoveElements();
};


// Wrap a C post data element structure with a C++ post data element class.
// This class may be instantiated and accessed wrapper-side only.
class CefPostDataElementCToCpp
    : public CefCToCpp<CefPostDataElementCToCpp, CefPostDataElement,
                       cef_post_data_element_t>
{
public:
  CefPostDataElementCToCpp(cef_post_data_element_t* str)
    : CefCToCpp<CefPostDataElementCToCpp, CefPostDataElement,
                cef_post_data_element_t>(str) {}
  virtual ~CefPostDataElementCToCpp() {}

  // CefPostDataElement methods
  virtual void SetToEmpty();
  virtual void SetToFile(const std::wstring& fileName);
  virtual void SetToBytes(size_t size, const void* bytes);
  virtual Type GetType();
  virtual std::wstring GetFile();
  virtual size_t GetBytesCount();
  virtual size_t GetBytes(size_t size, void *bytes);
};


#endif // USING_CEF_SHARED
#endif // _REQUEST_CTOCPP_H
