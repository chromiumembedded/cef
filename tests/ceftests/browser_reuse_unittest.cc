// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "include/base/cef_logging.h"
#include "include/views/cef_browser_view.h"
#include "include/views/cef_window.h"
#include "include/wrapper/cef_closure_task.h"
#include "include/wrapper/cef_stream_resource_handler.h"
#include "tests/ceftests/routing_test_handler.h"
#include "tests/ceftests/test_util.h"

namespace {

constexpr char kUrl[] = "https://tests/browser-reuse";
constexpr char kRedirectUrl[] = "https://tests/browser-reuse-redirect";
constexpr char kChildUrl[] = "https://other-tests/browser-reuse-child";
constexpr char kAfterUrl[] = "https://other-tests/browser-reuse-after";
constexpr char kRetiredFrameProbe[] = "BrowserReuseTest.RetiredFrame";

class HandlerOverrideClient : public CefClient,
                              public CefDisplayHandler,
                              public CefFindHandler {
 public:
  explicit HandlerOverrideClient(CefRefPtr<CefWaitableEvent> destroyed)
      : destroyed_(destroyed) {}
  ~HandlerOverrideClient() override { destroyed_->Signal(); }
  CefRefPtr<CefDisplayHandler> GetDisplayHandler() override { return this; }
  CefRefPtr<CefFindHandler> GetFindHandler() override { return this; }
  bool OnConsoleMessage(CefRefPtr<CefBrowser>,
                        cef_log_severity_t,
                        const CefString& message,
                        const CefString&,
                        int) override {
    EXPECT_EQ("override", message.ToString());
    ++console_calls;
    return true;
  }
  void OnFindResult(CefRefPtr<CefBrowser>,
                    int identifier,
                    int count,
                    const CefRect&,
                    int active_match,
                    bool final_update) override {
    EXPECT_EQ(3, identifier);
    EXPECT_EQ(4, count);
    EXPECT_EQ(2, active_match);
    EXPECT_TRUE(final_update);
    ++find_calls;
  }
  int console_calls = 0;
  int find_calls = 0;

 private:
  CefRefPtr<CefWaitableEvent> destroyed_;
  IMPLEMENT_REFCOUNTING(HandlerOverrideClient);
};

class ReuseTestHandler : public RoutingTestHandler,
                         public CefRenderHandler,
                         public CefFocusHandler {
 public:
  ReuseTestHandler(int width,
                   std::string label,
                   bool hold = false,
                   CompletionState* state = nullptr,
                   bool windowed = false,
                   bool views = true,
                   bool alloy = false,
                   bool chrome_window = false)
      : RoutingTestHandler(state),
        width_(width),
        label_(std::move(label)),
        hold_(hold),
        ready_(CefWaitableEvent::CreateWaitableEvent(true, false)),
        windowed_(windowed),
        views_(views),
        alloy_(alloy),
        chrome_window_(chrome_window) {}

  void RunTest() override {
    // Arm the UI timeout before browser creation, which can synchronously
    // create Views windows or wait for an eviction victim to close.
    SetTestTimeout(10000);
    std::string html = "<body>" + label_ + "</body>";
    if (frame_flow_) {
      html += "<iframe src='" + std::string(kChildUrl) + "'></iframe>";
      AddResource(kChildUrl, "<body>child:" + label_ + "</body>", "text/html");
      AddResource(kAfterUrl, "<body>after:" + label_ + "</body>", "text/html");
    }
    AddResource(kUrl, html, "text/html");
    const char* start_url = initial_redirect_ ? kRedirectUrl : kUrl;
    if (windowed_) {
      SetUseViews(views_);
      SetUseAlloyStyle(alloy_, alloy_ && !chrome_window_);
      // Stay above the Chrome window's platform-dependent minimum height.
      CreateReusableBrowser(start_url, CefSize(width_, 600),
                            CefBrowserSettings(), request_context_,
                            custom_client_);
      return;
    }
    CefWindowInfo info;
    info.SetAsWindowless(kNullWindowHandle);
    info.runtime_style = CEF_RUNTIME_STYLE_ALLOY;
    CreateReusableBrowser(info, start_url, CefBrowserSettings(),
                          request_context_, custom_client_);
  }
  CefRefPtr<CefRenderHandler> GetRenderHandler() override { return this; }
  CefRefPtr<CefFocusHandler> GetFocusHandler() override { return this; }
  bool OnSetFocus(CefRefPtr<CefBrowser>, FocusSource source) override {
    if (source == FOCUS_SOURCE_NAVIGATION) {
      navigation_focus_ = true;
    }
    return false;
  }
  void GetViewRect(CefRefPtr<CefBrowser>, CefRect& rect) override {
    rect = CefRect(0, 0, width_, 200);
  }
  bool GetScreenInfo(CefRefPtr<CefBrowser>, CefScreenInfo& info) override {
    info.device_scale_factor = 1;
    info.rect = info.available_rect = CefRect(0, 0, width_, 200);
    return true;
  }
  void OnPaint(CefRefPtr<CefBrowser>,
               PaintElementType,
               const RectList&,
               const void*,
               int,
               int) override {}
  void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int status) override {
    if (frame_flow_) {
      EXPECT_EQ(200, status);
      if (frame->IsMain() && frame->GetURL() == kUrl) {
        retired_root_ = frame;
      } else if (!frame->IsMain()) {
        retired_child_ = frame;
      } else {
        EXPECT_EQ(kAfterUrl, frame->GetURL().ToString());
        EXPECT_NE(retired_root_->GetIdentifier(), frame->GetIdentifier());
        auto client = browser->GetHost()->GetClient();
        // Retained BFCache/detached frames must not reach the current handler.
        for (const auto& old_frame : {retired_root_, retired_child_}) {
          EXPECT_FALSE(client->OnProcessMessageReceived(
              browser, old_frame, PID_RENDERER,
              CefProcessMessage::Create(kRetiredFrameProbe)));
        }
      }
      frame->ExecuteJavaScript(
          "const clean = window.reuseMarker === undefined;"
          "window.reuseMarker = 1;"
          "window.testQuery({request:(clean?'clean:':'dirty:') + "
          "document.body.firstChild.textContent})",
          frame->GetURL(), 0);
      return;
    }
    if (frame->IsMain()) {
      EXPECT_EQ(kUrl, frame->GetURL().ToString());
      EXPECT_EQ(200, status);
      // An unchanged URL must still create a fresh document and serve the new
      // handler's resource. Reusing the old DOM or cached response fails here.
      frame->ExecuteJavaScript(
          "const clean = window.reuseMarker === undefined;"
          "window.reuseMarker = 1;"
          "window.testQuery({request:(clean?'clean:':'dirty:') + "
          "document.body.textContent})",
          kUrl, 0);
    }
  }
  bool OnQuery(CefRefPtr<CefBrowser> browser,
               CefRefPtr<CefFrame>,
               int64_t,
               const CefString& request,
               bool,
               CefRefPtr<Callback> callback) override {
    callback->Success("");
    if (frame_flow_) {
      if (request == "clean:" + label_ || request == "clean:child:" + label_) {
        if (++initial_reports_ == 2) {
          browser->GetMainFrame()->LoadURL(kAfterUrl);
        }
        return true;
      }
      EXPECT_EQ("clean:after:" + label_, request.ToString());
    } else {
      EXPECT_EQ("clean:" + label_, request.ToString());
    }
    browser_id_ = browser->GetIdentifier();
    if (check_custom_client_) {
      auto client = browser->GetHost()->GetClient();
      if (custom_client_) {
        EXPECT_TRUE(client->GetDisplayHandler()->OnConsoleMessage(
            browser, LOGSEVERITY_INFO, "override", kUrl, 1));
        auto find = client->GetFindHandler();
        EXPECT_TRUE(find);
        if (find) {
          find->OnFindResult(browser, 3, 4, CefRect(), 2, true);
        }
        EXPECT_EQ(1, custom_client_->console_calls);
        EXPECT_EQ(1, custom_client_->find_calls);
        // Leave ownership with the harness so the test can detect retention
        // after this lease ends, while the pooled browser remains alive.
        custom_client_ = nullptr;
      } else {
        EXPECT_FALSE(client->GetFindHandler());
      }
    }
    if ((hold_ || check_custom_client_) && pooled_) {
      visited_browser_ = browser;
    }
    EXPECT_TRUE(navigation_focus_);
    if (windowed_) {
      EXPECT_EQ(alloy_ ? CEF_RUNTIME_STYLE_ALLOY : CEF_RUNTIME_STYLE_CHROME,
                browser->GetHost()->GetRuntimeStyle());
      if (views_) {
        auto view = CefBrowserView::GetForBrowser(browser);
        EXPECT_TRUE(view);
        if (view && view->GetWindow()) {
#if defined(OS_MAC)
          // macOS adds the native title bar to CenterWindow's requested size.
          const auto bounds = view->GetWindow()->GetClientAreaBoundsInScreen();
#else
          const auto bounds = view->GetWindow()->GetBounds();
#endif
          EXPECT_EQ(width_, bounds.width);
          EXPECT_EQ(600, bounds.height);
          EXPECT_EQ(alloy_ && !chrome_window_ ? CEF_RUNTIME_STYLE_ALLOY
                                              : CEF_RUNTIME_STYLE_CHROME,
                    view->GetWindow()->GetRuntimeStyle());
        } else {
          ADD_FAILURE() << "Missing Views window";
        }
      }
    }
    ready_->Signal();
    if (!hold_) {
      DestroyTest();
    }
    return true;
  }
  bool WaitUntilReady() { return ready_->TimedWait(10000); }
  void EnableFrameFlow() { frame_flow_ = true; }
  void EnableInitialRedirect() { initial_redirect_ = true; }
  void CheckCustomClient(bool inject) {
    check_custom_client_ = true;
    if (inject) {
      custom_destroyed_ = CefWaitableEvent::CreateWaitableEvent(true, false);
      custom_client_ = new HandlerOverrideClient(custom_destroyed_);
    }
  }
  bool WaitForCustomClientDestroyed() {
    return custom_destroyed_->TimedWait(10000);
  }
  void SetRequestContext(ReusableRequestContext context) {
    request_context_ = context;
  }
  CefRefPtr<CefResourceHandler> GetResourceHandler(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      CefRefPtr<CefRequest> request) override {
    if (initial_redirect_ && request->GetURL() == kRedirectUrl) {
      CefResponse::HeaderMap headers;
      headers.insert({"Location", kUrl});
      return new CefStreamResourceHandler(307, "Temporary Redirect",
                                          "text/html", headers, nullptr);
    }
    return RoutingTestHandler::GetResourceHandler(browser, frame, request);
  }
  bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
                                CefRefPtr<CefFrame> frame,
                                CefProcessId source,
                                CefRefPtr<CefProcessMessage> message) override {
    if (message->GetName() == kRetiredFrameProbe) {
      ADD_FAILURE() << "Forwarded a retired frame";
      return true;
    }
    return RoutingTestHandler::OnProcessMessageReceived(browser, frame, source,
                                                        message);
  }
  bool pooled() const { return pooled_; }
  int browser_id() const { return browser_id_; }
  CefRefPtr<CefBrowser> visited_browser() const { return visited_browser_; }
  void Complete() { DestroyTest(); }

 private:
  void OnReusableBrowserAttached(CefRefPtr<CefBrowser> browser) override {
    pooled_ = true;
    RoutingTestHandler::OnReusableBrowserAttached(browser);
    if (frame_flow_) {
      // The old page is still installed at attachment, before the new lease's
      // navigation. Even its main-frame messages must be gated here.
      EXPECT_FALSE(browser->GetHost()->GetClient()->OnProcessMessageReceived(
          browser, browser->GetMainFrame(), PID_RENDERER,
          CefProcessMessage::Create(kRetiredFrameProbe)));
    }
  }
  void OnReusableBrowserReleased(CefRefPtr<CefBrowser> browser) override {
    retired_root_ = retired_child_ = nullptr;
    RoutingTestHandler::OnReusableBrowserReleased(browser);
  }
  const int width_;
  const std::string label_;
  const bool hold_;
  CefRefPtr<CefWaitableEvent> ready_;
  bool pooled_ = false;
  bool check_custom_client_ = false;
  CefRefPtr<HandlerOverrideClient> custom_client_;
  CefRefPtr<CefWaitableEvent> custom_destroyed_;
  int browser_id_ = 0;
  ReusableRequestContext request_context_;
  CefRefPtr<CefBrowser> visited_browser_;
  const bool windowed_;
  const bool views_;
  const bool alloy_;
  const bool chrome_window_;
  bool frame_flow_ = false;
  bool initial_redirect_ = false;
  bool navigation_focus_ = false;
  int initial_reports_ = 0;
  CefRefPtr<CefFrame> retired_root_;
  CefRefPtr<CefFrame> retired_child_;
  IMPLEMENT_REFCOUNTING(ReuseTestHandler);
};

