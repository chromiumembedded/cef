// Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_HTTP_HEADER_UTILS_H_
#define CEF_LIBCEF_HTTP_HEADER_UTILS_H_
#pragma once

#include <string>

#include "include/cef_request.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebHTTPHeaderVisitor.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"

namespace HttpHeaderUtils {

typedef CefRequest::HeaderMap HeaderMap;

class HeaderVisitor : public WebKit::WebHTTPHeaderVisitor {
 public:
  explicit HeaderVisitor(HeaderMap* map) : map_(map) {}

  virtual void visitHeader(const WebKit::WebString& name,
                           const WebKit::WebString& value) {
    map_->insert(std::make_pair(string16(name), string16(value)));
  }

 private:
  HeaderMap* map_;
};

std::string GenerateHeaders(const HeaderMap& map);
void ParseHeaders(const std::string& header_str, HeaderMap& map);

};  // namespace HttpHeaderUtils

#endif  // CEF_LIBCEF_HTTP_HEADER_UTILS_H_
