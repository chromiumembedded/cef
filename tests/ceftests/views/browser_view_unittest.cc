// Copyright (c) 2025 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/base/cef_callback.h"
#include "include/cef_request_context.h"
#include "include/views/cef_box_layout.h"
#include "include/views/cef_browser_view.h"
#include "include/views/cef_window.h"
#include "include/wrapper/cef_closure_task.h"
#include "tests/ceftests/test_handler.h"
#include "tests/ceftests/thread_helper.h"
#include "tests/gtest/include/gtest/gtest.h"

#define BROWSER_VIEW_TEST_ASYNC(name) \
  UI_THREAD_TEST_ASYNC(ViewsBrowserViewTest, name)

#define VERBOSE_DEBUGGING 0

namespace {

const char kTestUrl1[] = "https://test1.example/page.html";
const char kTestUrl2[] = "https://test2.example/page.html";
const char kTestContent1[] = "<html><body>Test Page 1</body></html>";
const char kTestContent2[] = "<html><body>Test Page 2</body></html>";

// Helper to create BrowserView with test content.
CefRefPtr<CefBrowserView> CreateBrowserView(
    CefRefPtr<CefClient> client,
    const CefString& url,
    const CefBrowserSettings& settings,
    CefRefPtr<CefBrowserViewDelegate> delegate) {
  return CefBrowserView::CreateBrowserView(client, url, settings, nullptr,
                                           nullptr, delegate);
}

// Base test handler for BrowserView tests.
class BrowserViewTestHandlerBase : public TestHandler {
 public:
  using OnCompleteCallback = base::OnceCallback<void(/*success=*/bool)>;

  explicit BrowserViewTestHandlerBase(int expected_browser_count)
      : expected_browser_count_(expected_browser_count) {
    // We're not using the ExecuteTest() pattern, so don't expect DestroyTest().
    SetDestroyTestExpected(false);
    // These tests create views-hosted browsers.
    SetUseViews(true);
    // Set a test timeout in case the test hangs.
    SetTestTimeout();
  }

  void RunTest() override {
    // Not used - we create browsers manually via BrowserView.
  }

  void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int httpStatusCode) override {
    if (!frame->IsMain()) {
      return;
    }

    browsers_loaded_++;
    EXPECT_LE(browsers_loaded_, expected_browser_count_);
#if VERBOSE_DEBUGGING
    LOG(INFO) << "OnLoadEnd: browser " << browser->GetIdentifier()
              << ", loaded count=" << browsers_loaded_;
#endif

    if (on_complete_ && browsers_loaded_ >= expected_browser_count_) {
      CefPostTask(TID_UI, base::BindOnce(std::move(on_complete_), true));
    }
  }

  void SetOnComplete(OnCompleteCallback callback) {
    on_complete_ = std::move(callback);
  }

  void ClearCallbacks() { on_complete_.Reset(); }

  void DestroyTest() override {
    // Timeout occurred - fail the test and run completion callback.
    ADD_FAILURE() << "Test timeout";
    if (on_complete_) {
      std::move(on_complete_).Run(false);
    }
  }

 protected:
  const int expected_browser_count_;
  int browsers_loaded_ = 0;
  OnCompleteCallback on_complete_;
};

