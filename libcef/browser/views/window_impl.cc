// Copyright 2016 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "libcef/browser/views/window_impl.h"

#include "libcef/browser/browser_util.h"
#include "libcef/browser/thread_util.h"
#include "libcef/browser/views/display_impl.h"
#include "libcef/browser/views/fill_layout_impl.h"
#include "libcef/browser/views/layout_util.h"
#include "libcef/browser/views/view_util.h"
#include "libcef/browser/views/window_view.h"

#include "ui/base/test/ui_controls.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/menu/menu_runner.h"

#if defined(USE_AURA)
#include "ui/aura/test/ui_controls_factory_aura.h"
#include "ui/aura/window.h"
#include "ui/base/test/ui_controls_aura.h"
#if defined(OS_LINUX)
#include "ui/views/test/ui_controls_factory_desktop_aurax11.h"
#endif
#endif

#if defined(OS_WIN)
#include "ui/display/win/screen_win.h"
#endif

namespace {

// Based on chrome/test/base/interactive_ui_tests_main.cc.
void InitializeUITesting() {
  static bool initialized = false;
  if (!initialized) {
    ui_controls::EnableUIControls();

#if defined(USE_AURA)
#if defined(OS_LINUX)
    ui_controls::InstallUIControlsAura(
        views::test::CreateUIControlsDesktopAura());
#else
    ui_controls::InstallUIControlsAura(aura::test::CreateUIControlsAura(NULL));
#endif
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
  CefUnhandledKeyEventHandler(CefWindowImpl* window_impl,
                              views::Widget* widget)
    : window_impl_(window_impl),
      widget_(widget),
      window_(widget->GetNativeWindow()) {
    DCHECK(window_);
    window_->AddPostTargetHandler(this);
  }

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

  DISALLOW_COPY_AND_ASSIGN(CefUnhandledKeyEventHandler);
};

#endif  // defined(USE_AURA)

}  // namespace

// static
CefRefPtr<CefWindow> CefWindow::CreateTopLevelWindow(
    CefRefPtr<CefWindowDelegate> delegate) {
  return CefWindowImpl::Create(delegate);
}

// static
CefRefPtr<CefWindowImpl> CefWindowImpl::Create(
    CefRefPtr<CefWindowDelegate> delegate) {
  CEF_REQUIRE_UIT_RETURN(nullptr);
  CefRefPtr<CefWindowImpl> window = new CefWindowImpl(delegate);
  window->Initialize();
  window->CreateWidget();
  if (delegate)
    delegate->OnWindowCreated(window.get());
  return window;
}

void CefWindowImpl::Show() {
  CEF_REQUIRE_VALID_RETURN_VOID();
  if (widget_)
    widget_->Show();
}

void CefWindowImpl::Hide() {
  CEF_REQUIRE_VALID_RETURN_VOID();
  if (widget_)
    widget_->Hide();
}

void CefWindowImpl::CenterWindow(const CefSize& size) {
  CEF_REQUIRE_VALID_RETURN_VOID();
  if (widget_)
    widget_->CenterWindow(gfx::Size(size.width, size.height));
}

void CefWindowImpl::Close() {
  CEF_REQUIRE_VALID_RETURN_VOID();
  if (widget_ && !widget_->IsClosed())
    widget_->Close();
}

bool CefWindowImpl::IsClosed() {
  CEF_REQUIRE_UIT_RETURN(false);
  return destroyed_ || (widget_ && widget_->IsClosed());
}

void CefWindowImpl::Activate() {
  CEF_REQUIRE_VALID_RETURN_VOID();
  if (widget_ && widget_->CanActivate() && !widget_->IsActive())
    widget_->Activate();
}

void CefWindowImpl::Deactivate() {
  CEF_REQUIRE_VALID_RETURN_VOID();
  if (widget_&& widget_->CanActivate() && widget_->IsActive())
    widget_->Deactivate();
}

bool CefWindowImpl::IsActive() {
  CEF_REQUIRE_VALID_RETURN(false);
  if (widget_)
    return widget_->IsActive();
  return false;
}

void CefWindowImpl::BringToTop() {
  CEF_REQUIRE_VALID_RETURN_VOID();
  if (widget_)
    widget_->StackAtTop();
}

void CefWindowImpl::SetAlwaysOnTop(bool on_top) {
  CEF_REQUIRE_VALID_RETURN_VOID();
  if (widget_ && on_top != widget_->IsAlwaysOnTop())
    widget_->SetAlwaysOnTop(on_top);
}

bool CefWindowImpl::IsAlwaysOnTop() {
  CEF_REQUIRE_VALID_RETURN(false);
  if (widget_)
    return widget_->IsAlwaysOnTop();
  return false;
}

void CefWindowImpl::Maximize() {
  CEF_REQUIRE_VALID_RETURN_VOID();
  if (widget_ && !widget_->IsMaximized())
    widget_->Maximize();
}

void CefWindowImpl::Minimize() {
  CEF_REQUIRE_VALID_RETURN_VOID();
  if (widget_ && !widget_->IsMinimized())
    widget_->Minimize();
}

void CefWindowImpl::Restore() {
  CEF_REQUIRE_VALID_RETURN_VOID();
  if (widget_ && (widget_->IsMaximized() || widget_->IsMinimized()))
    widget_->Restore();
}

void CefWindowImpl::SetFullscreen(bool fullscreen) {
  CEF_REQUIRE_VALID_RETURN_VOID();
  if (widget_ && fullscreen != widget_->IsFullscreen())
    widget_->SetFullscreen(fullscreen);
}

bool CefWindowImpl::IsMaximized() {
  CEF_REQUIRE_VALID_RETURN(false);
  if (widget_)
    return widget_->IsMaximized();
  return false;
}

bool CefWindowImpl::IsMinimized() {
  CEF_REQUIRE_VALID_RETURN(false);
  if (widget_)
    return widget_->IsMinimized();
  return false;
}

bool CefWindowImpl::IsFullscreen() {
  CEF_REQUIRE_VALID_RETURN(false);
  if (widget_)
    return widget_->IsFullscreen();
  return false;
}

void CefWindowImpl::SetTitle(const CefString& title) {
  CEF_REQUIRE_VALID_RETURN_VOID();
  if (root_view())
    root_view()->SetTitle(title);
}

CefString CefWindowImpl::GetTitle() {
  CEF_REQUIRE_VALID_RETURN(CefString());
  if (root_view())
    return root_view()->title();
  return CefString();
}

void CefWindowImpl::SetWindowIcon(CefRefPtr<CefImage> image) {
  CEF_REQUIRE_VALID_RETURN_VOID();
  if (root_view())
    root_view()->SetWindowIcon(image);
}

CefRefPtr<CefImage> CefWindowImpl::GetWindowIcon() {
  CEF_REQUIRE_VALID_RETURN(nullptr);
  if (root_view())
    return root_view()->window_icon();
  return nullptr;
}

void CefWindowImpl::SetWindowAppIcon(CefRefPtr<CefImage> image) {
  CEF_REQUIRE_VALID_RETURN_VOID();
  if (root_view())
    root_view()->SetWindowAppIcon(image);
}

CefRefPtr<CefImage> CefWindowImpl::GetWindowAppIcon() {
  CEF_REQUIRE_VALID_RETURN(nullptr);
  if (root_view())
    return root_view()->window_app_icon();
  return nullptr;
}

void CefWindowImpl::GetDebugInfo(base::DictionaryValue* info,
                                 bool include_children) {
  ParentClass::GetDebugInfo(info, include_children);
  if (root_view())
    info->SetString("title", root_view()->title());
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
  if (widget_)
    bounds = widget_->GetWindowBoundsInScreen();
  return CefRect(bounds.x(), bounds.y(), bounds.width(), bounds.height());
}

CefRect CefWindowImpl::GetBoundsInScreen() {
  return GetBounds();
}

void CefWindowImpl::SetSize(const CefSize& size) {
  CEF_REQUIRE_VALID_RETURN_VOID();
  if (widget_)
    widget_->SetSize(gfx::Size(size.width, size.height));
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
    if (widget_->non_client_view())
      widget_->SetSize(widget_->non_client_view()->GetPreferredSize());
    else
      widget_->SetSize(root_view()->GetPreferredSize());
  }
}

