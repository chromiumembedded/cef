// Copyright 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/base/cef_build.h"

// Chrome style browsers created with a native parent window
// (CefWindowInfo::SetAsChild) are implemented via ChildWindowDelegate in
// libcef/browser/chrome/views/chrome_child_window.cc. Not supported on MacOS.
// On Linux a native parent window can only be created when running with X11.
#if defined(OS_WIN) || (defined(OS_LINUX) && defined(CEF_X11))

// These tests verify teardown of the browser and the native parent window in
// both orders:
// - CloseBrowserFirst: CloseBrowser() destroys the browser (and its platform
//   delegate) before the Widget/window teardown completes. The internal
//   window delegate must not retain a dangling reference to the platform
//   delegate.
// - DestroyParentFirst (Windows only): the client destroys the native parent
//   window (and thereby the browser's Widget) while the browser is still
//   alive. Browser host methods that use cached Widget state must safely
//   no-op afterwards.
//
// Dangling raw_ptr regressions in these scenarios are detected when running
// with --enable-features=PartitionAllocBackupRefPtr,PartitionAllocDanglingPtr
// or in an ASAN build.

#include "include/base/cef_callback.h"
#include "include/cef_browser.h"
#include "include/wrapper/cef_closure_task.h"
#include "tests/ceftests/test_handler.h"
#include "tests/ceftests/thread_helper.h"
#include "tests/ceftests/track_callback.h"
#include "tests/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

#if defined(OS_LINUX)
#include <X11/Xlib.h>
#endif

namespace {

const char kTestUrl[] = "https://tests.test/chrome-child-window.html";
const char kTestContent[] =
    "<html><body>Chrome Child Window Test</body></html>";

constexpr int kParentWidth = 800;
constexpr int kParentHeight = 600;

// Number of UI thread task round trips used to allow previously posted
// teardown tasks (browser destruction, Widget destruction) to complete
// before continuing.
constexpr size_t kTeardownRoundTrips = 3;

class ChromeChildWindowTestHandler : public TestHandler {
 public:
  enum class TeardownOrder {
    // Close the browser first. The native parent window is destroyed after
    // the browser and Widget teardown completes.
    kCloseBrowserFirst,
    // Destroy the native parent window (and thereby the browser's Widget)
    // first, while the browser is still alive.
    kDestroyParentFirst,
  };

  explicit ChromeChildWindowTestHandler(TeardownOrder order) : order_(order) {}

  ChromeChildWindowTestHandler(const ChromeChildWindowTestHandler&) = delete;
  ChromeChildWindowTestHandler& operator=(const ChromeChildWindowTestHandler&) =
      delete;

  void RunTest() override {
    AddResource(kTestUrl, kTestContent, "text/html");

    // Browser lifetime is managed by this test (see OnAfterCreated), so test
    // completion must be signaled explicitly.
    SetSignalTestCompletionCount(1);

    CefPostTask(TID_UI,
                base::BindOnce(
                    &ChromeChildWindowTestHandler::CreateChildBrowser, this));
    SetTestTimeout();
  }

  // The browser is Views-hosted internally (the CefWindow is created by
  // ChildWindowDelegate), but the window is not test-managed. Skip the
  // TestHandler implementations, which expect a TestWindowDelegate-managed
  // window for Views-hosted browsers, and track the browser directly.
  void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
    EXPECT_UI_THREAD();
    EXPECT_FALSE(browser_);
    browser_ = browser;
  }

  void OnBeforeClose(CefRefPtr<CefBrowser> browser) override {
    EXPECT_UI_THREAD();
    EXPECT_TRUE(browser_);
    EXPECT_TRUE(browser->IsSame(browser_));
    browser_ = nullptr;
    got_before_close_.yes();

    // Allow the posted browser and Widget teardown tasks to complete before
    // finishing the test.
    RunAfterUITaskRoundTrips(
        kTeardownRoundTrips,
        base::BindOnce(&ChromeChildWindowTestHandler::FinishTest, this));
  }

