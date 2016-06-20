// Copyright 2016 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "libcef/browser/views/view_util.h"

#include <utility>

#include "libcef/browser/views/view_adapter.h"

#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/non_client_view.h"

#if defined(OS_WIN)
#include "ui/display/win/screen_win.h"
#endif

namespace view_util {

namespace {

// Manages the association between views::View and CefView instances.
class UserData : public base::SupportsUserData::Data {
 public:
  // Create the initial association between the views::View and the CefView. The
  // CefView owns the views::View at this stage.
  static void Register(CefRefPtr<CefView> cef_view) {
    DCHECK(cef_view->IsValid());
    DCHECK(!cef_view->IsAttached());

    views::View* view = CefViewAdapter::GetFor(cef_view)->Get();
    DCHECK(view);

    // The CefView should not already be registered.
    DCHECK(!view->GetUserData(UserDataKey()));

    view->SetUserData(UserDataKey(), new UserData(cef_view));
  }

  static CefRefPtr<CefView> GetFor(const views::View* view) {
    DCHECK(view);
    UserData* data = static_cast<UserData*>(view->GetUserData(UserDataKey()));
    if (data)
      return data->view_ref_;
    return nullptr;
  }

  // Transfer ownership of the views::View to the caller. The views::View will
  // gain a ref-counted reference to the CefView and the CefView will keep an
  // unowned reference to the views::View. Destruction of the views::View will
  // release the ref-counted reference to the CefView.
  static std::unique_ptr<views::View> PassOwnership(
      CefRefPtr<CefView> cef_view) WARN_UNUSED_RESULT {
    DCHECK(cef_view->IsValid());
    DCHECK(!cef_view->IsAttached());

    std::unique_ptr<views::View> view =
        CefViewAdapter::GetFor(cef_view)->PassOwnership();
    DCHECK(view);

    UserData* data = static_cast<UserData*>(view->GetUserData(UserDataKey()));
    DCHECK(data);
    data->TakeReference();

    return view;
  }

  // The CefView resumes ownership of the views::View. The views::View no longer
  // keeps a ref-counted reference to the CefView.
  static void ResumeOwnership(CefRefPtr<CefView> cef_view) {
    DCHECK(cef_view->IsValid());
    DCHECK(cef_view->IsAttached());

    CefViewAdapter* adapter = CefViewAdapter::GetFor(cef_view);
    adapter->ResumeOwnership();

    views::View* view = adapter->Get();
    DCHECK(view);

    UserData* data = static_cast<UserData*>(view->GetUserData(UserDataKey()));
    DCHECK(data);
    data->ReleaseReference();
  }

 private:
  explicit UserData(CefRefPtr<CefView> cef_view)
      : view_ref_(cef_view.get()) {
    DCHECK(view_ref_);
  }

  ~UserData() override {
    if (view_) {
      // The CefView does not own the views::View. Remove the CefView's
      // reference to the views::View.
      CefViewAdapter::GetFor(view_)->Detach();
    }
  }

  void TakeReference() {
    view_ = view_ref_;
  }

  void ReleaseReference() {
    view_ = nullptr;
  }

  static void* UserDataKey() {
    // We just need a unique constant. Use the address of a static that
    // COMDAT folding won't touch in an optimizing linker.
    static int data_key = 0;
    return reinterpret_cast<void*>(&data_key);
  }

