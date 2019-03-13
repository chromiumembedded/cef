// Copyright (c) 2017 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_NET_NET_UTIL_H_
#define CEF_LIBCEF_BROWSER_NET_NET_UTIL_H_
#pragma once

namespace net {
class URLRequest;
}

namespace net_util {

// Returns true if |request| is handled internally and should not be exposed via
// the CEF API.
bool IsInternalRequest(const net::URLRequest* request);

}  // namespace net_util

#endif  // CEF_LIBCEF_BROWSER_NET_NET_UTIL_H_