// Base delegate for BrowserView tests.
template <typename Handler>
class BrowserViewDelegateBase : public CefWindowDelegate,
                                public CefBrowserViewDelegate {
 public:
  BrowserViewDelegateBase(CefRefPtr<Handler> handler,
                          CefRefPtr<CefWaitableEvent> event,
                          cef_runtime_style_t window_style,
                          int expected_browser_count)
      : handler_(handler),
        event_(event),
        window_style_(window_style),
        expected_browser_count_(expected_browser_count) {}

  // CefWindowDelegate methods:
  void OnWindowDestroyed(CefRefPtr<CefWindow> window) override {
    window_ = nullptr;
#if VERBOSE_DEBUGGING
    LOG(INFO) << "OnWindowDestroyed: browsers created="
              << browser_created_count_
              << " destroyed=" << browser_destroyed_count_;
#endif

    // Verify expected browsers were created and destroyed.
    EXPECT_EQ(expected_browser_count_, browser_created_count_);
    EXPECT_EQ(expected_browser_count_, browser_destroyed_count_);

    // Clear all callbacks that hold references to handler or delegate.
    handler_->ClearCallbacks();

    // Verify that after clearing callbacks, the handler only has one reference
    // left (the one held by handler_). This confirms we properly cleaned up
    // all circular references.
    // Note: Chrome-style browsers may hold additional references, so we only
    // check this for Alloy-style windows.
    if (window_style_ == CEF_RUNTIME_STYLE_ALLOY) {
      EXPECT_TRUE(handler_->HasOneRef());
    }

    event_->Signal();
  }

  cef_runtime_style_t GetWindowRuntimeStyle() override { return window_style_; }

  CefSize GetPreferredSize(CefRefPtr<CefView> view) override {
    return CefSize(800, 600);
  }

  // CefBrowserViewDelegate methods:
  void OnBrowserCreated(CefRefPtr<CefBrowserView> browser_view,
                        CefRefPtr<CefBrowser> browser) override {
    browser_created_count_++;
#if VERBOSE_DEBUGGING
    LOG(INFO) << "OnBrowserCreated: count=" << browser_created_count_;
#endif

    // Notify TestHandler about window creation for this browser.
    handler_->OnWindowCreated(browser->GetIdentifier());
  }

  void OnBrowserDestroyed(CefRefPtr<CefBrowserView> browser_view,
                          CefRefPtr<CefBrowser> browser) override {
    browser_destroyed_count_++;
#if VERBOSE_DEBUGGING
    LOG(INFO) << "OnBrowserDestroyed: count=" << browser_destroyed_count_;
#endif

    // Notify TestHandler about window destruction for this browser.
    handler_->OnWindowDestroyed(browser->GetIdentifier());
  }

  CefRefPtr<CefWindow> window() const { return window_; }

 protected:
  CefRefPtr<Handler> handler_;
  CefRefPtr<CefWaitableEvent> event_;
  CefRefPtr<CefWindow> window_;

  const cef_runtime_style_t window_style_;
  const int expected_browser_count_;

  int browser_created_count_ = 0;
  int browser_destroyed_count_ = 0;
};

// Test handler for single BrowserView.
class SingleBrowserViewTestHandler : public BrowserViewTestHandlerBase {
 public:
  SingleBrowserViewTestHandler() : BrowserViewTestHandlerBase(1) {}

  SingleBrowserViewTestHandler(const SingleBrowserViewTestHandler&) = delete;
  SingleBrowserViewTestHandler& operator=(const SingleBrowserViewTestHandler&) =
      delete;

  void AddResources() {
    // Can't do this from the constructor because it uses PostTask internally.
    AddResource(kTestUrl1, kTestContent1, "text/html");
  }

 private:
  IMPLEMENT_REFCOUNTING(SingleBrowserViewTestHandler);
};

// Window and BrowserView delegate for single BrowserView.
class SingleBrowserViewDelegate
    : public BrowserViewDelegateBase<SingleBrowserViewTestHandler> {
 public:
  SingleBrowserViewDelegate(CefRefPtr<SingleBrowserViewTestHandler> handler,
                            CefRefPtr<CefWaitableEvent> event,
                            cef_runtime_style_t window_style,
                            cef_runtime_style_t browser_style,
                            bool browser_as_child)
      : BrowserViewDelegateBase(handler, event, window_style, 1),
        browser_style_(browser_style),
        browser_as_child_(browser_as_child) {}

  SingleBrowserViewDelegate(const SingleBrowserViewDelegate&) = delete;
  SingleBrowserViewDelegate& operator=(const SingleBrowserViewDelegate&) =
      delete;

  // CefWindowDelegate methods:
  void OnWindowCreated(CefRefPtr<CefWindow> window) override {
    window_ = window;

    CefBrowserSettings settings;

    // Create browser view.
    browser_view_ = CreateBrowserView(handler_, kTestUrl1, settings, this);

    // Add browser view based on configuration.
    if (browser_as_child_) {
      // Add as child view.
      window->AddChildView(browser_view_);
    } else {
      // Add as overlay.
      overlay_controller_ = window->AddOverlayView(
          browser_view_, CEF_DOCKING_MODE_CUSTOM, /*can_activate=*/true);
      EXPECT_TRUE(overlay_controller_.get());

      CefRect bounds;
      bounds.x = 50;
      bounds.y = 50;
      bounds.width = 300;
      bounds.height = 200;
      overlay_controller_->SetBounds(bounds);
      overlay_controller_->SetVisible(true);
    }

    window->Show();
  }

  bool CanClose(CefRefPtr<CefWindow> window) override {
#if VERBOSE_DEBUGGING
    LOG(INFO) << "CanClose called";
#endif
    if (browser_view_) {
      auto browser = browser_view_->GetBrowser();
      if (browser) {
        bool can_close = browser->GetHost()->TryCloseBrowser();
#if VERBOSE_DEBUGGING
        LOG(INFO) << "  Browser: TryCloseBrowser=" << can_close;
#endif
        if (can_close) {
          // If browser is ready to close and it's in an overlay, destroy the
          // overlay controller and release the BrowserView reference
          // immediately so the browser can actually close.
          if (overlay_controller_ && overlay_controller_->IsValid()) {
#if VERBOSE_DEBUGGING
            LOG(INFO) << "  Destroying overlay controller";
#endif
            overlay_controller_->Destroy();
            overlay_controller_ = nullptr;
          }
          browser_view_ = nullptr;
        }
#if VERBOSE_DEBUGGING
        LOG(INFO) << "CanClose returning: " << can_close;
#endif
        return can_close;
      }
    }

#if VERBOSE_DEBUGGING
    LOG(INFO) << "CanClose returning: true (no browser)";
#endif
    return true;
  }

  // CefBrowserViewDelegate methods:
  cef_runtime_style_t GetBrowserRuntimeStyle() override {
    return browser_style_;
  }

  void OnBrowserDestroyed(CefRefPtr<CefBrowserView> browser_view,
                          CefRefPtr<CefBrowser> browser) override {
    // Call base class implementation.
    BrowserViewDelegateBase::OnBrowserDestroyed(browser_view, browser);

    // Clean up reference when browser is destroyed.
    if (browser_view_ && browser_view_->IsSame(browser_view)) {
      browser_view_ = nullptr;
    }

    // After browser is destroyed, close the window.
    if (window_) {
#if VERBOSE_DEBUGGING
      LOG(INFO) << "Browser destroyed - closing window";
#endif
      window_->Close();
    }
  }

 private:
  CefRefPtr<CefBrowserView> browser_view_;
  CefRefPtr<CefOverlayController> overlay_controller_;

  const cef_runtime_style_t browser_style_;
  const bool browser_as_child_;

  IMPLEMENT_REFCOUNTING(SingleBrowserViewDelegate);
};

