// Copyright (c) 2024 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFCLIENT_BROWSER_VIEWS_OVERLAY_BROWSER_H_
#define CEF_TESTS_CEFCLIENT_BROWSER_VIEWS_OVERLAY_BROWSER_H_
#pragma once

#include "include/base/cef_weak_ptr.h"
#include "include/views/cef_browser_view.h"
#include "include/views/cef_browser_view_delegate.h"
#include "include/views/cef_overlay_controller.h"

namespace client {

class ViewsWindow;

// Implements a browser view that receives absolute positioning on top of the
// main browser view. All methods must be called on the browser process UI
// thread.
class ViewsOverlayBrowser : public CefBrowserViewDelegate {
 public:
  explicit ViewsOverlayBrowser(ViewsWindow* owner_window);

  void Initialize(CefRefPtr<CefWindow> window,
                  CefRefPtr<CefClient> client,
                  const std::string& url,
                  const CefBrowserSettings& settings,
                  CefRefPtr<CefRequestContext> request_context);
  void Destroy();

  bool IsValid() const;

  // Move the overlay BrowserView to/from a popout Window.
  void ToggleBrowserView();

  // Update browser bounds.
  void UpdateBounds(CefInsets insets);

  // Exclude all regions obscured by overlays.
  void UpdateDraggableRegions(std::vector<CefDraggableRegion>& window_regions);

  bool OnAccelerator(CefRefPtr<CefWindow> window, int command_id);

  void PopOutWindowDestroyed();

 private:
  // CefViewDelegate methods:
  CefSize GetMinimumSize(CefRefPtr<CefView> view) override;

  // CefBrowserViewDelegate methods:
  void OnBrowserDestroyed(CefRefPtr<CefBrowserView> browser_view,
                          CefRefPtr<CefBrowser> browser) override;
  CefRefPtr<CefBrowserViewDelegate> GetDelegateForPopupBrowserView(
      CefRefPtr<CefBrowserView> browser_view,
      const CefBrowserSettings& settings,
      CefRefPtr<CefClient> client,
      bool is_devtools) override;
  bool OnPopupBrowserViewCreated(CefRefPtr<CefBrowserView> browser_view,
                                 CefRefPtr<CefBrowserView> popup_browser_view,
                                 bool is_devtools) override;
  cef_runtime_style_t GetBrowserRuntimeStyle() override;

  // Move the BrowserView to a new top-level Window.
  void PopOutBrowserView();

  // Return the BrowserView to the overlay.
  void PopInBrowserView();

  ViewsWindow* const owner_window_;
  CefRefPtr<CefWindow> window_;
  CefRefPtr<CefBrowserView> browser_view_;
  CefRefPtr<CefOverlayController> controller_;
  CefInsets last_insets_;

  CefRefPtr<CefWindow> popout_window_;

  base::WeakPtrFactory<ViewsOverlayBrowser> weak_ptr_factory_{this};

  IMPLEMENT_REFCOUNTING(ViewsOverlayBrowser);
  DISALLOW_COPY_AND_ASSIGN(ViewsOverlayBrowser);
};

}  // namespace client

#endif  // CEF_TESTS_CEFCLIENT_BROWSER_VIEWS_OVERLAY_BROWSER_H_
