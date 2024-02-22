// Copyright 2016 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "libcef/browser/views/window_view.h"

#include <memory>

#if BUILDFLAG(IS_LINUX)
#include "ui/base/ozone_buildflags.h"
#if BUILDFLAG(IS_OZONE_X11)
// Include first due to redefinition of x11::EventMask.
#include "ui/base/x/x11_util.h"
#endif
#endif

#include "libcef/browser/chrome/views/chrome_browser_frame.h"
#include "libcef/browser/geometry_util.h"
#include "libcef/browser/image_impl.h"
#include "libcef/browser/views/window_impl.h"
#include "libcef/features/runtime.h"

#include "ui/base/hit_test.h"
#include "ui/display/screen.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/native_frame_view.h"

#if BUILDFLAG(IS_LINUX)
#if BUILDFLAG(IS_OZONE_X11)
#include "ui/gfx/x/atom_cache.h"
#include "ui/linux/linux_ui_delegate.h"
#endif
#endif

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

#if BUILDFLAG(IS_WIN)
#include <dwmapi.h>

#include "base/win/windows_version.h"
#include "ui/display/win/screen_win.h"
#include "ui/views/win/hwnd_util.h"
#endif

#if defined(USE_AURA)
#include "ui/aura/window.h"
#endif

namespace {

// Specialize ClientView to handle Widget-related events.
class ClientViewEx : public views::ClientView {
 public:
  ClientViewEx(views::Widget* widget,
               views::View* contents_view,
               CefWindowView::Delegate* window_delegate)
      : views::ClientView(widget, contents_view),
        window_delegate_(window_delegate) {
    DCHECK(window_delegate_);
  }

  ClientViewEx(const ClientViewEx&) = delete;
  ClientViewEx& operator=(const ClientViewEx&) = delete;

  views::CloseRequestResult OnWindowCloseRequested() override {
    return window_delegate_->CanWidgetClose()
               ? views::CloseRequestResult::kCanClose
               : views::CloseRequestResult::kCannotClose;
  }

 private:
  CefWindowView::Delegate* window_delegate_;  // Not owned by this object.
};

// Extend NativeFrameView with draggable region handling.
class NativeFrameViewEx : public views::NativeFrameView {
 public:
  NativeFrameViewEx(views::Widget* widget, CefWindowView* view)
      : views::NativeFrameView(widget), widget_(widget), view_(view) {}

  NativeFrameViewEx(const NativeFrameViewEx&) = delete;
  NativeFrameViewEx& operator=(const NativeFrameViewEx&) = delete;

  gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const override {
#if BUILDFLAG(IS_MAC)
    // From NativeFrameView::GetWindowBoundsForClientBounds:
    gfx::Rect window_bounds = client_bounds;
    // Enforce minimum size (1, 1) in case that |client_bounds| is passed with
    // empty size.
    if (window_bounds.IsEmpty()) {
      window_bounds.set_size(gfx::Size(1, 1));
    }

    if (!view_->IsFrameless()) {
      if (auto titlebar_height = view_->GetTitlebarHeight(/*required=*/true)) {
        window_bounds.Inset(gfx::Insets::TLBR(-(*titlebar_height), 0, 0, 0));
      }
    }

    return window_bounds;
#elif BUILDFLAG(IS_WIN)
    HWND window = views::HWNDForWidget(widget_);
    CHECK(window);

    const DWORD style = GetWindowLong(window, GWL_STYLE);
    const DWORD ex_style = GetWindowLong(window, GWL_EXSTYLE);
    const bool has_menu = !(style & WS_CHILD) && (GetMenu(window) != nullptr);

    // Convert from DIP to pixel coordinates using a method that can handle
    // multiple displays with different DPI.
    const auto screen_rect =
        display::win::ScreenWin::DIPToScreenRect(window, client_bounds);

    RECT rect = {screen_rect.x(), screen_rect.y(),
                 screen_rect.x() + screen_rect.width(),
                 screen_rect.y() + screen_rect.height()};
    AdjustWindowRectEx(&rect, style, has_menu, ex_style);

    // Keep the original origin while potentially increasing the size to include
    // the frame non-client area.
    gfx::Rect pixel_rect(screen_rect.x(), screen_rect.y(),
                         rect.right - rect.left, rect.bottom - rect.top);

    // Convert back to DIP.
    return display::win::ScreenWin::ScreenToDIPRect(window, pixel_rect);
#else
    // Use the default implementation.
    return views::NativeFrameView::GetWindowBoundsForClientBounds(
        client_bounds);
#endif
  }