// Common implementation for testing single BrowserView configurations.
void SingleBrowserViewTestImpl(CefRefPtr<CefWaitableEvent> event,
                               cef_runtime_style_t window_style,
                               cef_runtime_style_t browser_style,
                               bool browser_as_child) {
  CefRefPtr<SingleBrowserViewTestHandler> test_handler =
      new SingleBrowserViewTestHandler();
  test_handler->AddResources();

  CefRefPtr<SingleBrowserViewDelegate> delegate = new SingleBrowserViewDelegate(
      test_handler, event, window_style, browser_style, browser_as_child);

  // Set up completion callback (called on success or timeout).
  test_handler->SetOnComplete(
      base::BindOnce([](CefRefPtr<SingleBrowserViewDelegate> delegate,
                        bool success) { delegate->window()->Close(); },
                     delegate));

  CefWindow::CreateTopLevelWindow(delegate);
}

// Single BrowserView test implementations.
void AlloyWindowOneBrowserViewImpl(CefRefPtr<CefWaitableEvent> event) {
  SingleBrowserViewTestImpl(event, CEF_RUNTIME_STYLE_ALLOY,
                            CEF_RUNTIME_STYLE_ALLOY, true);
}

void ChromeWindowOneBrowserViewImpl(CefRefPtr<CefWaitableEvent> event) {
  SingleBrowserViewTestImpl(event, CEF_RUNTIME_STYLE_CHROME,
                            CEF_RUNTIME_STYLE_CHROME, true);
}

void AlloyWindowOneAlloyOverlayImpl(CefRefPtr<CefWaitableEvent> event) {
  SingleBrowserViewTestImpl(event, CEF_RUNTIME_STYLE_ALLOY,
                            CEF_RUNTIME_STYLE_ALLOY, false);
}

void ChromeWindowOneAlloyOverlayImpl(CefRefPtr<CefWaitableEvent> event) {
  SingleBrowserViewTestImpl(event, CEF_RUNTIME_STYLE_CHROME,
                            CEF_RUNTIME_STYLE_ALLOY, false);
}

// Test handler for dynamic BrowserView movement.
class DynamicBrowserViewTestHandler : public BrowserViewTestHandlerBase {
 public:
  DynamicBrowserViewTestHandler() : BrowserViewTestHandlerBase(1) {}

  DynamicBrowserViewTestHandler(const DynamicBrowserViewTestHandler&) = delete;
  DynamicBrowserViewTestHandler& operator=(
      const DynamicBrowserViewTestHandler&) = delete;

  void AddResources() {
    // Can't do this from the constructor because it uses PostTask internally.
    AddResource(kTestUrl1, kTestContent1, "text/html");
  }

