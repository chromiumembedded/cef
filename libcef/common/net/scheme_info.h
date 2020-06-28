// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_COMMON_NET_SCHEME_INFO_H_
#define CEF_LIBCEF_COMMON_NET_SCHEME_INFO_H_
#pragma once

#include <string>

// Values are registered with all processes (url/url_util.h) and with Blink
// (SchemeRegistry) unless otherwise indicated.
struct CefSchemeInfo {
  // Lower-case ASCII scheme name.
  std::string scheme_name;

  // A scheme that is subject to URL canonicalization and parsing rules as
  // defined in the Common Internet Scheme Syntax RFC 1738 Section 3.1
  // available at http://www.ietf.org/rfc/rfc1738.txt.
  // This value is not registered with Blink.
  bool is_standard;

  // A scheme that will be treated the same as "file". For example, normal
  // pages cannot link to or access URLs of this scheme.
  bool is_local;

  // A scheme that can only be displayed from other content hosted with the
  // same scheme. For example, pages in other origins cannot create iframes or
  // hyperlinks to URLs with the scheme. For schemes that must be accessible
  // from other schemes set this value to false, set |is_cors_enabled| to
  // true, and use CORS "Access-Control-Allow-Origin" headers to further
  // restrict access.
  // This value is registered with Blink only.
  bool is_display_isolated;

  // A scheme that will be treated the same as "https". For example, loading
  // this scheme from other secure schemes will not trigger mixed content
  // warnings.
  bool is_secure;

  // A scheme that can be sent CORS requests. This value should be true in
  // most cases where |is_standard| is true.
  bool is_cors_enabled;

  // A scheme that can bypass Content-Security-Policy (CSP) checks. This value
  // should be false in most cases where |is_standard| is true.
  bool is_csp_bypassing;

  // A scheme that can perform fetch request.
  bool is_fetch_enabled;
};

#endif  // CEF_LIBCEF_COMMON_NET_SCHEME_INFO_H_