  int NonClientHitTest(const gfx::Point& point) override {
    if (widget_->IsFullscreen()) {
      return HTCLIENT;
    }

    // Test for mouse clicks that fall within the draggable region.
    SkRegion* draggable_region = view_->draggable_region();
    if (draggable_region && draggable_region->contains(point.x(), point.y())) {
      return HTCAPTION;
    }

    return views::NativeFrameView::NonClientHitTest(point);
  }

#if BUILDFLAG(IS_WIN)
  void OnThemeChanged() override {
    views::NativeFrameView::OnThemeChanged();

    // Value was 19 prior to Windows 10 20H1, according to
    // https://stackoverflow.com/a/70693198
    const DWORD dwAttribute =
        base::win::GetVersion() >= base::win::Version::WIN10_20H1
            ? DWMWA_USE_IMMERSIVE_DARK_MODE
            : 19;

    // From BrowserFrameViewWin::SetSystemMicaTitlebarAttributes:
    const BOOL dark_titlebar_enabled = GetNativeTheme()->ShouldUseDarkColors();
    DwmSetWindowAttribute(views::HWNDForWidget(widget_), dwAttribute,
                          &dark_titlebar_enabled,
                          sizeof(dark_titlebar_enabled));
  }
#endif

 private:
  // Not owned by this object.
  views::Widget* widget_;
  CefWindowView* view_;
};

// The area inside the frame border that can be clicked and dragged for resizing
// the window. Only used in restored mode.
const int kResizeBorderThickness = 4;

// The distance from each window corner that triggers diagonal resizing. Only
// used in restored mode.
const int kResizeAreaCornerSize = 16;

// Implement NonClientFrameView without the system default caption and icon but
// with a resizable border. Based on AppWindowFrameView and CustomFrameView.
class CaptionlessFrameView : public views::NonClientFrameView {
 public:
  CaptionlessFrameView(views::Widget* widget, CefWindowView* view)
      : widget_(widget), view_(view) {}

  CaptionlessFrameView(const CaptionlessFrameView&) = delete;
  CaptionlessFrameView& operator=(const CaptionlessFrameView&) = delete;

  gfx::Rect GetBoundsForClientView() const override {
    return client_view_bounds_;
  }

  gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const override {
    return client_bounds;
  }

  int NonClientHitTest(const gfx::Point& point) override {
    if (widget_->IsFullscreen()) {
      return HTCLIENT;
    }

    // Sanity check.
    if (!bounds().Contains(point)) {
      return HTNOWHERE;
    }

    // Check the frame first, as we allow a small area overlapping the contents
    // to be used for resize handles.
    bool can_ever_resize = widget_->widget_delegate()
                               ? widget_->widget_delegate()->CanResize()
                               : false;
    // Don't allow overlapping resize handles when the window is maximized or
    // fullscreen, as it can't be resized in those states.
    int resize_border_thickness = ResizeBorderThickness();
    int frame_component = GetHTComponentForFrame(
        point,
        gfx::Insets::VH(resize_border_thickness, resize_border_thickness),
        kResizeAreaCornerSize, kResizeAreaCornerSize, can_ever_resize);
    if (frame_component != HTNOWHERE) {
      return frame_component;
    }

    // Test for mouse clicks that fall within the draggable region.
    SkRegion* draggable_region = view_->draggable_region();
    if (draggable_region && draggable_region->contains(point.x(), point.y())) {
      return HTCAPTION;
    }

    int client_component = widget_->client_view()->NonClientHitTest(point);
    if (client_component != HTNOWHERE) {
      return client_component;
    }

    // Caption is a safe default.
    return HTCAPTION;
  }

  void GetWindowMask(const gfx::Size& size, SkPath* window_mask) override {
    // Nothing to do here.
  }

  void ResetWindowControls() override {
    // Nothing to do here.
  }

  void UpdateWindowIcon() override {
    // Nothing to do here.
  }

