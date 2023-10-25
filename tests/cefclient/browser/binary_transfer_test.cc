// Copyright (c) 2023 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/cefclient/browser/binary_transfer_test.h"

#include <algorithm>
#include <string>

namespace {

const char kTestUrlPath[] = "/binary_transfer";

// Handle messages in the browser process.
class Handler final : public CefMessageRouterBrowserSide::Handler {
 public:
  // Called due to cefQuery execution in binary_transfer.html.
  bool OnQuery(CefRefPtr<CefBrowser> browser,
               CefRefPtr<CefFrame> frame,
               int64_t query_id,
               const CefString& request,
               bool persistent,
               CefRefPtr<Callback> callback) override {
    // Only handle messages from the test URL.
    const std::string& url = frame->GetURL();
    if (!client::test_runner::IsTestURL(url, kTestUrlPath)) {
      return false;
    }

    callback->Success(request);
    return true;
  }

  // Called due to cefQuery execution in binary_transfer.html.
  bool OnQuery(CefRefPtr<CefBrowser> browser,
               CefRefPtr<CefFrame> frame,
               int64_t query_id,
               CefRefPtr<const CefBinaryBuffer> request,
               bool persistent,
               CefRefPtr<Callback> callback) override {
    // Only handle messages from the test URL.
    const std::string& url = frame->GetURL();
    if (!client::test_runner::IsTestURL(url, kTestUrlPath)) {
      return false;
    }

    callback->Success(request->GetData(), request->GetSize());
    return true;
  }
};

}  // namespace

namespace client::binary_transfer_test {

void CreateMessageHandlers(test_runner::MessageHandlerSet& handlers) {
  handlers.insert(new Handler());
}

}  // namespace client::binary_transfer_test
