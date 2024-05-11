// Copyright 2016 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "cef/libcef/browser/views/view_util.h"

#include <utility>

#include "base/memory/raw_ptr.h"
#include "cef/include/cef_color_ids.h"
#include "cef/libcef/browser/views/view_adapter.h"
#include "cef/libcef/browser/views/widget.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_manager.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/background.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/non_client_view.h"

#if BUILDFLAG(IS_WIN)
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

    view->SetUserData(UserDataKey(), base::WrapUnique(new UserData(cef_view)));
  }

  static CefRefPtr<CefView> GetFor(const views::View* view) {
    DCHECK(view);
    UserData* data = static_cast<UserData*>(view->GetUserData(UserDataKey()));
    if (data) {
      return data->view_ref_.get();
    }
    return nullptr;
  }

  // Transfer ownership of the views::View to the caller. The views::View will
  // gain a ref-counted reference to the CefView and the CefView will keep an
  // unowned reference to the views::View. Destruction of the views::View will
  // release the ref-counted reference to the CefView.
  [[nodiscard]] static std::unique_ptr<views::View> PassOwnership(
      CefRefPtr<CefView> cef_view) {
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
  friend std::default_delete<UserData>;

  explicit UserData(CefRefPtr<CefView> cef_view) : view_ref_(cef_view.get()) {
    DCHECK(view_ref_);
  }

  ~UserData() override {
    if (view_) {
      // The CefView does not own the views::View. Remove the CefView's
      // reference to the views::View.
      CefViewAdapter::GetFor(view_)->Detach();
    }
  }

  void TakeReference() { view_ = view_ref_.get(); }

  void ReleaseReference() { view_ = nullptr; }

  static void* UserDataKey() {
    // We just need a unique constant. Use the address of a static that
    // COMDAT folding won't touch in an optimizing linker.
    static int data_key = 0;
    return reinterpret_cast<void*>(&data_key);
  }

  CefRefPtr<CefView> view_;
  raw_ptr<CefView> view_ref_;
};

// Based on Widget::GetNativeTheme.
const ui::NativeTheme* GetDefaultNativeTheme() {
  return ui::NativeTheme::GetInstanceForNativeUi();
}

// Based on Widget::GetColorProviderKey.
ui::ColorProviderKey GetDefaultColorProviderKey() {
  return GetDefaultNativeTheme()->GetColorProviderKey(/*custom_theme=*/nullptr);
}

// Based on Widget::GetColorProvider.
ui::ColorProvider* GetDefaultColorProvider() {
  return ui::ColorProviderManager::Get().GetColorProviderFor(
      GetDefaultColorProviderKey());
}

}  // namespace

const char kDefaultFontList[] = "Arial, Helvetica, 14px";

void Register(CefRefPtr<CefView> view) {
  UserData::Register(view);
}