  void UpdateWindowTitle() override {
    // Nothing to do here.
  }

  void SizeConstraintsChanged() override {
    // Nothing to do here.
  }

  void OnPaint(gfx::Canvas* canvas) override {
    // Nothing to do here.
  }

  void Layout(views::View::PassKey) override {
    client_view_bounds_.SetRect(0, 0, width(), height());
    LayoutSuperclass<views::NonClientFrameView>(this);
  }

  gfx::Size CalculatePreferredSize() const override {
    return widget_->non_client_view()
        ->GetWindowBoundsForClientBounds(
            gfx::Rect(widget_->client_view()->GetPreferredSize()))
        .size();
  }

  gfx::Size GetMinimumSize() const override {
    return widget_->non_client_view()
        ->GetWindowBoundsForClientBounds(
            gfx::Rect(widget_->client_view()->GetMinimumSize()))
        .size();
  }

  gfx::Size GetMaximumSize() const override {
    gfx::Size max_size = widget_->client_view()->GetMaximumSize();
    gfx::Size converted_size =
        widget_->non_client_view()
            ->GetWindowBoundsForClientBounds(gfx::Rect(max_size))
            .size();
    return gfx::Size(max_size.width() == 0 ? 0 : converted_size.width(),
                     max_size.height() == 0 ? 0 : converted_size.height());
  }

 private:
  int ResizeBorderThickness() const {
    return (widget_->IsMaximized() || widget_->IsFullscreen()
                ? 0
                : kResizeBorderThickness);
  }

  // Not owned by this object.
  views::Widget* widget_;
  CefWindowView* view_;

  // The bounds of the client view, in this view's coordinates.
  gfx::Rect client_view_bounds_;
};

bool IsWindowBorderHit(int code) {
// On Windows HTLEFT = 10 and HTBORDER = 18. Values are not ordered the same
// in base/hit_test.h for non-Windows platforms.
#if BUILDFLAG(IS_WIN)
  return code >= HTLEFT && code <= HTBORDER;
#else
  return code == HTLEFT || code == HTRIGHT || code == HTTOP ||
         code == HTTOPLEFT || code == HTTOPRIGHT || code == HTBOTTOM ||
         code == HTBOTTOMLEFT || code == HTBOTTOMRIGHT || code == HTBORDER;
#endif
}

// Based on UpdateModalDialogPosition() from
// components/constrained_window/constrained_window_views.cc
void UpdateModalDialogPosition(views::Widget* widget,
                               views::Widget* host_widget) {
  // Do not forcibly update the dialog widget position if it is being dragged.
  if (widget->HasCapture()) {
    return;
  }

  const gfx::Size& size = widget->GetRootView()->GetPreferredSize();
  const gfx::Size& host_size =
      host_widget->GetClientAreaBoundsInScreen().size();

  // Center the dialog. Position is relative to the host.
  gfx::Point position;
  position.set_x((host_size.width() - size.width()) / 2);
  position.set_y((host_size.height() - size.height()) / 2);

  // Align the first row of pixels inside the border. This is the apparent top
  // of the dialog.
  position.set_y(position.y() -
                 widget->non_client_view()->frame_view()->GetInsets().top());

  const bool supports_global_screen_coordinates =
#if !BUILDFLAG(IS_OZONE)
      true;
#else
      ui::OzonePlatform::GetInstance()
          ->GetPlatformProperties()
          .supports_global_screen_coordinates;
#endif

  if (widget->is_top_level() && supports_global_screen_coordinates) {
    position += host_widget->GetClientAreaBoundsInScreen().OffsetFromOrigin();
    // If the dialog extends partially off any display, clamp its position to
    // be fully visible within that display. If the dialog doesn't intersect
    // with any display clamp its position to be fully on the nearest display.
    gfx::Rect display_rect = gfx::Rect(position, size);
    const display::Display display =
        display::Screen::GetScreen()->GetDisplayNearestView(
            view_util::GetNativeView(host_widget));
    const gfx::Rect work_area = display.work_area();
    if (!work_area.Contains(display_rect)) {
      display_rect.AdjustToFit(work_area);
    }
    position = display_rect.origin();
  }

  widget->SetBounds(gfx::Rect(position, size));
}

}  // namespace