struct RunResult {
  int id;
  bool pooled;
};

RunResult RunLease(int width, const std::string& label) {
  CefRefPtr<ReuseTestHandler> handler = new ReuseTestHandler(width, label);
  handler->ExecuteTest();
  RunResult result{handler->browser_id(), handler->pooled()};
  ReleaseAndWaitForDestructor(handler);
  return result;
}

enum class Scenario {
  Compatible,
  Lru,
  LeasedCapacity,
  ClosingCapacity,
  Windowed,
  Frames,
  InitialRedirect,
  IdleRequests,
  IdleViews,
  IdleNative,
  CustomClient,
  Contexts
};

// Start every test with an empty cache and close pooled browsers afterward,
// including when an assertion exits the test early.
class BrowserReuseTest : public testing::Test {
 public:
  BrowserReuseTest(Scenario scenario,
                   TestHandler::ReusableBrowserOptions options)
      : scenario_(scenario), options_(options) {}

 protected:
  void SetUp() override {
    TestHandler::ShutdownReusableBrowsers();
    original_options_ = TestHandler::GetReusableBrowserStats();
    TestHandler::SetReusableBrowserOptionsForTesting(options_);
    const auto stats = TestHandler::GetReusableBrowserStats();
    EXPECT_EQ(options_.enabled, stats.enabled);
    EXPECT_EQ(options_.enabled ? options_.max_browsers : 0, stats.limit);
    EXPECT_EQ(0U, stats.total);
  }
  void TearDown() override {
    TestHandler::ShutdownReusableBrowsers();
    TestHandler::SetReusableBrowserOptionsForTesting(std::nullopt);
    const auto stats = TestHandler::GetReusableBrowserStats();
    EXPECT_EQ(original_options_.enabled, stats.enabled);
    EXPECT_EQ(original_options_.limit, stats.limit);
    EXPECT_EQ(0U, stats.total);
  }

