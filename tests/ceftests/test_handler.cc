// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/ceftests/test_handler.h"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <functional>
#include <memory>
#include <mutex>
#include <sstream>
#include <tuple>

#include "include/base/cef_callback.h"
#include "include/base/cef_logging.h"
#include "include/cef_command_line.h"
#include "include/cef_request_context_handler.h"
#include "include/cef_stream.h"
#include "include/views/cef_browser_view.h"
#include "include/views/cef_window.h"
#include "include/wrapper/cef_closure_task.h"
#include "include/wrapper/cef_stream_resource_handler.h"
#include "tests/ceftests/test_request.h"
#include "tests/ceftests/test_util.h"

// Set to 1 to enable verbose debugging info logging.
#define VERBOSE_DEBUGGING 0

namespace {

auto BrowserSettingsKey(const CefBrowserSettings& s) {
  // Compare values, not struct padding or cef_string_t buffer addresses.
  return std::make_tuple(
      s.windowless_frame_rate, CefString(&s.standard_font_family).ToString16(),
      CefString(&s.fixed_font_family).ToString16(),
      CefString(&s.serif_font_family).ToString16(),
      CefString(&s.sans_serif_font_family).ToString16(),
      CefString(&s.cursive_font_family).ToString16(),
      CefString(&s.fantasy_font_family).ToString16(), s.default_font_size,
      s.default_fixed_font_size, s.minimum_font_size,
      s.minimum_logical_font_size, CefString(&s.default_encoding).ToString16(),
      s.remote_fonts, s.javascript, s.javascript_close_windows,
      s.javascript_access_clipboard, s.javascript_dom_paste, s.image_loading,
      s.image_shrink_standalone_to_fit, s.text_area_resize, s.tab_to_links,
      s.local_storage,
#if CEF_API_ADDED(13800)
      s.databases_deprecated,
#else
      s.databases,
#endif
      s.webgl, s.background_color, s.chrome_status_bubble, s.chrome_zoom_bubble
#if CEF_API_ADDED(CEF_EXPERIMENTAL)
      ,
      s.ax_viewport_collapse
#endif
  );
}

auto RectKey(const cef_rect_t& r) {
  return std::make_tuple(r.x, r.y, r.width, r.height);
}

struct BrowserReuseKey {
  size_t context_identity;
  decltype(BrowserSettingsKey(CefBrowserSettings())) settings;
  bool windowless;
  bool use_views;
  cef_runtime_style_t browser_style;
  cef_runtime_style_t window_style;
  cef_window_handle_t parent;
  bool shared_texture;
  bool external_begin_frame;
  decltype(RectKey(CefRect())) view_rect;
  bool provides_screen_info;
  float device_scale_factor;
  int depth;
  int depth_per_component;
  bool monochrome;
  decltype(RectKey(CefRect())) screen_rect;
  decltype(RectKey(CefRect())) available_rect;

  auto Values() const {
    return std::tie(context_identity, settings, windowless, use_views,
                    browser_style, window_style, shared_texture,
                    external_begin_frame, view_rect, provides_screen_info,
                    device_scale_factor, depth, depth_per_component, monochrome,
                    screen_rect, available_rect);
  }
  bool operator<(const BrowserReuseKey& other) const {
    if (parent != other.parent) {
      return std::less<cef_window_handle_t>()(parent, other.parent);
    }
    return Values() < other.Values();
  }
};

BrowserReuseKey MakeBrowserReuseKey(const CefWindowInfo& info,
                                    const CefBrowserSettings& settings,
                                    const CefRect& rect,
                                    const CefScreenInfo& screen,
                                    bool provides_screen_info,
                                    bool use_views,
                                    cef_runtime_style_t window_style,
                                    size_t context_identity) {
  // Normalize default runtime aliases. OSR always uses Alloy.
  CHECK(!provides_screen_info || screen.device_scale_factor > 0);
  CHECK(std::isfinite(screen.device_scale_factor));
  const auto browser_style =
      info.windowless_rendering_enabled ||
              info.runtime_style == CEF_RUNTIME_STYLE_ALLOY
          ? CEF_RUNTIME_STYLE_ALLOY
          : CEF_RUNTIME_STYLE_CHROME;
  return {context_identity,
          BrowserSettingsKey(settings),
          !!info.windowless_rendering_enabled,
          use_views,
          browser_style,
          use_views ? window_style : browser_style,
#if defined(OS_MAC)
          info.parent_view,
#else
          info.parent_window,
#endif
          !!info.shared_texture_enabled,
          !!info.external_begin_frame_enabled,
          RectKey(rect),
          provides_screen_info,
          screen.device_scale_factor,
          screen.depth,
          screen.depth_per_component,
          !!screen.is_monochrome,
          RectKey(screen.rect),
          RectKey(screen.available_rect)};
}

}  // namespace

void ReusableBrowserClosed(class ReusableBrowserClient* client);

// The client stays with the browser; each lease gets a fresh TestHandler.
// Interface getters may be called on the IO thread, so guard the delegate.
class ReusableBrowserClient : public CefClient,
                              public CefLifeSpanHandler,
                              public CefLoadHandler,
                              public CefRequestHandler,
                              public CefRenderHandler {
 public:
  explicit ReusableBrowserClient(bool windowless) : windowless_(windowless) {}
#if CEF_API_ADDED(CEF_NEXT)
  bool WindowKeyboardEvent(const CefKeyboardInputEvent& event) {
    EXPECT_UI_THREAD();
    auto handler = resetting_ ? nullptr : GetDelegate(true);
    return handler && browser_ &&
           handler->OnWindowKeyboardEvent(browser_->GetIdentifier(), event);
  }
#endif
  bool WindowKeyEvent(const CefKeyEvent& event) {
    EXPECT_UI_THREAD();
    auto handler = resetting_ ? nullptr : GetDelegate(true);
    return handler && browser_ &&
           handler->OnWindowKeyEvent(browser_->GetIdentifier(), event);
  }
  void WindowCreated() { window_alive_ = true; }
  void WindowDestroyed() {
    window_alive_ = false;
    if (!browser_) {
      ReusableBrowserClosed(this);
      RunCloseCallbacks();
    }
  }
  bool HasBrowser() const { return !!browser_; }
  bool IsWindowHidden() const { return idle_hidden_; }
  size_t WindowHides() const { return window_hides_; }
  size_t WindowShows() const { return window_shows_; }
  bool IsClosing() const { return closing_; }
  bool IsIdle() {
    EXPECT_UI_THREAD();
    return !closing_ && !GetDelegate();
  }
  auto LastUsed() const { return last_used_; }
  void AfterClose(base::OnceClosure done) {
    EXPECT_UI_THREAD();
    close_callbacks_.push_back(std::move(done));
  }
  void CancelAttach() {
    EXPECT_UI_THREAD();
    std::lock_guard<std::mutex> guard(lock_);
    delegate_ = nullptr;
  }
  void Attach(CefRefPtr<TestHandler> handler,
              const CefString& url,
              const CefRect& rect,
              const CefScreenInfo& screen_info,
              bool provides_screen_info) {
    EXPECT_UI_THREAD();
    CHECK(!GetDelegate());
    CHECK(!closing_);
    timing_start_ = std::chrono::steady_clock::now();
    TraceTiming("Attach");
    target_url_ = url;
    navigation_started_ = false;
    initial_load_failed_ = false;
    awaiting_commit_ = true;
    active_frame_id_.clear();
    ++leases_;
    {
      std::lock_guard<std::mutex> guard(lock_);
      delegate_ = handler;
    }
    rect_ = rect;
    screen_info_ = screen_info;
    provides_screen_info_ = provides_screen_info;
    if (browser_) {
      handler->OnReusableBrowserAttached(browser_);
      if (idle_hidden_) {
        CHECK(SetWindowVisible(true));
        idle_hidden_ = false;
      }
      // Match ordinary creation's navigation-focus request even for the same
      // URL. ReloadIgnoreCache does not issue FOCUS_SOURCE_NAVIGATION.
      browser_->GetMainFrame()->LoadURL(url);
    }
  }

  void Release(bool discard) {
    EXPECT_UI_THREAD();
    CHECK(browser_);
    resetting_ = true;
    TraceTiming(discard ? "Close" : "Release");
    discarding_ = discard;
    browser_->GetHost()->SetFocus(false);
    if (discard) {
      closing_ = true;
      browser_->GetHost()->CloseBrowser(true);
    } else {
      // Detach the handler outside the current callback. The next lease
      // replaces this document before enabling callbacks for its new handler.
      CefPostTask(TID_UI, base::BindOnce(&ReusableBrowserClient::FinishRelease,
                                         this, browser_));
    }
  }

  void Close(base::OnceClosure done) {
    EXPECT_UI_THREAD();
    CHECK(!GetDelegate());
    AfterClose(std::move(done));
    if (closing_) {
      return;
    }
    closing_ = true;
    LOG(INFO) << "Reusable browser: " << leases_ << " leases, " << creations_
              << " creations";
    if (browser_) {
      // CloseBrowser may synchronously invoke OnBeforeClose. Leave the cache
      // acquisition callback first, retaining this closing slot in the interim.
      CefPostTask(TID_UI, base::BindOnce(
                              [](CefRefPtr<CefBrowser> browser) {
                                browser->GetHost()->CloseBrowser(true);
                              },
                              browser_));
    } else if (!window_alive_) {
      ReusableBrowserClosed(this);
      RunCloseCallbacks();
    }
  }

#define FORWARD_HANDLER(name)                                   \
  CefRefPtr<Cef##name##Handler> Get##name##Handler() override { \
    auto handler = GetDelegate(true);                           \
    return handler ? handler->Get##name##Handler() : nullptr;   \
  }
  FORWARD_HANDLER(Audio)
  FORWARD_HANDLER(Command)
  FORWARD_HANDLER(ContextMenu)
  FORWARD_HANDLER(Dialog)
  FORWARD_HANDLER(Display)
  FORWARD_HANDLER(Download)
  FORWARD_HANDLER(Drag)
  FORWARD_HANDLER(Find)
  FORWARD_HANDLER(Frame)
  FORWARD_HANDLER(Permission)
  FORWARD_HANDLER(JSDialog)
  FORWARD_HANDLER(Keyboard)
  FORWARD_HANDLER(Print)