  CefRefPtr<CefView> view_;
  CefView* view_ref_;
};

}  // namespace

const SkColor kDefaultBackgroundColor = SkColorSetARGB(255, 255, 255, 255);
const char kDefaultFontList[] = "Arial, Helvetica, 14px";

void Register(CefRefPtr<CefView> view) {
  UserData::Register(view);
}

CefRefPtr<CefView> GetFor(const views::View* view, bool find_known_parent) {
  if (!view)
    return nullptr;

  if (!find_known_parent)
    return UserData::GetFor(view);

  CefRefPtr<CefView> cef_view;
  const views::View* current_view = view;
  do {
    cef_view = UserData::GetFor(current_view);
    if (cef_view)
      break;
    current_view = current_view->parent();
  } while (current_view);

  return cef_view;
}

views::View* GetFor(CefRefPtr<CefView> view) {
  return CefViewAdapter::GetFor(view)->Get();
}

std::unique_ptr<views::View> PassOwnership(CefRefPtr<CefView> view) {
  return UserData::PassOwnership(view);
}

void ResumeOwnership(CefRefPtr<CefView> view) {
  UserData::ResumeOwnership(view);
}

CefRefPtr<CefWindow> GetWindowFor(views::Widget* widget) {
  CefRefPtr<CefWindow> window;

  if (widget) {
    // The views::WidgetDelegate should be a CefWindowView and |content_view|
    // should be the same CefWindowView. However, just in case the views::Widget
    // was created by something else let's go about this the safer way.
    views::View* content_view = widget->widget_delegate()->GetContentsView();
    CefRefPtr<CefView> cef_view = GetFor(content_view, false);
    if (cef_view && cef_view->AsPanel())
      window = cef_view->AsPanel()->AsWindow();

    // The Window should always exist if we created the views::Widget.
    DCHECK(window);
  }
  
  return window;
}

display::Display GetDisplayNearestPoint(const gfx::Point& point,
                                        bool input_pixel_coords) {
  gfx::Point find_point = point;
#if defined(OS_WIN)
  if (input_pixel_coords)
    find_point = display::win::ScreenWin::ScreenToDIPPoint(point);
#endif
  return display::Screen::GetScreen()->GetDisplayNearestPoint(find_point);
}

display::Display GetDisplayMatchingBounds(const gfx::Rect& bounds,
                                          bool input_pixel_coords) {
  gfx::Rect find_bounds = bounds;
#if defined(OS_WIN)
  if (input_pixel_coords) {
    find_bounds = display::win::ScreenWin::ScreenToDIPRect(nullptr,
                                                           find_bounds);
  }
#endif
  return display::Screen::GetScreen()->GetDisplayMatching(find_bounds);
}

void ConvertPointFromPixels(gfx::Point* point,
                            int device_scale_factor) {
  *point = gfx::ToFlooredPoint(
      gfx::ScalePoint(gfx::PointF(*point), 1.0f / device_scale_factor));
}

void ConvertPointToPixels(gfx::Point* point,
                          int device_scale_factor) {
  *point = gfx::ToFlooredPoint(
      gfx::ScalePoint(gfx::PointF(*point), device_scale_factor));
}

bool ConvertPointToScreen(views::View* view,
                          gfx::Point* point,
                          bool output_pixel_coords) {
  if (!view->GetWidget())
    return false;

  views::View::ConvertPointToScreen(view, point);

  if (output_pixel_coords) {
    const display::Display& display = GetDisplayNearestPoint(*point, false);
    ConvertPointToPixels(point, display.device_scale_factor());
  }

  return true;
}

bool ConvertPointFromScreen(views::View* view,
                            gfx::Point* point,
                            bool input_pixel_coords) {
  if (!view->GetWidget())
    return false;

  if (input_pixel_coords) {
    const display::Display& display = GetDisplayNearestPoint(*point, true);
    ConvertPointFromPixels(point, display.device_scale_factor());
  }

  views::View::ConvertPointFromScreen(view, point);

  return true;
}

bool ConvertPointToWindow(views::View* view,
                          gfx::Point* point) {
  views::Widget* widget = view->GetWidget();
  if (!widget)
    return false;

  views::View::ConvertPointToWidget(view, point);

  if (widget->non_client_view()) {
    views::NonClientFrameView* non_client_frame_view =
        widget->non_client_view()->frame_view();
    if (non_client_frame_view) {
      // When using a custom drawn NonClientFrameView the native Window will not
      // know the actual client bounds. Adjust the native Window bounds for the
      // reported client bounds.
      const gfx::Rect& client_bounds =
          non_client_frame_view->GetBoundsForClientView();
      *point -= client_bounds.OffsetFromOrigin();
    }
  }

  return true;
}

bool ConvertPointFromWindow(views::View* view,
                            gfx::Point* point) {

  views::Widget* widget = view->GetWidget();
  if (!widget)
    return false;

  if (widget->non_client_view()) {
    views::NonClientFrameView* non_client_frame_view =
        widget->non_client_view()->frame_view();
    if (non_client_frame_view) {
      // When using a custom drawn NonClientFrameView the native Window will not
      // know the actual client bounds. Adjust the native Window bounds for the
      // reported client bounds.
      const gfx::Rect& client_bounds =
          non_client_frame_view->GetBoundsForClientView();
      *point += client_bounds.OffsetFromOrigin();
    }
  }

  views::View::ConvertPointFromWidget(view, point);

  return true;
}

gfx::NativeWindow GetNativeWindow(views::Widget* widget) {
  if (widget) {
    aura::Window* window = widget->GetNativeWindow();
    if (window)
      return window->GetRootWindow();
  }
  return nullptr;
}

CefWindowHandle GetWindowHandle(views::Widget* widget) {
  // Same implementation as views::HWNDForView() but cross-platform.
  if (widget) {
    aura::Window* window = widget->GetNativeWindow();
    if (window && window->GetRootWindow())
      return window->GetHost()->GetAcceleratedWidget();
  }
  return kNullWindowHandle;
}

}  // namespace view_util
