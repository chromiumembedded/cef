// Copyright (c) 2025 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/base/cef_callback.h"
#include "include/cef_parser.h"
#include "include/wrapper/cef_closure_task.h"
#include "tests/ceftests/test_handler.h"
#include "tests/ceftests/test_util.h"
#include "tests/gtest/include/gtest/gtest.h"

namespace {

const char kTestUrl[] = "https://tests/BrowserSettingsTest.html";

// Test that JavaScript can be disabled via CefBrowserSettings.
class BrowserSettingsJavaScriptDisabledTestHandler : public TestHandler {
 public:
  BrowserSettingsJavaScriptDisabledTestHandler() = default;

  void RunTest() override {
    // Create a simple HTML page with JavaScript that sets the page title.
    const std::string html =
        "<html><head>"
        "<script>"
        "document.title = 'JavaScript Executed';"
        "</script>"
        "</head>"
        "<body><h1>Testing</h1>"
        "</body></html>";

    AddResource(kTestUrl, html, "text/html");

    // Create browser with JavaScript disabled
    CefBrowserSettings settings;
    settings.javascript = STATE_DISABLED;
    CreateBrowserWithSettings(kTestUrl, settings);

    // Time out the test after a reasonable period
    SetTestTimeout();
  }

  void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int httpStatusCode) override {
    if (!frame->IsMain()) {
      return;
    }

    got_load_end_.yes();

    // Verify that the page loaded
    EXPECT_EQ(kTestUrl, frame->GetURL().ToString());

    // Wait to see if JavaScript sets the title
    CefPostDelayedTask(
        TID_UI,
        base::BindOnce(
            &BrowserSettingsJavaScriptDisabledTestHandler::VerifyNoTitleChange,
            this, browser),
        100);
  }

  void OnTitleChange(CefRefPtr<CefBrowser> browser,
                     const CefString& title) override {
    if (!got_title_change1_) {
      got_title_change1_.yes();
    } else if (!got_title_change2_) {
      got_title_change2_.yes();
    }

    // This should not be called with the updated title.
    if (title == "JavaScript Executed") {
      got_title_change_javascript_.yes();
    }
  }

  void VerifyNoTitleChange(CefRefPtr<CefBrowser> browser) {
    got_verify_.yes();

    EXPECT_FALSE(got_title_change_javascript_)
        << "OnTitleChange should not have been called with JavaScript disabled";

    DestroyTest();
  }

  void DestroyTest() override {
    EXPECT_TRUE(got_load_end_);
    EXPECT_TRUE(got_verify_);
    EXPECT_TRUE(got_title_change1_);
    // Sanity check we didn't get a second title change.
    EXPECT_FALSE(got_title_change2_);
    EXPECT_FALSE(got_title_change_javascript_);
    TestHandler::DestroyTest();
  }

 private:
  TrackCallback got_load_end_;
  TrackCallback got_verify_;
  TrackCallback got_title_change1_;
  TrackCallback got_title_change2_;
  TrackCallback got_title_change_javascript_;

  IMPLEMENT_REFCOUNTING(BrowserSettingsJavaScriptDisabledTestHandler);
};

}  // namespace

// Test that JavaScript can be disabled
TEST(BrowserSettingsTest, JavaScriptDisabled) {
  CefRefPtr<BrowserSettingsJavaScriptDisabledTestHandler> handler =
      new BrowserSettingsJavaScriptDisabledTestHandler();
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}