void CefWindowImpl::SetVisible(bool visible) {
  if (visible)
    Show();
  else
    Hide();
}

bool CefWindowImpl::IsVisible() {
  CEF_REQUIRE_VALID_RETURN(false);
  if (widget_)
    return widget_->IsVisible();
  return false;
}

bool CefWindowImpl::IsDrawn() {
  return IsVisible();
}

void CefWindowImpl::SetBackgroundColor(cef_color_t color) {
  CEF_REQUIRE_VALID_RETURN_VOID();
  ParentClass::SetBackgroundColor(color);
  if (widget_ && widget_->GetCompositor())
    widget_->GetCompositor()->SetBackgroundColor(color);
}

bool CefWindowImpl::CanWidgetClose() {
  if (delegate())
    return delegate()->CanClose(this);
  return true;
}

void CefWindowImpl::OnWindowClosing() {
#if defined(USE_AURA)
  unhandled_key_event_handler_.reset();
#endif
}

void CefWindowImpl::OnWindowViewDeleted() {
  CancelMenu();

  destroyed_ = true;
  widget_ = nullptr;

  if (delegate())
    delegate()->OnWindowDestroyed(this);

  // Call Detach() here instead of waiting for the root View to be deleted so
  // that any following attempts to call CefWindow methods from the delegate
  // will fail.
  Detach();
}