CefWindowView::CefWindowView(CefWindowDelegate* cef_delegate,
                             Delegate* window_delegate)
    : ParentClass(cef_delegate), window_delegate_(window_delegate) {
  DCHECK(window_delegate_);
}

void CefWindowView::CreateWidget(gfx::AcceleratedWidget parent_widget) {
  DCHECK(!GetWidget());

  // |widget| is owned by the NativeWidget and will be destroyed in response to
  // a native destruction message.
  views::Widget* widget = cef::IsChromeRuntimeEnabled() ? new ChromeBrowserFrame
                                                        : new views::Widget;

  views::Widget::InitParams params;
  params.delegate = this;

  views::Widget* host_widget = nullptr;

  bool can_activate = true;
  bool can_resize = true;

  const bool has_native_parent = parent_widget != gfx::kNullAcceleratedWidget;
  if (has_native_parent) {
    params.parent_widget = parent_widget;

    // Remove the window frame.
    is_frameless_ = true;

    // See CalculateWindowStylesFromInitParams in
    // ui/views/widget/widget_hwnd_utils.cc for the conversion of |params| to
    // Windows style flags.
    // - Set the WS_CHILD flag.
    params.child = true;
    // - Set the WS_VISIBLE flag.
    params.type = views::Widget::InitParams::TYPE_CONTROL;
    // - Don't set the WS_EX_COMPOSITED flag.
    params.opacity = views::Widget::InitParams::WindowOpacity::kOpaque;
  } else {
    params.type = views::Widget::InitParams::TYPE_WINDOW;
  }

  // WidgetDelegate::DeleteDelegate() will delete |this| after executing the
  // registered callback.
  SetOwnedByWidget(true);
  RegisterDeleteDelegateCallback(
      base::BindOnce(&CefWindowView::DeleteDelegate, base::Unretained(this)));

  if (cef_delegate()) {
    CefRefPtr<CefWindow> cef_window = GetCefWindow();

    auto bounds = cef_delegate()->GetInitialBounds(cef_window);
    params.bounds = gfx::Rect(bounds.x, bounds.y, bounds.width, bounds.height);

    if (has_native_parent) {
      DCHECK(!params.bounds.IsEmpty());
    } else {
      is_frameless_ = cef_delegate()->IsFrameless(cef_window);

      params.native_widget =
          view_util::CreateNativeWidget(widget, cef_window, cef_delegate());

      can_resize = cef_delegate()->CanResize(cef_window);

      const auto show_state = cef_delegate()->GetInitialShowState(cef_window);
      switch (show_state) {
        case CEF_SHOW_STATE_NORMAL:
          params.show_state = ui::SHOW_STATE_NORMAL;
          break;
        case CEF_SHOW_STATE_MINIMIZED:
          params.show_state = ui::SHOW_STATE_MINIMIZED;
          break;
        case CEF_SHOW_STATE_MAXIMIZED:
          params.show_state = ui::SHOW_STATE_MAXIMIZED;
          break;
        case CEF_SHOW_STATE_FULLSCREEN:
          params.show_state = ui::SHOW_STATE_FULLSCREEN;
          break;
        case CEF_SHOW_STATE_HIDDEN:
#if BUILDFLAG(IS_MAC)
          params.show_state = ui::SHOW_STATE_HIDDEN;
#else
          params.show_state = ui::SHOW_STATE_MINIMIZED;
#endif
          break;
      }

      bool is_menu = false;
      bool can_activate_menu = true;
      CefRefPtr<CefWindow> parent_window = cef_delegate()->GetParentWindow(
          cef_window, &is_menu, &can_activate_menu);
      if (parent_window && !parent_window->IsSame(cef_window)) {
        CefWindowImpl* parent_window_impl =
            static_cast<CefWindowImpl*>(parent_window.get());
        params.parent = view_util::GetNativeView(parent_window_impl->widget());

        // Aura uses the same types for NativeView and NativeWindow, which can
        // be confusing. Verify that we set |params.parent| correctly (to the
        // expected internal::NativeWidgetPrivate) for Widget::Init usage.
        DCHECK(views::Widget::GetWidgetForNativeView(params.parent));

        if (is_menu) {
          // Don't clip the window to parent bounds.
          params.type = views::Widget::InitParams::TYPE_MENU;

          // Don't set "always on top" for the window.
          params.z_order = ui::ZOrderLevel::kNormal;

          can_activate = can_activate_menu;
        } else {
          // Create a top-level window that is moveable and can exceed the
          // bounds of the parent window. By not setting |params.child| here we
          // cause OnBeforeWidgetInit to create a views::DesktopNativeWidgetAura
          // instead of a views::NativeWidgetAura. We need to use this desktop
          // variant with browser windows to get proper focus and shutdown
          // behavior.

#if !BUILDFLAG(IS_LINUX)
          // SetModalType doesn't work on Linux (no implementation in
          // DesktopWindowTreeHostLinux::InitModalType). See the X11-specific
          // implementation below that may work with some window managers.
          if (cef_delegate()->IsWindowModalDialog(cef_window)) {
            SetModalType(ui::MODAL_TYPE_WINDOW);
          }
#endif

          host_widget = parent_window_impl->widget();
        }
      }
    }
  }

  if (params.bounds.IsEmpty()) {
    // The window will be placed on the default screen with origin (0,0).
    params.bounds = gfx::Rect(CalculatePreferredSize());
    if (params.bounds.IsEmpty()) {
      // Choose a reasonable default size.
      params.bounds.set_size({800, 600});
    }
  }

  if (can_activate) {
    // Cause WidgetDelegate::CanActivate to return true.
    params.activatable = views::Widget::InitParams::Activatable::kYes;
  }

  SetCanResize(can_resize);

#if BUILDFLAG(IS_WIN)
  if (is_frameless_) {
    // Don't show the native window caption. Setting this value on Linux will
    // result in window resize artifacts.
    params.remove_standard_frame = true;
  }
#endif

  widget->Init(std::move(params));
  widget->AddObserver(this);

  // |widget| should now be associated with |this|.
  DCHECK_EQ(widget, GetWidget());
  // |widget| must be top-level for focus handling to work correctly.
  DCHECK(widget->is_top_level());

  if (can_activate) {
    // |widget| must be activatable for focus handling to work correctly.
    DCHECK(widget->widget_delegate()->CanActivate());
  }

#if BUILDFLAG(IS_LINUX)
#if BUILDFLAG(IS_OZONE_X11)
  auto x11window = static_cast<x11::Window>(view_util::GetWindowHandle(widget));
  CHECK(x11window != x11::Window::None);

  if (is_frameless_) {
    ui::SetUseOSWindowFrame(x11window, false);
  }

  if (host_widget) {
    auto parent = static_cast<gfx::AcceleratedWidget>(
        view_util::GetWindowHandle(host_widget));
    CHECK(parent);

    auto connection = x11::Connection::Get();

    if (cef_delegate() && cef_delegate()->IsWindowModalDialog(GetCefWindow())) {
      // The presence of _NET_WM_STATE_MODAL in _NET_SUPPORTED indicates
      // possible window manager support. However, some window managers still
      // don't support this properly.
      x11::Atom modal_atom = x11::GetAtom("_NET_WM_STATE_MODAL");
      if (connection->WmSupportsHint(modal_atom)) {
        ui::SetWMSpecState(x11window, true, modal_atom, x11::Atom::None);
      } else {
        LOG(ERROR)
            << "Window modal dialogs are not supported by the window manager";
      }
    }

    // From GtkUiPlatformX11::SetGtkWidgetTransientFor:
    connection->SetProperty(x11window, x11::Atom::WM_TRANSIENT_FOR,
                            x11::Atom::WINDOW, parent);
    connection->SetProperty(x11window, x11::GetAtom("_NET_WM_WINDOW_TYPE"),
                            x11::Atom::ATOM,
                            x11::GetAtom("_NET_WM_WINDOW_TYPE_DIALOG"));

    ui::LinuxUiDelegate::GetInstance()->SetTransientWindowForParent(
        parent, static_cast<gfx::AcceleratedWidget>(x11window));
  }
#endif
#endif

  if (host_widget) {
    // Position |widget| relative to |host_widget|.
    UpdateModalDialogPosition(widget, host_widget);

    // Track the lifespan of |host_widget|, which may be destroyed before
    // |widget|.
    host_widget_destruction_observer_ =
        std::make_unique<WidgetDestructionObserver>(host_widget);
  }
}

