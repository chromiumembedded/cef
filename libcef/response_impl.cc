// Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/cef.h"
#include "response_impl.h"

#include "base/logging.h"
#include "base/stringprintf.h"
#include "http_header_utils.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLResponse.h"

CefResponseImpl::CefResponseImpl()
  : status_code_(0)
{
}

CefResponseImpl::CefResponseImpl(const WebKit::WebURLResponse &response)
{
  DCHECK(!response.isNull());

  WebKit::WebString str;
  status_code_ = response.httpStatusCode();
  str = response.httpStatusText();
  status_text_ = CefString(str);
  str = response.mimeType();
  mime_type_ = CefString(str);

  HttpHeaderUtils::HeaderVisitor visitor(&header_map_);
  response.visitHTTPHeaderFields(&visitor);
}

int CefResponseImpl::GetStatus()
{
  AutoLock lock_scope(this);
  return status_code_;
}

void CefResponseImpl::SetStatus(int status)
{
  AutoLock lock_scope(this);
  status_code_ = status;
}

CefString CefResponseImpl::GetStatusText()
{
  AutoLock lock_scope(this);
  return status_text_;
}

void CefResponseImpl::SetStatusText(const CefString& statusText)
{
  AutoLock lock_scope(this);
  status_text_ = statusText;
}

CefString CefResponseImpl::GetMimeType()
{
  AutoLock lock_scope(this);
  return mime_type_;
}

void CefResponseImpl::SetMimeType(const CefString& mimeType)
{
  AutoLock lock_scope(this);
  mime_type_ = mimeType;
}

CefString CefResponseImpl::GetHeader(const CefString& name)
{
  AutoLock lock_scope(this);

  CefString value;

  HeaderMap::const_iterator it = header_map_.find(name);
  if (it != header_map_.end())
    value = it->second;

  return value;
}

void CefResponseImpl::GetHeaderMap(HeaderMap& map)
{
  AutoLock lock_scope(this);
  map = header_map_;
}

void CefResponseImpl::SetHeaderMap(const HeaderMap& headerMap)
{
  AutoLock lock_scope(this);
  header_map_ = headerMap;
}

CefString CefResponseImpl::GenerateResponseLine()
{
  AutoLock lock_scope(this);

  std::string response_line;
  std::string status_text;

  if(status_text_.empty())
    status_text = (status_code_ == 200)?"OK":"ERROR";
  else
    status_text = status_text_;

  base::SStringPrintf(&response_line, "HTTP/1.1 %d %s", status_code_,
                      status_text.c_str());

  CefString value(response_line);

  return value;
}