#if CEF_API_ADDED(CEF_NEXT)
  FORWARD_HANDLER(Input)
#endif
#undef FORWARD_HANDLER

  CefRefPtr<CefFocusHandler> GetFocusHandler() override {
    // Navigation focus can precede the document's initial commit. It belongs
    // to the new lease, just like its resource callbacks, and must be
    // forwarded.
    auto handler = GetDelegate();
    return handler ? handler->GetFocusHandler() : nullptr;
  }
  CefRefPtr<CefRequestHandler> GetRequestHandler() override {
    // Keep intercepting while idle so requests from the retired document do
    // not fall through to ordinary network handling without its test handler.
    return this;
  }

  bool OnBeforeBrowse(CefRefPtr<CefBrowser> browser,
                      CefRefPtr<CefFrame> frame,
                      CefRefPtr<CefRequest> request,
                      bool user_gesture,
                      bool is_redirect) override {
    EXPECT_UI_THREAD();
    if (resetting_) {
      return true;
    }
    if (frame->IsMain()) {
      if (awaiting_commit_ && !navigation_started_ &&
          request->GetURL() != target_url_) {
        // Reject a navigation queued by the outgoing document before this
        // lease's explicit LoadURL request reaches OnBeforeBrowse.
        return true;
      }
      // Main-frame navigations initiated by this lease may redirect or replace
      // an earlier request before its first commit.
      target_url_ = request->GetURL();
      navigation_started_ = true;
    }
    auto handler = GetRequestDelegate();
    return handler && handler->OnBeforeBrowse(browser, frame, request,
                                              user_gesture, is_redirect);
  }
  bool OnOpenURLFromTab(CefRefPtr<CefBrowser> browser,
                        CefRefPtr<CefFrame> frame,
                        const CefString& url,
                        CefRequestHandler::WindowOpenDisposition disposition,
                        bool user_gesture) override {
    auto handler = GetRequestDelegate(true);
    return handler && handler->OnOpenURLFromTab(browser, frame, url,
                                                disposition, user_gesture);
  }
  CefRefPtr<CefResourceRequestHandler> GetResourceRequestHandler(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      CefRefPtr<CefRequest> request,
      bool is_navigation,
      bool is_download,
      const CefString& initiator,
      bool& disable_default_handling) override {
    // Requests queued by the outgoing document (notably favicon requests) can
    // reach the IO thread after attachment, before the new OnBeforeBrowse.
    // Once navigation starts, allow its parser's subresources too: they can
    // arrive before the UI thread processes OnLoadStart.
    if (!navigation_started_) {
      disable_default_handling = true;
      return nullptr;
    }
    auto handler = GetRequestDelegate();
    return handler ? handler->GetResourceRequestHandler(
                         browser, frame, request, is_navigation, is_download,
                         initiator, disable_default_handling)
                   : nullptr;
  }
  bool GetAuthCredentials(CefRefPtr<CefBrowser> browser,
                          const CefString& origin,
                          bool proxy,
                          const CefString& host,
                          int port,
                          const CefString& realm,
                          const CefString& scheme,
                          CefRefPtr<CefAuthCallback> callback) override {
    auto handler = GetRequestDelegate();
    return handler &&
           handler->GetAuthCredentials(browser, origin, proxy, host, port,
                                       realm, scheme, callback);
  }
  bool OnCertificateError(CefRefPtr<CefBrowser> browser,
                          cef_errorcode_t error,
                          const CefString& url,
                          CefRefPtr<CefSSLInfo> info,
                          CefRefPtr<CefCallback> callback) override {
    auto handler = GetRequestDelegate();
    return handler &&
           handler->OnCertificateError(browser, error, url, info, callback);
  }
  bool OnSelectClientCertificate(
      CefRefPtr<CefBrowser> browser,
      bool proxy,
      const CefString& host,
      int port,
      const X509CertificateList& certificates,
      CefRefPtr<CefSelectClientCertificateCallback> callback) override {
    auto handler = GetRequestDelegate();
    return handler && handler->OnSelectClientCertificate(
                          browser, proxy, host, port, certificates, callback);
  }
  void OnRenderViewReady(CefRefPtr<CefBrowser> browser) override {
    if (auto handler = GetRequestDelegate(true)) {
      handler->OnRenderViewReady(browser);
    }
  }
  bool OnRenderProcessUnresponsive(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefUnresponsiveProcessCallback> callback) override {
    auto handler = GetRequestDelegate(true);
    return handler && handler->OnRenderProcessUnresponsive(browser, callback);
  }
  void OnRenderProcessResponsive(CefRefPtr<CefBrowser> browser) override {
    if (auto handler = GetRequestDelegate(true)) {
      handler->OnRenderProcessResponsive(browser);
    }
  }
  void OnRenderProcessTerminated(CefRefPtr<CefBrowser> browser,
                                 TerminationStatus status,
                                 int error,
                                 const CefString& text) override {
    if (auto handler = GetRequestDelegate(true)) {
      handler->OnRenderProcessTerminated(browser, status, error, text);
    }
  }
  void OnDocumentAvailableInMainFrame(CefRefPtr<CefBrowser> browser) override {
    if (auto handler = GetRequestDelegate(true)) {
      handler->OnDocumentAvailableInMainFrame(browser);
    }
  }

  CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
  CefRefPtr<CefLoadHandler> GetLoadHandler() override { return this; }
  CefRefPtr<CefRenderHandler> GetRenderHandler() override {
    auto handler = GetDelegate();
    return handler ? handler->GetRenderHandler()
                   : (windowless_ ? this : nullptr);
  }
  void GetViewRect(CefRefPtr<CefBrowser>, CefRect& rect) override {
    rect = rect_;
  }
  bool GetScreenInfo(CefRefPtr<CefBrowser>, CefScreenInfo& info) override {
    info = screen_info_;
    return provides_screen_info_;
  }
  void OnPaint(CefRefPtr<CefBrowser>,
               PaintElementType,
               const RectList&,
               const void*,
               int,
               int) override {}

  void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
    EXPECT_UI_THREAD();
    browser_ = browser;
    TraceTiming("BrowserCreated");
    ++creations_;
    GetDelegate()->OnReusableBrowserAttached(browser);
  }
  void OnBeforeClose(CefRefPtr<CefBrowser> browser) override {
    EXPECT_UI_THREAD();
    browser_ = nullptr;
    if (!window_alive_) {
      ReusableBrowserClosed(this);
    }
    if (GetDelegate()) {
      FinishRelease(browser);
    }
    if (!window_alive_) {
      RunCloseCallbacks();
    }
  }
  void OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                            bool loading,
                            bool back,
                            bool forward) override {
    if (!loading) {
      TraceTiming("LoadingStopped");
    }
    if (!resetting_) {
      if (auto handler = GetDelegate(true)) {
        handler->OnLoadingStateChange(browser, loading, back, forward);
      }
    }
  }
  void OnLoadStart(CefRefPtr<CefBrowser> browser,
                   CefRefPtr<CefFrame> frame,
                   TransitionType type) override {
    if (!resetting_) {
      // Require the requested navigation for the initial lease commit, even
      // when a redirect changes its final URL. The navigation entry retains
      // the original URL; an old document cannot satisfy this check merely
      // because its frame is still the browser's main frame.
      bool lease_navigation =
          !awaiting_commit_ || frame->GetURL() == target_url_;
      if (frame->IsMain() && !lease_navigation) {
        const auto entry = browser->GetHost()->GetVisibleNavigationEntry();
        lease_navigation = entry && entry->GetOriginalURL() == target_url_ &&
                           entry->GetURL() == frame->GetURL();
      }
      if (frame->IsMain() && lease_navigation &&
          frame->GetIdentifier() == browser->GetMainFrame()->GetIdentifier()) {
        active_frame_id_ = frame->GetIdentifier();
        awaiting_commit_ = false;
        TraceTiming("DocumentCommitted");
      }
      if (IsCurrentFrame(frame)) {
        if (auto handler = GetDelegate(true)) {
          handler->OnLoadStart(browser, frame, type);
        }
      }
    }
  }
  void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int status) override {
    if (frame->IsMain()) {
      TraceTiming("LoadEnd " + frame->GetURL().ToString());
    }
    // A failed initial navigation may finish without committing a document
    // (for example, an unhandled custom protocol). Forward its load completion
    // without admitting DOM messages from the document that remains installed.
    const bool failed_initial_load =
        !resetting_ && awaiting_commit_ && initial_load_failed_ &&
        frame->IsMain() &&
        frame->GetIdentifier() == browser->GetMainFrame()->GetIdentifier();
    if (IsCurrentFrame(frame) || failed_initial_load) {
      if (auto handler = GetDelegate(!failed_initial_load)) {
        handler->OnLoadEnd(browser, frame, status);
      }
    }
  }
  void OnLoadError(CefRefPtr<CefBrowser> browser,
                   CefRefPtr<CefFrame> frame,
                   ErrorCode code,
                   const CefString& text,
                   const CefString& url) override {
    if (!resetting_ && awaiting_commit_ && navigation_started_ &&
        frame->IsMain() && url == target_url_) {
      initial_load_failed_ = true;
    }
    // Before the new document commits, the outgoing document can still
    // report an aborted load (including chrome-error://chromewebdata/).
    // Only forward errors for the navigation started by this lease.
    if (!resetting_ &&
        (awaiting_commit_ ? frame->IsMain() && navigation_started_ &&
                               url == target_url_
                          : IsCurrentFrame(frame))) {
      if (auto handler = GetDelegate()) {
        handler->OnLoadError(browser, frame, code, text, url);
      }
    }
  }
  bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
                                CefRefPtr<CefFrame> frame,
                                CefProcessId source,
                                CefRefPtr<CefProcessMessage> message) override {
    if (IsCurrentFrame(frame)) {
      if (auto handler = GetDelegate(true)) {
        return handler->OnProcessMessageReceived(browser, frame, source,
                                                 message);
      }
    }
    return false;
  }

 private:
  CefRefPtr<CefRequestHandler> GetRequestDelegate(bool committed_only = false) {
    auto handler = GetDelegate(committed_only);
    return handler ? handler->GetRequestHandler() : nullptr;
  }
  bool SetWindowVisible(bool visible) {
    bool changed = false;
    if (auto view = CefBrowserView::GetForBrowser(browser_)) {
      auto window = view->GetWindow();
      if (!window) {
        return false;
      }
      if (visible) {
        window->Show();
      } else {
        window->Hide();
      }
      changed = true;
    } else {
      changed = SetNativeTestBrowserVisible(browser_, visible);
    }
    if (changed) {
      if (visible) {
        ++window_shows_;
      } else {
        ++window_hides_;
      }
    }
    return changed;
  }
  void HideIfStillIdle(size_t lease) {
    EXPECT_UI_THREAD();
    if (!browser_ || !IsIdle() || leases_ != lease) {
      return;
    }
    idle_hidden_ = SetWindowVisible(false);
    if (!idle_hidden_) {
      // Native visibility is unavailable on some platforms (e.g. Wayland).
      // Preserve immediate reuse but close after the same idle grace period.
      Close(base::BindOnce([] {}));
    }
  }
  bool IsCurrentFrame(CefRefPtr<CefFrame> frame) {
    EXPECT_UI_THREAD();
    if (resetting_ || awaiting_commit_ || !browser_ || !frame->IsValid() ||
        browser_->GetMainFrame()->GetIdentifier() != active_frame_id_) {
      return false;
    }
    if (frame->IsMain()) {
      return frame->GetIdentifier() == active_frame_id_;
    }
    // GetFrameIdentifiers excludes speculative and BFCache frames. A retained
    // old frame can still be valid, so IsValid/GetParent alone is insufficient.
    std::vector<CefString> identifiers;
    browser_->GetFrameIdentifiers(identifiers);
    if (std::find(identifiers.begin(), identifiers.end(),
                  frame->GetIdentifier()) == identifiers.end()) {
      return false;
    }
    while (auto parent = frame->GetParent()) {
      frame = parent;
    }
    return frame->IsMain() && frame->GetIdentifier() == active_frame_id_;
  }
  void TraceTiming(const std::string& stage) {
    if (!trace_timing_) {
      return;
    }
    LOG(INFO) << "REUSE_TIMING " << (browser_ ? browser_->GetIdentifier() : 0)
              << " "
              << std::chrono::duration_cast<std::chrono::milliseconds>(
                     std::chrono::steady_clock::now() - timing_start_)
                     .count()
              << "ms " << stage;
  }
  std::chrono::steady_clock::time_point timing_start_;
  const bool trace_timing_ = CefCommandLine::GetGlobalCommandLine()->HasSwitch(
      "reuse-test-browsers-timing");
  CefRefPtr<TestHandler> GetDelegate(bool committed_only = false) {
    std::lock_guard<std::mutex> guard(lock_);
    return committed_only && awaiting_commit_ ? nullptr : delegate_;
  }
  void FinishRelease(CefRefPtr<CefBrowser> browser) {
    TraceTiming("FinishRelease");
    EXPECT_UI_THREAD();
    if (discarding_ && browser_) {
      // If timeout interrupted an ordinary release, only OnBeforeClose may
      // return this lease; the closing browser must not be acquired again.
      return;
    }
    CefRefPtr<TestHandler> handler;
    {
      std::lock_guard<std::mutex> guard(lock_);
      handler.swap(delegate_);
    }
    if (!handler) {
      return;
    }
    resetting_ = false;
    discarding_ = false;
    awaiting_commit_ = true;
    navigation_started_ = false;
    initial_load_failed_ = false;
    active_frame_id_.clear();
    last_used_ = std::chrono::steady_clock::now();
    if (!windowless_ && !closing_) {
      // The lease count cancels older idle timers. Sequential acquisitions
      // avoid hide/show entirely; only genuinely idle windows are hidden.
      CefPostDelayedTask(TID_UI,
                         base::BindOnce(&ReusableBrowserClient::HideIfStillIdle,
                                        this, leases_),
                         1000);
    }
    handler->OnReusableBrowserReleased(browser);
  }
  void RunCloseCallbacks() {
    auto callbacks = std::move(close_callbacks_);
    for (auto& callback : callbacks) {
      std::move(callback).Run();
    }
  }
  std::mutex lock_;
  CefRefPtr<TestHandler> delegate_;
  CefRefPtr<CefBrowser> browser_;
  CefRect rect_;
  CefScreenInfo screen_info_;
  bool provides_screen_info_ = false;
  bool resetting_ = false;
  bool discarding_ = false;
  bool closing_ = false;
  const bool windowless_;
  bool window_alive_ = false;
  bool idle_hidden_ = false;
  size_t window_hides_ = 0;
  size_t window_shows_ = 0;
  std::chrono::steady_clock::time_point last_used_;
  std::atomic<bool> awaiting_commit_{true};
  std::atomic<bool> navigation_started_{false};
  bool initial_load_failed_ = false;
  CefString target_url_;
  CefString active_frame_id_;
  size_t leases_ = 0;
  size_t creations_ = 0;
  std::vector<base::OnceClosure> close_callbacks_;
  IMPLEMENT_REFCOUNTING(ReusableBrowserClient);
};

