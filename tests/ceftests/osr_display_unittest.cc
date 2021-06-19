// Copyright (c) 2017 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/base/cef_callback.h"
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

  void GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) override {
    rect = CefRect(0, 0, kOsrWidth, kOsrHeight);
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

      if (status_ == START) {
        OnStartIfDone();
      } else if (status_ == SHOW) {
        CefPostTask(TID_UI,
                    base::BindOnce(&DisplayTestHandler::DestroyTest, this));
      } else {
        ADD_FAILURE();
      }
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
            TID_UI, base::BindOnce(&DisplayTestHandler::OnNavigate, this), 250);
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

    CefBrowserHost::CreateBrowser(windowInfo, this, url, settings, nullptr,
                                  nullptr);
  }

  std::string GetPageContents(const std::string& name,
                              const std::string& status) {
    return "<html><body>" + name + "<script>window.testQuery({request:'" +
           std::string(kTestMsg) + ":" + status + "'});</script></body></html>";
  }

  void OnStartIfDone() {
    if (got_start_msg_ && got_paint_[START]) {
      CefPostTask(TID_UI, base::BindOnce(&DisplayTestHandler::OnStart, this));
    }
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

namespace {

const char kOsrPopupJSOtherClientMainUrl[] =
    "http://www.tests-pjse.com/main.html";

class OsrPopupJSOtherClientTestHandler : public TestHandler,
                                         public CefRenderHandler {
 public:
  OsrPopupJSOtherClientTestHandler(CefRefPtr<CefClient> other) {
    other_ = other;
  }

  virtual CefRefPtr<CefRenderHandler> GetRenderHandler() override {
    return this;
  }

  virtual void GetViewRect(CefRefPtr<CefBrowser> browser,
                           CefRect& rect) override {
    rect = CefRect(0, 0, kOsrWidth, kOsrHeight);
  }

  virtual void OnPaint(CefRefPtr<CefBrowser> browser,
                       PaintElementType type,
                       const RectList& dirtyRects,
                       const void* buffer,
                       int width,
                       int height) override {}

  void RunTest() override {
    AddResource(kOsrPopupJSOtherClientMainUrl, "<html>Main</html>",
                "text/html");

    // Create the browser.
    CreateOSRBrowser(kOsrPopupJSOtherClientMainUrl);

    // Time out the test after a reasonable period of time.
    SetTestTimeout();
  }

  bool OnBeforePopup(CefRefPtr<CefBrowser> browser,
                     CefRefPtr<CefFrame> frame,
                     const CefString& target_url,
                     const CefString& target_frame_name,
                     cef_window_open_disposition_t target_disposition,
                     bool user_gesture,
                     const CefPopupFeatures& popupFeatures,
                     CefWindowInfo& windowInfo,
                     CefRefPtr<CefClient>& client,
                     CefBrowserSettings& settings,
                     CefRefPtr<CefDictionaryValue>& extra_info,
                     bool* no_javascript_access) override {
#if defined(OS_WIN)
    windowInfo.SetAsWindowless(GetDesktopWindow());
#else
    windowInfo.SetAsWindowless(kNullWindowHandle);
#endif

    client = other_;

    got_before_popup_.yes();
    return false;
  }

  void Close(CefRefPtr<CefBrowser> browser) {
    browser->StopLoad();
    CloseBrowser(browser, true);
  }

  void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
    TestHandler::OnAfterCreated(browser);
    if (browser->IsPopup()) {
      got_after_created_popup_.yes();
    }
  }

  void OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                            bool isLoading,
                            bool canGoBack,
                            bool canGoForward) override {
    if (isLoading)
      return;

    if (browser->IsPopup()) {
      got_load_end_popup_.yes();
      CefPostDelayedTask(
          TID_UI,
          base::BindOnce(&OsrPopupJSOtherClientTestHandler::Close, this,
                         browser),
          100);
    } else {
      browser->GetMainFrame()->LoadURL("javascript:window.open('about:blank')");
    }
  }

  void OnBeforeClose(CefRefPtr<CefBrowser> browser) override {
    TestHandler::OnBeforeClose(browser);
    other_ = nullptr;
    if (browser->IsPopup()) {
      got_before_close_popup_.yes();
      DestroyTest();
    }
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

    CefBrowserHost::CreateBrowser(windowInfo, this, url, settings, nullptr,
                                  nullptr);
  }

  void DestroyTest() override {
    EXPECT_TRUE(got_after_created_popup_);
    EXPECT_TRUE(got_load_end_popup_);
    EXPECT_TRUE(got_before_close_popup_);
    EXPECT_TRUE(got_before_popup_);
    TestHandler::DestroyTest();
  }

  TrackCallback got_before_popup_;
  TrackCallback got_after_created_popup_;
  TrackCallback got_load_end_popup_;
  TrackCallback got_before_close_popup_;
  CefRefPtr<CefClient> other_;

  IMPLEMENT_REFCOUNTING(OsrPopupJSOtherClientTestHandler);
};

class OsrPopupJSOtherCefClient : public CefClient,
                                 public CefLoadHandler,
                                 public CefLifeSpanHandler,
                                 public CefRenderHandler {
 public:
  OsrPopupJSOtherCefClient() { handler_ = nullptr; }

  void SetHandler(CefRefPtr<OsrPopupJSOtherClientTestHandler> handler) {
    handler_ = handler;
  }

  virtual CefRefPtr<CefLoadHandler> GetLoadHandler() override { return this; }

  virtual CefRefPtr<CefRenderHandler> GetRenderHandler() override {
    return this;
  }

  virtual CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override {
    return this;
  }

  virtual void OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                                    bool isLoading,
                                    bool canGoBack,
                                    bool canGoForward) override {
    if (handler_) {
      handler_->OnLoadingStateChange(browser, isLoading, canGoBack,
                                     canGoForward);
    }
  }

  virtual void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
    handler_->OnAfterCreated(browser);
  }

  virtual void OnBeforeClose(CefRefPtr<CefBrowser> browser) override {
    handler_->OnBeforeClose(browser);
    handler_ = nullptr;
  }

  virtual bool OnBeforePopup(CefRefPtr<CefBrowser> browser,
                             CefRefPtr<CefFrame> frame,
                             const CefString& target_url,
                             const CefString& target_frame_name,
                             cef_window_open_disposition_t target_disposition,
                             bool user_gesture,
                             const CefPopupFeatures& popupFeatures,
                             CefWindowInfo& windowInfo,
                             CefRefPtr<CefClient>& client,
                             CefBrowserSettings& settings,
                             CefRefPtr<CefDictionaryValue>& extra_info,
                             bool* no_javascript_access) override {
    return true;
  }

  virtual void GetViewRect(CefRefPtr<CefBrowser> browser,
                           CefRect& rect) override {
    rect = CefRect(0, 0, kOsrWidth, kOsrHeight);
  }

  virtual void OnPaint(CefRefPtr<CefBrowser> browser,
                       PaintElementType type,
                       const RectList& dirtyRects,
                       const void* buffer,
                       int width,
                       int height) override {}

 private:
  CefRefPtr<OsrPopupJSOtherClientTestHandler> handler_;

  IMPLEMENT_REFCOUNTING(OsrPopupJSOtherCefClient);
};

}  // namespace

// Test creation of an OSR-popup with another client.
TEST(OSRTest, OsrPopupJSOtherClient) {
  CefRefPtr<OsrPopupJSOtherCefClient> client = new OsrPopupJSOtherCefClient();
  CefRefPtr<OsrPopupJSOtherClientTestHandler> handler =
      new OsrPopupJSOtherClientTestHandler(client);
  client->SetHandler(handler);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}