 private:
  void TestBody() override {
    switch (scenario_) {
      case Scenario::Compatible:
        CompatibleLeaseReplacesDocument();
        break;
      case Scenario::Lru:
        LeastRecentlyReleasedIsEvicted();
        break;
      case Scenario::LeasedCapacity:
        LeasedCapacityUsesIsolatedBrowser();
        break;
      case Scenario::ClosingCapacity:
        ClosingEntriesRetainTheirSlots();
        break;
      case Scenario::Windowed:
        WindowedCapabilities();
        break;
      case Scenario::Frames:
        FrameAndNavigationForwarding();
        break;
      case Scenario::InitialRedirect:
        RedirectedLeaseReplacesDocument();
        break;
      case Scenario::IdleRequests:
        IdleRequestsAreCanceled();
        break;
      case Scenario::IdleViews:
        IdleWindowVisibility(true);
        break;
      case Scenario::IdleNative:
        IdleWindowVisibility(false);
        break;
      case Scenario::Contexts:
        RequestContextCreationSettings();
        break;
      case Scenario::CustomClient:
        CustomClientHandlers();
        break;
    }
  }
  void CompatibleLeaseReplacesDocument();
  void RequestContextCreationSettings();
  void CustomClientHandlers();
  void LeastRecentlyReleasedIsEvicted();
  void LeasedCapacityUsesIsolatedBrowser();
  void ClosingEntriesRetainTheirSlots();
  void WindowedCapabilities();
  void FrameAndNavigationForwarding();
  void RedirectedLeaseReplacesDocument();
  void IdleRequestsAreCanceled();
  void IdleWindowVisibility(bool views);
  const Scenario scenario_;
  const TestHandler::ReusableBrowserOptions options_;
  TestHandler::ReusableBrowserStats original_options_;
};

