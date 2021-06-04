// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_NET_SERVICE_BROWSER_URLREQUEST_IMPL_H_
#define CEF_LIBCEF_BROWSER_NET_SERVICE_BROWSER_URLREQUEST_IMPL_H_

#include <memory>

#include "include/cef_urlrequest.h"

#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {
struct GlobalRequestID;
}

class CefBrowserURLRequest : public CefURLRequest {
 public:
  class Context;

  // TODO(network): After the old network code path is deleted move the
  // CefURLRequestClient::GetAuthCredentials callback to the context thread and
  // return just the CefBrowserURLRequest object here. The *Client object can
  // then be retrieved by calling GetClient() from the required thread.
  using RequestInfo = std::pair<CefRefPtr<CefBrowserURLRequest>,
                                CefRefPtr<CefURLRequestClient>>;

  // Retrieve the request objects, if any, associated with |request_id|.
  static absl::optional<RequestInfo> FromRequestID(int32_t request_id);
  static absl::optional<RequestInfo> FromRequestID(
      const content::GlobalRequestID& request_id);

  // If |frame| is nullptr requests can still be intercepted but no
  // browser/frame will be associated with them.
  CefBrowserURLRequest(CefRefPtr<CefFrame> frame,
                       CefRefPtr<CefRequest> request,
                       CefRefPtr<CefURLRequestClient> client,
                       CefRefPtr<CefRequestContext> request_context);
  ~CefBrowserURLRequest() override;

  bool Start();

  // CefURLRequest methods.
  CefRefPtr<CefRequest> GetRequest() override;
  CefRefPtr<CefURLRequestClient> GetClient() override;
  Status GetRequestStatus() override;
  ErrorCode GetRequestError() override;
  CefRefPtr<CefResponse> GetResponse() override;
  bool ResponseWasCached() override;
  void Cancel() override;

 private:
  bool VerifyContext();

  std::unique_ptr<Context> context_;

  IMPLEMENT_REFCOUNTING(CefBrowserURLRequest);
};

#endif  // CEF_LIBCEF_BROWSER_NET_SERVICE_BROWSER_URLREQUEST_IMPL_H_
