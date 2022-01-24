// Copyright 2016 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "libcef/browser/views/window_view.h"

#include "libcef/browser/chrome/views/chrome_browser_frame.h"
#include "libcef/browser/image_impl.h"
#include "libcef/browser/views/window_impl.h"
#include "libcef/features/runtime.h"

#include "ui/base/hit_test.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/native_frame_view.h"

#if BUILDFLAG(IS_LINUX)
#include "ui/ozone/buildflags.h"
#if BUILDFLAG(OZONE_PLATFORM_X11)
#include "ui/base/x/x11_util.h"
#endif
#endif

#if BUILDFLAG(IS_WIN)
#include "ui/display/screen.h"
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
#if BUILDFLAG(IS_WIN)
    // views::GetWindowBoundsForClientBounds() expects the input Rect to be in
    // pixel coordinates. NativeFrameView does not implement this correctly so
    // we need to provide our own implementation. See http://crbug.com/602692.
    gfx::Rect pixel_bounds =
        display::Screen::GetScreen()->DIPToScreenRectInWindow(
            view_util::GetNativeWindow(widget_), client_bounds);
    pixel_bounds = views::GetWindowBoundsForClientBounds(
        static_cast<View*>(const_cast<NativeFrameViewEx*>(this)), pixel_bounds);
    return display::Screen::GetScreen()->ScreenToDIPRectInWindow(
        view_util::GetNativeWindow(widget_), pixel_bounds);
#else
    // Use the default implementation.
    return views::NativeFrameView::GetWindowBoundsForClientBounds(
        client_bounds);
#endif
  }

  int NonClientHitTest(const gfx::Point& point) override {
    if (widget_->IsFullscreen())
      return HTCLIENT;

    // Test for mouse clicks that fall within the draggable region.
    SkRegion* draggable_region = view_->draggable_region();
    if (draggable_region && draggable_region->contains(point.x(), point.y()))
      return HTCAPTION;

    return views::NativeFrameView::NonClientHitTest(point);
  }

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
    if (widget_->IsFullscreen())
      return HTCLIENT;

    // Sanity check.
    if (!bounds().Contains(point))
      return HTNOWHERE;

    // Check the frame first, as we allow a small area overlapping the contents
    // to be used for resize handles.
    bool can_ever_resize = widget_->widget_delegate()
                               ? widget_->widget_delegate()->CanResize()
                               : false;
    // Don't allow overlapping resize handles when the window is maximized or
    // fullscreen, as it can't be resized in those states.
    int resize_border_thickness = ResizeBorderThickness();
    int frame_component = GetHTComponentForFrame(
        point, gfx::Insets(resize_border_thickness, resize_border_thickness),
        kResizeAreaCornerSize, kResizeAreaCornerSize, can_ever_resize);
    if (frame_component != HTNOWHERE)
      return frame_component;

    // Test for mouse clicks that fall within the draggable region.
    SkRegion* draggable_region = view_->draggable_region();
    if (draggable_region && draggable_region->contains(point.x(), point.y()))
      return HTCAPTION;

    int client_component = widget_->client_view()->NonClientHitTest(point);
    if (client_component != HTNOWHERE)
      return client_component;

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

  void Layout() override {
    client_view_bounds_.SetRect(0, 0, width(), height());
    views::NonClientFrameView::Layout();
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

}  // namespace

CefWindowView::CefWindowView(CefWindowDelegate* cef_delegate,
                             Delegate* window_delegate)
    : ParentClass(cef_delegate),
      window_delegate_(window_delegate),
      is_frameless_(false) {
  DCHECK(window_delegate_);
}

void CefWindowView::CreateWidget() {
  DCHECK(!GetWidget());

  // |widget| is owned by the NativeWidget and will be destroyed in response to
  // a native destruction message.
  views::Widget* widget = cef::IsChromeRuntimeEnabled() ? new ChromeBrowserFrame
                                                        : new views::Widget;

  views::Widget::InitParams params;
  params.delegate = this;
  params.type = views::Widget::InitParams::TYPE_WINDOW;
  bool can_activate = true;

  // WidgetDelegate::DeleteDelegate() will delete |this| after executing the
  // registered callback.
  SetOwnedByWidget(true);
  RegisterDeleteDelegateCallback(
      base::BindOnce(&CefWindowView::DeleteDelegate, base::Unretained(this)));

  if (cef_delegate()) {
    CefRefPtr<CefWindow> cef_window = GetCefWindow();
    is_frameless_ = cef_delegate()->IsFrameless(cef_window);

    auto bounds = cef_delegate()->GetInitialBounds(cef_window);
    params.bounds = gfx::Rect(bounds.x, bounds.y, bounds.width, bounds.height);

    SetCanResize(cef_delegate()->CanResize(cef_window));

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
    }

    bool is_menu = false;
    bool can_activate_menu = true;
    CefRefPtr<CefWindow> parent_window = cef_delegate()->GetParentWindow(
        cef_window, &is_menu, &can_activate_menu);
    if (parent_window && !parent_window->IsSame(cef_window)) {
      CefWindowImpl* parent_window_impl =
          static_cast<CefWindowImpl*>(parent_window.get());
      params.parent = view_util::GetNativeView(parent_window_impl->widget());
      if (is_menu) {
        // Don't clip the window to parent bounds.
        params.type = views::Widget::InitParams::TYPE_MENU;

        // Don't set "always on top" for the window.
        params.z_order = ui::ZOrderLevel::kNormal;

        can_activate = can_activate_menu;
        if (can_activate_menu)
          params.activatable = views::Widget::InitParams::Activatable::kYes;
      }
    }
  }

  if (params.bounds.IsEmpty()) {
    // The window will be placed on the default screen with origin (0,0).
    params.bounds = gfx::Rect(CalculatePreferredSize());
  }

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
#if BUILDFLAG(OZONE_PLATFORM_X11)
  if (is_frameless_) {
    auto window = view_util::GetWindowHandle(widget);
    DCHECK(window);
    ui::SetUseOSWindowFrame(static_cast<x11::Window>(window), false);
  }