void BrowserReuseTest::IdleWindowVisibility(bool views) {
  // Wait on the UI queue so the real idle deadline can expire, including the
  // timer from an earlier lease. This delay is limited to the policy probe.
  const auto wait_idle_deadline = [] {
    auto ready = CefWaitableEvent::CreateWaitableEvent(true, false);
    CefPostDelayedTask(
        TID_UI,
        base::BindOnce(
            [](CefRefPtr<CefWaitableEvent> ready) { ready->Signal(); }, ready),
        1200);
    EXPECT_TRUE(ready->TimedWait(10000));
  };
  int id = 0;
  for (const auto& label : {"first", "second"}) {
    CefRefPtr<ReuseTestHandler> handler =
        new ReuseTestHandler(800, label, false, nullptr, true, views);
    handler->ExecuteTest();
    if (id) {
      EXPECT_EQ(id, handler->browser_id());
    }
    id = handler->browser_id();
    ReleaseAndWaitForDestructor(handler);
  }
  auto stats = TestHandler::GetReusableBrowserStats();
  EXPECT_EQ(0U, stats.window_hides);
  EXPECT_EQ(0U, stats.window_shows);

  TestHandler::CompletionState state(1);
  CefRefPtr<ReuseTestHandler> held =
      new ReuseTestHandler(800, "held", true, &state, true, views);
  CefPostTask(TID_UI, base::BindOnce(&ReuseTestHandler::RunTest, held));
  EXPECT_TRUE(held->WaitUntilReady());
  EXPECT_EQ(id, held->browser_id());
  auto browser = held->visited_browser();
  wait_idle_deadline();
  EXPECT_EQ(0U, TestHandler::GetReusableBrowserStats().window_hides);
  held->Complete();
  state.WaitForTests();
  ReleaseAndWaitForDestructor(held);
  wait_idle_deadline();
  stats = TestHandler::GetReusableBrowserStats();
  // A native backend without visibility control closes idle browsers instead.
  if (!views && stats.total == 0) {
    return;
  }
  EXPECT_EQ(1U, stats.hidden_windows);
  EXPECT_EQ(1U, stats.window_hides);
  EXPECT_EQ(0U, stats.window_shows);
  if (views) {
    auto checked = CefWaitableEvent::CreateWaitableEvent(true, false);
    CefPostTask(TID_UI,
                base::BindOnce(
                    [](CefRefPtr<CefBrowser> browser,
                       CefRefPtr<CefWaitableEvent> done) {
                      auto window =
                          CefBrowserView::GetForBrowser(browser)->GetWindow();
                      EXPECT_FALSE(window->IsVisible());
                      done->Signal();
                    },
                    browser, checked));
    EXPECT_TRUE(checked->TimedWait(10000));
  }
  // Inspect visibility while the restored lease is active. Once released,
  // its idle timer can hide the window again before destruction finishes.
  TestHandler::CompletionState restored_state(1);
  CefRefPtr<ReuseTestHandler> restored =
      new ReuseTestHandler(800, "restored", true, &restored_state, true, views);
  CefPostTask(TID_UI, base::BindOnce(&ReuseTestHandler::RunTest, restored));
  EXPECT_TRUE(restored->WaitUntilReady());
  EXPECT_EQ(id, restored->browser_id());
  stats = TestHandler::GetReusableBrowserStats();
  EXPECT_EQ(0U, stats.hidden_windows);
  EXPECT_EQ(1U, stats.window_hides);
  EXPECT_EQ(1U, stats.window_shows);
  restored->Complete();
  restored_state.WaitForTests();
  ReleaseAndWaitForDestructor(restored);
}