 private:
  IMPLEMENT_REFCOUNTING(DynamicBrowserViewTestHandler);
};

// Window and BrowserView delegate for dynamic BrowserView movement test.
class DynamicBrowserViewDelegate
    : public BrowserViewDelegateBase<DynamicBrowserViewTestHandler> {
 public:
  DynamicBrowserViewDelegate(CefRefPtr<DynamicBrowserViewTestHandler> handler,
                             CefRefPtr<CefWaitableEvent> event,
                             cef_runtime_style_t window_style,
                             cef_runtime_style_t browser_style)
      : BrowserViewDelegateBase(handler, event, window_style, 1),
        browser_style_(browser_style) {}

  DynamicBrowserViewDelegate(const DynamicBrowserViewDelegate&) = delete;
  DynamicBrowserViewDelegate& operator=(const DynamicBrowserViewDelegate&) =
      delete;

  // CefWindowDelegate methods:
  void OnWindowCreated(CefRefPtr<CefWindow> window) override {
    window_ = window;

    CefBrowserSettings settings;

    // Create browser view and start as child.
    browser_view_ = CreateBrowserView(handler_, kTestUrl1, settings, this);
    window->AddChildView(browser_view_);

    window->Show();
  }

  bool CanClose(CefRefPtr<CefWindow> window) override {
#if VERBOSE_DEBUGGING
    LOG(INFO) << "CanClose called";
#endif
    if (browser_view_) {
      auto browser = browser_view_->GetBrowser();
      if (browser) {
        bool can_close = browser->GetHost()->TryCloseBrowser();
#if VERBOSE_DEBUGGING
        LOG(INFO) << "  Browser: TryCloseBrowser=" << can_close;
#endif
        if (can_close) {
          // Clean up overlay if present.
          if (overlay_controller_ && overlay_controller_->IsValid()) {
#if VERBOSE_DEBUGGING
            LOG(INFO) << "  Destroying overlay controller";
#endif
            overlay_controller_->Destroy();
            overlay_controller_ = nullptr;
          }
          browser_view_ = nullptr;
        }
#if VERBOSE_DEBUGGING
        LOG(INFO) << "CanClose returning: " << can_close;
#endif
        return can_close;
      }
    }

#if VERBOSE_DEBUGGING
    LOG(INFO) << "CanClose returning: true (no browser)";
#endif
    return true;
  }

  // CefBrowserViewDelegate methods:
  cef_runtime_style_t GetBrowserRuntimeStyle() override {
    return browser_style_;
  }

  void OnBrowserDestroyed(CefRefPtr<CefBrowserView> browser_view,
                          CefRefPtr<CefBrowser> browser) override {
    // Call base class implementation.
    BrowserViewDelegateBase::OnBrowserDestroyed(browser_view, browser);

    // Verify we completed all transitions.
    EXPECT_EQ(3, transition_count_);

    // Clean up reference when browser is destroyed.
    if (browser_view_ && browser_view_->IsSame(browser_view)) {
      browser_view_ = nullptr;
    }

    // After browser is destroyed, close the window.
    if (window_) {
#if VERBOSE_DEBUGGING
      LOG(INFO) << "Browser destroyed - closing window";
#endif
      window_->Close();
    }
  }

  // Called after each page load to trigger the next transition.
  void OnLoadComplete() {
    transition_count_++;
#if VERBOSE_DEBUGGING
    LOG(INFO) << "OnLoadComplete: transition " << transition_count_;
#endif

    if (transition_count_ == 1) {
      // Transition 1: Move from child to overlay.
      MoveToOverlay();

      // Trigger next transition.
      CefPostTask(
          TID_UI,
          base::BindOnce(&DynamicBrowserViewDelegate::OnLoadComplete, this));
    } else if (transition_count_ == 2) {
      // Transition 2: Move from overlay back to child.
      MoveToChild();

      // Trigger final transition.
      CefPostTask(
          TID_UI,
          base::BindOnce(&DynamicBrowserViewDelegate::OnLoadComplete, this));
    } else if (transition_count_ == 3) {
      // Transition 3: All done, close the window.
#if VERBOSE_DEBUGGING
      LOG(INFO) << "All transitions complete - closing";
#endif
      window_->Close();
    }
  }

  void MoveToOverlay() {
#if VERBOSE_DEBUGGING
    LOG(INFO) << "Moving BrowserView from child to overlay";
#endif
    EXPECT_TRUE(browser_view_);
    EXPECT_FALSE(overlay_controller_);

    // Remove from child views.
    window_->RemoveChildView(browser_view_);

    // We hold the only reference to the BrowserView.
    DCHECK(browser_view_->HasOneRef());

    // Add as overlay.
    overlay_controller_ = window_->AddOverlayView(
        browser_view_, CEF_DOCKING_MODE_CUSTOM, /*can_activate=*/true);
    EXPECT_TRUE(overlay_controller_.get());

    CefRect bounds;
    bounds.x = 50;
    bounds.y = 50;
    bounds.width = 300;
    bounds.height = 200;
    overlay_controller_->SetBounds(bounds);
    overlay_controller_->SetVisible(true);
  }

  void MoveToChild() {
#if VERBOSE_DEBUGGING
    LOG(INFO) << "Moving BrowserView from overlay to child";
#endif
    EXPECT_TRUE(browser_view_);
    EXPECT_TRUE(overlay_controller_);

    // Destroy the overlay controller (this detaches the view).
    overlay_controller_->Destroy();
    overlay_controller_ = nullptr;

    // We hold the only reference to the BrowserView.
    DCHECK(browser_view_->HasOneRef());

    // The view is now detached. Add it back as a child view.
    window_->AddChildView(browser_view_);
  }

 private:
  CefRefPtr<CefBrowserView> browser_view_;
  CefRefPtr<CefOverlayController> overlay_controller_;

  const cef_runtime_style_t browser_style_;

  int transition_count_ = 0;

  IMPLEMENT_REFCOUNTING(DynamicBrowserViewDelegate);
};

