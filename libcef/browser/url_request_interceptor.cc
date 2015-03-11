// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/browser/url_request_interceptor.h"

#include <string>

#include "libcef/browser/browser_host_impl.h"
#include "libcef/browser/resource_request_job.h"
#include "libcef/browser/thread_util.h"
#include "libcef/common/http_header_utils.h"
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

        // Give the client an opportunity to redirect the request.
        CefString newUrlStr = location.spec();
        handler->OnResourceRedirect(browser.get(), frame, cefRequest,
                                    newUrlStr);
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

  CefRefPtr<CefRequest> cefRequest = new CefRequestImpl();
  static_cast<CefRequestImpl*>(cefRequest.get())->Set(request);

  CefRefPtr<CefResponse> cefResponse = new CefResponseImpl();
  static_cast<CefResponseImpl*>(cefResponse.get())->Set(request);
  static_cast<CefResponseImpl*>(cefResponse.get())->SetReadOnly(true);

  // Give the client an opportunity to retry or redirect the request.
  if (!handler->OnResourceResponse(browser.get(), frame, cefRequest,
                                   cefResponse)) {
    return NULL;
  }

  // This flag will be reset by URLRequest::RestartWithJob() calling
  // URLRequest::PrepareToRestart() after this method returns but we need it
  // reset sooner so that we can modify the request headers without asserting.
  request->set_is_pending(false);

  // Update the request headers to match the CefRequest.
  CefRequest::HeaderMap cefHeaders;
  cefRequest->GetHeaderMap(cefHeaders);

  CefString referrerStr;
  referrerStr.FromASCII(net::HttpRequestHeaders::kReferer);
  CefRequest::HeaderMap::iterator it = cefHeaders.find(referrerStr);
  if (it != cefHeaders.end()) {
    request->SetReferrer(it->second);
    cefHeaders.erase(it);
  }

  net::HttpRequestHeaders netHeaders;
  netHeaders.AddHeadersFromString(HttpHeaderUtils::GenerateHeaders(cefHeaders));
  request->SetExtraRequestHeaders(netHeaders);

  // Update the request body to match the CefRequest.
  CefRefPtr<CefPostData> post_data = cefRequest->GetPostData();
  if (post_data.get()) {
    request->set_upload(
        make_scoped_ptr(static_cast<CefPostDataImpl*>(post_data.get())->Get()));
  } else if (request->get_upload()) {
    request->set_upload(scoped_ptr<net::UploadDataStream>());
  }

  // If the URL was modified redirect the request.
  const GURL url(cefRequest->GetURL().ToString());
  if (url != request->url()) {
    return new net::URLRequestRedirectJob(
        request, network_delegate, url,
        net::URLRequestRedirectJob::REDIRECT_307_TEMPORARY_REDIRECT,
        "Resource Redirect");
  }

  // Otherwise queue a new job.
  return net::URLRequestJobManager::GetInstance()->CreateJob(
      request, network_delegate);
}