CefRefPtr<CefView> GetFor(const views::View* view, bool find_known_parent) {
  if (!view) {
    return nullptr;
  }

  if (!find_known_parent) {
    return UserData::GetFor(view);
  }

  CefRefPtr<CefView> cef_view;
  const views::View* current_view = view;
  do {
    cef_view = UserData::GetFor(current_view);
    if (cef_view) {
      break;
    }
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

  // If |widget| is an overlay, retrieve the host Widget.
  if (widget) {
    if (auto widget_view = GetHostView(widget)) {
      widget = widget_view->GetWidget();
    }
  }

  if (widget) {
    // The views::WidgetDelegate should be a CefWindowView and |content_view|
    // should be the same CefWindowView. However, just in case the views::Widget
    // was created by something else let's go about this the safer way.
    views::View* content_view = widget->widget_delegate()->GetContentsView();
    CefRefPtr<CefView> cef_view = GetFor(content_view, false);
    if (cef_view && cef_view->AsPanel()) {
      window = cef_view->AsPanel()->AsWindow();
    }

    // The Window should always exist if we created the views::Widget.
    DCHECK(window);
  }

  return window;
}

display::Display GetDisplayNearestPoint(const gfx::Point& point,
                                        bool input_pixel_coords) {
  gfx::Point find_point = point;
#if BUILDFLAG(IS_WIN)
  if (input_pixel_coords) {
    find_point = gfx::ToFlooredPoint(
        display::win::ScreenWin::ScreenToDIPPoint(gfx::PointF(point)));
  }
#endif
  return display::Screen::GetScreen()->GetDisplayNearestPoint(find_point);
}

display::Display GetDisplayMatchingBounds(const gfx::Rect& bounds,
                                          bool input_pixel_coords) {
  gfx::Rect find_bounds = bounds;
#if BUILDFLAG(IS_WIN)
  if (input_pixel_coords) {
    find_bounds =
        display::win::ScreenWin::ScreenToDIPRect(nullptr, find_bounds);
  }
#endif
  return display::Screen::GetScreen()->GetDisplayMatching(find_bounds);
}

void ConvertPointFromPixels(gfx::Point* point, float device_scale_factor) {
  *point = gfx::ToFlooredPoint(
      gfx::ScalePoint(gfx::PointF(*point), 1.0f / device_scale_factor));
}

void ConvertPointToPixels(gfx::Point* point, float device_scale_factor) {
  *point = gfx::ToFlooredPoint(
      gfx::ScalePoint(gfx::PointF(*point), device_scale_factor));
}

#if BUILDFLAG(IS_WIN)
gfx::Point ConvertPointFromPixels(const gfx::Point& point) {
  return gfx::ToFlooredPoint(
      display::win::ScreenWin::ScreenToDIPPoint(gfx::PointF(point)));
}

gfx::Point ConvertPointToPixels(const gfx::Point& point) {
  return display::win::ScreenWin::DIPToScreenPoint(point);
}

gfx::Rect ConvertRectFromPixels(const gfx::Rect& rect) {
  return display::win::ScreenWin::ScreenToDIPRect(nullptr, rect);
}

gfx::Rect ConvertRectToPixels(const gfx::Rect& rect) {
  return display::win::ScreenWin::DIPToScreenRect(nullptr, rect);
}
#endif  // BUILDFLAG(IS_WIN)

bool ConvertPointToScreen(views::View* view,
                          gfx::Point* point,
                          bool output_pixel_coords) {
  if (!view->GetWidget()) {
    return false;
  }

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
  if (!view->GetWidget()) {
    return false;
  }

  if (input_pixel_coords) {
    const display::Display& display = GetDisplayNearestPoint(*point, true);
    ConvertPointFromPixels(point, display.device_scale_factor());
  }

  views::View::ConvertPointFromScreen(view, point);

  return true;
}

bool ConvertPointToWindow(views::View* view, gfx::Point* point) {
  views::Widget* widget = view->GetWidget();
  if (!widget) {
    return false;
  }

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

bool ConvertPointFromWindow(views::View* view, gfx::Point* point) {
  views::Widget* widget = view->GetWidget();
  if (!widget) {
    return false;
  }

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

SkColor GetColor(const views::View* view, ui::ColorId id) {
  // Verify that our enum matches Chromium's values.
  static_assert(static_cast<int>(CEF_ChromeColorsEnd) ==
                    static_cast<int>(kChromeColorsEnd),
                "enum mismatch");

  // |color_provider| will be nullptr if |view| has not yet been added to a
  // Widget.
  if (const auto* color_provider = view->GetColorProvider()) {
    return color_provider->GetColor(id);
  }

  return GetDefaultColorProvider()->GetColor(id);
}

void SetColor(views::View* view, ui::ColorId id, SkColor color) {
  auto* color_provider = view->GetColorProvider();
  if (!color_provider) {
    color_provider = GetDefaultColorProvider();
  }

  if (color_provider) {
    color_provider->SetColorForTesting(id, color);
  }
}

std::optional<SkColor> GetBackgroundColor(const views::View* view,
                                          bool allow_transparent) {
  // Return the configured background color, if any.
  if (view->background()) {
    return view->background()->get_color();
  }

  // If the containing Widget is an overlay then it has a transparent background
  // by default.
  if (allow_transparent) {
    const bool is_overlay_hosted =
        view->GetWidget() && GetHostView(view->GetWidget()) != nullptr;
    if (is_overlay_hosted) {
      return std::nullopt;
    }
  }

  // Return the default background color.
  return GetColor(view, ui::kColorPrimaryBackground);
}

bool ShouldUseDarkTheme(views::Widget* widget) {
  DCHECK(widget);

  Profile* profile = nullptr;
  if (auto* cef_widget = CefWidget::GetForWidget(widget)) {
    profile = cef_widget->GetThemeProfile();
  }

  if (profile) {
    const auto* theme_service = ThemeServiceFactory::GetForProfile(profile);
    const auto browser_color_scheme = theme_service->GetBrowserColorScheme();
    if (browser_color_scheme != ThemeService::BrowserColorScheme::kSystem) {
      // Override the native theme value.
      return browser_color_scheme == ThemeService::BrowserColorScheme::kDark;
    }
  }

  // Use the native theme value.
  return widget->GetNativeTheme()->ShouldUseDarkColors();
}

}  // namespace view_util
