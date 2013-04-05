// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_SCHEME_IMPL_H_
#define CEF_LIBCEF_BROWSER_SCHEME_IMPL_H_
#pragma once

namespace net {
class NetworkDelegate;
class URLRequest;
class URLRequestJob;
}

namespace scheme {

// Helper for chaining net::URLRequestJobFactory::ProtocolHandler
// implementations.
net::URLRequestJob* GetRequestJob(net::URLRequest* request,
                                  net::NetworkDelegate* network_delegate);

}  // namespace scheme

#endif  // CEF_LIBCEF_BROWSER_SCHEME_IMPL_H_