CefRefPtr<CefWindow> CefWindowView::GetCefWindow() const {
  CefRefPtr<CefWindow> window = GetCefPanel()->AsWindow();
  DCHECK(window);
  return window;
}

void CefWindowView::DeleteDelegate() {
  // Remove all child Views before deleting the Window so that notifications
  // resolve correctly.
  RemoveAllChildViews();

  window_delegate_->OnWindowViewDeleted();
}

bool CefWindowView::CanMinimize() const {
  if (!cef_delegate()) {
    return true;
  }
  return cef_delegate()->CanMinimize(GetCefWindow());
}

bool CefWindowView::CanMaximize() const {
  if (!cef_delegate()) {
    return true;
  }
  return cef_delegate()->CanMaximize(GetCefWindow());
}

std::u16string CefWindowView::GetWindowTitle() const {
  return title_;
}

ui::ImageModel CefWindowView::GetWindowIcon() {
  if (!window_icon_) {
    return ParentClass::GetWindowIcon();
  }
  auto image_skia =
      static_cast<CefImageImpl*>(window_icon_.get())
          ->GetForced1xScaleRepresentation(GetDisplay().device_scale_factor());
  return ui::ImageModel::FromImageSkia(image_skia);
}

ui::ImageModel CefWindowView::GetWindowAppIcon() {
  if (!window_app_icon_) {
    return ParentClass::GetWindowAppIcon();
  }
  auto image_skia =
      static_cast<CefImageImpl*>(window_app_icon_.get())
          ->GetForced1xScaleRepresentation(GetDisplay().device_scale_factor());
  return ui::ImageModel::FromImageSkia(image_skia);
}