namespace {
// These delegates live with the cached window and never retain a fixture.
class ReusableWindowDelegate : public CefWindowDelegate {
 public:
  ReusableWindowDelegate(CefRefPtr<ReusableBrowserClient> client,
                         CefRefPtr<CefBrowserView> view,
                         CefSize size,
                         cef_runtime_style_t style)
      : client_(client), view_(view), size_(size), style_(style) {}
  void OnWindowCreated(CefRefPtr<CefWindow> window) override {
    client_->WindowCreated();
    window->CenterWindow(size_);
    window->SetTitle(ComputeViewsWindowTitle(window, view_));
    window->AddChildView(view_);
    window->Show();
  }
  void OnWindowDestroyed(CefRefPtr<CefWindow>) override {
    view_ = nullptr;
    client_->WindowDestroyed();
  }
  bool CanClose(CefRefPtr<CefWindow>) override {
    auto browser = view_->GetBrowser();
    return !browser || browser->GetHost()->TryCloseBrowser();
  }
#if CEF_API_ADDED(CEF_NEXT)
  bool OnKeyboardEvent(CefRefPtr<CefWindow>,
                       const CefKeyboardInputEvent& event) override {
    return client_->WindowKeyboardEvent(event);
  }
#endif
  bool OnKeyEvent(CefRefPtr<CefWindow>, const CefKeyEvent& event) override {
    return client_->WindowKeyEvent(event);
  }
  cef_runtime_style_t GetWindowRuntimeStyle() override { return style_; }