// Common implementation for testing dynamic BrowserView movement.
void DynamicBrowserViewTestImpl(CefRefPtr<CefWaitableEvent> event,
                                cef_runtime_style_t window_style,
                                cef_runtime_style_t browser_style) {
  CefRefPtr<DynamicBrowserViewTestHandler> test_handler =
      new DynamicBrowserViewTestHandler();
  test_handler->AddResources();

  CefRefPtr<DynamicBrowserViewDelegate> delegate =
      new DynamicBrowserViewDelegate(test_handler, event, window_style,
                                     browser_style);

  // Set up completion callback (called on success or timeout).
  // Trigger first transition after initial load.
  test_handler->SetOnComplete(base::BindOnce(
      [](CefRefPtr<DynamicBrowserViewDelegate> delegate, bool success) {
        if (success) {
          delegate->OnLoadComplete();
        } else {
          delegate->window()->Close();
        }
      },
      delegate));

  CefWindow::CreateTopLevelWindow(delegate);
}

// Dynamic BrowserView movement test implementations.
void AlloyWindowDynamicBrowserViewImpl(CefRefPtr<CefWaitableEvent> event) {
  DynamicBrowserViewTestImpl(event, CEF_RUNTIME_STYLE_ALLOY,
                             CEF_RUNTIME_STYLE_ALLOY);
}

void ChromeWindowDynamicBrowserViewImpl(CefRefPtr<CefWaitableEvent> event) {
  // Chrome window with Alloy-style BrowserView (required for overlay support).
  DynamicBrowserViewTestImpl(event, CEF_RUNTIME_STYLE_CHROME,
                             CEF_RUNTIME_STYLE_ALLOY);
}

// Test handler for multiple BrowserViews.
class MultipleBrowserViewTestHandler : public BrowserViewTestHandlerBase {
 public:
  MultipleBrowserViewTestHandler() : BrowserViewTestHandlerBase(2) {}

  MultipleBrowserViewTestHandler(const MultipleBrowserViewTestHandler&) =
      delete;
  MultipleBrowserViewTestHandler& operator=(
      const MultipleBrowserViewTestHandler&) = delete;

  void AddResources() {
    // Can't do this from the constructor because it uses PostTask internally.
    AddResource(kTestUrl1, kTestContent1, "text/html");
    AddResource(kTestUrl2, kTestContent2, "text/html");
  }

 private:
  IMPLEMENT_REFCOUNTING(MultipleBrowserViewTestHandler);
};

