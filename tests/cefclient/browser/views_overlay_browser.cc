// Copyright (c) 2024 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/cefclient/browser/views_overlay_browser.h"

#include "include/views/cef_window.h"
#include "tests/cefclient/browser/views_window.h"

namespace client {

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

  browser_view_ = CefBrowserView::CreateBrowserView(
      client, url, settings, nullptr, request_context, this);
  CHECK(browser_view_);

  controller_ = window_->AddOverlayView(browser_view_, CEF_DOCKING_MODE_CUSTOM,
                                        /*can_activate=*/true);
  CHECK(controller_);
}

void ViewsOverlayBrowser::Destroy() {
  window_ = nullptr;
  browser_view_ = nullptr;
  controller_->Destroy();
  controller_ = nullptr;
}

void ViewsOverlayBrowser::UpdateBounds(CefInsets insets) {
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

CefSize ViewsOverlayBrowser::GetMinimumSize(CefRefPtr<CefView> view) {
  return CefSize(200, 200);
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
