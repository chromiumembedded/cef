// Copyright (c) 2019 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_NET_SERVICE_RESOURCE_REQUEST_HANDLER_WRAPPER_H_
#define CEF_LIBCEF_BROWSER_NET_SERVICE_RESOURCE_REQUEST_HANDLER_WRAPPER_H_

#include "base/functional/callback_forward.h"
#include "content/public/browser/web_contents.h"

namespace content {
class BrowserContext;
class RenderFrameHost;
}  // namespace content

namespace network {
struct ResourceRequest;
}

namespace url {
class Origin;
}

namespace net_service {

class InterceptedRequestHandler;

// Create an InterceptedRequestHandler that will delegate to a
// CefResourceRequestHandler. The resulting object should be passed to
// ProxyURLLoaderFactory::CreateProxy. Called on the UI thread only.
std::unique_ptr<InterceptedRequestHandler> CreateInterceptedRequestHandler(
    content::BrowserContext* browser_context,
    content::RenderFrameHost* frame,
    int render_process_id,
    bool is_navigation,
    bool is_download,
    const url::Origin& request_initiator);

// Create an InterceptedRequestHandler that will delegate to a
// CefResourceRequestHandler. The resulting object should be passed to
// ProxyURLLoaderFactory::CreateProxy. Called on the UI thread only.
std::unique_ptr<InterceptedRequestHandler> CreateInterceptedRequestHandler(
    content::WebContents::Getter web_contents_getter,
    int frame_tree_node_id,
    const network::ResourceRequest& request,
    const base::RepeatingClosure& unhandled_request_callback);

}  // namespace net_service

#endif  // CEF_LIBCEF_BROWSER_NET_SERVICE_RESOURCE_REQUEST_HANDLER_WRAPPER_H_
