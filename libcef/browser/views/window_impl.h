// Copyright 2016 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_VIEWS_WINDOW_IMPL_H_
#define CEF_LIBCEF_BROWSER_VIEWS_WINDOW_IMPL_H_
#pragma once

#include <map>

#include "include/views/cef_window.h"
#include "include/views/cef_window_delegate.h"

#include "libcef/browser/menu_model_impl.h"
#include "libcef/browser/views/panel_impl.h"
#include "libcef/browser/views/window_view.h"

#include "ui/base/accelerators/accelerator.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/widget/widget.h"

namespace views {
class MenuButton;
}

class CefWindowImpl
    : public CefPanelImpl<CefWindowView, CefWindow, CefWindowDelegate>,
      public CefWindowView::Delegate,
      public ui::AcceleratorTarget {
 public:
  using ParentClass = CefPanelImpl<CefWindowView, CefWindow, CefWindowDelegate>;

  CefWindowImpl(const CefWindowImpl&) = delete;
  CefWindowImpl& operator=(const CefWindowImpl&) = delete;

  // Create a new CefWindow instance. |delegate| may be nullptr. |parent_widget|
  // will be used when creating a Chrome child window.
  static CefRefPtr<CefWindowImpl> Create(CefRefPtr<CefWindowDelegate> delegate,
                                         gfx::AcceleratedWidget parent_widget);

  // CefWindow methods:
  void Show() override;
  void ShowAsBrowserModalDialog(
      CefRefPtr<CefBrowserView> browser_view) override;
  void Hide() override;
  void CenterWindow(const CefSize& size) override;
  void Close() override;
  bool IsClosed() override;
  void Activate() override;
  void Deactivate() override;
  bool IsActive() override;
  void BringToTop() override;
  void SetAlwaysOnTop(bool on_top) override;
  bool IsAlwaysOnTop() override;
  void Maximize() override;
  void Minimize() override;
  void Restore() override;
  void SetFullscreen(bool fullscreen) override;
  bool IsMaximized() override;
  bool IsMinimized() override;
  bool IsFullscreen() override;
  void SetTitle(const CefString& title) override;
  CefString GetTitle() override;
  void SetWindowIcon(CefRefPtr<CefImage> image) override;
  CefRefPtr<CefImage> GetWindowIcon() override;
  void SetWindowAppIcon(CefRefPtr<CefImage> image) override;
  CefRefPtr<CefImage> GetWindowAppIcon() override;
  CefRefPtr<CefOverlayController> AddOverlayView(
      CefRefPtr<CefView> view,
      cef_docking_mode_t docking_mode,
      bool can_activate) override;
  void ShowMenu(CefRefPtr<CefMenuModel> menu_model,
                const CefPoint& screen_point,
                cef_menu_anchor_position_t anchor_position) override;
  void CancelMenu() override;
  CefRefPtr<CefDisplay> GetDisplay() override;
  CefRect GetClientAreaBoundsInScreen() override;
  void SetDraggableRegions(
      const std::vector<CefDraggableRegion>& regions) override;
  CefWindowHandle GetWindowHandle() override;
  void SendKeyPress(int key_code, uint32_t event_flags) override;
  void SendMouseMove(int screen_x, int screen_y) override;
  void SendMouseEvents(cef_mouse_button_type_t button,
                       bool mouse_down,
                       bool mouse_up) override;
  void SetAccelerator(int command_id,
                      int key_code,
                      bool shift_pressed,
                      bool ctrl_pressed,
                      bool alt_pressed,
                      bool high_priority) override;
  void RemoveAccelerator(int command_id) override;
  void RemoveAllAccelerators() override;

  // CefViewAdapter methods:
  void Detach() override;

  // CefPanel methods:
  CefRefPtr<CefWindow> AsWindow() override { return this; }

  // CefView methods:
  void SetBounds(const CefRect& bounds) override;
  CefRect GetBounds() override;
  CefRect GetBoundsInScreen() override;
  void SetSize(const CefSize& bounds) override;
  void SetPosition(const CefPoint& position) override;
  void SizeToPreferredSize() override;
  void SetVisible(bool visible) override;
  bool IsVisible() override;
  bool IsDrawn() override;
  void SetBackgroundColor(cef_color_t color) override;

  // CefWindowView::Delegate methods:
  bool CanWidgetClose() override;
  void OnWindowClosing() override;
  void OnWindowViewDeleted() override;

  // CefViewAdapter methods:
  std::string GetDebugType() override { return "Window"; }
  void GetDebugInfo(base::Value::Dict* info, bool include_children) override;

  // ui::AcceleratorTarget methods:
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  bool CanHandleAccelerators() const override;

  // Called for key events that have not been handled by other controls in the
  // window. Returns true if the event was handled.
  bool OnKeyEvent(const CefKeyEvent& event);

  void ShowMenu(views::MenuButton* menu_button,
                CefRefPtr<CefMenuModel> menu_model,
                const CefPoint& screen_point,
                cef_menu_anchor_position_t anchor_position);
  void MenuClosed();

  CefWindowView* cef_window_view() const;

  views::Widget* widget() const { return widget_; }
  bool initialized() const { return initialized_; }

 private:
  // Create a new implementation object.
  // Always call Initialize() after creation.
  // |delegate| may be nullptr.
  explicit CefWindowImpl(CefRefPtr<CefWindowDelegate> delegate);

  // CefViewImpl methods:
  CefWindowView* CreateRootView() override;
  void InitializeRootView() override;

  // Initialize the Widget.
  void CreateWidget(gfx::AcceleratedWidget parent_widget);

  views::Widget* widget_ = nullptr;

  // True if the window has been initialized.
  bool initialized_ = false;

  // True if the window has been destroyed.
  bool destroyed_ = false;

  // The currently active menu model and runner.
  CefRefPtr<CefMenuModelImpl> menu_model_;
  std::unique_ptr<views::MenuRunner> menu_runner_;

  // Map of command_id to accelerator.
  using AcceleratorMap = std::map<int, ui::Accelerator>;
  AcceleratorMap accelerator_map_;

#if defined(USE_AURA)
  // Native widget's handler to receive events after the event target.
  std::unique_ptr<ui::EventHandler> unhandled_key_event_handler_;
#endif

  // True if this window was shown using ShowAsBrowserModalDialog().
  bool shown_as_browser_modal_ = false;

  IMPLEMENT_REFCOUNTING_DELETE_ON_UIT(CefWindowImpl);
};

#endif  // CEF_LIBCEF_BROWSER_VIEWS_WINDOW_IMPL_H_