 private:
  CefRefPtr<ReusableBrowserClient> client_;
  CefRefPtr<CefBrowserView> view_;
  const CefSize size_;
  const cef_runtime_style_t style_;
  IMPLEMENT_REFCOUNTING(ReusableWindowDelegate);
};

class ReusableBrowserViewDelegate : public CefBrowserViewDelegate {
 public:
  explicit ReusableBrowserViewDelegate(cef_runtime_style_t style)
      : style_(style) {}
  cef_runtime_style_t GetBrowserRuntimeStyle() override { return style_; }
  void OnBrowserDestroyed(CefRefPtr<CefBrowserView> view,
                          CefRefPtr<CefBrowser>) override {
    if (auto window = view->GetWindow()) {
      window->Close();
    }
  }

 private:
  const cef_runtime_style_t style_;
  IMPLEMENT_REFCOUNTING(ReusableBrowserViewDelegate);
};

// Accessed only on the UI thread; explicitly drained before CefShutdown.
using ReusableBrowserMap =
    std::multimap<BrowserReuseKey, CefRefPtr<ReusableBrowserClient>>;
ReusableBrowserMap* reusable_browsers = nullptr;
std::optional<TestHandler::ReusableBrowserOptions> reusable_options_override;

auto RequestContextSettingsKey(const CefRequestContextSettings& settings) {
  return std::make_tuple(
      CefString(&settings.cache_path).ToString16(),
      settings.persist_session_cookies,
      CefString(&settings.accept_language_list).ToString16(),
      CefString(&settings.cookieable_schemes_list).ToString16(),
      settings.cookieable_schemes_exclude_defaults);
}
struct ReusableContextEntry {
  TestHandler::ReusableRequestContext context;
  std::chrono::steady_clock::time_point last_used;
  bool initialized = false;
  std::vector<base::OnceCallback<void(TestHandler::ReusableRequestContext)>>
      callbacks;
};
using ReusableContextMap =
    std::map<decltype(RequestContextSettingsKey(CefRequestContextSettings())),
             std::shared_ptr<ReusableContextEntry>>;
ReusableContextMap* reusable_contexts = nullptr;
size_t next_context_identity = 0;

class ReusableContextHandler : public CefRequestContextHandler {
 public:
  explicit ReusableContextHandler(
      base::OnceCallback<void(CefRefPtr<CefRequestContext>)> callback)
      : callback_(std::move(callback)) {}
  void OnRequestContextInitialized(
      CefRefPtr<CefRequestContext> context) override {
    std::move(callback_).Run(context);
  }

 private:
  base::OnceCallback<void(CefRefPtr<CefRequestContext>)> callback_;
  IMPLEMENT_REFCOUNTING(ReusableContextHandler);
};

TestHandler::ReusableBrowserOptions CurrentReusableBrowserOptions() {
  if (reusable_options_override) {
    return *reusable_options_override;
  }
  TestHandler::ReusableBrowserOptions options;
  const auto command_line = CefCommandLine::GetGlobalCommandLine();
  options.enabled = command_line->HasSwitch("reuse-test-browsers");
  if (!options.enabled || !command_line->HasSwitch("reuse-test-browsers-max")) {
    return options;
  }
  const std::string value =
      command_line->GetSwitchValue("reuse-test-browsers-max");
  size_t limit = 0;
  const auto parsed =
      std::from_chars(value.data(), value.data() + value.size(), limit);
  CHECK(parsed.ec == std::errc() && parsed.ptr == value.data() + value.size())
      << "Invalid --reuse-test-browsers-max: " << value;
  options.max_browsers = limit;
  return options;
}
}  // namespace

void ReusableBrowserClosed(ReusableBrowserClient* client) {
  EXPECT_UI_THREAD();
  if (reusable_browsers) {
    for (auto it = reusable_browsers->begin(); it != reusable_browsers->end();
         ++it) {
      if (it->second.get() == client) {
        reusable_browsers->erase(it);
        break;
      }
    }
  }
}

namespace {

cef_runtime_style_t GetExpectedRuntimeStyle(TestHandler* handler,
                                            bool is_devtools_popup,
                                            bool is_window) {
  const bool alloy_requested = is_window ? handler->use_alloy_style_window()
                                         : handler->use_alloy_style_browser();

  // Alloy style is not supported with Chrome DevTools popups.
  if (alloy_requested && !is_devtools_popup) {
    return CEF_RUNTIME_STYLE_ALLOY;
  }
  return CEF_RUNTIME_STYLE_CHROME;
}

// Delegate implementation for the CefWindow that will host the Views-based
// browser.
class TestWindowDelegate : public CefWindowDelegate {
 public:
  // Create a new top-level Window hosting |browser_view|.
  static void CreateBrowserWindow(TestHandler* handler,
                                  CefRefPtr<CefBrowserView> browser_view,
                                  bool is_devtools_popup,
                                  CefSize size = CefSize(800, 600)) {
    auto window = CefWindow::CreateTopLevelWindow(
        new TestWindowDelegate(handler, browser_view, is_devtools_popup, size));
    EXPECT_EQ(
        GetExpectedRuntimeStyle(handler, is_devtools_popup, /*is_window=*/true),
        window->GetRuntimeStyle());
  }

  // CefWindowDelegate methods:

  void OnWindowCreated(CefRefPtr<CefWindow> window) override {
    // Add the browser view and show the window.
    window->CenterWindow(size_);
    window->SetTitle(ComputeViewsWindowTitle(window, browser_view_));
    window->AddChildView(browser_view_);
    window->Show();

    // With Chrome style, the Browser is not created until after the
    // BrowserView is assigned to the Window.
    browser_id_ = browser_view_->GetBrowser()->GetIdentifier();
    handler_->OnWindowCreated(browser_id_);
  }

  void OnWindowDestroyed(CefRefPtr<CefWindow> window) override {
    auto browser = browser_view_->GetBrowser();
    browser_view_ = nullptr;
    handler_->OnWindowDestroyed(browser_id_);
  }

  void OnWindowActivationChanged(CefRefPtr<CefWindow> window,
                                 bool active) override {
    handler_->OnWindowActivationChanged(browser_id_, active);
  }

#if CEF_API_ADDED(CEF_NEXT)
  bool OnKeyboardEvent(CefRefPtr<CefWindow> window,
                       const CefKeyboardInputEvent& event) override {
    return handler_->OnWindowKeyboardEvent(browser_id_, event);
  }
#endif  // CEF_API_ADDED(CEF_NEXT)

  // DEPRECATED: Use OnKeyboardEvent(); retained for legacy route tests.
  bool OnKeyEvent(CefRefPtr<CefWindow> window,
                  const CefKeyEvent& event) override {
    return handler_->OnWindowKeyEvent(browser_id_, event);
  }

  bool CanClose(CefRefPtr<CefWindow> window) override {
    // Allow the window to close if the browser says it's OK.
    CefRefPtr<CefBrowser> browser = browser_view_->GetBrowser();
    if (browser) {
      return browser->GetHost()->TryCloseBrowser();
    }
    return true;
  }

  cef_runtime_style_t GetWindowRuntimeStyle() override {
    // Alloy style is not supported with Chrome DevTools popups.
    if (handler_->use_alloy_style_window() && !is_devtools_popup_) {
      return CEF_RUNTIME_STYLE_ALLOY;
    }
    return CEF_RUNTIME_STYLE_DEFAULT;
  }

 private:
  TestWindowDelegate(TestHandler* handler,
                     CefRefPtr<CefBrowserView> browser_view,
                     bool is_devtools_popup,
                     CefSize size)
      : handler_(handler),
        browser_view_(browser_view),
        is_devtools_popup_(is_devtools_popup),
        size_(size) {}

  TestHandler* const handler_;
  CefRefPtr<CefBrowserView> browser_view_;
  const bool is_devtools_popup_;
  const CefSize size_;
  int browser_id_ = 0;

  IMPLEMENT_REFCOUNTING(TestWindowDelegate);
};

// Delegate implementation for the CefBrowserView.
class TestBrowserViewDelegate : public CefBrowserViewDelegate {
 public:
  TestBrowserViewDelegate(TestHandler* handler, bool is_devtools_popup)
      : handler_(handler), is_devtools_popup_(is_devtools_popup) {}

  TestBrowserViewDelegate(const TestBrowserViewDelegate&) = delete;
  TestBrowserViewDelegate& operator=(const TestBrowserViewDelegate&) = delete;

  // CefBrowserViewDelegate methods:

