// Copyright 2016 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "libcef/browser/views/window_impl.h"

#include <memory>

#include "libcef/browser/browser_util.h"
#include "libcef/browser/chrome/views/chrome_browser_frame.h"
#include "libcef/browser/thread_util.h"
#include "libcef/browser/views/browser_view_impl.h"
#include "libcef/browser/views/display_impl.h"
#include "libcef/browser/views/fill_layout_impl.h"
#include "libcef/browser/views/layout_util.h"
#include "libcef/browser/views/view_util.h"
#include "libcef/browser/views/window_view.h"
#include "libcef/features/runtime.h"

#include "base/i18n/rtl.h"
#include "components/constrained_window/constrained_window_views.h"
#include "ui/base/test/ui_controls.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/webview/webview.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#endif  // defined(USE_AURA)

#if BUILDFLAG(IS_WIN)
#include "ui/aura/test/ui_controls_aurawin.h"
#include "ui/display/win/screen_win.h"
#endif

namespace {

// Based on chrome/test/base/interactive_ui_tests_main.cc.
void InitializeUITesting() {
  static bool initialized = false;
  if (!initialized) {
#if BUILDFLAG(IS_WIN)
    aura::test::EnableUIControlsAuraWin();
#else
    ui_controls::EnableUIControls();
#endif

    initialized = true;
  }
}

#if defined(USE_AURA)

// This class forwards KeyEvents to the CefWindowImpl associated with a widget.
// This allows KeyEvents to be processed after all other targets.
// Events originating from CefBrowserView will instead be delivered via
// CefBrowserViewImpl::HandleKeyboardEvent.
class CefUnhandledKeyEventHandler : public ui::EventHandler {
 public:
  CefUnhandledKeyEventHandler(CefWindowImpl* window_impl, views::Widget* widget)
      : window_impl_(window_impl),
        widget_(widget),
        window_(widget->GetNativeWindow()) {
    DCHECK(window_);
    window_->AddPostTargetHandler(this);
  }

  CefUnhandledKeyEventHandler(const CefUnhandledKeyEventHandler&) = delete;
  CefUnhandledKeyEventHandler& operator=(const CefUnhandledKeyEventHandler&) =
      delete;

  ~CefUnhandledKeyEventHandler() override {
    window_->RemovePostTargetHandler(this);
  }

  // Implementation of ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override {
    // Give the FocusManager a chance to handle accelerators first.
    // Widget::OnKeyEvent would normally call this after all EventHandlers have
    // had a shot but we don't want to wait.
    if (widget_->GetFocusManager() &&
        !widget_->GetFocusManager()->OnKeyEvent(*event)) {
      event->StopPropagation();
      return;
    }

    CefKeyEvent cef_event;
    if (browser_util::GetCefKeyEvent(*event, cef_event) &&
        window_impl_->OnKeyEvent(cef_event)) {
      event->StopPropagation();
    }
  }

 private:
  // Members are guaranteed to outlive this object.
  CefWindowImpl* window_impl_;
  views::Widget* widget_;

  // |window_| is the event target that is associated with this class.
  aura::Window* window_;
};

#endif  // defined(USE_AURA)

}  // namespace

// static
CefRefPtr<CefWindow> CefWindow::CreateTopLevelWindow(
    CefRefPtr<CefWindowDelegate> delegate) {
  return CefWindowImpl::Create(delegate, gfx::kNullAcceleratedWidget);
}

// static
CefRefPtr<CefWindowImpl> CefWindowImpl::Create(
    CefRefPtr<CefWindowDelegate> delegate,
    gfx::AcceleratedWidget parent_widget) {
  CEF_REQUIRE_UIT_RETURN(nullptr);
  CefRefPtr<CefWindowImpl> window = new CefWindowImpl(delegate);
  window->Initialize();
  window->CreateWidget(parent_widget);
  return window;
}

void CefWindowImpl::Show() {
  CEF_REQUIRE_VALID_RETURN_VOID();
  if (widget_) {
    shown_as_browser_modal_ = false;
    widget_->Show();
  }
}