void BrowserReuseTest::CustomClientHandlers() {
  int browser_id = 0;
  for (bool inject : {true, false, true}) {
    CefRefPtr<ReuseTestHandler> handler = new ReuseTestHandler(400, "custom");
    handler->CheckCustomClient(inject);
    handler->ExecuteTest();
    if (inject) {
      EXPECT_TRUE(handler->WaitForCustomClientDestroyed());
    }
    if (handler->pooled()) {
      auto checked = CefWaitableEvent::CreateWaitableEvent(true, false);
      CefPostTask(TID_UI, base::BindOnce(
                              [](CefRefPtr<CefBrowser> browser,
                                 CefRefPtr<CefWaitableEvent> checked) {
                                auto client = browser->GetHost()->GetClient();
                                EXPECT_FALSE(client->GetDisplayHandler());
                                EXPECT_FALSE(client->GetFindHandler());
                                checked->Signal();
                              },
                              handler->visited_browser(), checked));
      EXPECT_TRUE(checked->TimedWait(10000));
    }
    if (browser_id && options_.enabled && options_.max_browsers > 0) {
      EXPECT_EQ(browser_id, handler->browser_id());
    }
    browser_id = handler->browser_id();
    ReleaseAndWaitForDestructor(handler);
  }
}

void BrowserReuseTest::IdleRequestsAreCanceled() {
  TestHandler::CompletionState state(1);
  CefRefPtr<ReuseTestHandler> handler =
      new ReuseTestHandler(400, "held", true, &state);
  CefPostTask(TID_UI, base::BindOnce(&ReuseTestHandler::RunTest, handler));
  if (!handler->WaitUntilReady()) {
    ADD_FAILURE() << "Held browser did not load";
    handler->Complete();
    state.WaitForTests();
    ReleaseAndWaitForDestructor(handler);
    return;
  }
  auto browser = handler->visited_browser();
  handler->Complete();
  state.WaitForTests();
  ReleaseAndWaitForDestructor(handler);

  auto checked = CefWaitableEvent::CreateWaitableEvent(true, false);
  CefPostTask(
      TID_IO,
      base::BindOnce(
          [](CefRefPtr<CefBrowser> browser, CefRefPtr<CefWaitableEvent> done) {
            auto request_handler =
                browser->GetHost()->GetClient()->GetRequestHandler();
            EXPECT_TRUE(request_handler);
            if (request_handler) {
              auto request = CefRequest::Create();
              request->SetURL(std::string(kUrl) + "/late-resource");
              bool disable_default_handling = false;
              EXPECT_FALSE(request_handler->GetResourceRequestHandler(
                  browser, browser->GetMainFrame(), request, false, false, kUrl,
                  disable_default_handling));
              // Returning null alone would send this retired document's
              // request to the real network instead of canceling it.
              EXPECT_TRUE(disable_default_handling);
            }
            done->Signal();
          },
          browser, checked));
  EXPECT_TRUE(checked->TimedWait(10000));
}

