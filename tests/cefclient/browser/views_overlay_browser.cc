// Copyright (c) 2024 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/cefclient/browser/views_overlay_browser.h"

#include "include/views/cef_window.h"
#include "tests/cefclient/browser/resource.h"
#include "tests/cefclient/browser/views_window.h"

namespace client {

namespace {

void AddPopOutAccelerator(CefRefPtr<CefWindow> window) {
  // Add an accelerator to toggle the BrowserView popout. OnAccelerator will be
  // called when the accelerator is triggered.
  window->SetAccelerator(ID_POPOUT_OVERLAY, 'O', /*shift_pressed=*/true,
                         /*ctrl_pressed=*/true, /*alt_pressed=*/false,
                         /*high_priority=*/true);
}

// Popout window delegate implementation.
class PopoutWindowDelegate : public CefWindowDelegate {
 public:
  PopoutWindowDelegate(base::WeakPtr<ViewsOverlayBrowser> overlay,
                       CefRefPtr<CefBrowserView> browser_view)
      : overlay_(overlay), browser_view_(browser_view) {}

  PopoutWindowDelegate(const PopoutWindowDelegate&) = delete;
  PopoutWindowDelegate& operator=(const PopoutWindowDelegate&) = delete;

  static PopoutWindowDelegate* GetForWindow(CefRefPtr<CefWindow> window) {
    return static_cast<PopoutWindowDelegate*>(window->GetDelegate().get());
  }

  void OnWindowCreated(CefRefPtr<CefWindow> window) override {
    window->AddChildView(browser_view_);
    window->Show();

    // Add the popout accelerator to the popout Window.
    AddPopOutAccelerator(window);

    browser_view_->RequestFocus();
  }

  bool CanClose(CefRefPtr<CefWindow> window) override {
    CefRefPtr<CefBrowser> browser =
        browser_view_ ? browser_view_->GetBrowser() : nullptr;

    if (overlay_ && browser && !browser->GetHost()->IsReadyToBeClosed()) {
      // Proceed with the window close, but don't close the browser. The browser
      // will be returned to the overlay in OnWindowClosing().
      return_to_overlay_ = true;
      return true;
    }

    if (browser) {
      // We must close the browser, either because the popout Window is the
      // final owner of the BrowserView, or because the browser is ready to be
      // closed internally (e.g. `window.close()` was called).
      return browser->GetHost()->TryCloseBrowser();
    }

    return true;
  }

  void OnWindowClosing(CefRefPtr<CefWindow> window) override {
    if (overlay_ && return_to_overlay_) {
      // Give the browser back to the overlay.
      overlay_->ToggleBrowserView();
    }
  }

  void OnWindowDestroyed(CefRefPtr<CefWindow> window) override {
    if (overlay_) {
      overlay_->PopOutWindowDestroyed();
      overlay_ = nullptr;
    }
    browser_view_ = nullptr;
  }

  cef_runtime_style_t GetWindowRuntimeStyle() override {
    return browser_view_->GetRuntimeStyle();
  }

  bool OnAccelerator(CefRefPtr<CefWindow> window, int command_id) override {
    if (overlay_) {
      return overlay_->OnAccelerator(window, command_id);
    }
    return false;
  }

  [[nodiscard]] CefRefPtr<CefBrowserView> DetachBrowserView() {
    overlay_ = nullptr;
    auto browser_view = browser_view_;
    browser_view_ = nullptr;
    return browser_view;
  }

  void OverlayDestroyed() { overlay_ = nullptr; }

 private:
  base::WeakPtr<ViewsOverlayBrowser> overlay_;
  CefRefPtr<CefBrowserView> browser_view_;
  bool return_to_overlay_ = false;

