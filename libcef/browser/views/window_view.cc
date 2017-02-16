// Copyright 2016 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "libcef/browser/views/window_view.h"

#include "libcef/browser/image_impl.h"

#include "third_party/skia/include/core/SkRegion.h"
#include "ui/aura/window.h"
#include "ui/base/hit_test.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/native_frame_view.h"

#if defined(OS_LINUX)
#include <X11/Xlib.h>
#include "ui/gfx/x/x11_types.h"
#endif

#if defined(OS_WIN)
#include "ui/display/screen.h"
#include "ui/views/win/hwnd_util.h"
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

  bool CanClose() override {
    return window_delegate_->CanWidgetClose();
  }

 private:
  CefWindowView::Delegate* window_delegate_;  // Not owned by this object.

  DISALLOW_COPY_AND_ASSIGN(ClientViewEx);
};

// Extend NativeFrameView with draggable region handling.
class NativeFrameViewEx : public views::NativeFrameView {
 public:
  NativeFrameViewEx(views::Widget* widget,
                    CefWindowView* view)
    : views::NativeFrameView(widget),
      widget_(widget),
      view_(view) {
  }

  gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const override {
#if defined(OS_WIN)
    // views::GetWindowBoundsForClientBounds() expects the input Rect to be in
    // pixel coordinates. NativeFrameView does not implement this correctly so
    // we need to provide our own implementation. See http://crbug.com/602692.
    gfx::Rect pixel_bounds =
        display::Screen::GetScreen()->DIPToScreenRectInWindow(
            view_util::GetNativeWindow(widget_), client_bounds);
    pixel_bounds = views::GetWindowBoundsForClientBounds(
        static_cast<View*>(const_cast<NativeFrameViewEx*>(this)),
        pixel_bounds);
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

  DISALLOW_COPY_AND_ASSIGN(NativeFrameViewEx);
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
  CaptionlessFrameView(views::Widget* widget,
                       CefWindowView* view)
    : widget_(widget),
      view_(view) {
  }

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
    int frame_component = GetHTComponentForFrame(point,
                                                 resize_border_thickness,
                                                 resize_border_thickness,
                                                 kResizeAreaCornerSize,
                                                 kResizeAreaCornerSize,
                                                 can_ever_resize);
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

  void GetWindowMask(const gfx::Size& size, gfx::Path* window_mask) override {
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
  }

  gfx::Size GetPreferredSize() const override {
    return widget_->non_client_view()->GetWindowBoundsForClientBounds(
      gfx::Rect(widget_->client_view()->GetPreferredSize())).size();
  }

  gfx::Size GetMinimumSize() const override {
    return widget_->non_client_view()->GetWindowBoundsForClientBounds(
      gfx::Rect(widget_->client_view()->GetMinimumSize())).size();
  }

  gfx::Size GetMaximumSize() const override {
    gfx::Size max_size = widget_->client_view()->GetMaximumSize();
    gfx::Size converted_size =
        widget_->non_client_view()->GetWindowBoundsForClientBounds(
            gfx::Rect(max_size)).size();
    return gfx::Size(max_size.width() == 0 ? 0 : converted_size.width(),
                     max_size.height() == 0 ? 0 : converted_size.height());
  }

 private:
  int ResizeBorderThickness() const {
    return (widget_->IsMaximized() || widget_->IsFullscreen() ?
        0 : kResizeBorderThickness);
  }

  // Not owned by this object.
  views::Widget* widget_;
  CefWindowView* view_;

  // The bounds of the client view, in this view's coordinates.
  gfx::Rect client_view_bounds_;

  DISALLOW_COPY_AND_ASSIGN(CaptionlessFrameView);
};

bool IsWindowBorderHit(int code) {
  // On Windows HTLEFT = 10 and HTBORDER = 18. Values are not ordered the same
  // in base/hit_test.h for non-Windows platforms.
#if defined(OS_WIN)
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
  views::Widget* widget = new views::Widget;

  views::Widget::InitParams params;
  params.delegate = this;
  params.type = views::Widget::InitParams::TYPE_WINDOW;

  if (cef_delegate())
    is_frameless_ = cef_delegate()->IsFrameless(GetCefWindow());

#if defined(OS_WIN)
  if (is_frameless_) {
    // Don't show the native window caption. Setting this value on Linux will
    // result in window resize artifacts.
    params.remove_standard_frame = true;
  }
#endif

  widget->Init(params);

  // |widget| should now be associated with |this|.
  DCHECK_EQ(widget, GetWidget());
  // |widget| must be top-level for focus handling to work correctly.
  DCHECK(widget->is_top_level());
  // |widget| must be activatable for focus handling to work correctly.
  DCHECK(widget->widget_delegate()->CanActivate());

#if defined(OS_LINUX)
  if (is_frameless_) {
    ::Window window = view_util::GetWindowHandle(widget);
    DCHECK(window);
    ::Display* display = gfx::GetXDisplay();
    DCHECK(display);

    // Make the window borderless. From
    // http://stackoverflow.com/questions/1904445/borderless-windows-on-linux
    struct MwmHints {
      unsigned long flags;
      unsigned long functions;
      unsigned long decorations;
      long input_mode;
      unsigned long status;
    };
    enum {
      MWM_HINTS_FUNCTIONS = (1L << 0),
      MWM_HINTS_DECORATIONS =  (1L << 1),

      MWM_FUNC_ALL = (1L << 0),
      MWM_FUNC_RESIZE = (1L << 1),
      MWM_FUNC_MOVE = (1L << 2),
      MWM_FUNC_MINIMIZE = (1L << 3),
      MWM_FUNC_MAXIMIZE = (1L << 4),
      MWM_FUNC_CLOSE = (1L << 5)
    };

    Atom mwmHintsProperty = XInternAtom(display, "_MOTIF_WM_HINTS", 0);
    struct MwmHints hints;
    hints.flags = MWM_HINTS_DECORATIONS;
    hints.decorations = 0;
    XChangeProperty(display, window, mwmHintsProperty, mwmHintsProperty, 32,
                    PropModeReplace, (unsigned char *)&hints, 5);
  }
#endif  // defined(OS_LINUX)
}

CefRefPtr<CefWindow> CefWindowView::GetCefWindow() const {
  CefRefPtr<CefWindow> window = GetCefPanel()->AsWindow();
  DCHECK(window);
  return window;
}

void CefWindowView::DeleteDelegate() {
  // Remove all child Views before deleting the Window so that notifications
  // resolve correctly.
  RemoveAllChildViews(true);

  window_delegate_->OnWindowViewDeleted();

  // Deletes |this|.
  views::WidgetDelegateView::DeleteDelegate();
}

bool CefWindowView::CanResize() const {
  if (!cef_delegate())
    return true;
  return cef_delegate()->CanResize(GetCefWindow());
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

base::string16 CefWindowView::GetWindowTitle() const {
  return title_;
}

gfx::ImageSkia CefWindowView::GetWindowIcon() {
  if (!window_icon_)
    return ParentClass::GetWindowIcon();
  return static_cast<CefImageImpl*>(window_icon_.get())->
      GetForced1xScaleRepresentation(GetDisplay().device_scale_factor());
}

gfx::ImageSkia CefWindowView::GetWindowAppIcon() {
  if (!window_app_icon_)
    return ParentClass::GetWindowAppIcon();
  return static_cast<CefImageImpl*>(window_app_icon_.get())->
      GetForced1xScaleRepresentation(GetDisplay().device_scale_factor());
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

views::NonClientFrameView* CefWindowView::CreateNonClientFrameView(
      views::Widget* widget) {
  if (is_frameless_) {
    // Custom frame type that doesn't render a caption.
    return new CaptionlessFrameView(widget, this);
  } else if (widget->ShouldUseNativeFrame()) {
    // DesktopNativeWidgetAura::CreateNonClientFrameView() returns
    // NativeFrameView by default. Extend that type.
    return new NativeFrameViewEx(widget, this);
  } else {
    // Widget::CreateNonClientFrameView() returns CustomFrameView by default.
    // Need to extend CustomFrameView on this platform.
    NOTREACHED() << "Platform does not use NativeFrameView";
  }

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

void CefWindowView::ViewHierarchyChanged(
    const views::View::ViewHierarchyChangedDetails& details) {
  if (details.child == this) {
    // This View's parent types (RootView, ClientView) are not exposed via the
    // CEF API. Therefore don't send notifications about this View's parent
    // changes.
    return;
  }

  ParentClass::ViewHierarchyChanged(details);
}

display::Display CefWindowView::GetDisplay() const {
  const views::Widget* widget = GetWidget();
  if (widget) {
    return view_util::GetDisplayMatchingBounds(
        widget->GetWindowBoundsInScreen(), false);
  }
  return display::Display();
}

void CefWindowView::SetTitle(const base::string16& title) {
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
        region.bounds.x,
        region.bounds.y,
        region.bounds.x + region.bounds.width,
        region.bounds.y + region.bounds.height,
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
