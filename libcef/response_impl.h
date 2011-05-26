// Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef _RESPONSE_IMPL_H
#define _RESPONSE_IMPL_H

#include "include/cef.h"

namespace net {
class HttpResponseHeaders;
}
namespace WebKit {
class WebURLResponse;
};

// Implementation of CefResponse.
class CefResponseImpl : public CefResponse
{
public:
  CefResponseImpl();
  CefResponseImpl(const WebKit::WebURLResponse& response);
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

#endif // _RESPONSE_IMPL_H