// Will only be called if CanHandleAccelerators() returns true.
bool CefWindowImpl::AcceleratorPressed(const ui::Accelerator& accelerator) {
  for (const auto& entry : accelerator_map_) {
    if (entry.second == accelerator)
      return delegate()->OnAccelerator(this, entry.first);
  }
  return false;
}

bool CefWindowImpl::CanHandleAccelerators() const {
  if (delegate() && widget_)
    return widget_->IsActive();
  return false;
}

bool CefWindowImpl::OnKeyEvent(const CefKeyEvent& event) {
  if (delegate())
    return delegate()->OnKeyEvent(this, event);
  return false;
}

void CefWindowImpl::ShowMenu(views::MenuButton* menu_button,
                             CefRefPtr<CefMenuModel> menu_model,
                             const CefPoint& screen_point,
                             cef_menu_anchor_position_t anchor_position) {
  CancelMenu();

  if (!widget_)
    return;
  
  CefMenuModelImpl* menu_model_impl =
      static_cast<CefMenuModelImpl*>(menu_model.get());
  if (!menu_model_impl || !menu_model_impl->model())
    return;

  menu_model_ = menu_model_impl;

  // We'll send the MenuClosed notification manually for better accuracy.
  menu_model_->set_auto_notify_menu_closed(false);

  menu_runner_.reset(
      new views::MenuRunner(menu_model_impl->model(),
                            views::MenuRunner::ASYNC |
                            (menu_button ? views::MenuRunner::HAS_MNEMONICS :
                                           views::MenuRunner::CONTEXT_MENU),
                            base::Bind(&CefWindowImpl::MenuClosed, this)));

  views::MenuRunner::RunResult result = menu_runner_->RunMenuAt(
      widget_,
      menu_button,
      gfx::Rect(gfx::Point(screen_point.x, screen_point.y), gfx::Size()),
      static_cast<views::MenuAnchorPosition>(anchor_position),
      ui::MENU_SOURCE_NONE);
  ALLOW_UNUSED_LOCAL(result);
}

void CefWindowImpl::MenuClosed() {
  menu_model_->NotifyMenuClosed();
  menu_model_ = nullptr;
  menu_runner_.reset(nullptr);
}

void CefWindowImpl::CancelMenu() {
  CEF_REQUIRE_VALID_RETURN_VOID();
  if (menu_runner_)
    menu_runner_->Cancel();
  DCHECK(!menu_model_);
  DCHECK(!menu_runner_);
}