  void OnBrowserDestroyed(CefRefPtr<CefBrowserView> browser_view,
                          CefRefPtr<CefBrowser> browser) override {
#if VERBOSE_DEBUGGING
    LOG(INFO) << handler_->debug_string_prefix() << browser->GetIdentifier()
              << ": OnBrowserDestroyed";
#endif
    // Always close the containing Window when the browser is destroyed.
    if (auto window = browser_view->GetWindow()) {
#if VERBOSE_DEBUGGING
      LOG(INFO) << handler_->debug_string_prefix() << browser->GetIdentifier()
                << ": OnBrowserDestroyed Close";
#endif
      window->Close();
    }
  }

  CefRefPtr<CefBrowserViewDelegate> GetDelegateForPopupBrowserView(
      CefRefPtr<CefBrowserView> browser_view,
      const CefBrowserSettings& settings,
      CefRefPtr<CefClient> client,
      bool is_devtools) override {
    auto* handler = static_cast<TestHandler*>(client.get());
    if (handler == handler_) {
      // Use the same Delegate when using the same TestHandler instance if
      // allowed (e.g. runtime style is also the same).
      if (GetExpectedRuntimeStyle(handler_, is_devtools, /*is_window=*/false) ==
          browser_view->GetRuntimeStyle()) {
        return this;
      }
    }

    // Otherwise return a new Delegate instance.
    return new TestBrowserViewDelegate(handler, is_devtools);
  }

  bool OnPopupBrowserViewCreated(CefRefPtr<CefBrowserView> browser_view,
                                 CefRefPtr<CefBrowserView> popup_browser_view,
                                 bool is_devtools) override {
    // The popup may use a different TestHandler instance.
    auto* handler = static_cast<TestHandler*>(
        popup_browser_view->GetBrowser()->GetHost()->GetClient().get());

    // Create our own Window for popups. It will show itself after creation.
    TestWindowDelegate::CreateBrowserWindow(handler, popup_browser_view,
                                            is_devtools);
    return true;
  }

  cef_runtime_style_t GetBrowserRuntimeStyle() override {
    // Alloy style is not supported with Chrome DevTools popups.
    if (handler_->use_alloy_style_browser() && !is_devtools_popup_) {
      return CEF_RUNTIME_STYLE_ALLOY;
    }
    return CEF_RUNTIME_STYLE_DEFAULT;
  }

 private:
  TestHandler* const handler_;
  const bool is_devtools_popup_;

  IMPLEMENT_REFCOUNTING(TestBrowserViewDelegate);
};

}  // namespace

// TestHandler::CompletionState

TestHandler::CompletionState::CompletionState(int total) : total_(total) {
  event_ = CefWaitableEvent::CreateWaitableEvent(true, false);
}

void TestHandler::CompletionState::TestComplete() {
  if (++count_ == total_) {
    count_ = 0;

    // Signal that the test is now complete. Do not access any object members
    // after this call because |this| might be deleted.
    event_->Signal();
  }
}

void TestHandler::CompletionState::WaitForTests() {
  // Wait for the test to complete
  event_->Wait();

  // Reset the event so the same test can be executed again.
  event_->Reset();
}

// TestHandler::Collection

TestHandler::Collection::Collection(CompletionState* completion_state)
    : completion_state_(completion_state) {
  EXPECT_TRUE(completion_state_);
}

void TestHandler::Collection::AddTestHandler(TestHandler* test_handler) {
  EXPECT_EQ(test_handler->completion_state_, completion_state_);
  handler_list_.push_back(test_handler);
}

void TestHandler::Collection::ExecuteTests() {
  EXPECT_GT(handler_list_.size(), 0UL);

  for (const auto& handler : handler_list_) {
    handler->SetupTest();
  }

  completion_state_->WaitForTests();

  for (const auto& handler : handler_list_) {
    handler->RunTest();
  }

  completion_state_->WaitForTests();
}

// TestHandler::UIThreadHelper

TestHandler::UIThreadHelper::UIThreadHelper() : weak_ptr_factory_(this) {}

void TestHandler::UIThreadHelper::PostTask(base::OnceClosure task) {
  EXPECT_UI_THREAD();
  CefPostTask(TID_UI,
              base::BindOnce(&UIThreadHelper::TaskHelper,
                             weak_ptr_factory_.GetWeakPtr(), std::move(task)));
}

void TestHandler::UIThreadHelper::PostDelayedTask(base::OnceClosure task,
                                                  int delay_ms) {
  EXPECT_UI_THREAD();
  CefPostDelayedTask(
      TID_UI,
      base::BindOnce(&UIThreadHelper::TaskHelper,
                     weak_ptr_factory_.GetWeakPtr(), std::move(task)),
      delay_ms);
}

void TestHandler::UIThreadHelper::TaskHelper(base::OnceClosure task) {
  EXPECT_UI_THREAD();
  std::move(task).Run();
}

// TestHandler

// static
std::atomic<size_t> TestHandler::test_handler_count_{0U};

TestHandler::TestHandler(CompletionState* completion_state)
    : debug_string_prefix_(MakeDebugStringPrefix()),
      use_views_(UseViewsGlobal()),
      use_alloy_style_browser_(UseAlloyStyleBrowserGlobal()),
      use_alloy_style_window_(UseAlloyStyleWindowGlobal()) {
  test_handler_count_++;

  if (completion_state) {
    completion_state_ = completion_state;
    completion_state_owned_ = false;
  } else {
    completion_state_ = new CompletionState(1);
    completion_state_owned_ = true;
  }
}

TestHandler::~TestHandler() {
  DCHECK(!ui_thread_helper_.get());
  if (destroy_test_expected_) {
    EXPECT_TRUE(destroy_test_called_);
  } else {
    EXPECT_FALSE(destroy_test_called_);
  }
  EXPECT_TRUE(browser_map_.empty());
  EXPECT_TRUE(browser_status_map_.empty());

  if (completion_state_owned_) {
    delete completion_state_;
  }

  if (destroy_event_) {
    destroy_event_->Signal();
  }

  test_handler_count_--;
}

bool TestHandler::OnConsoleMessage(CefRefPtr<CefBrowser> browser,
                                   cef_log_severity_t level,
                                   const CefString& message,
                                   const CefString& source,
                                   int line) {
  // Log console messages to stdout ourselves. We skip the default Chromium
  // content::LogConsoleMessage() implementation because it ignores messages
  // from incognito Profiles.
  std::cout << "CONSOLE: \"" << message.ToString()
            << "\", source: " << source.ToString() << " (" << line << ")"
            << std::endl;
  return true;
}

void TestHandler::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
  EXPECT_UI_THREAD();

  const int browser_id = browser->GetIdentifier();
  EXPECT_EQ(browser_map_.find(browser_id), browser_map_.end());
  if (browser_map_.empty()) {
    first_browser_id_ = browser_id;
    first_browser_ = browser;
  }
  browser_map_.insert(std::make_pair(browser_id, browser));

  const bool views_hosted = !!CefBrowserView::GetForBrowser(browser);
  OnCreated(browser_id, NT_BROWSER, views_hosted);
}

void TestHandler::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
  EXPECT_UI_THREAD();

  EXPECT_TRUE(browser->GetHost()->IsReadyToBeClosed());

  // Free the browser pointer so that the browser can be destroyed.
  const int browser_id = browser->GetIdentifier();
  BrowserMap::iterator it = browser_map_.find(browser_id);
  EXPECT_NE(it, browser_map_.end());
  browser_map_.erase(it);

  if (browser_id == first_browser_id_) {
    first_browser_id_ = 0;
    first_browser_ = nullptr;
  }

  OnClosed(browser_id, NT_BROWSER);
}

void TestHandler::OnWindowCreated(int browser_id) {
  CHECK(use_views_);
  EXPECT_UI_THREAD();
  OnCreated(browser_id, NT_WINDOW, /*views_hosted=*/true);
}

void TestHandler::OnWindowDestroyed(int browser_id) {
  CHECK(use_views_);
  EXPECT_UI_THREAD();
  OnClosed(browser_id, NT_WINDOW);
}

void TestHandler::OnCreated(int browser_id,
                            NotifyType type,
                            bool views_hosted) {
  CHECK(use_views_ || !views_hosted);

  const bool has_value = browser_status_map_.contains(browser_id);

  auto& browser_status = browser_status_map_[browser_id];
  if (has_value) {
    CHECK_EQ(browser_status.views_hosted, views_hosted);
  } else {
    browser_status.views_hosted = views_hosted;
  }

  EXPECT_FALSE(browser_status.got_created[type])
      << "Duplicate call to OnCreated(" << browser_id << ", "
      << (type == NT_BROWSER ? "BROWSER" : "WINDOW") << ")";
  browser_status.got_created[type].yes();

#if VERBOSE_DEBUGGING
  bool creation_complete = false;

  // When using Views, wait for both Browser and Window notifications.
  if (browser_status.views_hosted) {
    creation_complete = browser_status.got_created[NT_BROWSER] &&
                        browser_status.got_created[NT_WINDOW];
  } else {
    creation_complete = browser_status.got_created[NT_BROWSER];
  }

  LOG(INFO) << debug_string_prefix_ << browser_id << ": OnCreated type="
            << (type == NT_BROWSER ? "BROWSER" : "WINDOW")
            << " creation_complete=" << creation_complete;
#endif  // VERBOSE_DEBUGGING
}