void CefWindowView::WindowClosing() {
#if BUILDFLAG(IS_LINUX)
#if BUILDFLAG(IS_OZONE_X11)
  if (host_widget()) {
    auto parent = static_cast<gfx::AcceleratedWidget>(
        view_util::GetWindowHandle(host_widget()));
    CHECK(parent);

    // From GtkUiPlatformX11::ClearTransientFor:
    ui::LinuxUiDelegate::GetInstance()->SetTransientWindowForParent(
        parent, static_cast<gfx::AcceleratedWidget>(x11::Window::None));
  }
#endif
#endif

  window_delegate_->OnWindowClosing();
}

views::View* CefWindowView::GetContentsView() {
  // |this| will be the "Contents View" hosted by the Widget via ClientView and
  // RootView.
  return this;
}

views::ClientView* CefWindowView::CreateClientView(views::Widget* widget) {
  return new ClientViewEx(widget, GetContentsView(), window_delegate_);
}

std::unique_ptr<views::NonClientFrameView>
CefWindowView::CreateNonClientFrameView(views::Widget* widget) {
  if (is_frameless_) {
    // Custom frame type that doesn't render a caption.
    return std::make_unique<CaptionlessFrameView>(widget, this);
  } else if (widget->ShouldUseNativeFrame()) {
    // DesktopNativeWidgetAura::CreateNonClientFrameView() returns
    // NativeFrameView by default. Extend that type.
    return std::make_unique<NativeFrameViewEx>(widget, this);
  }

  // Use Chromium provided CustomFrameView. In case if we would like to
  // customize the frame, provide own implementation.
  return nullptr;
}

bool CefWindowView::ShouldDescendIntoChildForEventHandling(
    gfx::NativeView child,
    const gfx::Point& location) {
  if (is_frameless_) {
    // If the window is resizable it should claim mouse events that fall on the
    // window border.
    views::NonClientFrameView* ncfv = GetNonClientFrameView();
    if (ncfv) {
      int result = ncfv->NonClientHitTest(location);
      if (IsWindowBorderHit(result)) {
        return false;
      }
    }
  }

  // The window should claim mouse events that fall within the draggable region.
  return !draggable_region_.get() ||
         !draggable_region_->contains(location.x(), location.y());
}

