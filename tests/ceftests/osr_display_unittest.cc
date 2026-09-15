// Copyright (c) 2017 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/base/cef_callback.h"
#include "include/wrapper/cef_closure_task.h"
#include "tests/ceftests/routing_test_handler.h"
#include "tests/ceftests/test_util.h"
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
  DisplayTestHandler() = default;

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
               int64_t query_id,
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
  Status status_ = START;

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

const char kResizeCacheUrlA[] = "https://tests/osr-cache-a.html";
const char kResizeCacheUrlB[] = "https://tests/osr-cache-b.html";

class OsrBackForwardCacheResizeTestHandler : public RoutingTestHandler,
                                             public CefRenderHandler {
 public:
  CefRefPtr<CefRenderHandler> GetRenderHandler() override { return this; }

  void GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) override {
    rect = CefRect(0, 0, width_, height_);
  }

  bool GetScreenInfo(CefRefPtr<CefBrowser> browser,
                     CefScreenInfo& screen_info) override {
    screen_info.device_scale_factor = 1.0f;
    screen_info.rect = CefRect(0, 0, 1920, 1080);
    screen_info.available_rect = screen_info.rect;
    return true;
  }

  void RunTest() override {
    AddResource(kResizeCacheUrlA, Page("a", "#ff0000"), "text/html");
    AddResource(kResizeCacheUrlB, Page("b", "#0000ff"), "text/html");
    CefWindowInfo window_info;
    window_info.SetAsWindowless(kNullWindowHandle);
    CefBrowserHost::CreateBrowser(window_info, this, kResizeCacheUrlA,
                                  CefBrowserSettings(), nullptr, nullptr);
    SetTestTimeout();
  }

  bool OnQuery(CefRefPtr<CefBrowser> browser,
               CefRefPtr<CefFrame> frame,
               int64_t query_id,
               const CefString& request,
               bool persistent,
               CefRefPtr<Callback> callback) override {
    const std::string message = request.ToString();
    callback->Success("");
    if (step_ == kBack) {
      if (message.find("restored:") == 0) {
        restored_ = true;
      } else if (message.find("a:") == 0) {
        ADD_FAILURE() << "Page A was reloaded instead of restored from BFCache";
        step_ = kDone;
        CefPostTask(
            TID_UI,
            base::BindOnce(&OsrBackForwardCacheResizeTestHandler::DestroyTest,
                           this));
        return true;
      }
    }
    last_viewport_ = message;
    const std::string expected = std::string(step_ == kInitial ? "a:"
                                             : step_ == kBack  ? "restored:"
                                                               : "b:") +
                                 std::to_string(width_) + "x" +
                                 std::to_string(height_);
    if (message == expected) {
      got_viewport_ = true;
      AdvanceIfReady();
    }
    return true;
  }

  void OnPaint(CefRefPtr<CefBrowser> browser,
               PaintElementType type,
               const RectList& dirty_rects,
               const void* buffer,
               int width,
               int height) override {
    if (type != PET_VIEW || step_ == kDone) {
      return;
    }
    // The restored page changes to green in pageshow. This excludes queued
    // frames from B and the old red surface of A, even at the expected size.
    const auto* pixels = static_cast<const unsigned char*>(buffer);
    const int channel = step_ == kInitial ? 2 : step_ == kBack ? 1 : 0;
    if (pixels[channel] != 255 || pixels[(channel + 1) % 3] != 0 ||
        pixels[(channel + 2) % 3] != 0) {
      return;
    }
    last_paint_width_ = width;
    last_paint_height_ = height;
    if (width == width_ && height == height_) {
      got_paint_ = true;
      AdvanceIfReady();
    }
  }

  void DestroyTest() override {
    EXPECT_TRUE(restored_) << "No persisted pageshow event";
    EXPECT_EQ(kDone, step_)
        << "Last viewport: " << last_viewport_
        << "; last matching-page paint: " << last_paint_width_ << "x"
        << last_paint_height_;
    EXPECT_TRUE(got_viewport_);
    EXPECT_TRUE(got_paint_);
    RoutingTestHandler::DestroyTest();
  }

 private:
  static std::string Page(const std::string& name, const std::string& color) {
    return "<!doctype html><html style='background:" + color +
           "'><style>@keyframes pulse{to{opacity:0}}"
           "div{position:absolute;left:10px;top:10px;width:10px;height:10px;"
           "background:white;animation:pulse .1s infinite alternate}</style>"
           "<body><div></div><script>let name='" +
           name +
           "';function report(){window.testQuery({request:name+':' +"
           "innerWidth+'x'+innerHeight});}"
           "addEventListener('pageshow',e=>{if(e.persisted){name='restored';"
           "document.documentElement.style.background='#00ff00';}report();});"
           "addEventListener('resize',report);</script></body></html>";
  }

  void AdvanceIfReady() {
    if (got_viewport_ && got_paint_ && !advance_pending_) {
      advance_pending_ = true;
      CefPostTask(
          TID_UI,
          base::BindOnce(&OsrBackForwardCacheResizeTestHandler::Advance, this));
    }
  }

  void Advance() {
    advance_pending_ = false;
    if (step_ == kDone) {
      return;
    }
    if (step_ == kBack) {
      step_ = kDone;
      DestroyTest();
      return;
    }
    got_viewport_ = false;
    got_paint_ = false;
    if (step_ == kInitial) {
      step_ = kNavigate;
      GetBrowser()->GetMainFrame()->LoadURL(kResizeCacheUrlB);
    } else if (step_ == kNavigate) {
      step_ = kResize;
      width_ = 800;
      height_ = 600;
      GetBrowser()->GetHost()->WasResized();
    } else {
      step_ = kBack;
      GetBrowser()->GoBack();
    }
  }

  enum Step { kInitial, kNavigate, kResize, kBack, kDone };
  Step step_ = kInitial;
  int width_ = kOsrWidth;
  int height_ = kOsrHeight;
  bool got_viewport_ = false;
  bool got_paint_ = false;
  bool advance_pending_ = false;
  bool restored_ = false;
  std::string last_viewport_;
  int last_paint_width_ = 0;
  int last_paint_height_ = 0;

  IMPLEMENT_REFCOUNTING(OsrBackForwardCacheResizeTestHandler);
};

}  // namespace