// Window and BrowserView delegate for multiple BrowserViews.
class MultipleBrowserViewDelegate
    : public BrowserViewDelegateBase<MultipleBrowserViewTestHandler> {
 public:
  MultipleBrowserViewDelegate(CefRefPtr<MultipleBrowserViewTestHandler> handler,
                              CefRefPtr<CefWaitableEvent> event,
                              cef_runtime_style_t window_style,
                              cef_runtime_style_t browser1_style,
                              cef_runtime_style_t browser2_style,
                              bool browser1_as_child,
                              bool browser2_as_child)
      : BrowserViewDelegateBase(handler, event, window_style, 2),
        browser1_style_(browser1_style),
        browser2_style_(browser2_style),
        browser1_as_child_(browser1_as_child),
        browser2_as_child_(browser2_as_child) {}

  MultipleBrowserViewDelegate(const MultipleBrowserViewDelegate&) = delete;
  MultipleBrowserViewDelegate& operator=(const MultipleBrowserViewDelegate&) =
      delete;

  // CefWindowDelegate methods:
  void OnWindowCreated(CefRefPtr<CefWindow> window) override {
    window_ = window;

    CefBrowserSettings settings;

    // Create first browser view.
    next_browser_style_ = browser1_style_;
    browser_view1_ = CreateBrowserView(handler_, kTestUrl1, settings, this);

    // Create second browser view.
    next_browser_style_ = browser2_style_;
    browser_view2_ = CreateBrowserView(handler_, kTestUrl2, settings, this);

    // Add browser views based on configuration.
    if (browser1_as_child_ && browser2_as_child_) {
      // Both as child views (side by side).
      CefBoxLayoutSettings layout_settings;
      layout_settings.horizontal = true;
      window->SetToBoxLayout(layout_settings);

      window->AddChildView(browser_view1_);
      window->AddChildView(browser_view2_);

      auto layout = window->GetLayout()->AsBoxLayout();
      layout->SetFlexForView(browser_view1_, 1);
      layout->SetFlexForView(browser_view2_, 1);
    } else if (browser1_as_child_ && !browser2_as_child_) {
      // Browser 1 as child, browser 2 as overlay.
      window->AddChildView(browser_view1_);

      overlay_controller2_ = window->AddOverlayView(
          browser_view2_, CEF_DOCKING_MODE_CUSTOM, /*can_activate=*/true);
      EXPECT_TRUE(overlay_controller2_.get());

      CefRect bounds;
      bounds.x = 450;
      bounds.y = 350;
      bounds.width = 300;
      bounds.height = 200;
      overlay_controller2_->SetBounds(bounds);
      overlay_controller2_->SetVisible(true);
    } else {
      // Both as overlays.
      overlay_controller1_ = window->AddOverlayView(
          browser_view1_, CEF_DOCKING_MODE_CUSTOM, /*can_activate=*/true);
      EXPECT_TRUE(overlay_controller1_.get());

      CefRect bounds1;
      bounds1.x = 0;
      bounds1.y = 0;
      bounds1.width = 400;
      bounds1.height = 300;
      overlay_controller1_->SetBounds(bounds1);
      overlay_controller1_->SetVisible(true);

      overlay_controller2_ = window->AddOverlayView(
          browser_view2_, CEF_DOCKING_MODE_CUSTOM, /*can_activate=*/true);
      EXPECT_TRUE(overlay_controller2_.get());

      CefRect bounds2;
      bounds2.x = 400;
      bounds2.y = 300;
      bounds2.width = 400;
      bounds2.height = 300;
      overlay_controller2_->SetBounds(bounds2);
      overlay_controller2_->SetVisible(true);
    }

    window->Show();
  }

  bool CanClose(CefRefPtr<CefWindow> window) override {
#if VERBOSE_DEBUGGING
    LOG(INFO) << "CanClose called";
#endif
    // Try to close all browsers. TryCloseBrowser() returns false if the
    // browser isn't ready to close (and starts the close process), or true
    // if the browser is ready to close.
    bool all_can_close = true;

    if (browser_view1_) {
      auto browser = browser_view1_->GetBrowser();
      if (browser) {
        bool can_close = browser->GetHost()->TryCloseBrowser();
#if VERBOSE_DEBUGGING
        LOG(INFO) << "  Browser1: TryCloseBrowser=" << can_close;
#endif
        if (can_close) {
          // If browser is ready to close and it's in an overlay, destroy the
          // overlay controller and release the BrowserView reference
          // immediately so the browser can actually close.
          auto overlay = GetAndClearOverlayForBrowserView(browser_view1_);
          if (overlay) {
#if VERBOSE_DEBUGGING
            LOG(INFO) << "  Destroying overlay controller for Browser1";
#endif
            overlay->Destroy();
            browser_view1_ = nullptr;
          }
        } else {
          all_can_close = false;
        }
      }
    }

    if (browser_view2_) {
      auto browser = browser_view2_->GetBrowser();
      if (browser) {
        bool can_close = browser->GetHost()->TryCloseBrowser();
#if VERBOSE_DEBUGGING
        LOG(INFO) << "  Browser2: TryCloseBrowser=" << can_close;
#endif
        if (can_close) {
          // If browser is ready to close and it's in an overlay, destroy the
          // overlay controller and release the BrowserView reference
          // immediately so the browser can actually close.
          auto overlay = GetAndClearOverlayForBrowserView(browser_view2_);
          if (overlay) {
#if VERBOSE_DEBUGGING
            LOG(INFO) << "  Destroying overlay controller for Browser2";
#endif
            overlay->Destroy();
            browser_view2_ = nullptr;
          }
        } else {
          all_can_close = false;
        }
      }
    }

    // If all browsers are ready to close, release any remaining BrowserView
    // references (child views).
    if (all_can_close) {
#if VERBOSE_DEBUGGING
      LOG(INFO) << "All browsers ready - releasing remaining BrowserView "
                   "references";
#endif
      browser_view1_ = nullptr;
      browser_view2_ = nullptr;
    }

#if VERBOSE_DEBUGGING
    LOG(INFO) << "CanClose returning: " << all_can_close;
#endif
    return all_can_close;
  }

  // CefBrowserViewDelegate methods:
  cef_runtime_style_t GetBrowserRuntimeStyle() override {
    return next_browser_style_;
  }

  void OnBrowserDestroyed(CefRefPtr<CefBrowserView> browser_view,
                          CefRefPtr<CefBrowser> browser) override {
    // Call base class implementation.
    BrowserViewDelegateBase::OnBrowserDestroyed(browser_view, browser);

    // Clean up references when browsers are destroyed.
    if (browser_view1_ && browser_view1_->IsSame(browser_view)) {
      browser_view1_ = nullptr;
    }
    if (browser_view2_ && browser_view2_->IsSame(browser_view)) {
      browser_view2_ = nullptr;
    }

    // After each browser is destroyed, try closing the window again.
    // This ensures CanClose is called to check remaining browsers.
    if (window_) {
#if VERBOSE_DEBUGGING
      if (browser_destroyed_count_ >= browser_created_count_) {
        LOG(INFO) << "All browsers destroyed - closing window";
      } else {
        LOG(INFO) << "Browser destroyed (" << browser_destroyed_count_ << "/"
                  << browser_created_count_ << ") - closing window";
      }
#endif
      window_->Close();
    }
  }

 private:
  // Get and clear the overlay controller for a specific browser view.
  // Returns the controller if found, and clears the stored reference.
  CefRefPtr<CefOverlayController> GetAndClearOverlayForBrowserView(
      CefRefPtr<CefBrowserView> browser_view) {
    // Check if overlay_controller1_ contains this browser view.
    if (overlay_controller1_ && overlay_controller1_->IsValid()) {
      auto view = overlay_controller1_->GetContentsView();
      if (view && view->IsSame(browser_view)) {
        auto controller = overlay_controller1_;
        overlay_controller1_ = nullptr;
        return controller;
      }
    }
    // Check if overlay_controller2_ contains this browser view.
    if (overlay_controller2_ && overlay_controller2_->IsValid()) {
      auto view = overlay_controller2_->GetContentsView();
      if (view && view->IsSame(browser_view)) {
        auto controller = overlay_controller2_;
        overlay_controller2_ = nullptr;
        return controller;
      }
    }
    return nullptr;
  }

  CefRefPtr<CefBrowserView> browser_view1_;
  CefRefPtr<CefBrowserView> browser_view2_;
  CefRefPtr<CefOverlayController> overlay_controller1_;
  CefRefPtr<CefOverlayController> overlay_controller2_;

  const cef_runtime_style_t browser1_style_;
  const cef_runtime_style_t browser2_style_;
  const bool browser1_as_child_;
  const bool browser2_as_child_;
  cef_runtime_style_t next_browser_style_ = CEF_RUNTIME_STYLE_ALLOY;

 private:
  IMPLEMENT_REFCOUNTING(MultipleBrowserViewDelegate);
};