void TestHandler::OnClosed(int browser_id, NotifyType type) {
  bool close_complete = false;

  auto& browser_status = browser_status_map_[browser_id];
  EXPECT_FALSE(browser_status.got_closed[type])
      << "Duplicate call to OnClosed(" << browser_id << ", "
      << (type == NT_BROWSER ? "BROWSER" : "WINDOW") << ")";
  browser_status.got_closed[type].yes();

  // When using Views, wait for both Browser and Window notifications.
  if (browser_status.views_hosted) {
    close_complete = browser_status.got_closed[NT_BROWSER] &&
                     browser_status.got_closed[NT_WINDOW];
  } else {
    close_complete = browser_status.got_closed[NT_BROWSER];
  }

  if (close_complete) {
    browser_status_map_.erase(browser_id);
  }

  // Test may be complete if no Browsers/Windows are remaining.
  const bool all_browsers_closed = AllBrowsersClosed();

#if VERBOSE_DEBUGGING
  LOG(INFO) << debug_string_prefix_ << browser_id
            << ": OnClosed type=" << (type == NT_BROWSER ? "BROWSER" : "WINDOW")
            << " close_complete=" << close_complete
            << " all_browsers_closed=" << all_browsers_closed;
#endif

  if (all_browsers_closed) {
    // May result in |this| being deleted.
    MaybeTestComplete();
  }
}

std::string TestHandler::MakeDebugStringPrefix() const {
#if VERBOSE_DEBUGGING
  std::stringstream ss;
  ss << "TestHandler [0x" << std::hex << this << "]: ";
  return ss.str();
#else
  return std::string();
#endif
}

namespace {

CefResponse::HeaderMap ToCefHeaderMap(
    const ResourceContent::HeaderMap& headerMap) {
  CefResponse::HeaderMap result;
  for (const auto& [key, value] : headerMap) {
    result.insert(std::pair<CefString, CefString>(key, value));
  }
  return result;
}

}  // namespace

CefRefPtr<CefResourceHandler> TestHandler::GetResourceHandler(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefRefPtr<CefRequest> request) {
  EXPECT_IO_THREAD();

  if (resource_map_.size() > 0) {
    const std::string& url = test_request::GetPathURL(request->GetURL());
    ResourceMap::const_iterator it = resource_map_.find(url);
    if (it != resource_map_.end()) {
      // Return the previously mapped resource
      CefRefPtr<CefStreamReader> stream = CefStreamReader::CreateForData(
          static_cast<void*>(const_cast<char*>(it->second.content().c_str())),
          it->second.content().length());
      return new CefStreamResourceHandler(
          200, "OK", it->second.mimeType(),
          ToCefHeaderMap(it->second.headerMap()), stream);
    }
  }

  return nullptr;
}

void TestHandler::OnRenderProcessTerminated(CefRefPtr<CefBrowser> browser,
                                            TerminationStatus status,
                                            int error_code,
                                            const CefString& error_string) {
  LOG(WARNING) << "OnRenderProcessTerminated: status = " << status
               << ", error = " << error_string.ToString() << ".";
}

CefRefPtr<CefBrowser> TestHandler::GetBrowser() const {
  return first_browser_;
}

int TestHandler::GetBrowserId() const {
  return first_browser_id_;
}

void TestHandler::GetAllBrowsers(BrowserMap* map) {
  EXPECT_UI_THREAD();
  EXPECT_TRUE(map);
  *map = browser_map_;
}

void TestHandler::ExecuteTest() {
  EXPECT_EQ(completion_state_->total(), 1);

  // Reset any state from the previous run.
  if (test_timeout_called_) {
    test_timeout_called_ = false;
  }
  if (destroy_test_called_) {
    destroy_test_called_ = false;
  }

  // Run the test.
  RunTest();

  // Wait for the test to complete.
  completion_state_->WaitForTests();
}

void TestHandler::SetupComplete() {
  // Signal that the test setup is complete.
  completion_state_->TestComplete();
}

void TestHandler::DestroyTest() {
  if (!CefCurrentlyOn(TID_UI)) {
    CefPostTask(TID_UI, base::BindOnce(&TestHandler::DestroyTest, this));
    return;
  }

  EXPECT_TRUE(destroy_test_expected_);
  if (destroy_test_called_) {
    if (reusable_client_ && test_timeout_called_) {
      // A stuck reset must close the browser and release the test handler.
      reusable_client_->Release(true);
    }
    return;
  }
  destroy_test_called_ = true;

  if (reusable_creation_pending_) {
    reusable_creation_pending_ = false;
    TestComplete();
    return;
  }

  if (reusable_client_) {
    reusable_client_->Release(test_timeout_called_);
    return;
  }

  if (!browser_map_.empty()) {
    // Use a copy of the map since the original may be modified while we're
    // iterating.
    BrowserMap browser_map = browser_map_;

    // Tell all browsers to close.
    for (const auto& [id, browser] : browser_map) {
      CloseBrowser(browser, false);
    }
  }
}

void TestHandler::OnTestTimeout(int timeout_ms, bool treat_as_error) {
  EXPECT_UI_THREAD();

  EXPECT_FALSE(test_timeout_called_);
  test_timeout_called_ = true;

  if (treat_as_error) {
    EXPECT_TRUE(false) << "Test timed out after " << timeout_ms << "ms";
  }

  EXPECT_FALSE(!reusable_creation_pending_ && AllBrowsersClosed() &&
               AllowTestCompletionWhenAllBrowsersClose())
      << "Test timed out unexpectedly; should be complete";

  // Keep |this| alive until after the method completes.
  CefRefPtr<TestHandler> self(this);

  // Close any remaining browsers.
  DestroyTest();

  // Reset signal completion count.
  if (signal_completion_count_ > 0) {
    signal_completion_count_ = 0;
    MaybeTestComplete();
  }
}

void TestHandler::CreateReusableRequestContext(
    const CefRequestContextSettings& settings,
    base::OnceCallback<void(ReusableRequestContext)> callback) {
  if (!CefCurrentlyOn(TID_UI)) {
    CefPostTask(TID_UI,
                base::BindOnce(&TestHandler::CreateReusableRequestContext,
                               settings, std::move(callback)));
    return;
  }
  const auto options = CurrentReusableBrowserOptions();
  const bool cache = options.enabled && options.max_browsers > 0;
  const auto key = RequestContextSettingsKey(settings);
  if (cache && reusable_contexts) {
    auto it = reusable_contexts->find(key);
    if (it != reusable_contexts->end()) {
      auto entry = it->second;
      entry->last_used = std::chrono::steady_clock::now();
      if (entry->initialized) {
        std::move(callback).Run(entry->context);
      } else {
        entry->callbacks.push_back(std::move(callback));
      }
      return;
    }
  }
  auto entry = std::make_shared<ReusableContextEntry>();
  entry->context.identity = ++next_context_identity;
  entry->last_used = std::chrono::steady_clock::now();
  entry->callbacks.push_back(std::move(callback));
  if (cache) {
    if (!reusable_contexts) {
      reusable_contexts = new ReusableContextMap;
    }
    if (reusable_contexts->size() >= options.max_browsers) {
      auto oldest = reusable_contexts->end();
      for (auto it = reusable_contexts->begin(); it != reusable_contexts->end();
           ++it) {
        if (it->second->initialized &&
            (oldest == reusable_contexts->end() ||
             it->second->last_used < oldest->second->last_used)) {
          oldest = it;
        }
      }
      if (oldest != reusable_contexts->end()) {
        reusable_contexts->erase(oldest);
      }
    }
    if (reusable_contexts->size() < options.max_browsers) {
      // Reserve before initialization so matching pending requests do not
      // create duplicate contexts. If every entry is pending, stay isolated.
      reusable_contexts->emplace(key, entry);
    }
  }
  entry->context.context = CefRequestContext::CreateContext(
      settings, new ReusableContextHandler(base::BindOnce(
                    [](std::shared_ptr<ReusableContextEntry> entry,
                       CefRefPtr<CefRequestContext> context) {
                      entry->context.context = context;
                      entry->initialized = true;
                      auto callbacks = std::move(entry->callbacks);
                      for (auto& callback : callbacks) {
                        std::move(callback).Run(entry->context);
                      }
                    },
                    entry)));
}

void TestHandler::CreateReusableBrowser(
    const CefWindowInfo& window_info,
    const CefString& url,
    const CefBrowserSettings& settings,
    ReusableRequestContext request_context) {
  CHECK(window_info.windowless_rendering_enabled);
  CreateReusableBrowserImpl(window_info, url, settings, false,
                            CEF_RUNTIME_STYLE_ALLOY, request_context);
}