bool CefWindowView::MaybeGetMinimumSize(gfx::Size* size) const {
#if BUILDFLAG(IS_LINUX)
  // Resize is disabled on Linux by returning the preferred size as the min/max
  // size.
  if (!CanResize()) {
    *size = CalculatePreferredSize();
    return true;
  }
#endif
  return false;
}

bool CefWindowView::MaybeGetMaximumSize(gfx::Size* size) const {
#if BUILDFLAG(IS_LINUX)
  // Resize is disabled on Linux by returning the preferred size as the min/max
  // size.
  if (!CanResize()) {
    *size = CalculatePreferredSize();
    return true;
  }
#endif
  return false;
}

void CefWindowView::ViewHierarchyChanged(
    const views::ViewHierarchyChangedDetails& details) {
  if (details.child == this) {
    // This View's parent types (RootView, ClientView) are not exposed via the
    // CEF API. Therefore don't send notifications about this View's parent
    // changes.
    return;
  }

  ParentClass::ViewHierarchyChanged(details);
}

void CefWindowView::OnWidgetActivationChanged(views::Widget* widget,
                                              bool active) {
  if (cef_delegate()) {
    cef_delegate()->OnWindowActivationChanged(GetCefWindow(), active);
  }
}

void CefWindowView::OnWidgetBoundsChanged(views::Widget* widget,
                                          const gfx::Rect& new_bounds) {
  MoveOverlaysIfNecessary();

  if (cef_delegate()) {
    cef_delegate()->OnWindowBoundsChanged(
        GetCefWindow(), {new_bounds.x(), new_bounds.y(), new_bounds.width(),
                         new_bounds.height()});
  }
}

display::Display CefWindowView::GetDisplay() const {
  const views::Widget* widget = GetWidget();
  if (widget) {
    return view_util::GetDisplayMatchingBounds(
        widget->GetWindowBoundsInScreen(), false);
  }
  return display::Display();
}

void CefWindowView::SetTitle(const std::u16string& title) {
  title_ = title;
  views::Widget* widget = GetWidget();
  if (widget) {
    widget->UpdateWindowTitle();
  }
}

void CefWindowView::SetWindowIcon(CefRefPtr<CefImage> window_icon) {
  if (std::max(window_icon->GetWidth(), window_icon->GetHeight()) != 16U) {
    DLOG(ERROR) << "Window icons must be 16 DIP in size.";
    return;
  }

  window_icon_ = window_icon;
  views::Widget* widget = GetWidget();
  if (widget) {
    widget->UpdateWindowIcon();
  }
}

void CefWindowView::SetWindowAppIcon(CefRefPtr<CefImage> window_app_icon) {
  window_app_icon_ = window_app_icon;
  views::Widget* widget = GetWidget();
  if (widget) {
    widget->UpdateWindowIcon();
  }
}

CefRefPtr<CefOverlayController> CefWindowView::AddOverlayView(
    CefRefPtr<CefView> view,
    cef_docking_mode_t docking_mode,
    bool can_activate) {
  DCHECK(view.get());
  DCHECK(view->IsValid());
  if (!view.get() || !view->IsValid()) {
    return nullptr;
  }

  views::Widget* widget = GetWidget();
  if (widget) {
    // Owned by the View hierarchy. Acts as a z-order reference for the overlay.
    auto overlay_host_view = AddChildView(std::make_unique<views::View>());

    overlay_hosts_.push_back(
        std::make_unique<CefOverlayViewHost>(this, docking_mode));

    auto& overlay_host = overlay_hosts_.back();
    overlay_host->Init(overlay_host_view, view, can_activate);

    return overlay_host->controller();
  }

  return nullptr;
}

void CefWindowView::MoveOverlaysIfNecessary() {
  if (overlay_hosts_.empty()) {
    return;
  }
  for (auto& overlay_host : overlay_hosts_) {
    overlay_host->MoveIfNecessary();
  }
}

void CefWindowView::InvalidateExclusionRegions() {
  if (last_dialog_top_inset_ != -1) {
    last_dialog_top_y_ = last_dialog_top_inset_ = -1;
  }
}