  IMPLEMENT_REFCOUNTING(PopoutWindowDelegate);
};

}  // namespace

ViewsOverlayBrowser::ViewsOverlayBrowser(ViewsWindow* owner_window)
    : owner_window_(owner_window) {}

void ViewsOverlayBrowser::Initialize(
    CefRefPtr<CefWindow> window,
    CefRefPtr<CefClient> client,
    const std::string& url,
    const CefBrowserSettings& settings,
    CefRefPtr<CefRequestContext> request_context) {
  CHECK(!window_);
  window_ = window;
  CHECK(window_);

  // Add the accelerator to the main window.
  AddPopOutAccelerator(window_);

  browser_view_ = CefBrowserView::CreateBrowserView(
      client, url, settings, nullptr, request_context, this);
  CHECK(browser_view_);

  // Add the BrowserView to an overlay in the main window.
  controller_ = window_->AddOverlayView(browser_view_, CEF_DOCKING_MODE_CUSTOM,
                                        /*can_activate=*/true);
  CHECK(controller_);
}

void ViewsOverlayBrowser::Destroy() {
  window_ = nullptr;

  if (popout_window_) {
    // The BrowserView is popped out, and the main Window is closed first.
    // Let the popout Window handle BrowserView destruction.
    PopoutWindowDelegate::GetForWindow(popout_window_)->OverlayDestroyed();
    popout_window_->Close();
    popout_window_ = nullptr;
  }

  if (controller_) {
    if (controller_->IsValid()) {
      controller_->Destroy();
    }
    controller_ = nullptr;
    owner_window_->UpdateDraggableRegions();
  }

  if (browser_view_) {
    // We hold the last reference to the BrowserView, and releasing it will
    // trigger overlay Browser destruction. OnBeforeClose for that Browser may
    // be called synchronously or asynchronously depending on whether
    // beforeunload needs to be dispatched.
    DCHECK(browser_view_->HasOneRef());
    browser_view_ = nullptr;
  }
}

bool ViewsOverlayBrowser::IsValid() const {
  // Intentionally not checking |popout_window_->IsValid()| here because the
  // pop-in behavior will be triggered by |popout_window_| closing.
  return (controller_ && controller_->IsValid()) || popout_window_;
}

void ViewsOverlayBrowser::ToggleBrowserView() {
  if (browser_view_) {
    PopOutBrowserView();
  } else {
    PopInBrowserView();
  }

  owner_window_->UpdateDraggableRegions();
}

void ViewsOverlayBrowser::PopOutBrowserView() {
  CHECK(browser_view_);

  DCHECK(controller_ && controller_->IsValid());
  controller_->Destroy();
  controller_ = nullptr;

  // We hold the only reference to the BrowserView.
  DCHECK(browser_view_->HasOneRef());

  // Create a new popout Window and pass ownership of the BrowserView.
  CHECK(!popout_window_);
  popout_window_ = CefWindow::CreateTopLevelWindow(
      new PopoutWindowDelegate(weak_ptr_factory_.GetWeakPtr(), browser_view_));
  browser_view_ = nullptr;
}

void ViewsOverlayBrowser::PopInBrowserView() {
  CHECK(!browser_view_);

  CefRefPtr<CefView> last_focused_view = window_->GetFocusedView();

  // Resume ownership of the BrowserView and close the popout Window.
  CHECK(popout_window_);
  browser_view_ =
      PopoutWindowDelegate::GetForWindow(popout_window_)->DetachBrowserView();

  const bool should_focus_browser =
      popout_window_->IsActive() && browser_view_->HasFocus();
  popout_window_->RemoveChildView(browser_view_);
  popout_window_->Close();
  popout_window_ = nullptr;

  // We hold the only reference to the BrowserView.
  DCHECK(browser_view_->HasOneRef());

  // Add the BrowserView to an overlay in the main window.
  controller_ = window_->AddOverlayView(browser_view_, CEF_DOCKING_MODE_CUSTOM,
                                        /*can_activate=*/true);
  CHECK(controller_);

  // Make sure the overlay is positioned correctly.
  UpdateBounds(last_insets_);

  if (should_focus_browser) {
    // Keep the BrowserView focused.
    browser_view_->RequestFocus();
  } else if (last_focused_view) {
    // Keep focus unchanged in the main Window.
    last_focused_view->RequestFocus();
  }
}

void ViewsOverlayBrowser::UpdateBounds(CefInsets insets) {
  last_insets_ = insets;

  if (!controller_) {
    return;
  }

  // Update location bar size, position and visibility.
  const auto window_bounds = window_->GetBounds();

  // Client coordinates with insets.
  CefRect bounds;
  bounds.x = insets.left;
  bounds.width = window_bounds.width - insets.left - insets.right;
  bounds.y = insets.top;
  bounds.height = window_bounds.height - insets.top - insets.bottom;

  const auto min_size = browser_view_->GetMinimumSize();
  if (bounds.width < min_size.width || bounds.height < min_size.height) {
    // Not enough space.
    controller_->SetVisible(false);
  } else {
    controller_->SetSize(CefSize(bounds.width, bounds.height));
    controller_->SetBounds(bounds);
    controller_->SetVisible(true);
  }
}

void ViewsOverlayBrowser::UpdateDraggableRegions(
    std::vector<CefDraggableRegion>& window_regions) {
  if (controller_ && controller_->IsVisible()) {
    window_regions.emplace_back(controller_->GetBounds(),
                                /*draggable=*/false);
  }
}

bool ViewsOverlayBrowser::OnAccelerator(CefRefPtr<CefWindow> window,
                                        int command_id) {
  if (IsValid() && command_id == ID_POPOUT_OVERLAY) {
    ToggleBrowserView();
    return true;
  }
  return false;
}

void ViewsOverlayBrowser::PopOutWindowDestroyed() {
  popout_window_ = nullptr;
}

bool ViewsOverlayBrowser::RequestFocus() {
  if (browser_view_) {
    browser_view_->RequestFocus();
    return true;
  }
  return false;
}

CefSize ViewsOverlayBrowser::GetMinimumSize(CefRefPtr<CefView> view) {
  return CefSize(200, 200);
}

void ViewsOverlayBrowser::OnBrowserDestroyed(
    CefRefPtr<CefBrowserView> browser_view,
    CefRefPtr<CefBrowser> browser) {
  // Might be popped out currently.
  if (!controller_) {
    return;
  }

  // Destroy the overlay controller if the browser is destroyed first (e.g. via
  // `window.close()`).
  controller_->Destroy();
  controller_ = nullptr;
  owner_window_->UpdateDraggableRegions();
}

CefRefPtr<CefBrowserViewDelegate>
ViewsOverlayBrowser::GetDelegateForPopupBrowserView(
    CefRefPtr<CefBrowserView> browser_view,
    const CefBrowserSettings& settings,
    CefRefPtr<CefClient> client,
    bool is_devtools) {
  return owner_window_->GetDelegateForPopupBrowserView(browser_view, settings,
                                                       client, is_devtools);
}

bool ViewsOverlayBrowser::OnPopupBrowserViewCreated(
    CefRefPtr<CefBrowserView> browser_view,
    CefRefPtr<CefBrowserView> popup_browser_view,
    bool is_devtools) {
  return owner_window_->OnPopupBrowserViewCreated(
      browser_view, popup_browser_view, is_devtools);
}

cef_runtime_style_t ViewsOverlayBrowser::GetBrowserRuntimeStyle() {
  // Overlay browser view must always be Alloy style.
  return CEF_RUNTIME_STYLE_ALLOY;
}

}  // namespace client