void CefWindowImpl::ShowAsBrowserModalDialog(
    CefRefPtr<CefBrowserView> browser_view) {
  CEF_REQUIRE_VALID_RETURN_VOID();
  if (widget_) {
    auto* browser_view_impl =
        static_cast<CefBrowserViewImpl*>(browser_view.get());

    // |browser_view| must belong to the host widget.
    auto* host_widget = cef_window_view()->host_widget();
    CHECK(host_widget &&
          browser_view_impl->root_view()->GetWidget() == host_widget);

    if (auto web_view = browser_view_impl->web_view()) {
      if (auto web_contents = web_view->web_contents()) {
        shown_as_browser_modal_ = true;
        constrained_window::ShowModalDialog(widget_->GetNativeWindow(),
                                            web_contents);

        // NativeWebContentsModalDialogManagerViews::ManageDialog() disables
        // movement. That has no impact on native frames but interferes with
        // draggable regions.
        widget_->set_movement_disabled(false);
      }
    }
  }
}

void CefWindowImpl::Hide() {
  CEF_REQUIRE_VALID_RETURN_VOID();
  if (widget_) {
    widget_->Hide();
  }
}

void CefWindowImpl::CenterWindow(const CefSize& size) {
  CEF_REQUIRE_VALID_RETURN_VOID();
  if (widget_) {
    widget_->CenterWindow(gfx::Size(size.width, size.height));
  }
}

void CefWindowImpl::Close() {
  CEF_REQUIRE_VALID_RETURN_VOID();
  if (widget_ && !widget_->IsClosed()) {
    widget_->Close();
  }
}

bool CefWindowImpl::IsClosed() {
  CEF_REQUIRE_UIT_RETURN(false);
  return destroyed_ || (widget_ && widget_->IsClosed());
}

void CefWindowImpl::Activate() {
  CEF_REQUIRE_VALID_RETURN_VOID();
  if (widget_ && widget_->CanActivate() && !widget_->IsActive()) {
    widget_->Activate();
  }
}

void CefWindowImpl::Deactivate() {
  CEF_REQUIRE_VALID_RETURN_VOID();
  if (widget_ && widget_->CanActivate() && widget_->IsActive()) {
    widget_->Deactivate();
  }
}

bool CefWindowImpl::IsActive() {
  CEF_REQUIRE_VALID_RETURN(false);
  if (widget_) {
    return widget_->IsActive();
  }
  return false;
}

void CefWindowImpl::BringToTop() {
  CEF_REQUIRE_VALID_RETURN_VOID();
  if (widget_) {
    widget_->StackAtTop();
  }
}

void CefWindowImpl::SetAlwaysOnTop(bool on_top) {
  CEF_REQUIRE_VALID_RETURN_VOID();
  if (widget_ && on_top != (widget_->GetZOrderLevel() ==
                            ui::ZOrderLevel::kFloatingWindow)) {
    widget_->SetZOrderLevel(on_top ? ui::ZOrderLevel::kFloatingWindow
                                   : ui::ZOrderLevel::kNormal);
  }
}

bool CefWindowImpl::IsAlwaysOnTop() {
  CEF_REQUIRE_VALID_RETURN(false);
  if (widget_) {
    return widget_->GetZOrderLevel() == ui::ZOrderLevel::kFloatingWindow;
  }
  return false;
}

void CefWindowImpl::Maximize() {
  CEF_REQUIRE_VALID_RETURN_VOID();
  if (widget_ && !widget_->IsMaximized()) {
    widget_->Maximize();
  }
}

void CefWindowImpl::Minimize() {
  CEF_REQUIRE_VALID_RETURN_VOID();
  if (widget_ && !widget_->IsMinimized()) {
    widget_->Minimize();
  }
}

void CefWindowImpl::Restore() {
  CEF_REQUIRE_VALID_RETURN_VOID();
  if (widget_ && (widget_->IsMaximized() || widget_->IsMinimized())) {
    widget_->Restore();
  }
}

