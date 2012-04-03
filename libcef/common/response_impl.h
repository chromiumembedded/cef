// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_COMMON_RESPONSE_IMPL_H_
#define CEF_LIBCEF_COMMON_RESPONSE_IMPL_H_
#pragma once

#include "include/cef_response.h"

namespace net {
class HttpResponseHeaders;
}

// Implementation of CefResponse.
class CefResponseImpl : public CefResponse {
 public:
  CefResponseImpl();
  ~CefResponseImpl() {}

  // CefResponse API
  virtual int GetStatus();
  virtual void SetStatus(int status);
  virtual CefString GetStatusText();
  virtual void SetStatusText(const CefString& statusText);
  virtual CefString GetMimeType();
  virtual void SetMimeType(const CefString& mimeType);
  virtual CefString GetHeader(const CefString& name);
  virtual void GetHeaderMap(HeaderMap& headerMap);
  virtual void SetHeaderMap(const HeaderMap& headerMap);

  net::HttpResponseHeaders* GetResponseHeaders();

 protected:
  int status_code_;
  CefString status_text_;
  CefString mime_type_;
  HeaderMap header_map_;

  IMPLEMENT_REFCOUNTING(CefResponseImpl);
  IMPLEMENT_LOCKING(CefResponseImpl);
};

#endif  // CEF_LIBCEF_COMMON_RESPONSE_IMPL_H_
