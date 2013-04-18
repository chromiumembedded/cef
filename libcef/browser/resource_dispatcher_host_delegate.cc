// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/browser/resource_dispatcher_host_delegate.h"
#include "libcef/browser/browser_host_impl.h"
#include "libcef/browser/origin_whitelist_impl.h"
#include "content/public/common/resource_response.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"

CefResourceDispatcherHostDelegate::CefResourceDispatcherHostDelegate() {
}

CefResourceDispatcherHostDelegate::~CefResourceDispatcherHostDelegate() {
}

bool CefResourceDispatcherHostDelegate::HandleExternalProtocol(const GURL& url,
                                                               int child_id,
                                                               int route_id) {
  CefRefPtr<CefBrowserHostImpl> browser =
      CefBrowserHostImpl::GetBrowserByRoutingID(child_id, route_id);
  if (browser.get())
    browser->HandleExternalProtocol(url);
  return false;
}

void CefResourceDispatcherHostDelegate::OnRequestRedirected(
    const GURL& redirect_url,
    net::URLRequest* request,
    content::ResourceContext* resource_context,
    content::ResourceResponse* response) {
  const GURL& active_url = request->url();
  if (active_url.is_valid() && redirect_url.is_valid() &&
      active_url.GetOrigin() != redirect_url.GetOrigin() &&
      HasCrossOriginWhitelistEntry(active_url, redirect_url)) {
    if (!response->head.headers)
      response->head.headers = new net::HttpResponseHeaders(std::string());

    // Add CORS headers to support XMLHttpRequest redirects.
    response->head.headers->AddHeader("Access-Control-Allow-Origin: " +
        active_url.scheme() + "://" + active_url.host());
    response->head.headers->AddHeader("Access-Control-Allow-Credentials: true");
  }
}
