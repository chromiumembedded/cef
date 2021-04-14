// Copyright (c) 2021 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_COMMON_NET_URL_UTIL_H_
#define CEF_LIBCEF_COMMON_NET_URL_UTIL_H_
#pragma once

#include "include/cef_base.h"

class GURL;

namespace url_util {

// Convert |url| to a GURL, adding a scheme prefix if necessary.
// If |fixup| is true then FixupGURL() will also be called.
GURL MakeGURL(const CefString& url, bool fixup);

// Fix common problems with user-typed text. Among other things, this:
// - Converts absolute file paths to "file://" URLs.
// - Normalizes "about:" and "chrome:" to "chrome://" URLs
// Modifies |gurl| if necessary. Returns true if |gurl| is empty or valid.
bool FixupGURL(GURL& gurl);

}  // namespace url_util

#endif  // CEF_LIBCEF_COMMON_NET_URL_UTIL_H_
