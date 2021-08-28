// Copyright (c) 2021 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFCLIENT_BROWSER_VIEWS_OVERLAY_CONTROLS_H_
#define CEF_TESTS_CEFCLIENT_BROWSER_VIEWS_OVERLAY_CONTROLS_H_
#pragma once

#include "include/views/cef_button_delegate.h"
#include "include/views/cef_label_button.h"
#include "include/views/cef_menu_button.h"
#include "include/views/cef_overlay_controller.h"
#include "include/views/cef_panel.h"

namespace client {

// Implements window overlay controls that receive absolute positioning on top
// of the browser view. All methods must be called on the browser process UI
// thread.
class ViewsOverlayControls : public CefButtonDelegate {
 public:
  enum class Command {
    kMinimize = 1,
    kMaximize,
    kClose,
  };

  ViewsOverlayControls();

  void Initialize(CefRefPtr<CefWindow> window,
                  CefRefPtr<CefMenuButton> menu_button,
                  CefRefPtr<CefView> location_bar,
                  bool is_chrome_toolbar);
  void Destroy();

  // Update window control button state and location bar bounds.
  void UpdateControls();

  // Exclude all regions obscured by overlays.
  void UpdateDraggableRegions(std::vector<CefDraggableRegion>& window_regions);

 private:
  // CefButtonDelegate methods:
  void OnButtonPressed(CefRefPtr<CefButton> button) override;

  CefRefPtr<CefLabelButton> CreateButton(Command command);

  void MaybeUpdateMaximizeButton();

  CefRefPtr<CefWindow> window_;
  bool window_maximized_;

  // Window control buttons.
  CefRefPtr<CefPanel> panel_;
  CefRefPtr<CefOverlayController> panel_controller_;

  // Location bar.
  CefRefPtr<CefView> location_bar_;
  bool is_chrome_toolbar_;
  CefRefPtr<CefOverlayController> location_controller_;

  // Menu button.
  CefRefPtr<CefOverlayController> menu_controller_;

  IMPLEMENT_REFCOUNTING(ViewsOverlayControls);
  DISALLOW_COPY_AND_ASSIGN(ViewsOverlayControls);
};

}  // namespace client

#endif  // CEF_TESTS_CEFCLIENT_BROWSER_VIEWS_OVERLAY_CONTROLS_H_