// Common implementation for testing various BrowserView configurations.
// Parameters control window style, browser styles, and layout.
void BrowserViewTestImpl(CefRefPtr<CefWaitableEvent> event,
                         cef_runtime_style_t window_style,
                         cef_runtime_style_t browser1_style,
                         cef_runtime_style_t browser2_style,
                         bool browser1_as_child,  // false = overlay
                         bool browser2_as_child) {
  CefRefPtr<MultipleBrowserViewTestHandler> test_handler =
      new MultipleBrowserViewTestHandler();
  test_handler->AddResources();

  CefRefPtr<MultipleBrowserViewDelegate> delegate =
      new MultipleBrowserViewDelegate(test_handler, event, window_style,
                                      browser1_style, browser2_style,
                                      browser1_as_child, browser2_as_child);

  // Set up completion callback (called on success or timeout).
  test_handler->SetOnComplete(
      base::BindOnce([](CefRefPtr<MultipleBrowserViewDelegate> delegate,
                        bool success) { delegate->window()->Close(); },
                     delegate));

  CefWindow::CreateTopLevelWindow(delegate);
}

// Alloy-style test implementations.
void AlloyWindowTwoBrowserViewImpl(CefRefPtr<CefWaitableEvent> event) {
  BrowserViewTestImpl(event, CEF_RUNTIME_STYLE_ALLOY, CEF_RUNTIME_STYLE_ALLOY,
                      CEF_RUNTIME_STYLE_ALLOY, true, true);
}