  void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int httpStatusCode) override {
    EXPECT_UI_THREAD();
    if (!frame->IsMain()) {
      return;
    }
    EXPECT_FALSE(got_load_end_);
    got_load_end_.yes();
    EXPECT_EQ(200, httpStatusCode);

    if (order_ == TeardownOrder::kCloseBrowserFirst) {
      // Destroys the browser (and its platform delegate) first. The Widget
      // and window teardown will follow.
      DestroyTest();
    } else {
      CefPostTask(TID_UI,
                  base::BindOnce(
                      &ChromeChildWindowTestHandler::DestroyParentFirst, this));
    }
  }

  void DestroyTest() override {
    EXPECT_UI_THREAD();

    if (browser_) {
      CloseBrowser(browser_, /*force_close=*/true);
    }

    TestHandler::DestroyTest();
  }

  void OnTestTimeout(int timeout_ms, bool treat_as_error) override {
    timed_out_ = true;
    TestHandler::OnTestTimeout(timeout_ms, treat_as_error);
  }

 private:
  void CreateChildBrowser() {
    EXPECT_UI_THREAD();

    parent_window_ = CreateParentWindow();
    if (parent_window_ == kNullWindowHandle) {
      ADD_FAILURE() << "Failed to create native parent window";
      DestroyTest();
      SignalTestCompletion();
      return;
    }

    CefWindowInfo window_info;
    window_info.SetAsChild(parent_window_,
                           CefRect(0, 0, kParentWidth, kParentHeight));

    // ChildWindowDelegate is only used with Chrome style browsers.
    window_info.runtime_style = CEF_RUNTIME_STYLE_CHROME;

    CefBrowserSettings settings;
    CefBrowserHost::CreateBrowser(window_info, this, kTestUrl, settings,
                                  /*extra_info=*/nullptr,
                                  /*request_context=*/nullptr);
  }

  // kDestroyParentFirst only: destroy the native parent window while the
  // browser is still alive. This also destroys the browser's Widget.
  void DestroyParentFirst() {
    EXPECT_UI_THREAD();
    EXPECT_TRUE(browser_);

    DestroyParentWindow();

    // Allow the Widget teardown to complete, then exercise the browser before
    // closing it.
    RunAfterUITaskRoundTrips(
        kTeardownRoundTrips,
        base::BindOnce(&ChromeChildWindowTestHandler::PokeBrowserAndClose,
                       this));
  }

  // kDestroyParentFirst only: called after the Widget has been destroyed.
  void PokeBrowserAndClose() {
    EXPECT_UI_THREAD();

    // The browser may have started closing as a result of the Widget
    // destruction.
    if (browser_) {
      // These calls use Widget state that was cached by the browser's
      // platform delegate and must safely no-op after the Widget has been
      // destroyed.
      browser_->GetHost()->SetFocus(true);
      browser_->GetHost()->NotifyMoveOrResizeStarted();
    }

    // Close the browser (if still open) and complete the test.
    DestroyTest();
  }

  // Last step for both teardown orders; called after the browser has closed
  // and teardown tasks have completed.
  void FinishTest() {
    EXPECT_UI_THREAD();
    EXPECT_TRUE(got_load_end_);
    EXPECT_TRUE(got_before_close_);

    DestroyParentWindow();

    if (!timed_out_) {
      SignalTestCompletion();
    }
  }

  // Executes |callback| on the UI thread after |count| task round trips.
  void RunAfterUITaskRoundTrips(size_t count, base::OnceClosure callback) {
    EXPECT_UI_THREAD();
    if (count == 0) {
      std::move(callback).Run();
      return;
    }
    CefPostTask(
        TID_UI,
        base::BindOnce(&ChromeChildWindowTestHandler::RunAfterUITaskRoundTrips,
                       this, count - 1, std::move(callback)));
  }

  CefWindowHandle CreateParentWindow() {
    EXPECT_UI_THREAD();
#if defined(OS_WIN)
    const wchar_t kClassName[] = L"ceftests_ChildWindowParent";
    const HINSTANCE instance = GetModuleHandle(nullptr);

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = instance;
    wc.lpszClassName = kClassName;
    // May fail if the class is already registered; ignored.
    RegisterClassExW(&wc);

    return CreateWindowExW(0, kClassName, L"ChromeChildWindowTest",
                           WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_VISIBLE,
                           CW_USEDEFAULT, CW_USEDEFAULT, kParentWidth,
                           kParentHeight, nullptr, nullptr, instance, nullptr);
#elif defined(OS_LINUX)
    xdisplay_ = XOpenDisplay(nullptr);
    if (!xdisplay_) {
      return kNullWindowHandle;
    }
    const ::Window window = XCreateSimpleWindow(
        xdisplay_, DefaultRootWindow(xdisplay_), 0, 0, kParentWidth,
        kParentHeight, /*border_width=*/0, /*border=*/0, /*background=*/0);
    XMapWindow(xdisplay_, window);
    XFlush(xdisplay_);
    return static_cast<CefWindowHandle>(window);
#endif
  }

  // Safe to call multiple times.
  void DestroyParentWindow() {
    EXPECT_UI_THREAD();
    if (parent_window_ == kNullWindowHandle) {
      return;
    }
#if defined(OS_WIN)
    ::DestroyWindow(parent_window_);
#elif defined(OS_LINUX)
    XDestroyWindow(xdisplay_, parent_window_);
    XFlush(xdisplay_);
    XCloseDisplay(xdisplay_);
    xdisplay_ = nullptr;
#endif
    parent_window_ = kNullWindowHandle;
  }

  const TeardownOrder order_;

  CefRefPtr<CefBrowser> browser_;
  CefWindowHandle parent_window_ = kNullWindowHandle;
#if defined(OS_LINUX)
  ::Display* xdisplay_ = nullptr;
#endif
  bool timed_out_ = false;

  TrackCallback got_load_end_;
  TrackCallback got_before_close_;

  IMPLEMENT_REFCOUNTING(ChromeChildWindowTestHandler);
};

}  // namespace

// Close the browser first; the platform delegate is destroyed before the
// Widget/window teardown completes.
TEST(ChromeChildWindowTest, CloseBrowserFirst) {
  CefRefPtr<ChromeChildWindowTestHandler> handler =
      new ChromeChildWindowTestHandler(
          ChromeChildWindowTestHandler::TeardownOrder::kCloseBrowserFirst);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

#if defined(OS_WIN)
// Destroy the native parent window (and thereby the browser's Widget) while
// the browser is still alive.
TEST(ChromeChildWindowTest, DestroyParentFirst) {
  CefRefPtr<ChromeChildWindowTestHandler> handler =
      new ChromeChildWindowTestHandler(
          ChromeChildWindowTestHandler::TeardownOrder::kDestroyParentFirst);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}
#endif  // defined(OS_WIN)

#endif  // defined(OS_WIN) || (defined(OS_LINUX) && defined(CEF_X11))