void CefWindowImpl::SetFullscreen(bool fullscreen) {
  CEF_REQUIRE_VALID_RETURN_VOID();
  if (widget_ && fullscreen != widget_->IsFullscreen()) {
    if (cef::IsChromeRuntimeEnabled()) {
      // If a BrowserView exists, toggle fullscreen mode via the Chrome command
      // for consistent behavior.
      auto* browser_frame = static_cast<ChromeBrowserFrame*>(widget_);
      if (browser_frame->browser_view()) {
        browser_frame->ToggleFullscreenMode();
        return;
      }
    }

    // Call the Widget method directly with Alloy runtime, or Chrome runtime
    // when no BrowserView exists.
    widget_->SetFullscreen(fullscreen);

    // Use a synchronous callback notification on Windows/Linux. Chrome runtime
    // on Windows/Linux gets notified synchronously via ChromeBrowserDelegate
    // callbacks when a BrowserView exists. MacOS (both runtimes) gets notified
    // asynchronously via CefNativeWidgetMac callbacks.
#if !BUILDFLAG(IS_MAC)
    if (delegate()) {
      delegate()->OnWindowFullscreenTransition(this, /*is_completed=*/true);
    }
#endif
  }
}

bool CefWindowImpl::IsMaximized() {
  CEF_REQUIRE_VALID_RETURN(false);
  if (widget_) {
    return widget_->IsMaximized();
  }
  return false;
}

bool CefWindowImpl::IsMinimized() {
  CEF_REQUIRE_VALID_RETURN(false);
  if (widget_) {
    return widget_->IsMinimized();
  }
  return false;
}

bool CefWindowImpl::IsFullscreen() {
  CEF_REQUIRE_VALID_RETURN(false);
  if (widget_) {
    return widget_->IsFullscreen();
  }
  return false;
}

void CefWindowImpl::SetTitle(const CefString& title) {
  CEF_REQUIRE_VALID_RETURN_VOID();
  if (root_view()) {
    root_view()->SetTitle(title);
  }
}

CefString CefWindowImpl::GetTitle() {
  CEF_REQUIRE_VALID_RETURN(CefString());
  if (root_view()) {
    return root_view()->title();
  }
  return CefString();
}

void CefWindowImpl::SetWindowIcon(CefRefPtr<CefImage> image) {
  CEF_REQUIRE_VALID_RETURN_VOID();
  if (root_view()) {
    root_view()->SetWindowIcon(image);
  }
}

CefRefPtr<CefImage> CefWindowImpl::GetWindowIcon() {
  CEF_REQUIRE_VALID_RETURN(nullptr);
  if (root_view()) {
    return root_view()->window_icon();
  }
  return nullptr;
}

void CefWindowImpl::SetWindowAppIcon(CefRefPtr<CefImage> image) {
  CEF_REQUIRE_VALID_RETURN_VOID();
  if (root_view()) {
    root_view()->SetWindowAppIcon(image);
  }
}

CefRefPtr<CefImage> CefWindowImpl::GetWindowAppIcon() {
  CEF_REQUIRE_VALID_RETURN(nullptr);
  if (root_view()) {
    return root_view()->window_app_icon();
  }
  return nullptr;
}

CefRefPtr<CefOverlayController> CefWindowImpl::AddOverlayView(
    CefRefPtr<CefView> view,
    cef_docking_mode_t docking_mode,
    bool can_activate) {
  CEF_REQUIRE_VALID_RETURN(nullptr);
  if (root_view()) {
    return root_view()->AddOverlayView(view, docking_mode, can_activate);
  }
  return nullptr;
}

void CefWindowImpl::GetDebugInfo(base::Value::Dict* info,
                                 bool include_children) {
  ParentClass::GetDebugInfo(info, include_children);
  if (root_view()) {
    info->Set("title", root_view()->title());
  }
}

void CefWindowImpl::ShowMenu(CefRefPtr<CefMenuModel> menu_model,
                             const CefPoint& screen_point,
                             cef_menu_anchor_position_t anchor_position) {
  ShowMenu(nullptr, menu_model, screen_point, anchor_position);
}

void CefWindowImpl::Detach() {
  // OnDeleteDelegate should always be called before Detach().
  DCHECK(!widget_);

  ParentClass::Detach();
}

void CefWindowImpl::SetBounds(const CefRect& bounds) {
  CEF_REQUIRE_VALID_RETURN_VOID();
  if (widget_) {
    widget_->SetBounds(
        gfx::Rect(bounds.x, bounds.y, bounds.width, bounds.height));
  }
}

