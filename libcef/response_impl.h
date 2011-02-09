// Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef _RESPONSE_IMPL_H
#define _RESPONSE_IMPL_H

#include "include/cef.h"

namespace WebKit {
class WebURLResponse;
};

// Implementation of CefResponse.
class CefResponseImpl : public CefThreadSafeBase<CefResponse>
{
public:
  CefResponseImpl(const WebKit::WebURLResponse& response);
  ~CefResponseImpl() {}

  // CefResponse API
  virtual int GetStatus();
  virtual CefString GetStatusText();
  virtual CefString GetHeader(const CefString& name);
  virtual void GetHeaderMap(HeaderMap& headerMap);

protected:
  int status_code_;
  CefString status_text_;
  HeaderMap header_map_;
};

#endif // _RESPONSE_IMPL_H