TEST(OSRTest, ResizeWhileInBackForwardCache) {
  if (!IsBFCacheEnabled()) {
    GTEST_SKIP() << "Requires BackForwardCache";
  }
  CefRefPtr<OsrBackForwardCacheResizeTestHandler> handler =
      new OsrBackForwardCacheResizeTestHandler;
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

namespace {

const char kOsrPopupJSOtherClientMainUrl[] =
    "http://www.tests-pjse.com/main.html";

class OsrPopupJSOtherClientTestHandler : public TestHandler,
                                         public CefRenderHandler {
 public:
  explicit OsrPopupJSOtherClientTestHandler(CefRefPtr<CefClient> other) {
    other_ = other;
  }

  CefRefPtr<CefRenderHandler> GetRenderHandler() override { return this; }

  void GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) override {
    rect = CefRect(0, 0, kOsrWidth, kOsrHeight);
  }

  void OnPaint(CefRefPtr<CefBrowser> browser,
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
                     int popup_id,
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
    if (isLoading) {
      return;
    }

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

  CefRefPtr<CefLoadHandler> GetLoadHandler() override { return this; }

  CefRefPtr<CefRenderHandler> GetRenderHandler() override { return this; }

  CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }

  void OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                            bool isLoading,
                            bool canGoBack,
                            bool canGoForward) override {
    if (handler_) {
      handler_->OnLoadingStateChange(browser, isLoading, canGoBack,
                                     canGoForward);
    }
  }

  void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
    handler_->OnAfterCreated(browser);
  }

  void OnBeforeClose(CefRefPtr<CefBrowser> browser) override {
    handler_->OnBeforeClose(browser);
    handler_ = nullptr;
  }

  bool OnBeforePopup(CefRefPtr<CefBrowser> browser,
                     CefRefPtr<CefFrame> frame,
                     int popup_id,
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

  void GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) override {
    rect = CefRect(0, 0, kOsrWidth, kOsrHeight);
  }

  void OnPaint(CefRefPtr<CefBrowser> browser,
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
