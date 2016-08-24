// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/browser/net/url_request_interceptor.h"

#include <string>

#include "libcef/browser/browser_host_impl.h"
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
  CefRefPtr<CefBrowserHostImpl> browser =
      CefBrowserHostImpl::GetBrowserForRequest(request);
  if (browser.get()) {
    CefRefPtr<CefClient> client = browser->GetClient();
    if (client.get()) {
      CefRefPtr<CefRequestHandler> handler = client->GetRequestHandler();
      if (handler.get()) {
        CefRefPtr<CefFrame> frame = browser->GetFrameForRequest(request);

        // Populate the request data.
        CefRefPtr<CefRequest> req(CefRequest::Create());
        static_cast<CefRequestImpl*>(req.get())->Set(request);

        // Give the client an opportunity to replace the request.
        CefRefPtr<CefResourceHandler> resourceHandler =
            handler->GetResourceHandler(browser.get(), frame, req);
        if (resourceHandler.get()) {
          return new CefResourceRequestJob(request, network_delegate,
                                           resourceHandler);
        }
      }
    }
  }

  return NULL;
}

net::URLRequestJob* CefRequestInterceptor::MaybeInterceptRedirect(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate,
    const GURL& location) const {
  CefRefPtr<CefBrowserHostImpl> browser =
      CefBrowserHostImpl::GetBrowserForRequest(request);
  if (browser.get()) {
    CefRefPtr<CefClient> client = browser->GetClient();
    if (client.get()) {
      CefRefPtr<CefRequestHandler> handler = client->GetRequestHandler();
      if (handler.get()) {
        CefRefPtr<CefFrame> frame = browser->GetFrameForRequest(request);

        CefRefPtr<CefRequest> cefRequest = new CefRequestImpl();
        static_cast<CefRequestImpl*>(cefRequest.get())->Set(request);
        static_cast<CefRequestImpl*>(cefRequest.get())->SetReadOnly(true);

        CefRefPtr<CefResponse> cefResponse = new CefResponseImpl();
        static_cast<CefResponseImpl*>(cefResponse.get())->Set(request);
        static_cast<CefResponseImpl*>(cefResponse.get())->SetReadOnly(true);

        // Give the client an opportunity to redirect the request.
        CefString newUrlStr = location.spec();
        handler->OnResourceRedirect(browser.get(), frame, cefRequest,
                                    cefResponse, newUrlStr);
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
    }
  }

  return NULL;
}

net::URLRequestJob* CefRequestInterceptor::MaybeInterceptResponse(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate) const {
  CefRefPtr<CefBrowserHostImpl> browser =
      CefBrowserHostImpl::GetBrowserForRequest(request);
  if (!browser.get())
    return NULL;

  CefRefPtr<CefClient> client = browser->GetClient();
  if (!client.get())
    return NULL;

  CefRefPtr<CefRequestHandler> handler = client->GetRequestHandler();
  if (!handler.get())
    return NULL;

  CefRefPtr<CefFrame> frame = browser->GetFrameForRequest(request);

  CefRefPtr<CefRequestImpl> cefRequest = new CefRequestImpl();
  cefRequest->Set(request);
  cefRequest->SetTrackChanges(true);

  CefRefPtr<CefResponseImpl> cefResponse = new CefResponseImpl();
  cefResponse->Set(request);
  cefResponse->SetReadOnly(true);

  // Give the client an opportunity to retry or redirect the request.
  if (!handler->OnResourceResponse(browser.get(), frame, cefRequest.get(),
                                   cefResponse.get())) {
    return NULL;
  }

  // This flag will be reset by URLRequest::RestartWithJob() calling
  // URLRequest::PrepareToRestart() after this method returns but we need it
  // reset sooner so that we can modify the request headers without asserting.
  request->set_is_pending(false);

  // Update the URLRequest with only the values that have been changed by the
  // client.
  cefRequest->Get(request, true);

  // If the URL was changed then redirect the request.
  if (!!(cefRequest->GetChanges() & CefRequestImpl::kChangedUrl)) {
    const GURL url(cefRequest->GetURL().ToString());
    DCHECK_NE(url, request->url());
    return new net::URLRequestRedirectJob(
        request, network_delegate, url,
        net::URLRequestRedirectJob::REDIRECT_307_TEMPORARY_REDIRECT,
        "Resource Redirect");
  }

  // Otherwise queue a new job.
  return net::URLRequestJobManager::GetInstance()->CreateJob(
      request, network_delegate);
}
