// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/browser/resource_dispatcher_host_delegate.h"
#include "libcef/browser/browser_host_impl.h"
#include "libcef/browser/origin_whitelist_impl.h"
#include "libcef/browser/thread_util.h"
#include "libcef/common/request_impl.h"

#include "base/memory/scoped_vector.h"
#include "components/navigation_interception/intercept_navigation_resource_throttle.h"
#include "components/navigation_interception/navigation_params.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/common/resource_response.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"

namespace {

bool NavigationOnUIThread(
    int64 frame_id,
    CefRefPtr<CefRequestImpl> request,
    content::WebContents* source,
    const navigation_interception::NavigationParams& params) {
  CEF_REQUIRE_UIT();

  bool ignore_navigation = false;

  CefRefPtr<CefBrowserHostImpl> browser =
      CefBrowserHostImpl::GetBrowserForContents(source);
  DCHECK(browser.get());
  if (browser.get()) {
    CefRefPtr<CefClient> client = browser->GetClient();
    if (client.get()) {
      CefRefPtr<CefRequestHandler> handler = client->GetRequestHandler();
      if (handler.get()) {
        CefRefPtr<CefFrame> frame;
        if (frame_id >= 0)
          frame = browser->GetFrame(frame_id);
        DCHECK(frame.get());
        if (frame.get()) {
          ignore_navigation = handler->OnBeforeBrowse(
              browser.get(), frame, request.get(), params.is_redirect());
        }
      }
    }
  }

  return ignore_navigation;
}

}  // namespace

CefResourceDispatcherHostDelegate::CefResourceDispatcherHostDelegate() {
}

CefResourceDispatcherHostDelegate::~CefResourceDispatcherHostDelegate() {
}

void CefResourceDispatcherHostDelegate::RequestBeginning(
    net::URLRequest* request,
    content::ResourceContext* resource_context,
    content::AppCacheService* appcache_service,
    content::ResourceType resource_type,
    ScopedVector<content::ResourceThrottle>* throttles) {
  if (resource_type == content::ResourceType::RESOURCE_TYPE_MAIN_FRAME ||
      resource_type == content::ResourceType::RESOURCE_TYPE_SUB_FRAME) {
    int64 frame_id = -1;

    // ResourceRequestInfo will not exist for requests originating from
    // WebURLLoader in the render process.
    const content::ResourceRequestInfo* info =
        content::ResourceRequestInfo::ForRequest(request);
    if (info)
      frame_id = info->GetRenderFrameID();

    if (frame_id >= 0) {
      CefRefPtr<CefRequestImpl> cef_request(new CefRequestImpl);
      cef_request->Set(request);
      cef_request->SetReadOnly(true);

      content::ResourceThrottle* throttle =
        new navigation_interception::InterceptNavigationResourceThrottle(
            request,
            base::Bind(&NavigationOnUIThread, frame_id, cef_request));
      throttles->push_back(throttle);
    }
  }
}

bool CefResourceDispatcherHostDelegate::HandleExternalProtocol(
    const GURL& url,
    int child_id,
    int route_id) {
  CefRefPtr<CefBrowserHostImpl> browser =
      CefBrowserHostImpl::GetBrowserForView(child_id, route_id);
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
    if (!response->head.headers.get())
      response->head.headers = new net::HttpResponseHeaders(std::string());

    // Add CORS headers to support XMLHttpRequest redirects.
    response->head.headers->AddHeader("Access-Control-Allow-Origin: " +
        active_url.scheme() + "://" + active_url.host());
    response->head.headers->AddHeader("Access-Control-Allow-Credentials: true");
  }
}