CefRefPtr<CefDisplay> CefWindowImpl::GetDisplay() {
  CEF_REQUIRE_VALID_RETURN(nullptr);
  if (widget_ && root_view()) {
    const display::Display& display = root_view()->GetDisplay();
    if (display.is_valid())
      return new CefDisplayImpl(display);
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
  if (root_view())
    root_view()->SetDraggableRegions(regions);
}

CefWindowHandle CefWindowImpl::GetWindowHandle() {
  CEF_REQUIRE_VALID_RETURN(kNullWindowHandle);
  return view_util::GetWindowHandle(widget_);
}

void CefWindowImpl::SendKeyPress(int key_code,
                                 uint32 event_flags) {
  CEF_REQUIRE_VALID_RETURN_VOID();
  InitializeUITesting();

  gfx::NativeWindow native_window = view_util::GetNativeWindow(widget_);
  if (!native_window)
    return;

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

  gfx::Point point(screen_x, screen_y);
#if defined(OS_WIN)
  // Windows expects pixel coordinates.
  point = display::win::ScreenWin::DIPToScreenPoint(point);
#endif

  ui_controls::SendMouseMove(point.x(), point.y());
}

void CefWindowImpl::SendMouseEvents(cef_mouse_button_type_t button,
                                    bool mouse_down,
                                    bool mouse_up) {
  CEF_REQUIRE_VALID_RETURN_VOID();
  if (!mouse_down && !mouse_up)
    return;

  InitializeUITesting();

  ui_controls::MouseButton type = ui_controls::LEFT;
  if (button == MBT_MIDDLE)
    type = ui_controls::MIDDLE;
  else if (button == MBT_RIGHT)
    type = ui_controls::RIGHT;

  int state = 0;
  if (mouse_down)
    state |= ui_controls::DOWN;
  if (mouse_up)
    state |= ui_controls::UP;

  ui_controls::SendMouseEvents(type, state);
}

void CefWindowImpl::SetAccelerator(int command_id,
                                   int key_code,
                                   bool shift_pressed,
                                   bool ctrl_pressed,
                                   bool alt_pressed) {
  CEF_REQUIRE_VALID_RETURN_VOID();
  if (!widget_)
    return;

  AcceleratorMap::const_iterator it = accelerator_map_.find(command_id);
  if (it != accelerator_map_.end())
    RemoveAccelerator(command_id);

  int modifiers = 0;
  if (shift_pressed)
    modifiers |= ui::EF_SHIFT_DOWN;
  if (ctrl_pressed)
    modifiers |= ui::EF_CONTROL_DOWN;
  if (alt_pressed)
    modifiers |= ui::EF_ALT_DOWN;
  ui::Accelerator accelerator(static_cast<ui::KeyboardCode>(key_code),
                              modifiers);

  accelerator_map_.insert(std::make_pair(command_id, accelerator));

  views::FocusManager* focus_manager = widget_->GetFocusManager();
  DCHECK(focus_manager);
  focus_manager->RegisterAccelerator(
      accelerator, ui::AcceleratorManager::kNormalPriority, this);
}

void CefWindowImpl::RemoveAccelerator(int command_id) {
  CEF_REQUIRE_VALID_RETURN_VOID();
  if (!widget_)
    return;

  AcceleratorMap::iterator it = accelerator_map_.find(command_id);
  if (it == accelerator_map_.end())
    return;

  ui::Accelerator accelerator = it->second;

  accelerator_map_.erase(it);

  views::FocusManager* focus_manager = widget_->GetFocusManager();
  DCHECK(focus_manager);
  focus_manager->UnregisterAccelerator(accelerator, this);
}

void CefWindowImpl::RemoveAllAccelerators() {
  CEF_REQUIRE_VALID_RETURN_VOID();
  if (!widget_)
    return;

  accelerator_map_.clear();

  views::FocusManager* focus_manager = widget_->GetFocusManager();
  DCHECK(focus_manager);
  focus_manager->UnregisterAccelerators(this);
}

CefWindowImpl::CefWindowImpl(CefRefPtr<CefWindowDelegate> delegate)
    : ParentClass(delegate),
      widget_(nullptr),
      destroyed_(false) {
}

CefWindowView* CefWindowImpl::CreateRootView() {
  return new CefWindowView(delegate(), this);
}

void CefWindowImpl::InitializeRootView() {
  static_cast<CefWindowView*>(root_view())->Initialize();
}

void CefWindowImpl::CreateWidget() {
  DCHECK(!widget_);

  root_view()->CreateWidget();
  widget_ = root_view()->GetWidget();
  DCHECK(widget_);

#if defined(USE_AURA)
  unhandled_key_event_handler_ =
      base::MakeUnique<CefUnhandledKeyEventHandler>(this, widget_);
#endif

  // The Widget and root View are owned by the native window. Therefore don't
  // keep an owned reference.
  std::unique_ptr<views::View> view_ptr = view_util::PassOwnership(this);
  views::View* view = view_ptr.release();
  ALLOW_UNUSED_LOCAL(view);
}