void CefWindowView::SetDraggableRegions(
    const std::vector<CefDraggableRegion>& regions) {
  if (regions.empty() && !draggable_region_) {
    // Still empty.
    return;
  }

  InvalidateExclusionRegions();

  if (regions.empty()) {
    draggable_region_.reset();
    draggable_rects_.clear();
    return;
  }

  draggable_region_ = std::make_unique<SkRegion>();
  for (const CefDraggableRegion& region : regions) {
    draggable_region_->op(
        {region.bounds.x, region.bounds.y,
         region.bounds.x + region.bounds.width,
         region.bounds.y + region.bounds.height},
        region.draggable ? SkRegion::kUnion_Op : SkRegion::kDifference_Op);

    if (region.draggable) {
      draggable_rects_.emplace_back(region.bounds.x, region.bounds.y,
                                    region.bounds.width, region.bounds.height);
    }
  }
}

void CefWindowView::OnOverlayBoundsChanged() {
  InvalidateExclusionRegions();
}

views::NonClientFrameView* CefWindowView::GetNonClientFrameView() const {
  const views::Widget* widget = GetWidget();
  if (!widget) {
    return nullptr;
  }
  if (!widget->non_client_view()) {
    return nullptr;
  }
  return widget->non_client_view()->frame_view();
}

void CefWindowView::UpdateBoundingBox(gfx::Rect* bounds,
                                      bool add_titlebar_height) const {
  // Max distance from the edges of |bounds| to qualify for subtraction.
  const int kMaxDistance = 10;

  for (auto& overlay_host : overlay_hosts_) {
    *bounds = SubtractOverlayFromBoundingBox(*bounds, overlay_host->bounds(),
                                             kMaxDistance);
  }

  for (auto& rect : draggable_rects_) {
    *bounds = SubtractOverlayFromBoundingBox(*bounds, rect, kMaxDistance);
  }

  if (auto titlebar_height = GetTitlebarHeight(add_titlebar_height)) {
    gfx::Insets inset;
    if (add_titlebar_height) {
      inset.set_top(*titlebar_height);
    } else if (bounds->y() < *titlebar_height) {
      inset.set_top(*titlebar_height - bounds->y());
    }

    if (!inset.IsEmpty()) {
      bounds->Inset(inset);
    }
  }
}

void CefWindowView::UpdateFindBarBoundingBox(gfx::Rect* bounds) const {
#if BUILDFLAG(IS_MAC)
  // For framed windows on macOS we must add the titlebar height.
  const bool add_titlebar_height = !is_frameless_;
#else
  const bool add_titlebar_height = false;
#endif

  UpdateBoundingBox(bounds, add_titlebar_height);
}

void CefWindowView::UpdateDialogTopInset(int* dialog_top_y) const {
  if (*dialog_top_y == last_dialog_top_y_ && last_dialog_top_inset_ != -1) {
    // Return the cached value.
    *dialog_top_y = last_dialog_top_inset_;
    return;
  }

  const views::Widget* widget = GetWidget();
  if (!widget) {
    return;
  }

  gfx::Rect bounds(widget->GetSize());
  if (*dialog_top_y > 0) {
    // Start with the value computed in
    // BrowserViewLayout::LayoutBookmarkAndInfoBars.
    gfx::Insets inset;
    inset.set_top(*dialog_top_y);
    bounds.Inset(inset);
  }

  UpdateBoundingBox(&bounds, /*add_titlebar_height=*/false);

  last_dialog_top_y_ = *dialog_top_y;
  last_dialog_top_inset_ = bounds.y();

  *dialog_top_y = bounds.y();
}

views::Widget* CefWindowView::host_widget() const {
  if (host_widget_destruction_observer_) {
    return host_widget_destruction_observer_->widget();
  }
  return nullptr;
}

std::optional<float> CefWindowView::GetTitlebarHeight(bool required) const {
  if (cef_delegate()) {
    float title_bar_height = 0;
    const bool has_title_bar_height =
        cef_delegate()->GetTitlebarHeight(GetCefWindow(), &title_bar_height);
    if (has_title_bar_height) {
      return title_bar_height;
    }
  }

#if BUILDFLAG(IS_MAC)
  if (required) {
    // For framed windows on macOS we must include the titlebar height in the
    // UpdateFindBarBoundingBox() calculation.
    return view_util::GetNSWindowTitleBarHeight(
        const_cast<views::Widget*>(GetWidget()));
  }
#endif

  return std::nullopt;
}
