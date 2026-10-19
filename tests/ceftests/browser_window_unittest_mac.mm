// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "include/wrapper/cef_closure_task.h"
#include "tests/ceftests/test_handler.h"
#include "tests/ceftests/test_util.h"

namespace {

class RetainedHostViewTestHandler : public TestHandler {
 public:
  void RunTest() override {
    SetTestTimeout(10000);
    SetUseViews(false);
    SetUseAlloyStyle(true, true);
    CreateBrowser("about:blank");
  }

  void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
    TestHandler::OnAfterCreated(browser);
    // AppKit or an embedder may retain the host view beyond window closure.
    // This must not delay OnBeforeClose until that reference is released.
    host_view_ =
        CAST_CEF_WINDOW_HANDLE_TO_NSVIEW(browser->GetHost()->GetWindowHandle());
    EXPECT_NE(nil, host_view_);
    CefPostTask(TID_UI, base::BindOnce(
                            &RetainedHostViewTestHandler::DestroyTest, this));
  }

  void OnBeforeClose(CefRefPtr<CefBrowser> browser) override {
    EXPECT_NE(nil, host_view_);
    // Releasing the view after browser destruction must not notify it again.
    host_view_ = nil;
    TestHandler::OnBeforeClose(browser);
  }

  void OnTestTimeout(int timeout_ms, bool treat_as_error) override {
    TestHandler::OnTestTimeout(timeout_ms, treat_as_error);
    // Also permit teardown with the old implementation, which waits for the
    // host view to deallocate before delivering OnBeforeClose.
    host_view_ = nil;
  }

 private:
  NSView* __strong host_view_ = nil;

  IMPLEMENT_REFCOUNTING(RetainedHostViewTestHandler);
};

}  // namespace

TEST(BrowserWindowTest, CloseWithRetainedHostView) {
  CefRefPtr<RetainedHostViewTestHandler> handler =
      new RetainedHostViewTestHandler;
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}