void TestHandler::CreateReusableBrowser(
    const CefString& url,
    const CefSize& window_size,
    const CefBrowserSettings& settings,
    ReusableRequestContext request_context) {
  if (!CefCurrentlyOn(TID_UI)) {
    CefPostTask(
        TID_UI,
        base::BindOnce(
            [](CefRefPtr<TestHandler> handler, CefString url, CefSize size,
               CefBrowserSettings settings, ReusableRequestContext context) {
              handler->CreateReusableBrowser(url, size, settings, context);
            },
            CefRefPtr<TestHandler>(this), url, window_size, settings,
            request_context));
    return;
  }
  CHECK_GT(window_size.width, 0);
  CHECK_GT(window_size.height, 0);
  CefWindowInfo info;
#if defined(OS_WIN)
  info.SetAsPopup(nullptr, ComputeNativeWindowTitle(use_alloy_style_browser_));
  info.style |= WS_VISIBLE;
#endif
  info.bounds = CefRect(0, 0, window_size.width, window_size.height);
  info.runtime_style = use_alloy_style_browser_ ? CEF_RUNTIME_STYLE_ALLOY
                                                : CEF_RUNTIME_STYLE_CHROME;
  CreateReusableBrowserImpl(info, url, settings, use_views_,
                            use_alloy_style_window_ ? CEF_RUNTIME_STYLE_ALLOY
                                                    : CEF_RUNTIME_STYLE_CHROME,
                            request_context);
}

void TestHandler::CreateReusableBrowserImpl(
    const CefWindowInfo& window_info,
    const CefString& url,
    const CefBrowserSettings& settings,
    bool use_views,
    cef_runtime_style_t window_style,
    ReusableRequestContext request_context) {
  if (!CefCurrentlyOn(TID_UI)) {
    CefPostTask(TID_UI,
                base::BindOnce(&TestHandler::CreateReusableBrowserImpl, this,
                               window_info, url, settings, use_views,
                               window_style, request_context));
    return;
  }
  if (destroy_test_called_) {
    // An eviction wait may have been canceled by test timeout.
    return;
  }
  reusable_creation_pending_ = false;
  CHECK(url != "about:blank");
  const auto options = CurrentReusableBrowserOptions();
  CHECK_EQ(!!request_context.context, request_context.identity != 0);
  const auto context = request_context.context
                           ? request_context.context
                           : CefRequestContext::GetGlobalContext();
  const auto create_isolated = [&] {
    if (use_views) {
      CreateBrowserWithSettings(
          url, settings, context, nullptr,
          CefSize(window_info.bounds.width, window_info.bounds.height));
    } else {
      EXPECT_TRUE(CefBrowserHost::CreateBrowser(window_info, this, url,
                                                settings, nullptr, context));
    }
  };
  if (!options.enabled) {
    create_isolated();
    return;
  }
  if (!reusable_browsers) {
    reusable_browsers = new ReusableBrowserMap;
  }
  CefRect rect(window_info.bounds);
  CefScreenInfo screen_info;
  bool provides_screen_info = false;
  if (window_info.windowless_rendering_enabled) {
    auto render = GetRenderHandler();
    CHECK(render);
    render->GetViewRect(nullptr, rect);
    provides_screen_info = render->GetScreenInfo(nullptr, screen_info);
  }
  if (!provides_screen_info) {
    // Ignore output from a declined callback; CEF supplies monitor defaults.
    screen_info = CefScreenInfo();
  }
  const auto key = MakeBrowserReuseKey(window_info, settings, rect, screen_info,
                                       provides_screen_info, use_views,
                                       window_style, request_context.identity);
  CefRefPtr<ReusableBrowserClient> client;
  const auto [first, last] = reusable_browsers->equal_range(key);
  for (auto it = first; it != last; ++it) {
    if (it->second->IsIdle()) {
      client = it->second;
      break;
    }
  }
  if (!client && reusable_browsers->size() >= options.max_browsers) {
    CefRefPtr<ReusableBrowserClient> oldest;
    for (const auto& [cached_key, cached] : *reusable_browsers) {
      if (cached->IsIdle() &&
          (!oldest || cached->LastUsed() < oldest->LastUsed())) {
        oldest = cached;
      }
    }
    if (oldest) {
      reusable_creation_pending_ = true;
      // Keep the closing entry counted until browser/window closure removes
      // it; another pending acquisition may have claimed that slot.
      oldest->Close(base::BindOnce(
          [](CefRefPtr<TestHandler> handler, CefWindowInfo info, CefString url,
             CefBrowserSettings settings, bool use_views,
             cef_runtime_style_t window_style, ReusableRequestContext context) {
            CefPostTask(TID_UI,
                        base::BindOnce(&TestHandler::CreateReusableBrowserImpl,
                                       handler, info, url, settings, use_views,
                                       window_style, context));
          },
          CefRefPtr<TestHandler>(this), window_info, url, settings, use_views,
          window_style, request_context));
      return;
    }
    // Every pool slot is leased or closing (or the configured limit is zero).
    // This isolated browser never enters the cache and closes normally.
    create_isolated();
    return;
  }
  if (!client) {
    CHECK_LT(reusable_browsers->size(), options.max_browsers);
    client =
        new ReusableBrowserClient(window_info.windowless_rendering_enabled);
    reusable_browsers->emplace(key, client);
  }
  reusable_client_ = client;
  const bool create = !client->HasBrowser();
  client->Attach(this, url, rect, screen_info, provides_screen_info);
  if (create) {
    bool created;
    if (use_views) {
      auto view = CefBrowserView::CreateBrowserView(
          client, url, settings, nullptr, context,
          new ReusableBrowserViewDelegate(window_info.runtime_style));
      created = !!view;
      if (view) {
        CefWindow::CreateTopLevelWindow(new ReusableWindowDelegate(
            client, view, CefSize(rect.width, rect.height), window_style));
      }
    } else {
      created = CefBrowserHost::CreateBrowser(window_info, client, url,
                                              settings, nullptr, context);
    }
    EXPECT_TRUE(created);
    if (!created) {
      client->CancelAttach();
      ReusableBrowserClosed(client.get());
      reusable_client_ = nullptr;
      destroy_test_called_ = true;
      TestComplete();
    }
  }
}

void TestHandler::OnReusableBrowserAttached(CefRefPtr<CefBrowser> browser) {
  // Register ownership without manufacturing a lifespan callback.
  TestHandler::OnAfterCreated(browser);
}

void TestHandler::OnReusableBrowserReleased(CefRefPtr<CefBrowser> browser) {
  EXPECT_UI_THREAD();
  CHECK_EQ(browser_map_.size(), 1U);
  CHECK_EQ(browser->GetIdentifier(), first_browser_id_);
  browser_map_.clear();
  browser_status_map_.clear();
  first_browser_ = nullptr;
  first_browser_id_ = 0;
  reusable_client_ = nullptr;
  MaybeTestComplete();
}

void TestHandler::ShutdownReusableBrowsers() {
  CHECK(!CefCurrentlyOn(TID_UI));
  auto done = CefWaitableEvent::CreateWaitableEvent(true, false);
  CefPostTask(TID_UI,
              base::BindOnce(
                  [](CefRefPtr<CefWaitableEvent> done) {
                    delete reusable_contexts;
                    reusable_contexts = nullptr;
                    if (!reusable_browsers || reusable_browsers->empty()) {
                      delete reusable_browsers;
                      reusable_browsers = nullptr;
                      done->Signal();
                      return;
                    }
                    auto remaining =
                        std::make_shared<size_t>(reusable_browsers->size());
                    auto browsers = std::move(*reusable_browsers);
                    delete reusable_browsers;
                    reusable_browsers = nullptr;
                    for (auto& [key, client] : browsers) {
                      client->Close(base::BindOnce(
                          [](std::shared_ptr<size_t> remaining,
                             CefRefPtr<CefWaitableEvent> done) {
                            if (--*remaining == 0) {
                              done->Signal();
                            }
                          },
                          remaining, done));
                    }
                  },
                  done));
  done->Wait();
}

TestHandler::ReusableBrowserStats TestHandler::GetReusableBrowserStats() {
  if (!CefCurrentlyOn(TID_UI)) {
    ReusableBrowserStats result;
    auto done = CefWaitableEvent::CreateWaitableEvent(true, false);
    CefPostTask(TID_UI, base::BindOnce(
                            [](ReusableBrowserStats* result,
                               CefRefPtr<CefWaitableEvent> done) {
                              *result = GetReusableBrowserStats();
                              done->Signal();
                            },
                            &result, done));
    done->Wait();
    return result;
  }
  ReusableBrowserStats result;
  const auto options = CurrentReusableBrowserOptions();
  result.enabled = options.enabled;
  result.limit = result.enabled ? options.max_browsers : 0;
  if (reusable_browsers) {
    result.total = reusable_browsers->size();
    for (const auto& [key, client] : *reusable_browsers) {
      result.hidden_windows += client->IsWindowHidden();
      result.window_hides += client->WindowHides();
      result.window_shows += client->WindowShows();
      if (client->IsClosing()) {
        ++result.closing;
      } else if (client->IsIdle()) {
        ++result.idle;
      } else {
        ++result.leased;
      }
    }
  }
  return result;
}