void BrowserReuseTest::RedirectedLeaseReplacesDocument() {
  int id = 0;
  for (const auto& label : {"first", "second"}) {
    CefRefPtr<ReuseTestHandler> handler = new ReuseTestHandler(400, label);
    handler->EnableInitialRedirect();
    handler->ExecuteTest();
    EXPECT_TRUE(handler->pooled());
    if (id) {
      EXPECT_EQ(id, handler->browser_id());
    }
    id = handler->browser_id();
    ReleaseAndWaitForDestructor(handler);
  }
}

void BrowserReuseTest::FrameAndNavigationForwarding() {
  int id = 0;
  for (const auto& label : {"first", "second"}) {
    CefRefPtr<ReuseTestHandler> handler = new ReuseTestHandler(400, label);
    handler->EnableFrameFlow();
    handler->ExecuteTest();
    EXPECT_TRUE(handler->pooled());
    if (id) {
      EXPECT_EQ(id, handler->browser_id());
    }
    id = handler->browser_id();
    ReleaseAndWaitForDestructor(handler);
  }
}

void BrowserReuseTest::WindowedCapabilities() {
  const auto stats = TestHandler::GetReusableBrowserStats();
  const bool reuse = stats.enabled && stats.limit > 0;
  std::set<int> configurations;
  const struct {
    bool views;
    bool alloy;
    bool chrome_window;
  } configs[] = {{true, false, false},
                 {true, true, false},
                 {true, true, true},
                 {false, false, false},
                 {false, true, false}};
  for (const auto& config : configs) {
    for (int width : {400, 600}) {
      int previous_id = 0;
      for (int lease = 0; lease < 2; ++lease) {
        const std::string label =
            "views=" + std::to_string(config.views) +
            " alloy=" + std::to_string(config.alloy) +
            " chrome_window=" + std::to_string(config.chrome_window) +
            " width=" + std::to_string(width) +
            " lease=" + std::to_string(lease);
        SCOPED_TRACE(label);
        LOG(INFO) << "WindowedCapabilities starting " << label;
        CefRefPtr<ReuseTestHandler> handler = new ReuseTestHandler(
            width, "windowed" + std::to_string(lease), false, nullptr, true,
            config.views, config.alloy, config.chrome_window);
        handler->ExecuteTest();
        EXPECT_EQ(reuse, handler->pooled());
        const int id = handler->browser_id();
        if (lease == 0) {
          EXPECT_TRUE(configurations.insert(id).second)
              << "Different window capabilities shared a browser";
        } else {
          EXPECT_EQ(reuse, previous_id == id);
        }
        previous_id = id;
        ReleaseAndWaitForDestructor(handler);
        LOG(INFO) << "WindowedCapabilities completed " << label;
      }
    }
  }
}

void BrowserReuseTest::RequestContextCreationSettings() {
  const auto create = [](const CefRequestContextSettings& settings,
                         bool is_global = false) {
    TestHandler::ReusableRequestContext context;
    auto ready = CefWaitableEvent::CreateWaitableEvent(true, false);
    TestHandler::CreateReusableRequestContext(
        settings, is_global,
        base::BindOnce(
            [](TestHandler::ReusableRequestContext* result,
               CefRefPtr<CefWaitableEvent> ready,
               TestHandler::ReusableRequestContext context) {
              *result = context;
              ready->Signal();
            },
            &context, ready));
    EXPECT_TRUE(ready->TimedWait(10000));
    return context;
  };
  const auto lease = [](TestHandler::ReusableRequestContext context) {
    CefRefPtr<ReuseTestHandler> handler = new ReuseTestHandler(400, "context");
    handler->SetRequestContext(context);
    handler->ExecuteTest();
    const int id = handler->browser_id();
    ReleaseAndWaitForDestructor(handler);
    return id;
  };
  CefRequestContextSettings settings;
  TestHandler::ReusableRequestContext first, same;
  auto ready = CefWaitableEvent::CreateWaitableEvent(true, false);
  CefPostTask(TID_UI,
              base::BindOnce(
                  [](CefRequestContextSettings settings,
                     TestHandler::ReusableRequestContext* first,
                     TestHandler::ReusableRequestContext* same,
                     CefRefPtr<CefWaitableEvent> ready) {
                    auto remaining = std::make_shared<int>(2);
                    for (auto* result : {first, same}) {
                      TestHandler::CreateReusableRequestContext(
                          settings, /*is_global=*/false,
                          base::BindOnce(
                              [](TestHandler::ReusableRequestContext* result,
                                 std::shared_ptr<int> remaining,
                                 CefRefPtr<CefWaitableEvent> ready,
                                 TestHandler::ReusableRequestContext context) {
                                *result = context;
                                if (--*remaining == 0) {
                                  ready->Signal();
                                }
                              },
                              result, remaining, ready));
                    }
                  },
                  settings, &first, &same, ready));
  ASSERT_TRUE(ready->TimedWait(10000));
  ASSERT_TRUE(first.context);
  ASSERT_TRUE(same.context);
  const bool reuse = options_.enabled && options_.max_browsers > 0;
  EXPECT_EQ(reuse, first.identity == same.identity);
  const int id = lease(first);
  EXPECT_EQ(reuse, id == lease(same));
  CefString(&settings.cookieable_schemes_list) = "custom";
  auto different = create(settings);
  ASSERT_TRUE(different.context);
  EXPECT_NE(first.identity, different.identity);
  EXPECT_NE(id, lease(different));
  if (reuse && options_.max_browsers == 1) {
    settings = CefRequestContextSettings();
    auto replacement = create(settings);
    ASSERT_TRUE(replacement.context);
    EXPECT_NE(first.identity, replacement.identity);
  }
  auto shared_global = create(CefRequestContextSettings(), true);
  ASSERT_TRUE(shared_global.context);
  EXPECT_FALSE(shared_global.context->IsGlobal());
  EXPECT_TRUE(shared_global.context->IsSharingWith(
      CefRequestContext::GetGlobalContext()));
  EXPECT_NE(first.identity, shared_global.identity);
  auto same_global = create(CefRequestContextSettings(), true);
  EXPECT_EQ(reuse, shared_global.identity == same_global.identity);
}