void AlloyWindowOneBrowserViewOneAlloyOverlayImpl(
    CefRefPtr<CefWaitableEvent> event) {
  BrowserViewTestImpl(event, CEF_RUNTIME_STYLE_ALLOY, CEF_RUNTIME_STYLE_ALLOY,
                      CEF_RUNTIME_STYLE_ALLOY, true, false);
}

void AlloyWindowTwoAlloyOverlayImpl(CefRefPtr<CefWaitableEvent> event) {
  BrowserViewTestImpl(event, CEF_RUNTIME_STYLE_ALLOY, CEF_RUNTIME_STYLE_ALLOY,
                      CEF_RUNTIME_STYLE_ALLOY, false, false);
}

// Chrome-style test implementations.
void ChromeWindowOneBrowserViewOneAlloyOverlayImpl(
    CefRefPtr<CefWaitableEvent> event) {
  BrowserViewTestImpl(event, CEF_RUNTIME_STYLE_CHROME, CEF_RUNTIME_STYLE_CHROME,
                      CEF_RUNTIME_STYLE_ALLOY, true, false);
}

void ChromeWindowTwoAlloyOverlayImpl(CefRefPtr<CefWaitableEvent> event) {
  BrowserViewTestImpl(event, CEF_RUNTIME_STYLE_CHROME, CEF_RUNTIME_STYLE_ALLOY,
                      CEF_RUNTIME_STYLE_ALLOY, false, false);
}

}  // namespace

// Test single BrowserView with different runtime style combinations.
//
// Alloy-style tests:
// - Single Alloy child BrowserView
// - Single Alloy overlay (no child)
BROWSER_VIEW_TEST_ASYNC(AlloyWindowOneBrowserView)
BROWSER_VIEW_TEST_ASYNC(AlloyWindowOneAlloyOverlay)

// Chrome-style tests:
// - Single Chrome child BrowserView
// - Single Chrome window with Alloy overlay (no child)
BROWSER_VIEW_TEST_ASYNC(ChromeWindowOneBrowserView)
BROWSER_VIEW_TEST_ASYNC(ChromeWindowOneAlloyOverlay)

// Test dynamic BrowserView movement between child and overlay.
//
// Alloy-style test:
// - Alloy BrowserView starts as child, moves to overlay, moves back to child
BROWSER_VIEW_TEST_ASYNC(AlloyWindowDynamicBrowserView)

// Chrome-style test:
// - Chrome window with Alloy BrowserView (overlays require Alloy style)
// - BrowserView starts as child, moves to overlay, moves back to child
BROWSER_VIEW_TEST_ASYNC(ChromeWindowDynamicBrowserView)

// Test multiple BrowserViews with different runtime style combinations.
//
// Alloy-style tests:
// - Multiple Alloy child BrowserViews are supported (side-by-side layout)
// - Multiple Alloy overlays are supported
// - Mix of Alloy child and Alloy overlay is supported
BROWSER_VIEW_TEST_ASYNC(AlloyWindowTwoBrowserView)
BROWSER_VIEW_TEST_ASYNC(AlloyWindowOneBrowserViewOneAlloyOverlay)
BROWSER_VIEW_TEST_ASYNC(AlloyWindowTwoAlloyOverlay)

// Chrome-style tests:
// - Chrome-style windows can have at most one Chrome-style child BrowserView
// - Overlays are always Alloy-style (Chrome overlays not supported)
// - Chrome child + Alloy overlay(s) is supported
// - Chrome window with only Alloy overlay(s) is supported (no child)
BROWSER_VIEW_TEST_ASYNC(ChromeWindowOneBrowserViewOneAlloyOverlay)
BROWSER_VIEW_TEST_ASYNC(ChromeWindowTwoAlloyOverlay)