#endif
#endif
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
  if (!cef_delegate())
    return true;
  return cef_delegate()->CanMinimize(GetCefWindow());
}

bool CefWindowView::CanMaximize() const {
  if (!cef_delegate())
    return true;
  return cef_delegate()->CanMaximize(GetCefWindow());
}

std::u16string CefWindowView::GetWindowTitle() const {
  return title_;
}

ui::ImageModel CefWindowView::GetWindowIcon() {
  if (!window_icon_)
    return ParentClass::GetWindowIcon();
  auto image_skia =
      static_cast<CefImageImpl*>(window_icon_.get())
          ->GetForced1xScaleRepresentation(GetDisplay().device_scale_factor());
  return ui::ImageModel::FromImageSkia(image_skia);
}

ui::ImageModel CefWindowView::GetWindowAppIcon() {
  if (!window_app_icon_)
    return ParentClass::GetWindowAppIcon();
  auto image_skia =
      static_cast<CefImageImpl*>(window_app_icon_.get())
          ->GetForced1xScaleRepresentation(GetDisplay().device_scale_factor());
  return ui::ImageModel::FromImageSkia(image_skia);
}

void CefWindowView::WindowClosing() {
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
      if (IsWindowBorderHit(result))
        return false;
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

void CefWindowView::OnWidgetBoundsChanged(views::Widget* widget,
                                          const gfx::Rect& new_bounds) {
  MoveOverlaysIfNecessary();
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
  if (widget)
    widget->UpdateWindowTitle();
}

void CefWindowView::SetWindowIcon(CefRefPtr<CefImage> window_icon) {
  if (std::max(window_icon->GetWidth(), window_icon->GetHeight()) != 16U) {
    DLOG(ERROR) << "Window icons must be 16 DIP in size.";
    return;
  }

  window_icon_ = window_icon;
  views::Widget* widget = GetWidget();
  if (widget)
    widget->UpdateWindowIcon();
}

void CefWindowView::SetWindowAppIcon(CefRefPtr<CefImage> window_app_icon) {
  window_app_icon_ = window_app_icon;
  views::Widget* widget = GetWidget();
  if (widget)
    widget->UpdateWindowIcon();
}

CefRefPtr<CefOverlayController> CefWindowView::AddOverlayView(
    CefRefPtr<CefView> view,
    cef_docking_mode_t docking_mode) {
  DCHECK(view.get());
  DCHECK(view->IsValid());
  if (!view.get() || !view->IsValid())
    return nullptr;

  views::Widget* widget = GetWidget();
  if (widget) {
    // Owned by the View hierarchy. Acts as a z-order reference for the overlay.
    auto overlay_host_view = AddChildView(std::make_unique<views::View>());

    overlay_hosts_.push_back(
        std::make_unique<CefOverlayViewHost>(this, docking_mode));

    auto& overlay_host = overlay_hosts_.back();
    overlay_host->Init(overlay_host_view, view);

    return overlay_host->controller();
  }

  return nullptr;
}

void CefWindowView::MoveOverlaysIfNecessary() {
  if (overlay_hosts_.empty())
    return;
  for (auto& overlay_host : overlay_hosts_) {
    overlay_host->MoveIfNecessary();
  }
}

void CefWindowView::SetDraggableRegions(
    const std::vector<CefDraggableRegion>& regions) {
  if (regions.empty()) {
    if (draggable_region_)
      draggable_region_.reset(nullptr);
    return;
  }

  draggable_region_.reset(new SkRegion);
  for (const CefDraggableRegion& region : regions) {
    draggable_region_->op(
        {region.bounds.x, region.bounds.y,
         region.bounds.x + region.bounds.width,
         region.bounds.y + region.bounds.height},
        region.draggable ? SkRegion::kUnion_Op : SkRegion::kDifference_Op);
  }
}

views::NonClientFrameView* CefWindowView::GetNonClientFrameView() const {
  const views::Widget* widget = GetWidget();
  if (!widget)
    return nullptr;
  if (!widget->non_client_view())
    return nullptr;
  return widget->non_client_view()->frame_view();
}
