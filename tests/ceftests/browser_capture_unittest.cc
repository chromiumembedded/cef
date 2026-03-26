// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/cef_browser_capture.h"
#include "include/cef_waitable_event.h"
#include "tests/ceftests/test_handler.h"
#include "tests/gtest/include/gtest/gtest.h"

namespace {

const char kCaptureTestUrl[] = "https://tests/capture.html";

class BrowserCaptureTestHandler;

class SnapshotVisitor : public CefStringVisitor {
 public:
  explicit SnapshotVisitor(BrowserCaptureTestHandler* test_handler)
      : test_handler_(test_handler) {}

  void Visit(const CefString& string) override;

 private:
  BrowserCaptureTestHandler* const test_handler_;

  IMPLEMENT_REFCOUNTING(SnapshotVisitor);
};

class ScreenshotCallback : public CefScreenshotCallback {
 public:
  explicit ScreenshotCallback(BrowserCaptureTestHandler* test_handler)
      : test_handler_(test_handler) {}

  void OnScreenshotCaptured(const CefString& path,
                            const CefString& error) override;

 private:
  BrowserCaptureTestHandler* const test_handler_;

  IMPLEMENT_REFCOUNTING(ScreenshotCallback);
};

class BrowserCaptureTestHandler : public TestHandler {
 public:
  BrowserCaptureTestHandler() = default;

  void RunTest() override {
    AddResource(kCaptureTestUrl, "<html><body>Capture</body></html>",
                "text/html");
    CreateBrowser(kCaptureTestUrl);
    SetTestTimeout();
  }

  void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int httpStatusCode) override {
    if (!frame->IsMain() || got_snapshot_) {
      return;
    }

    CefRefPtr<CefBrowserCapture> capture = browser->GetHost()->GetCapture();
    EXPECT_TRUE(capture);
    EXPECT_TRUE(browser->GetHost()->GetCapture().get());

    CefSnapshotSettings settings;
    settings.interactive_only = 1;
    settings.compact = 1;
    settings.max_depth = 5;
    capture->Snapshot(settings, new SnapshotVisitor(this));
  }

  void OnSnapshot(const CefString& text) {
    EXPECT_UI_THREAD();
    EXPECT_FALSE(got_snapshot_);
    EXPECT_STREQ("Browser capture snapshot scaffolding is not implemented.",
                 text.ToString().c_str());
    got_snapshot_.yes();

    CefAnnotatedScreenshotSettings settings;
    settings.annotate = 1;
    settings.quality = 80;
    CefRefPtr<CefBrowserCapture> capture = GetBrowser()->GetHost()->GetCapture();
    capture->CaptureAnnotatedScreenshot("capture.png", settings,
                                        new ScreenshotCallback(this));
  }

  void OnScreenshot(const CefString& path, const CefString& error) {
    EXPECT_UI_THREAD();
    EXPECT_FALSE(got_screenshot_);
    EXPECT_STREQ("capture.png", path.ToString().c_str());
    EXPECT_STREQ(
        "Browser capture screenshot scaffolding is not implemented.",
        error.ToString().c_str());
    got_screenshot_.yes();
    DestroyTest();
  }

 private:
  void DestroyTest() override {
    EXPECT_TRUE(got_snapshot_);
    EXPECT_TRUE(got_screenshot_);
    TestHandler::DestroyTest();
  }

  TrackCallback got_snapshot_;
  TrackCallback got_screenshot_;

  IMPLEMENT_REFCOUNTING(BrowserCaptureTestHandler);
};

void SnapshotVisitor::Visit(const CefString& string) {
  test_handler_->OnSnapshot(string);
}

void ScreenshotCallback::OnScreenshotCaptured(const CefString& path,
                                             const CefString& error) {
  test_handler_->OnScreenshot(path, error);
}

}  // namespace

TEST(BrowserCaptureTest, ScaffoldCallbacks) {
  CefRefPtr<BrowserCaptureTestHandler> handler = new BrowserCaptureTestHandler;
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}
