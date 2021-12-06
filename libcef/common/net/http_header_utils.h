// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_COMMON_NET_HTTP_HEADER_UTILS_H_
#define CEF_LIBCEF_COMMON_NET_HTTP_HEADER_UTILS_H_
#pragma once

#include <map>
#include <string>

#include "include/cef_base.h"

namespace HttpHeaderUtils {

using HeaderMap = std::multimap<CefString, CefString>;

std::string GenerateHeaders(const HeaderMap& map);
void ParseHeaders(const std::string& header_str, HeaderMap& map);

// Convert |str| to lower-case.
void MakeASCIILower(std::string* str);

// Finds the first instance of |name| (already lower-case) in |map| with
// case-insensitive comparison.
HeaderMap::iterator FindHeaderInMap(const std::string& name, HeaderMap& map);

}  // namespace HttpHeaderUtils

#endif  // CEF_LIBCEF_COMMON_NET_HTTP_HEADER_UTILS_H_
