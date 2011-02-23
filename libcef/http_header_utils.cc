// Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/cef.h"
#include "http_header_utils.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"

using net::HttpResponseHeaders;

namespace HttpHeaderUtils {

std::string GenerateHeaders(const HeaderMap& map)
{
  std::string headers;

  for(HeaderMap::const_iterator header = map.begin();
      header != map.end();
      ++header) {
    const CefString& key = header->first;
    const CefString& value = header->second;

    if(!key.empty()) {
      // Delimit with "\r\n".
      if(!headers.empty())
        headers += "\r\n";

      headers += std::string(key) + ": " + std::string(value);
    }
  }

  return headers;
}

void ParseHeaders(const std::string& header_str, HeaderMap& map)
{
  // Parse the request header values
  std::string headerStr = "HTTP/1.1 200 OK\n";
  headerStr += header_str;
  scoped_refptr<net::HttpResponseHeaders> headers =
      new HttpResponseHeaders(net::HttpUtil::AssembleRawHeaders(
          headerStr.c_str(), headerStr.length()));
  void* iter = NULL;
  std::string name, value;
  while(headers->EnumerateHeaderLines(&iter, &name, &value))
    map.insert(std::make_pair(name, value));
}

} // namespace HttpHeaderUtils
