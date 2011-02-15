// Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/cef.h"
#include "response_impl.h"

#include "base/logging.h"
#include "http_header_utils.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLResponse.h"

CefResponseImpl::CefResponseImpl(const WebKit::WebURLResponse &response)
{
  DCHECK(!response.isNull());

  WebKit::WebString str;
  status_code_ = response.httpStatusCode();
  str = response.httpStatusText();
  status_text_ = CefString(str);

  HttpHeaderUtils::HeaderVisitor visitor(&header_map_);
  response.visitHTTPHeaderFields(&visitor);
}

int CefResponseImpl::GetStatus()
{
  return status_code_;
}

CefString CefResponseImpl::GetStatusText()
{
  return status_text_;
}

CefString CefResponseImpl::GetHeader(const CefString& name)
{
  CefString value;

  HeaderMap::const_iterator it = header_map_.find(name);
  if (it != header_map_.end())
    value = it->second;

  return value;
}

void CefResponseImpl::GetHeaderMap(HeaderMap& map)
{
  map = header_map_;
}
