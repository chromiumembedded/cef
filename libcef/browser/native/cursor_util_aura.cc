// Copyright 2023 The Chromium Embedded Framework Authors. Portions copyright
// 2012 The Chromium Authors. All rights reserved. Use of this source code is
// governed by a BSD-style license that can be found in the LICENSE file.

#include "libcef/browser/native/cursor_util.h"

#include "libcef/browser/browser_host_base.h"

#include "content/common/cursors/webcursor.h"
#include "content/public/browser/render_widget_host_view.h"
#include "ui/base/cursor/cursor_factory.h"
#include "ui/base/cursor/mojom/cursor_type.mojom.h"
#include "ui/display/display.h"
#include "ui/display/display_util.h"
#include "ui/display/screen.h"
#include "ui/wm/core/cursor_loader.h"

#if BUILDFLAG(IS_LINUX)
#include "ui/base/ozone_buildflags.h"
#if BUILDFLAG(IS_OZONE_X11)
#include "ui/base/x/x11_cursor.h"
#elif BUILDFLAG(IS_OZONE)
#include "ui/ozone/common/bitmap_cursor.h"
#endif
#endif  // BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_WIN)
#include "ui/base/win/win_cursor.h"
#endif

namespace cursor_util {

namespace {

display::Display::Rotation OrientationAngleToRotation(
    uint16_t orientation_angle) {
  // The Display rotation and the ScreenInfo orientation are not the same
  // angle. The former is the physical display rotation while the later is the
  // rotation required by the content to be shown properly on the screen, in
  // other words, relative to the physical display.
  if (orientation_angle == 0) {
    return display::Display::ROTATE_0;
  }
  if (orientation_angle == 90) {
    return display::Display::ROTATE_270;
  }
  if (orientation_angle == 180) {
    return display::Display::ROTATE_180;
  }
  if (orientation_angle == 270) {
    return display::Display::ROTATE_90;
  }
  DCHECK(false);
  return display::Display::ROTATE_0;
}

// It would be better if CursorLoader took a ScreenInfo argument.
// See https://crbug.com/1149906#c33.
display::Display GetDisplay(CefRefPtr<CefBrowser> browser) {
  if (auto web_contents =
          static_cast<CefBrowserHostBase*>(browser.get())->GetWebContents()) {
    if (auto view = web_contents->GetRenderWidgetHostView()) {
      // Windowless browsers always return nullptr from GetNativeView().
      if (auto native_view = view->GetNativeView()) {
        return display::Screen::GetScreen()->GetDisplayNearestView(native_view);
      }

      // Make a minimal-effort fake Display object to satisfy the actual usage
      // by CursorLoader::SetDisplay.
      display::Display fake_display;
      auto screen_info = view->GetScreenInfo();
      fake_display.set_device_scale_factor(screen_info.device_scale_factor);
      fake_display.set_rotation(
          OrientationAngleToRotation(screen_info.orientation_angle));
      return fake_display;
    }
  }

  return display::Display::GetDefaultDisplay();
}

scoped_refptr<ui::PlatformCursor> ToPlatformCursor(
    CefRefPtr<CefBrowser> browser,
    const ui::Cursor& ui_cursor) {
  wm::CursorLoader cursor_loader;
  scoped_refptr<ui::PlatformCursor> platform_cursor;

  ui::Cursor loaded_cursor = ui_cursor;

  if (ui_cursor.type() == ui::mojom::CursorType::kCustom) {
    platform_cursor = ui::CursorFactory::GetInstance()->CreateImageCursor(
        ui::mojom::CursorType::kCustom, ui_cursor.custom_bitmap(),
        ui_cursor.custom_hotspot(), ui_cursor.image_scale_factor());
  } else {
    cursor_loader.SetDisplay(GetDisplay(browser));

    // Attempts to load the cursor via the platform or from pak resources.
    cursor_loader.SetPlatformCursor(&loaded_cursor);
    platform_cursor = loaded_cursor.platform();
  }

  return platform_cursor;
}

#if BUILDFLAG(IS_WIN)
using CursorType = ui::WinCursor;
inline cef_cursor_handle_t GetCursorHandleImpl(CursorType* cursor) {
  return cursor->hcursor();
}
#elif BUILDFLAG(IS_OZONE_X11)
// See https://crbug.com/1029142 for background.
using CursorType = ui::X11Cursor;
inline cef_cursor_handle_t GetCursorHandleImpl(CursorType* cursor) {
  return static_cast<cef_cursor_handle_t>(cursor->xcursor());
}
#elif BUILDFLAG(IS_OZONE)
using CursorType = ui::BitmapCursor;
inline cef_cursor_handle_t GetCursorHandleImpl(CursorType* cursor) {
  return static_cast<cef_cursor_handle_t>(cursor->platform_data());
}
#else
#error Unsupported platform
#endif

class ScopedCursorHandleImpl : public ScopedCursorHandle {
 public:
  explicit ScopedCursorHandleImpl(
      scoped_refptr<ui::PlatformCursor> platform_cursor) {
    if (platform_cursor) {
      cursor_ = CursorType::FromPlatformCursor(platform_cursor);
    }
  }

  cef_cursor_handle_t GetCursorHandle() override {
    if (cursor_) {
      return GetCursorHandleImpl(cursor_.get());
    }
    return kNullCursorHandle;
  }

 private:
  scoped_refptr<CursorType> cursor_;
};

}  // namespace

// static
std::unique_ptr<ScopedCursorHandle> ScopedCursorHandle::Create(
    CefRefPtr<CefBrowser> browser,
    const ui::Cursor& ui_cursor) {
  return std::make_unique<ScopedCursorHandleImpl>(
      ToPlatformCursor(browser, ui_cursor));
}

}  // namespace cursor_util
