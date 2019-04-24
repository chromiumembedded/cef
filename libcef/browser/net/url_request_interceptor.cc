// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/browser/net/url_request_interceptor.h"

#include <string>

#include "libcef/browser/browser_host_impl.h"
#include "libcef/browser/net/net_util.h"
#include "libcef/browser/net/resource_request_job.h"
#include "libcef/browser/thread_util.h"
#include "libcef/common/net/http_header_utils.h"
#include "libcef/common/request_impl.h"
#include "libcef/common/response_impl.h"

#include "net/base/upload_data_stream.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request_job_manager.h"
#include "net/url_request/url_request_redirect_job.h"

CefRequestInterceptor::CefRequestInterceptor() {
  CEF_REQUIRE_IOT();
}

CefRequestInterceptor::~CefRequestInterceptor() {
  CEF_REQUIRE_IOT();
}

net::URLRequestJob* CefRequestInterceptor::MaybeInterceptRequest(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate) const {
  if (net_util::IsInternalRequest(request))
    return nullptr;

  CefRefPtr<CefRequestImpl> requestPtr;
  CefRefPtr<CefBrowser> browser;
  CefRefPtr<CefFrame> frame;
  CefRefPtr<CefResourceRequestHandler> handler =
      net_util::GetResourceRequestHandler(request, requestPtr, browser, frame);
  if (handler) {
    // Give the client an opportunity to replace the request.
    CefRefPtr<CefResourceHandler> resourceHandler =
        handler->GetResourceHandler(browser, frame, requestPtr.get());
    if (resourceHandler) {
      return new CefResourceRequestJob(request, network_delegate,
                                       resourceHandler);
    }
  }

  return nullptr;
}

net::URLRequestJob* CefRequestInterceptor::MaybeInterceptRedirect(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate,
    const GURL& location) const {
  if (net_util::IsInternalRequest(request))
    return nullptr;

  CefRefPtr<CefRequestImpl> requestPtr;
  CefRefPtr<CefBrowser> browser;
  CefRefPtr<CefFrame> frame;
  CefRefPtr<CefResourceRequestHandler> handler =
      net_util::GetResourceRequestHandler(request, requestPtr, browser, frame);
  if (handler) {
    CefRefPtr<CefResponseImpl> responsePtr = new CefResponseImpl();
    responsePtr->Set(request);
    responsePtr->SetReadOnly(true);

    // Give the client an opportunity to redirect the request.
    CefString newUrlStr = location.spec();
    handler->OnResourceRedirect(browser, frame, requestPtr.get(),
                                responsePtr.get(), newUrlStr);
    if (newUrlStr != location.spec()) {
      const GURL new_url = GURL(newUrlStr.ToString());
      if (!new_url.is_empty() && new_url.is_valid()) {
        return new net::URLRequestRedirectJob(
            request, network_delegate, new_url,
            net::URLRequestRedirectJob::REDIRECT_307_TEMPORARY_REDIRECT,
            "Resource Redirect");
      }
    }
  }

  return nullptr;
}

net::URLRequestJob* CefRequestInterceptor::MaybeInterceptResponse(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate) const {
  if (net_util::IsInternalRequest(request))
    return nullptr;

  CefRefPtr<CefRequestImpl> requestPtr;
  CefRefPtr<CefBrowser> browser;
  CefRefPtr<CefFrame> frame;
  CefRefPtr<CefResourceRequestHandler> handler =
      net_util::GetResourceRequestHandler(request, requestPtr, browser, frame);
  if (!handler)
    return nullptr;

  // The below callback allows modification of the request object.
  requestPtr->SetReadOnly(false);
  requestPtr->SetTrackChanges(true);

  CefRefPtr<CefResponseImpl> responsePtr = new CefResponseImpl();
  responsePtr->Set(request);
  responsePtr->SetReadOnly(true);

  const GURL old_url = request->url();

  // Give the client an opportunity to retry or redirect the request.
  if (!handler->OnResourceResponse(browser, frame, requestPtr.get(),
                                   responsePtr.get())) {
    return nullptr;
  }

  // This flag will be reset by URLRequest::RestartWithJob() calling
  // URLRequest::PrepareToRestart() after this method returns but we need it
  // reset sooner so that we can modify the request headers without asserting.
  request->set_is_pending(false);

  // Update the URLRequest with only the values that have been changed by the
  // client.
  requestPtr->Get(request, true);

  // If the URL was changed then redirect the request.
  if (!!(requestPtr->GetChanges() & CefRequestImpl::kChangedUrl)) {
    const GURL new_url = old_url.Resolve(requestPtr->GetURL().ToString());
    if (new_url != old_url) {
      return new net::URLRequestRedirectJob(
          request, network_delegate, new_url,
          net::URLRequestRedirectJob::REDIRECT_307_TEMPORARY_REDIRECT,
          "Resource Redirect");
    }
  }

  // Otherwise queue a new job.
  return net::URLRequestJobManager::GetInstance()->CreateJob(request,
                                                             network_delegate);
}