CefRect CefWindowImpl::GetBounds() {
  CEF_REQUIRE_VALID_RETURN(CefRect());
  gfx::Rect bounds;
  if (widget_) {
    bounds = widget_->GetWindowBoundsInScreen();
  }
  return CefRect(bounds.x(), bounds.y(), bounds.width(), bounds.height());
}

CefRect CefWindowImpl::GetBoundsInScreen() {
  return GetBounds();
}

void CefWindowImpl::SetSize(const CefSize& size) {
  CEF_REQUIRE_VALID_RETURN_VOID();
  if (widget_) {
    widget_->SetSize(gfx::Size(size.width, size.height));
  }
}

void CefWindowImpl::SetPosition(const CefPoint& position) {
  CEF_REQUIRE_VALID_RETURN_VOID();
  if (widget_) {
    gfx::Rect bounds = widget_->GetWindowBoundsInScreen();
    bounds.set_origin(gfx::Point(position.x, position.y));
    widget_->SetBounds(bounds);
  }
}

void CefWindowImpl::SizeToPreferredSize() {
  CEF_REQUIRE_VALID_RETURN_VOID();
  if (widget_) {
    if (widget_->non_client_view()) {
      widget_->SetSize(widget_->non_client_view()->GetPreferredSize());
    } else {
      widget_->SetSize(root_view()->GetPreferredSize());
    }
  }
}

void CefWindowImpl::SetVisible(bool visible) {
  if (visible) {
    Show();
  } else {
    Hide();
  }
}

bool CefWindowImpl::IsVisible() {
  CEF_REQUIRE_VALID_RETURN(false);
  if (widget_) {
    return widget_->IsVisible();
  }
  return false;
}

bool CefWindowImpl::IsDrawn() {
  return IsVisible();
}

void CefWindowImpl::SetBackgroundColor(cef_color_t color) {
  CEF_REQUIRE_VALID_RETURN_VOID();
  ParentClass::SetBackgroundColor(color);
  if (widget_ && widget_->GetCompositor()) {
    widget_->GetCompositor()->SetBackgroundColor(color);
  }
}

bool CefWindowImpl::CanWidgetClose() {
  if (shown_as_browser_modal_) {
    // Always allow the close for browser modal dialogs to avoid an infinite
    // loop in WebContentsModalDialogManager::CloseAllDialogs().
    return true;
  }
  if (delegate()) {
    return delegate()->CanClose(this);
  }
  return true;
}

void CefWindowImpl::OnWindowClosing() {
#if defined(USE_AURA)
  unhandled_key_event_handler_.reset();
#endif

  if (delegate()) {
    delegate()->OnWindowClosing(this);
  }
}

void CefWindowImpl::OnWindowViewDeleted() {
  CancelMenu();

  destroyed_ = true;
  widget_ = nullptr;

  if (delegate()) {
    delegate()->OnWindowDestroyed(this);
  }

  // Call Detach() here instead of waiting for the root View to be deleted so
  // that any following attempts to call CefWindow methods from the delegate
  // will fail.
  Detach();
}

// Will only be called if CanHandleAccelerators() returns true.
bool CefWindowImpl::AcceleratorPressed(const ui::Accelerator& accelerator) {
  for (const auto& entry : accelerator_map_) {
    if (entry.second == accelerator) {
      return delegate()->OnAccelerator(this, entry.first);
    }
  }
  return false;
}

bool CefWindowImpl::CanHandleAccelerators() const {
  if (delegate() && widget_) {
    return widget_->IsActive();
  }
  return false;
}

bool CefWindowImpl::OnKeyEvent(const CefKeyEvent& event) {
  if (delegate()) {
    return delegate()->OnKeyEvent(this, event);
  }
  return false;
}

