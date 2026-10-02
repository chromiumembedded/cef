// Copyright 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/base/cef_build.h"

// Alloy style browsers created with a native X11 parent window
// (CefWindowInfo::SetAsChild + CEF_RUNTIME_STYLE_ALLOY) are hosted by a
// CefWindowX11 (libcef/browser/native/window_x11.cc), under which the Ozone
// compositor (DesktopWindowTreeHostLinux) window is created as a child. This
// test is Linux/X11 only.
#if defined(OS_LINUX) && defined(CEF_X11)

// Regression coverage for issue #3396: on a HiDPI display the Ozone child
// compositor was sized in DIP-as-pixels, leaving it scale-times too large
// inside the host window. CreateHostWindow() now derives the scale from the
// host window's actual display and pins the child to the exact requested pixel
// size, so the host and its child are the requested size at any scale.
//
// The invariant asserted here (host == child == requested pixels) holds at
// every device scale factor, so the test can run in the default suite. It is a
// trivial pass at the default scale of 1.0, where the buggy and fixed code
// produce the same size. The regression only becomes observable at a non-unit
// scale, so run it as a dedicated pass:
//
//   xvfb-run -a ceftests --no-sandbox --force-device-scale-factor=1.25 \
//     --gtest_filter=AlloyChildWindowTest.InitialPixelSize
//
// At 1.25 the pre-fix child is 1000x750 inside the 800x600 host; with the fix
// both are 800x600 immediately. The requested size is kept different from the
// Xvfb monitor size so this stays independent of the separate fullscreen-size
// workaround in X11Window::AdjustSizeForDisplay(). Note that a uniform forced
// scale exercises the initial-size invariant only; the multi-monitor display
// selection in GetDisplayMatchingPixelBounds() needs real mixed-DPI hardware.

#include "include/base/cef_callback.h"
#include "include/base/cef_logging.h"
#include "include/cef_browser.h"
#include "include/cef_command_line.h"
#include "include/wrapper/cef_closure_task.h"
#include "tests/ceftests/test_handler.h"
#include "tests/ceftests/thread_helper.h"
#include "tests/ceftests/track_callback.h"
#include "tests/gtest/include/gtest/gtest.h"

#include <X11/Xlib.h>

namespace {

const char kTestUrl[] = "https://tests.test/alloy-native-window.html";
const char kTestContent[] =
    "<html><body>Alloy Native Window Test</body></html>";

// Requested browser size, in pixels. Kept different from any typical Xvfb
// monitor size so the assertion is independent of the fullscreen-size
// workaround in X11Window::AdjustSizeForDisplay().
constexpr int kRequestedWidth = 800;
constexpr int kRequestedHeight = 600;

// Number of UI thread task round trips used to let previously posted teardown
// tasks (browser destruction, Widget destruction) complete before continuing.
constexpr size_t kTeardownRoundTrips = 3;

class AlloyNativeWindowTestHandler : public TestHandler {
 public:
  AlloyNativeWindowTestHandler() = default;

  AlloyNativeWindowTestHandler(const AlloyNativeWindowTestHandler&) = delete;
  AlloyNativeWindowTestHandler& operator=(const AlloyNativeWindowTestHandler&) =
      delete;

  void RunTest() override {
    AddResource(kTestUrl, kTestContent, "text/html");

    // Browser lifetime is managed by this test (see OnAfterCreated), so test
    // completion must be signaled explicitly.
    SetSignalTestCompletionCount(1);

    CefPostTask(TID_UI,
                base::BindOnce(
                    &AlloyNativeWindowTestHandler::CreateChildBrowser, this));
    SetTestTimeout();
  }

  // The Alloy browser is hosted by a CefWindowX11, not a test-managed Views
  // window, so skip the TestHandler implementation and track the browser
  // directly.
  void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
    EXPECT_UI_THREAD();
    EXPECT_FALSE(browser_);
    browser_ = browser;

    // Check the size the browser was created with, once its initialization X11
    // requests have flushed. Use task round trips rather than a resize: a
    // manual parent resize would route through CefWindowX11::ProcessXEvent()
    // and resize the child to match, masking #3396.
    RunAfterUITaskRoundTrips(
        2, base::BindOnce(
               &AlloyNativeWindowTestHandler::CheckInitialPixelSize, this));
  }

  void OnBeforeClose(CefRefPtr<CefBrowser> browser) override {
    EXPECT_UI_THREAD();
    EXPECT_TRUE(browser_);
    EXPECT_TRUE(browser->IsSame(browser_));
    browser_ = nullptr;

    // Allow the posted browser and Widget teardown tasks to complete before
    // finishing the test.
    RunAfterUITaskRoundTrips(
        kTeardownRoundTrips,
        base::BindOnce(&AlloyNativeWindowTestHandler::FinishTest, this));
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
    window_info.SetAsChild(
        parent_window_,
        CefRect(0, 0, kRequestedWidth, kRequestedHeight));

    // This is the native (CefWindowX11) window path affected by issue #3396.
    window_info.runtime_style = CEF_RUNTIME_STYLE_ALLOY;

    CefBrowserSettings settings;
    CefBrowserHost::CreateBrowser(window_info, this, kTestUrl, settings,
                                  /*extra_info=*/nullptr,
                                  /*request_context=*/nullptr);
  }

