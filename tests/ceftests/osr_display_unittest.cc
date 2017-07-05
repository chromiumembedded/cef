// Copyright (c) 2017 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/base/cef_bind.h"
#include "include/wrapper/cef_closure_task.h"

#include "tests/ceftests/routing_test_handler.h"
#include "tests/gtest/include/gtest/gtest.h"

namespace {

const char kTestUrl1[] = "https://tests/DisplayTestHandler.START";
const char kTestUrl2[] = "https://tests/DisplayTestHandler.NAVIGATE";
const char kTestMsg[] = "DisplayTestHandler.Status";

// Default OSR widget size.
const int kOsrWidth = 600;
const int kOsrHeight = 400;

class DisplayTestHandler : public RoutingTestHandler, public CefRenderHandler {
 public:
  DisplayTestHandler() : status_(START) {}

  CefRefPtr<CefRenderHandler> GetRenderHandler() override { return this; }

  bool GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) override {
    rect = CefRect(0, 0, kOsrWidth, kOsrHeight);
    return true;
  }

  bool GetScreenInfo(CefRefPtr<CefBrowser> browser,
                     CefScreenInfo& screen_info) override {
    screen_info.rect = CefRect(0, 0, kOsrWidth, kOsrHeight);
    screen_info.available_rect = screen_info.rect;
    return true;
  }

  void OnPaint(CefRefPtr<CefBrowser> browser,
               CefRenderHandler::PaintElementType type,
               const CefRenderHandler::RectList& dirtyRects,
               const void* buffer,
               int width,
               int height) override {
    if (!got_paint_[status_]) {
      got_paint_[status_].yes();

      if (status_ == START)
        OnStartIfDone();
      else if (status_ == SHOW)
        CefPostTask(TID_UI, base::Bind(&DisplayTestHandler::DestroyTest, this));
      else
        EXPECT_FALSE(true);  // Not reached.
    }
  }

  void RunTest() override {
    // Add the resources that we will navigate to/from.
    AddResource(kTestUrl1, GetPageContents("Page1", "START"), "text/html");
    AddResource(kTestUrl2, GetPageContents("Page2", "NAVIGATE"), "text/html");

    // Create the browser.
    CreateOSRBrowser(kTestUrl1);

    // Time out the test after a reasonable period of time.
    SetTestTimeout(5000);
  }

  bool OnQuery(CefRefPtr<CefBrowser> browser,
               CefRefPtr<CefFrame> frame,
               int64 query_id,
               const CefString& request,
               bool persistent,
               CefRefPtr<Callback> callback) override {
    const std::string& request_str = request.ToString();
    if (request_str.find(kTestMsg) == 0) {
      const std::string& status = request_str.substr(sizeof(kTestMsg));
      if (status == "START") {
        got_start_msg_.yes();
        OnStartIfDone();
      } else if (status == "NAVIGATE") {
        got_navigate_msg_.yes();
        // Wait a bit to verify no OnPaint callback.
        CefPostDelayedTask(
            TID_UI, base::Bind(&DisplayTestHandler::OnNavigate, this), 250);
      }
    }
    callback->Success("");
    return true;
  }

  void DestroyTest() override {
    EXPECT_TRUE(got_paint_[START]);
    EXPECT_FALSE(got_paint_[NAVIGATE]);
    EXPECT_TRUE(got_paint_[SHOW]);

    EXPECT_TRUE(got_start_msg_);
    EXPECT_TRUE(got_navigate_msg_);

    EXPECT_EQ(status_, SHOW);

    RoutingTestHandler::DestroyTest();
  }

 private:
  void CreateOSRBrowser(const CefString& url) {
    CefWindowInfo windowInfo;
    CefBrowserSettings settings;

#if defined(OS_WIN)
    windowInfo.SetAsWindowless(GetDesktopWindow());
#else
    windowInfo.SetAsWindowless(kNullWindowHandle);
#endif

    CefBrowserHost::CreateBrowser(windowInfo, this, url, settings, NULL);
  }

  std::string GetPageContents(const std::string& name,
                              const std::string& status) {
    return "<html><body>Page1<script>window.testQuery({request:'" +
           std::string(kTestMsg) + ":" + status + "'});</script></body></html>";
  }

  void OnStartIfDone() {
    if (got_start_msg_ && got_paint_[START])
      CefPostTask(TID_UI, base::Bind(&DisplayTestHandler::OnStart, this));
  }

  void OnStart() {
    EXPECT_EQ(status_, START);

    // Hide the browser. OnPaint should not be called again until
    // WasHidden(false) is explicitly called.
    GetBrowser()->GetHost()->WasHidden(true);
    status_ = NAVIGATE;

    GetBrowser()->GetMainFrame()->LoadURL(kTestUrl2);
  }

  void OnNavigate() {
    EXPECT_EQ(status_, NAVIGATE);

    // Show the browser.
    status_ = SHOW;
    GetBrowser()->GetHost()->WasHidden(false);

    // Force a call to OnPaint.
    GetBrowser()->GetHost()->Invalidate(PET_VIEW);
  }

  enum Status {
    START,
    NAVIGATE,
    SHOW,
    STATUS_COUNT,
  };
  Status status_;

  TrackCallback got_paint_[STATUS_COUNT];
  TrackCallback got_start_msg_;
  TrackCallback got_navigate_msg_;

  IMPLEMENT_REFCOUNTING(DisplayTestHandler);
};

}  // namespace

// Test that browser visibility is not changed due to navigation.
TEST(OSRTest, NavigateWhileHidden) {
  CefRefPtr<DisplayTestHandler> handler = new DisplayTestHandler();
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}