void CefWindowImpl::ShowMenu(views::MenuButton* menu_button,
                             CefRefPtr<CefMenuModel> menu_model,
                             const CefPoint& screen_point,
                             cef_menu_anchor_position_t anchor_position) {
  CancelMenu();

  if (!widget_) {
    return;
  }

  CefMenuModelImpl* menu_model_impl =
      static_cast<CefMenuModelImpl*>(menu_model.get());
  if (!menu_model_impl || !menu_model_impl->model()) {
    return;
  }

  menu_model_ = menu_model_impl;

  // We'll send the MenuClosed notification manually for better accuracy.
  menu_model_->set_auto_notify_menu_closed(false);

  menu_runner_ = std::make_unique<views::MenuRunner>(
      menu_model_impl->model(),
      menu_button ? views::MenuRunner::HAS_MNEMONICS
                  : views::MenuRunner::CONTEXT_MENU,
      base::BindRepeating(&CefWindowImpl::MenuClosed, this));

  menu_runner_->RunMenuAt(
      widget_, menu_button ? menu_button->button_controller() : nullptr,
      gfx::Rect(gfx::Point(screen_point.x, screen_point.y), gfx::Size()),
      static_cast<views::MenuAnchorPosition>(anchor_position),
      ui::MENU_SOURCE_NONE);
}

void CefWindowImpl::MenuClosed() {
  menu_model_->NotifyMenuClosed();
  menu_model_ = nullptr;
  menu_runner_.reset(nullptr);
}

void CefWindowImpl::CancelMenu() {
  CEF_REQUIRE_VALID_RETURN_VOID();
  if (menu_runner_) {
    menu_runner_->Cancel();
  }
  DCHECK(!menu_model_);
  DCHECK(!menu_runner_);
}

CefRefPtr<CefDisplay> CefWindowImpl::GetDisplay() {
  CEF_REQUIRE_VALID_RETURN(nullptr);
  if (widget_ && root_view()) {
    const display::Display& display = root_view()->GetDisplay();
    if (display.is_valid()) {
      return new CefDisplayImpl(display);
    }
  }
  return nullptr;
}

CefRect CefWindowImpl::GetClientAreaBoundsInScreen() {
  CEF_REQUIRE_VALID_RETURN(CefRect());
  if (widget_) {
    gfx::Rect bounds = widget_->GetClientAreaBoundsInScreen();

    views::NonClientFrameView* non_client_frame_view =
        root_view()->GetNonClientFrameView();
    if (non_client_frame_view) {
      // When using a custom drawn NonClientFrameView the native Window will not
      // know the actual client bounds. Adjust the native Window bounds for the
      // reported client bounds.
      const gfx::Rect& client_bounds =
          non_client_frame_view->GetBoundsForClientView();
      bounds.set_origin(bounds.origin() + client_bounds.OffsetFromOrigin());
      bounds.set_size(client_bounds.size());
    }

    return CefRect(bounds.x(), bounds.y(), bounds.width(), bounds.height());
  }
  return CefRect();
}

void CefWindowImpl::SetDraggableRegions(
    const std::vector<CefDraggableRegion>& regions) {
  CEF_REQUIRE_VALID_RETURN_VOID();
  if (root_view()) {
    root_view()->SetDraggableRegions(regions);
  }
}

CefWindowHandle CefWindowImpl::GetWindowHandle() {
  CEF_REQUIRE_VALID_RETURN(kNullWindowHandle);
  return view_util::GetWindowHandle(widget_);
}

void CefWindowImpl::SendKeyPress(int key_code, uint32_t event_flags) {
  CEF_REQUIRE_VALID_RETURN_VOID();
  InitializeUITesting();

  gfx::NativeWindow native_window = view_util::GetNativeWindow(widget_);
  if (!native_window) {
    return;
  }

  ui_controls::SendKeyPress(native_window,
                            static_cast<ui::KeyboardCode>(key_code),
                            !!(event_flags & EVENTFLAG_CONTROL_DOWN),
                            !!(event_flags & EVENTFLAG_SHIFT_DOWN),
                            !!(event_flags & EVENTFLAG_ALT_DOWN),
                            false);  // Command key is not supported by Aura.
}

void CefWindowImpl::SendMouseMove(int screen_x, int screen_y) {
  CEF_REQUIRE_VALID_RETURN_VOID();
  InitializeUITesting();

  // Converts to pixel coordinates internally on Windows.
  gfx::Point point(screen_x, screen_y);
  ui_controls::SendMouseMove(point.x(), point.y());
}