  void CheckInitialPixelSize() {
    EXPECT_UI_THREAD();
    ASSERT_TRUE(browser_);

    const CefWindowHandle host_handle =
        browser_->GetHost()->GetWindowHandle();
    EXPECT_NE(host_handle, kNullWindowHandle);

    // The browser's configure requests are issued on its own X connection and
    // flushed during the task round trips above; round-trip our connection to
    // the server (XSync) so they are reflected before we query geometry.
    XSync(xdisplay_, /*discard=*/False);

    const ::Window host = static_cast<::Window>(host_handle);

    // The CefWindowX11 host must contain exactly one child: the Ozone
    // DesktopWindowTreeHostLinux compositor window (mirrors FindChild() in
    // libcef/browser/native/window_x11.cc).
    ::Window root = 0, parent = 0;
    ::Window* children = nullptr;
    unsigned int nchildren = 0;
    if (XQueryTree(xdisplay_, host, &root, &parent, &children, &nchildren) &&
        nchildren == 1) {
      const ::Window child = children[0];

      XWindowAttributes host_attrs = {};
      XWindowAttributes child_attrs = {};
      EXPECT_TRUE(XGetWindowAttributes(xdisplay_, host, &host_attrs));
      EXPECT_TRUE(XGetWindowAttributes(xdisplay_, child, &child_attrs));

      // The requested size is in pixels; after the fix both the host and the
      // child compositor are exactly that size, independent of scale.
      EXPECT_EQ(kRequestedWidth, host_attrs.width);
      EXPECT_EQ(kRequestedHeight, host_attrs.height);
      EXPECT_EQ(kRequestedWidth, child_attrs.width);
      EXPECT_EQ(kRequestedHeight, child_attrs.height);

      const std::string forced_scale =
          CefCommandLine::GetGlobalCommandLine()
              ->GetSwitchValue("force-device-scale-factor")
              .ToString();
      LOG(INFO) << "AlloyChildWindowTest.InitialPixelSize:"
                << " requested=" << kRequestedWidth << "x" << kRequestedHeight
                << " host=" << host_attrs.width << "x" << host_attrs.height
                << " child=" << child_attrs.width << "x" << child_attrs.height
                << " force-device-scale-factor='"
                << (forced_scale.empty() ? "(unset; 1.0, trivial pass)"
                                         : forced_scale)
                << "'";
    } else {
      ADD_FAILURE() << "Expected exactly one child of the host window, got "
                    << nchildren;
    }

    if (children) {
      XFree(children);
    }

    DestroyTest();
  }

  // Last step; called after the browser has closed and teardown tasks have
  // completed.
  void FinishTest() {
    EXPECT_UI_THREAD();

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
        base::BindOnce(&AlloyNativeWindowTestHandler::RunAfterUITaskRoundTrips,
                       this, count - 1, std::move(callback)));
  }

  CefWindowHandle CreateParentWindow() {
    EXPECT_UI_THREAD();
    xdisplay_ = XOpenDisplay(nullptr);
    if (!xdisplay_) {
      return kNullWindowHandle;
    }
    const ::Window window = XCreateSimpleWindow(
        xdisplay_, DefaultRootWindow(xdisplay_), 0, 0, kRequestedWidth,
        kRequestedHeight, /*border_width=*/0, /*border=*/0, /*background=*/0);
    XMapWindow(xdisplay_, window);
    XFlush(xdisplay_);
    return static_cast<CefWindowHandle>(window);
  }

  // Safe to call multiple times.
  void DestroyParentWindow() {
    EXPECT_UI_THREAD();
    if (parent_window_ == kNullWindowHandle) {
      return;
    }
    XDestroyWindow(xdisplay_, parent_window_);
    XFlush(xdisplay_);
    XCloseDisplay(xdisplay_);
    xdisplay_ = nullptr;
    parent_window_ = kNullWindowHandle;
  }

  CefRefPtr<CefBrowser> browser_;
  CefWindowHandle parent_window_ = kNullWindowHandle;
  ::Display* xdisplay_ = nullptr;
  bool timed_out_ = false;

  IMPLEMENT_REFCOUNTING(AlloyNativeWindowTestHandler);
};

}  // namespace

// Verifies that an Alloy browser created as a native X11 child is sized to the
// exact requested pixel size (host and Ozone child), which regressed on HiDPI
// displays as issue #3396. Meaningful under --force-device-scale-factor; see
// the file comment for the regression invocation.
TEST(AlloyChildWindowTest, InitialPixelSize) {
  CefRefPtr<AlloyNativeWindowTestHandler> handler =
      new AlloyNativeWindowTestHandler();
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

#endif  // defined(OS_LINUX) && defined(CEF_X11)