void BrowserReuseTest::CompatibleLeaseReplacesDocument() {
  const auto stats = TestHandler::GetReusableBrowserStats();
  const auto first = RunLease(400, "first");
  const auto second = RunLease(400, "second");
  const bool reuse = stats.enabled && stats.limit > 0;
  EXPECT_EQ(reuse, first.pooled);
  EXPECT_EQ(reuse, second.pooled);
  EXPECT_EQ(reuse, first.id == second.id);
  EXPECT_EQ(reuse ? 1U : 0U, TestHandler::GetReusableBrowserStats().total);
}

void BrowserReuseTest::LeastRecentlyReleasedIsEvicted() {
  const size_t limit = TestHandler::GetReusableBrowserStats().limit;
  std::vector<int> ids;
  for (size_t i = 0; i < limit; ++i) {
    ids.push_back(RunLease(400 + static_cast<int>(i) * 10, "fill").id);
  }
  const auto refreshed = RunLease(400, "refresh");
  if (limit > 0) {
    EXPECT_EQ(ids[0], refreshed.id);
  }
  RunLease(400 + static_cast<int>(limit) * 10 + 10, "overflow");
  const auto retained = RunLease(400, "retained");
  if (limit > 1) {
    EXPECT_EQ(ids[0], retained.id);
    // Refreshing slot zero makes slot one the eviction victim, independent of
    // the capability-key ordering in the cache.
    EXPECT_NE(ids[1], RunLease(410, "evicted").id);
  } else {
    EXPECT_NE(refreshed.id, retained.id);
  }
  EXPECT_LE(TestHandler::GetReusableBrowserStats().total, limit);
}

void BrowserReuseTest::LeasedCapacityUsesIsolatedBrowser() {
  const size_t limit = TestHandler::GetReusableBrowserStats().limit;
  const size_t count = limit + 1;
  TestHandler::CompletionState state(static_cast<int>(count));
  std::vector<CefRefPtr<ReuseTestHandler>> handlers;
  for (size_t i = 0; i < count; ++i) {
    auto handler = CefRefPtr<ReuseTestHandler>(
        new ReuseTestHandler(400, "held", true, &state));
    handlers.push_back(handler);
    handler->RunTest();
    if (!handler->WaitUntilReady()) {
      ADD_FAILURE() << "Capacity browser " << i << " did not load";
      for (size_t j = i + 1; j < count; ++j) {
        state.TestComplete();
      }
      break;
    }
    EXPECT_EQ(i < limit, handler->pooled());
    const auto stats = TestHandler::GetReusableBrowserStats();
    EXPECT_EQ(std::min(i + 1, limit), stats.leased);
    EXPECT_EQ(stats.leased, stats.total);
  }
  for (auto& handler : handlers) {
    handler->Complete();
  }
  state.WaitForTests();
  for (auto& handler : handlers) {
    ReleaseAndWaitForDestructor(handler);
  }
  EXPECT_EQ(limit, TestHandler::GetReusableBrowserStats().idle);
}

