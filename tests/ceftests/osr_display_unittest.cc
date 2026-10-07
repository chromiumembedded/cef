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

const char kFocusCacheUrlA[] = "https://tests/osr-focus-a.html";
const char kFocusCacheUrlB[] = "https://tests/osr-focus-b.html";
const char kFocusCacheFrameUrl[] = "https://other-tests/osr-focus-select.html";
const char kFocusLinkUrlB[] = "https://navigation-tests/osr-focus-b.html";

class OsrHistoryPopupTestHandler : public RoutingTestHandler,
                                   public CefRenderHandler,
                                   public CefFocusHandler {
 public:
  explicit OsrHistoryPopupTestHandler(bool cancel_focus = false,
                                      bool use_iframe = false,
                                      bool use_link = false)
      : cancel_focus_(cancel_focus),
        use_iframe_(use_iframe),
        use_link_(use_link) {}

  CefRefPtr<CefRenderHandler> GetRenderHandler() override { return this; }
  CefRefPtr<CefFocusHandler> GetFocusHandler() override { return this; }

  bool OnSetFocus(CefRefPtr<CefBrowser> browser, FocusSource source) override {
    if (checking_focus_) {
      got_focus_request_ = true;
      if (source == FOCUS_SOURCE_SYSTEM) {
        got_system_focus_request_ = true;
      }
      if (!use_link_) {
        EXPECT_EQ(FOCUS_SOURCE_SYSTEM, source);
      }
      return cancel_focus_;
    }
    return false;
  }

  void OnGotFocus(CefRefPtr<CefBrowser> browser) override {
    if (checking_focus_) {
      EXPECT_FALSE(cancel_focus_);
      got_focus_notification_ = true;
    }
  }

  void GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) override {
    rect = CefRect(0, 0, kOsrWidth, kOsrHeight);
  }

  bool GetScreenInfo(CefRefPtr<CefBrowser> browser,
                     CefScreenInfo& info) override {
    info.device_scale_factor = 1.0f;
    info.rect = info.available_rect = CefRect(0, 0, kOsrWidth, kOsrHeight);
    return true;
  }

  void RunTest() override {
    AddResource(kFocusCacheUrlA, Page("a"), "text/html");
    AddResource(use_link_ ? kFocusLinkUrlB : kFocusCacheUrlB, Page("b"),
                "text/html");
    if (use_iframe_) {
      AddResource(kFocusCacheFrameUrl,
                  "<!doctype html><body style='margin:0;background:#00ffff'>" +
                      SelectHTML() + "</body>",
                  "text/html");
    }
    CefWindowInfo window_info;
    window_info.SetAsWindowless(kNullWindowHandle);
    CefBrowserHost::CreateBrowser(window_info, this, kFocusCacheUrlA,
                                  CefBrowserSettings(), nullptr, nullptr);
    SetTestTimeout();
  }

  bool OnQuery(CefRefPtr<CefBrowser> browser,
               CefRefPtr<CefFrame> frame,
               int64_t query_id,
               const CefString& request,
               bool persistent,
               CefRefPtr<Callback> callback) override {
    callback->Success("");
    if (request == "link-click") {
      EXPECT_TRUE(use_link_);
      got_link_click_ = true;
      return true;
    }
    if (request == "select-click") {
      EXPECT_EQ(!use_iframe_, frame->IsMain());
      EXPECT_EQ(use_iframe_ ? kFocusCacheFrameUrl
                : use_link_ ? kFocusLinkUrlB
                            : kFocusCacheUrlB,
                frame->GetURL().ToString());
      got_select_click_ = true;
      return true;
    }
    if (request == "focused" || request == "blurred") {
      EXPECT_EQ(cancel_focus_ ? "blurred" : "focused", request.ToString());
      got_focus_result_ = true;
      if (cancel_focus_) {
        CefPostTask(
            TID_UI,
            base::BindOnce(&OsrHistoryPopupTestHandler::DestroyTest, this));
      } else {
        browser->GetMainFrame()->ExecuteJavaScript(
            "document.getElementById('ready').style.background='#00ff00';",
            browser->GetMainFrame()->GetURL(), 0);
      }
      return true;
    }
    const std::string expected[] = {
        "a:load", "b:load", IsBFCacheEnabled() ? "a:restored" : "a:load",
        IsBFCacheEnabled() ? "b:restored" : "b:load"};
    EXPECT_LT(step_, 4);
    if (step_ >= 4) {
      return true;
    }
    EXPECT_EQ(expected[step_], request.ToString());
    if (step_ == 0) {
      browser->GetHost()->SetFocus(true);
    }
    ++step_;
    CefPostDelayedTask(
        TID_UI, base::BindOnce(&OsrHistoryPopupTestHandler::Advance, this),
        100);
    return true;
  }

  void OnPopupShow(CefRefPtr<CefBrowser> browser, bool show) override {
    if (show) {
      EXPECT_EQ(FinalStep(), step_);
      got_popup_show_ = true;
    }
  }

  void OnPaint(CefRefPtr<CefBrowser> browser,
               PaintElementType type,
               const RectList& dirty_rects,
               const void* buffer,
               int width,
               int height) override {
    if (type == PET_VIEW && got_focus_result_ && !cancel_focus_ &&
        !click_pending_ && width == kOsrWidth && height == kOsrHeight) {
      const auto* pixels = static_cast<const unsigned char*>(buffer);
      // Wait for the final page's marker and, for the iframe variant, its
      // child surface. pageshow can precede the compositor's hit-test data.
      const int child_pixel = (100 * width + 100) * 4;
      if (pixels[0] == 0 && pixels[1] == 255 && pixels[2] == 0 &&
          (!use_iframe_ ||
           (pixels[child_pixel] == 255 && pixels[child_pixel + 1] == 255 &&
            pixels[child_pixel + 2] == 0))) {
        click_pending_ = true;
        CefPostTask(
            TID_UI,
            base::BindOnce(&OsrHistoryPopupTestHandler::ClickSelect, this));
      }
    }
    if (type == PET_POPUP && !got_popup_paint_) {
      EXPECT_TRUE(got_popup_show_);
      EXPECT_GT(width, 0);
      EXPECT_GT(height, 0);
      got_popup_paint_ = true;
      CefPostTask(TID_UI, base::BindOnce(
                              &OsrHistoryPopupTestHandler::DestroyTest, this));
    }
  }

  void DestroyTest() override {
    EXPECT_EQ(FinalStep(), step_);
    EXPECT_EQ(use_link_, got_link_click_);
    EXPECT_TRUE(got_focus_request_);
    EXPECT_TRUE(got_system_focus_request_);
    EXPECT_TRUE(got_focus_result_);
    EXPECT_EQ(!cancel_focus_, got_focus_notification_);
    EXPECT_EQ(!cancel_focus_, got_select_click_);
    EXPECT_EQ(!cancel_focus_, got_popup_show_);
    EXPECT_EQ(!cancel_focus_, got_popup_paint_);
    RoutingTestHandler::DestroyTest();
  }

 private:
  static std::string SelectHTML() {
    return "<select onmousedown=\"testQuery({request:'select-click'})\" "
           "style='position:absolute;left:20px;top:20px;width:160px;"
           "height:30px'>"
           "<option>First</option><option>Second</option></select>";
  }

  std::string Page(const std::string& name) const {
    const std::string content =
        use_iframe_ && name == "b"
            ? std::string(
                  "<iframe style='border:0' width=600 height=400 src='") +
                  kFocusCacheFrameUrl + "'></iframe>"
            : SelectHTML();
    const std::string link =
        use_link_ && name == "a"
            ? std::string("<a href='") + kFocusLinkUrlB +
                  "' style='position:absolute;left:20px;top:70px;width:160px;"
                  "height:30px' "
                  "onmousedown=\"testQuery({request:'link-click'})\">"
                  "Navigate</a>"
            : "";
    return "<!doctype html><body style='margin:0'>" + content + link +
           "<div id='ready' style='position:fixed;left:0;top:0;width:10px;"
           "height:10px;background:red;z-index:10'></div>"
           "<script>addEventListener('pageshow',e=>testQuery({request:'" +
           name + "'+(e.persisted?':restored':':load')}));</script></body>";
  }

  void Advance() {
    auto browser = GetBrowser();
    if (use_link_ && step_ == 1) {
      checking_focus_ = true;
      CefMouseEvent event;
      event.x = 100;
      event.y = 85;
      browser->GetHost()->SendMouseMoveEvent(event, false);
      SendMouseClickEvent(browser, event, MBT_LEFT);
    } else if (step_ == FinalStep()) {
      if (!use_link_) {
        // Model focus returning from the toolbar through the public CEF API.
        // The client must be able to cancel this request.
        browser->GetHost()->SetFocus(true);
      }
      browser->GetMainFrame()->ExecuteJavaScript(
          "testQuery({request:document.hasFocus()?'focused':'blurred'});",
          browser->GetMainFrame()->GetURL(), 0);
    } else if (step_ == 1) {
      browser->GetMainFrame()->LoadURL(kFocusCacheUrlB);
    } else if (step_ == 2) {
      // Model focus moving to the back button before B is cached.
      browser->GetHost()->SetFocus(false);
      browser->GoBack();
    } else if (step_ == 3) {
      browser->GetHost()->SetFocus(true);
      checking_focus_ = true;
      browser->GoForward();
    }
  }

  int FinalStep() const { return use_link_ ? 2 : 4; }

  void ClickSelect() {
    CefMouseEvent event;
    event.x = 100;
    event.y = 35;
    auto host = GetBrowser()->GetHost();
    host->SendMouseMoveEvent(event, false);
    SendMouseClickEvent(GetBrowser(), event, MBT_LEFT);
  }

  const bool cancel_focus_;
  const bool use_iframe_;
  const bool use_link_;
  int step_ = 0;
  bool checking_focus_ = false;
  bool got_focus_request_ = false;
  bool got_system_focus_request_ = false;
  bool got_link_click_ = false;
  bool got_focus_notification_ = false;
  bool got_focus_result_ = false;
  bool click_pending_ = false;
  bool got_select_click_ = false;
  bool got_popup_show_ = false;
  bool got_popup_paint_ = false;

  IMPLEMENT_REFCOUNTING(OsrHistoryPopupTestHandler);
};

}  // namespace

TEST(OSRTest, PopupAfterHistoryNavigation) {
  CefRefPtr<OsrHistoryPopupTestHandler> handler =
      new OsrHistoryPopupTestHandler;
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

TEST(OSRTest, CancelFocusAfterHistoryNavigation) {
  CefRefPtr<OsrHistoryPopupTestHandler> handler =
      new OsrHistoryPopupTestHandler(true);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

TEST(OSRTest, PopupInIframeAfterHistoryNavigation) {
  CefRefPtr<OsrHistoryPopupTestHandler> handler =
      new OsrHistoryPopupTestHandler(false, true);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

TEST(OSRTest, PopupAfterLinkNavigation) {
  CefRefPtr<OsrHistoryPopupTestHandler> handler =
      new OsrHistoryPopupTestHandler(false, false, true);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

TEST(OSRTest, PopupInIframeAfterLinkNavigation) {
  CefRefPtr<OsrHistoryPopupTestHandler> handler =
      new OsrHistoryPopupTestHandler(false, true, true);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

TEST(OSRTest, CancelFocusAfterLinkNavigation) {
  CefRefPtr<OsrHistoryPopupTestHandler> handler =
      new OsrHistoryPopupTestHandler(true, false, true);
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
