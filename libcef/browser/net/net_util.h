// Copyright (c) 2017 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_NET_NET_UTIL_H_
#define CEF_LIBCEF_BROWSER_NET_NET_UTIL_H_
#pragma once

#include "include/cef_resource_request_handler.h"
#include "libcef/common/request_impl.h"

#include "content/public/browser/resource_request_info.h"

namespace net {
class URLRequest;
}

class GURL;

class CefBrowserHostImpl;
class CefBrowserInfo;

namespace net_util {

// Returns true if |request| is handled internally and should not be exposed via
// the CEF API.
bool IsInternalRequest(const net::URLRequest* request);

// Returns the browser associated with the specified URLRequest.
CefRefPtr<CefBrowserHostImpl> GetBrowserForRequest(
    const net::URLRequest* request);

// Returns the frame associated with the specified URLRequest.
CefRefPtr<CefFrame> GetFrameForRequest(
    scoped_refptr<CefBrowserInfo> browser_info,
    const net::URLRequest* request);

// Returns the appropriate CefResourceRequestHandler as determined by the
// associated CefBrowser/CefRequestHandler and/or CefRequestContextHandler, if
// any. The out-params will be nullptr if no handler is returned. Otherwise,
// the |cef_request| parameter will be set based on the contents of |request|
// (read-only by default), and the |cef_browser| and |cef_frame| parameters
// will be set if the request is associated with a browser.
CefRefPtr<CefResourceRequestHandler> GetResourceRequestHandler(
    const net::URLRequest* request,
    CefRefPtr<CefRequestImpl>& cef_request,
    CefRefPtr<CefBrowser>& cef_browser,
    CefRefPtr<CefFrame>& cef_frame);

void HandleExternalProtocol(
    CefRefPtr<CefRequestImpl> request,
    const content::ResourceRequestInfo::WebContentsGetter& web_contents_getter);

}  // namespace net_util

#endif  // CEF_LIBCEF_BROWSER_NET_NET_UTIL_H_
