// Copyright 2015 The Chromium Embedded Framework Authors.
// Portions copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_EXTENSIONS_URL_REQUEST_UTIL_H_
#define CEF_LIBCEF_BROWSER_EXTENSIONS_URL_REQUEST_UTIL_H_

#include <string>

namespace base {
class FilePath;
}

namespace net {
class NetworkDelegate;
class URLRequest;
class URLRequestJob;
}

namespace extensions {
class Extension;
class InfoMap;

// Utilities related to URLRequest jobs for extension resources. Based on
// chrome/browser/extensions/chrome_url_request_util.cc.
namespace url_request_util {

// Creates a URLRequestJob for loading component extension resources out of
// a CEF resource bundle. Returns NULL if the requested resource is not a
// component extension resource.
net::URLRequestJob* MaybeCreateURLRequestResourceBundleJob(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate,
    const base::FilePath& directory_path,
    const std::string& content_security_policy,
    bool send_cors_header);

}  // namespace url_request_util
}  // namespace extensions

#endif  // CEF_LIBCEF_BROWSER_EXTENSIONS_URL_REQUEST_UTIL_H_