void CefWindowImpl::SendMouseEvents(cef_mouse_button_type_t button,
                                    bool mouse_down,
                                    bool mouse_up) {
  CEF_REQUIRE_VALID_RETURN_VOID();
  if (!mouse_down && !mouse_up) {
    return;
  }

  InitializeUITesting();

  ui_controls::MouseButton type = ui_controls::LEFT;
  if (button == MBT_MIDDLE) {
    type = ui_controls::MIDDLE;
  } else if (button == MBT_RIGHT) {
    type = ui_controls::RIGHT;
  }

  int state = 0;
  if (mouse_down) {
    state |= ui_controls::DOWN;
  }
  if (mouse_up) {
    state |= ui_controls::UP;
  }

  ui_controls::SendMouseEvents(type, state);
}

void CefWindowImpl::SetAccelerator(int command_id,
                                   int key_code,
                                   bool shift_pressed,
                                   bool ctrl_pressed,
                                   bool alt_pressed,
                                   bool high_priority) {
  CEF_REQUIRE_VALID_RETURN_VOID();
  if (!widget_) {
    return;
  }

  AcceleratorMap::const_iterator it = accelerator_map_.find(command_id);
  if (it != accelerator_map_.end()) {
    RemoveAccelerator(command_id);
  }

  int modifiers = 0;
  if (shift_pressed) {
    modifiers |= ui::EF_SHIFT_DOWN;
  }
  if (ctrl_pressed) {
    modifiers |= ui::EF_CONTROL_DOWN;
  }
  if (alt_pressed) {
    modifiers |= ui::EF_ALT_DOWN;
  }
  ui::Accelerator accelerator(static_cast<ui::KeyboardCode>(key_code),
                              modifiers);

  accelerator_map_.insert(std::make_pair(command_id, accelerator));

  views::FocusManager* focus_manager = widget_->GetFocusManager();
  DCHECK(focus_manager);
  focus_manager->RegisterAccelerator(
      accelerator,
      high_priority ? ui::AcceleratorManager::kHighPriority
                    : ui::AcceleratorManager::kNormalPriority,
      this);
}

void CefWindowImpl::RemoveAccelerator(int command_id) {
  CEF_REQUIRE_VALID_RETURN_VOID();
  if (!widget_) {
    return;
  }

  AcceleratorMap::iterator it = accelerator_map_.find(command_id);
  if (it == accelerator_map_.end()) {
    return;
  }

  ui::Accelerator accelerator = it->second;

  accelerator_map_.erase(it);

  views::FocusManager* focus_manager = widget_->GetFocusManager();
  DCHECK(focus_manager);
  focus_manager->UnregisterAccelerator(accelerator, this);
}

void CefWindowImpl::RemoveAllAccelerators() {
  CEF_REQUIRE_VALID_RETURN_VOID();
  if (!widget_) {
    return;
  }

  accelerator_map_.clear();

  views::FocusManager* focus_manager = widget_->GetFocusManager();
  DCHECK(focus_manager);
  focus_manager->UnregisterAccelerators(this);
}

CefWindowView* CefWindowImpl::cef_window_view() const {
  return static_cast<CefWindowView*>(root_view());
}

CefWindowImpl::CefWindowImpl(CefRefPtr<CefWindowDelegate> delegate)
    : ParentClass(delegate) {}

CefWindowView* CefWindowImpl::CreateRootView() {
  return new CefWindowView(delegate(), this);
}

void CefWindowImpl::InitializeRootView() {
  cef_window_view()->Initialize();
}

void CefWindowImpl::CreateWidget(gfx::AcceleratedWidget parent_widget) {
  DCHECK(!widget_);

  root_view()->CreateWidget(parent_widget);
  widget_ = root_view()->GetWidget();
  DCHECK(widget_);

#if defined(USE_AURA)
  unhandled_key_event_handler_ =
      std::make_unique<CefUnhandledKeyEventHandler>(this, widget_);
#endif

  // The Widget and root View are owned by the native window. Therefore don't
  // keep an owned reference.
  std::unique_ptr<views::View> view_ptr = view_util::PassOwnership(this);
  [[maybe_unused]] views::View* view = view_ptr.release();

  initialized_ = true;

  if (delegate()) {
    delegate()->OnWindowCreated(this);
  }
}