void BrowserReuseTest::ClosingEntriesRetainTheirSlots() {
  const size_t limit = TestHandler::GetReusableBrowserStats().limit;
  for (size_t i = 0; i < limit; ++i) {
    RunLease(400 + static_cast<int>(i) * 10, "fill");
  }
  const size_t count = limit + 1;
  TestHandler::CompletionState state(static_cast<int>(count));
  std::vector<CefRefPtr<ReuseTestHandler>> handlers;
  for (size_t i = 0; i < count; ++i) {
    handlers.push_back(new ReuseTestHandler(1000 + static_cast<int>(i) * 10,
                                            "replacement", true, &state));
  }
  auto started = CefWaitableEvent::CreateWaitableEvent(true, false);
  CefPostTask(TID_UI,
              base::BindOnce(
                  [](std::vector<CefRefPtr<ReuseTestHandler>> handlers,
                     size_t limit, CefRefPtr<CefWaitableEvent> started) {
                    // Issue these acquisitions in a single UI task, before
                    // asynchronous OnBeforeClose callbacks can remove any of
                    // the eviction victims.
                    for (size_t i = 0; i < handlers.size(); ++i) {
                      handlers[i]->RunTest();
                      const auto stats = TestHandler::GetReusableBrowserStats();
                      EXPECT_EQ(limit, stats.total);
                      EXPECT_EQ(std::min(i + 1, limit), stats.closing);
                    }
                    started->Signal();
                  },
                  handlers, limit, started));
  EXPECT_TRUE(started->TimedWait(10000));
  for (size_t i = 0; i < count; ++i) {
    EXPECT_TRUE(handlers[i]->WaitUntilReady());
    EXPECT_EQ(i < limit, handlers[i]->pooled());
  }
  for (auto& handler : handlers) {
    handler->Complete();
  }
  state.WaitForTests();
  for (auto& handler : handlers) {
    ReleaseAndWaitForDestructor(handler);
  }
  EXPECT_EQ(limit, TestHandler::GetReusableBrowserStats().idle);
}

bool RegisterBrowserReuseTests() {
  const struct {
    const char* name;
    Scenario scenario;
  } scenarios[] = {
      {"ConfigurationAndDocumentReset", Scenario::Compatible},
      {"LeastRecentlyReleasedIsEvicted", Scenario::Lru},
      {"LeasedCapacityUsesIsolatedBrowser", Scenario::LeasedCapacity},
      {"ClosingEntriesRetainTheirSlots", Scenario::ClosingCapacity},
      {"WindowedCapabilities", Scenario::Windowed},
      {"FrameAndNavigationForwarding", Scenario::Frames},
      {"RedirectedLeaseReplacesDocument", Scenario::InitialRedirect},
      {"IdleRequestsAreCanceled", Scenario::IdleRequests},
      {"IdleViewsWindowVisibility", Scenario::IdleViews},
      {"IdleNativeWindowVisibility", Scenario::IdleNative},
      {"RequestContextCreationSettings", Scenario::Contexts},
      {"CustomClientHandlers", Scenario::CustomClient}};
  const struct {
    const char* name;
    TestHandler::ReusableBrowserOptions options;
  } modes[] = {{"Disabled", {false}},
               {"ZeroCapacity", {true, 0}},
               {"CapacityOne", {true, 1}},
               {"DefaultCapacity", {true}}};
  for (const auto& scenario : scenarios) {
    for (const auto& mode : modes) {
      // The small configuration probe covers equivalent disabled/zero-capacity
      // fallbacks and enabled configurations. Cache policy scenarios only need
      // enabled modes; capacity one exercises boundary behavior. Windowed
      // compatibility is independent of the configured cache limit.
      if (scenario.scenario != Scenario::Compatible &&
          scenario.scenario != Scenario::Contexts &&
          scenario.scenario != Scenario::CustomClient &&
          (!mode.options.enabled || mode.options.max_browsers == 0)) {
        continue;
      }
      const bool single_mode = scenario.scenario == Scenario::Windowed ||
                               scenario.scenario == Scenario::Frames ||
                               scenario.scenario == Scenario::InitialRedirect ||
                               scenario.scenario == Scenario::IdleRequests ||
                               scenario.scenario == Scenario::IdleViews ||
                               scenario.scenario == Scenario::IdleNative;
      if (single_mode &&
          mode.options.max_browsers !=
              TestHandler::ReusableBrowserOptions().max_browsers) {
        continue;
      }
      const std::string name =
          std::string(scenario.name) + (single_mode ? "" : mode.name);
      const auto options = mode.options;
      const auto value = scenario.scenario;
      testing::RegisterTest("BrowserReuseTest", name.c_str(), nullptr, nullptr,
                            __FILE__, __LINE__,
                            [value, options]() -> testing::Test* {
                              return new BrowserReuseTest(value, options);
                            });
    }
  }
  return true;
}

[[maybe_unused]] const bool kRegisteredBrowserReuseTests =
    RegisterBrowserReuseTests();

}  // namespace
