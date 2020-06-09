// Copyright (c) 2019 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_COMMON_NET_SERVICE_NET_SERVICE_UTIL_H_
#define CEF_LIBCEF_COMMON_NET_SERVICE_NET_SERVICE_UTIL_H_

#include <map>
#include <string>

#include "include/internal/cef_types_wrappers.h"

#include "base/memory/scoped_refptr.h"
#include "net/cookies/cookie_constants.h"

namespace net {
class CanonicalCookie;
class HttpResponseHeaders;
struct RedirectInfo;
}  // namespace net

namespace network {
struct ResourceRequest;
}  // namespace network

class GURL;

namespace net_service {

// HTTP header names.
extern const char kHTTPLocationHeaderName[];
extern const char kHTTPSetCookieHeaderName[];

// HTTP header values.
extern const char kContentTypeApplicationFormURLEncoded[];

// Make an HTTP response status line.
// Set |for_replacement| to true if the result will be passed to
// HttpResponseHeaders::ReplaceStatusLine and false if the result will
// be passed to the HttpResponseHeaders constructor.
std::string MakeStatusLine(int status_code,
                           const std::string& status_text,
                           bool for_replacement);

// Make an HTTP Content-Type response header value.
std::string MakeContentTypeValue(const std::string& mime_type,
                                 const std::string& charset);

// Make a new HttpResponseHeaders object.
scoped_refptr<net::HttpResponseHeaders> MakeResponseHeaders(
    int status_code,
    const std::string& status_text,
    const std::string& mime_type,
    const std::string& charset,
    int64_t content_length,
    const std::multimap<std::string, std::string>& extra_headers,
    bool allow_existing_header_override);

// Make a RedirectInfo structure.
net::RedirectInfo MakeRedirectInfo(const network::ResourceRequest& request,
                                   const net::HttpResponseHeaders* headers,
                                   const GURL& new_location,
                                   int status_code);

// Populate |cookie|. Returns true on success.
bool MakeCefCookie(const net::CanonicalCookie& cc, CefCookie& cookie);
bool MakeCefCookie(const GURL& url,
                   const std::string& cookie_line,
                   CefCookie& cookie);

net::CookieSameSite MakeCookieSameSite(cef_cookie_same_site_t value);
net::CookiePriority MakeCookiePriority(cef_cookie_priority_t value);

}  // namespace net_service

#endif  // CEF_LIBCEF_COMMON_NET_SERVICE_NET_SERVICE_UTIL_H_