void TestHandler::SetReusableBrowserOptionsForTesting(
    std::optional<ReusableBrowserOptions> options) {
  if (!CefCurrentlyOn(TID_UI)) {
    auto done = CefWaitableEvent::CreateWaitableEvent(true, false);
    CefPostTask(TID_UI, base::BindOnce(
                            [](std::optional<ReusableBrowserOptions> options,
                               CefRefPtr<CefWaitableEvent> done) {
                              SetReusableBrowserOptionsForTesting(options);
                              done->Signal();
                            },
                            options, done));
    done->Wait();
    return;
  }
  CHECK(!reusable_browsers || reusable_browsers->empty());
  reusable_options_override = options;
}

void TestHandler::CreateBrowser(const CefString& url,
                                CefRefPtr<CefRequestContext> request_context,
                                CefRefPtr<CefDictionaryValue> extra_info) {
  CefBrowserSettings settings;
  CreateBrowserWithSettings(url, settings, request_context, extra_info);
}

void TestHandler::CreateBrowserWithSettings(
    const CefString& url,
    const CefBrowserSettings& settings,
    CefRefPtr<CefRequestContext> request_context,
    CefRefPtr<CefDictionaryValue> extra_info,
    const CefSize& window_size) {
  if (!CefCurrentlyOn(TID_UI)) {
    CefPostTask(TID_UI, base::BindOnce(&TestHandler::CreateBrowserWithSettings,
                                       this, url, settings, request_context,
                                       extra_info, window_size));
    return;
  }

  CefWindowInfo windowInfo;

  if (use_views_) {
    // Create the BrowserView.
    CefRefPtr<CefBrowserView> browser_view = CefBrowserView::CreateBrowserView(
        this, url, settings, extra_info, request_context,
        new TestBrowserViewDelegate(this, /*is_devtools_popup=*/false));
    EXPECT_EQ(GetExpectedRuntimeStyle(this, /*is_devtools_popup=*/false,
                                      /*is_window=*/false),
              browser_view->GetRuntimeStyle());

    // Create the Window. It will show itself after creation.
    TestWindowDelegate::CreateBrowserWindow(this, browser_view,
                                            /*is_devtools_popup=*/false,
                                            window_size);
  } else {
#if defined(OS_WIN)
    windowInfo.SetAsPopup(nullptr,
                          ComputeNativeWindowTitle(use_alloy_style_browser_));
    windowInfo.style |= WS_VISIBLE;
#endif
    if (use_alloy_style_browser_) {
      windowInfo.runtime_style = CEF_RUNTIME_STYLE_ALLOY;
    }
    CefBrowserHost::CreateBrowser(windowInfo, this, url, settings, extra_info,
                                  request_context);
  }
}

// static
void TestHandler::CloseBrowser(CefRefPtr<CefBrowser> browser,
                               bool force_close) {
#if VERBOSE_DEBUGGING
  LOG(INFO) << "TestHandler: " << browser->GetIdentifier()
            << ": CloseBrowser force_close=" << force_close;
#endif
  browser->GetHost()->CloseBrowser(force_close);
}

void TestHandler::AddResource(const std::string& url,
                              const std::string& content,
                              const std::string& mime_type,
                              const ResourceContent::HeaderMap& header_map) {
  ResourceContent rc = ResourceContent(content, mime_type, header_map);
  AddResourceEx(url, rc);
}

void TestHandler::AddResourceEx(const std::string& url,
                                const ResourceContent& content) {
  if (!CefCurrentlyOn(TID_IO)) {
    CefPostTask(TID_IO, base::BindOnce(&TestHandler::AddResourceEx, this, url,
                                       content));
    return;
  }

  // Ignore the query component, if any.
  std::string urlStr = url;
  size_t idx = urlStr.find('?');
  if (idx > 0) {
    urlStr = urlStr.substr(0, idx);
  }

  resource_map_.insert(std::make_pair(urlStr, content));
}

void TestHandler::ClearResources() {
  if (!CefCurrentlyOn(TID_IO)) {
    CefPostTask(TID_IO, base::BindOnce(&TestHandler::ClearResources, this));
    return;
  }

  resource_map_.clear();
}

void TestHandler::SetTestTimeout(int timeout_ms, bool treat_as_error) {
  if (!CefCurrentlyOn(TID_UI)) {
    CefPostTask(TID_UI, base::BindOnce(&TestHandler::SetTestTimeout, this,
                                       timeout_ms, treat_as_error));
    return;
  }

  if (destroy_test_called_) {
    // No need to set the timeout if the test has already completed.
    return;
  }

  const auto timeout = GetConfiguredTestTimeout(timeout_ms);
  if (treat_as_error && !timeout) {
    return;
  }

  // Use a weak reference to |this| via UIThreadHelper so that the TestHandler
  // can be destroyed before the timeout expires.
  GetUIThreadHelper()->PostDelayedTask(
      base::BindOnce(&TestHandler::OnTestTimeout, base::Unretained(this),
                     *timeout, treat_as_error),
      *timeout);
}

void TestHandler::SetUseViews(bool use_views) {
  if (!CefCurrentlyOn(TID_UI)) {
    CefPostTask(TID_UI,
                base::BindOnce(&TestHandler::SetUseViews, this, use_views));
    return;
  }

  use_views_ = use_views;
}

void TestHandler::SetUseAlloyStyle(bool use_alloy_style_browser,
                                   bool use_alloy_style_window) {
  if (!CefCurrentlyOn(TID_UI)) {
    CefPostTask(TID_UI, base::BindOnce(&TestHandler::SetUseAlloyStyle, this,
                                       use_alloy_style_browser,
                                       use_alloy_style_window));
    return;
  }

  use_alloy_style_browser_ = use_alloy_style_browser;
  use_alloy_style_window_ = use_alloy_style_window;
}

void TestHandler::SetSignalTestCompletionCount(size_t count) {
#if VERBOSE_DEBUGGING
  LOG(INFO) << debug_string_prefix_
            << "SetSignalTestCompletionCount count=" << count;
#endif
  signal_completion_count_ = count;
}

void TestHandler::SignalTestCompletion() {
  if (!CefCurrentlyOn(TID_UI)) {
    CefPostTask(TID_UI,
                base::BindOnce(&TestHandler::SignalTestCompletion, this));
    return;
  }

  if (test_timeout_called_) {
    // Ignore any signals that arrive after test timeout.
    return;
  }

  CHECK_GT(signal_completion_count_, 0U);
  signal_completion_count_--;

#if VERBOSE_DEBUGGING
  LOG(INFO) << debug_string_prefix_
            << "SignalTestComplete remaining=" << signal_completion_count_;
#endif

  if (signal_completion_count_ == 0) {
    // May result in |this| being deleted.
    MaybeTestComplete();
  }
}

bool TestHandler::AllowTestCompletionWhenAllBrowsersClose() const {
  EXPECT_UI_THREAD();
  return signal_completion_count_ == 0U;
}

bool TestHandler::AllBrowsersClosed() const {
  EXPECT_UI_THREAD();
  return browser_status_map_.empty();
}

void TestHandler::MaybeTestComplete() {
  EXPECT_UI_THREAD();

  const bool all_browsers_closed = AllBrowsersClosed();
  const bool allow_test_completion = AllowTestCompletionWhenAllBrowsersClose();

#if VERBOSE_DEBUGGING
  LOG(INFO) << debug_string_prefix_
            << "MaybeTestComplete all_browsers_closed=" << all_browsers_closed
            << " allow_test_completion=" << allow_test_completion;
#endif

  if (all_browsers_closed && allow_test_completion) {
    TestComplete();
  }
}

void TestHandler::TestComplete() {
  EXPECT_UI_THREAD();
  EXPECT_TRUE(AllBrowsersClosed());
  EXPECT_TRUE(AllowTestCompletionWhenAllBrowsersClose());

#if VERBOSE_DEBUGGING
  LOG(INFO) << debug_string_prefix_ << "TestComplete";
#endif

  // Cancel any pending tasks posted via UIThreadHelper.
  ui_thread_helper_.reset();

  completion_state_->TestComplete();
}

TestHandler::UIThreadHelper* TestHandler::GetUIThreadHelper() {
  EXPECT_UI_THREAD();
  CHECK(!destroy_test_called_);

  if (!ui_thread_helper_.get()) {
    ui_thread_helper_ = std::make_unique<UIThreadHelper>();
  }
  return ui_thread_helper_.get();
}

// global functions

bool TestFailed() {
  CefRefPtr<CefCommandLine> command_line =
      CefCommandLine::GetGlobalCommandLine();
  if (command_line->HasSwitch("single-process")) {
    // Check for a failure on the current test only.
    return ::testing::UnitTest::GetInstance()
        ->current_test_info()
        ->result()
        ->Failed();
  } else {
    // Check for any global failure.
    return ::testing::UnitTest::GetInstance()->Failed();
  }
}
