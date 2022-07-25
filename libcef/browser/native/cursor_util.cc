// Copyright 2020 The Chromium Embedded Framework Authors. Portions copyright
// 2012 The Chromium Authors. All rights reserved. Use of this source code is
// governed by a BSD-style license that can be found in the LICENSE file.

#include "libcef/browser/native/cursor_util.h"

#include "libcef/browser/browser_host_base.h"

#include "content/common/cursors/webcursor.h"
#include "content/public/browser/render_widget_host_view.h"
#include "ui/base/cursor/cursor_factory.h"
#include "ui/base/cursor/mojom/cursor_type.mojom.h"

#if defined(USE_AURA)
#include "ui/display/display_util.h"
#include "ui/wm/core/cursor_loader.h"
#endif

namespace cursor_util {

namespace {

#if defined(USE_AURA)

display::ScreenInfo GetScreenInfo(CefRefPtr<CefBrowser> browser) {
  display::ScreenInfo screen_info;

  bool screen_info_set = false;
  if (auto web_contents =
          static_cast<CefBrowserHostBase*>(browser.get())->GetWebContents()) {
    if (auto view = web_contents->GetRenderWidgetHostView()) {
      const auto screen_infos = view->GetScreenInfos();
      if (!screen_infos.screen_infos.empty()) {
        screen_info = screen_infos.current();
        screen_info_set = true;
      }
    }
  }

  if (!screen_info_set) {
    display::DisplayUtil::GetDefaultScreenInfo(&screen_info);
  }

  return screen_info;
}

display::Display::Rotation OrientationAngleToRotation(
    uint16_t orientation_angle) {
  // The Display rotation and the ScreenInfo orientation are not the same
  // angle. The former is the physical display rotation while the later is the
  // rotation required by the content to be shown properly on the screen, in
  // other words, relative to the physical display.
  if (orientation_angle == 0)
    return display::Display::ROTATE_0;
  if (orientation_angle == 90)
    return display::Display::ROTATE_270;
  if (orientation_angle == 180)
    return display::Display::ROTATE_180;
  if (orientation_angle == 270)
    return display::Display::ROTATE_90;
  NOTREACHED();
  return display::Display::ROTATE_0;
}

#endif  // defined(USE_AURA)

}  // namespace

bool OnCursorChange(CefRefPtr<CefBrowser> browser,
                    const ui::Cursor& ui_cursor) {
  auto client = browser->GetHost()->GetClient();
  if (!client)
    return false;
  auto handler = client->GetDisplayHandler();
  if (!handler)
    return false;

  const cef_cursor_type_t cursor_type =
      static_cast<cef_cursor_type_t>(ui_cursor.type());
  CefCursorInfo custom_cursor_info;
  if (ui_cursor.type() == ui::mojom::CursorType::kCustom) {
    custom_cursor_info.hotspot.x = ui_cursor.custom_hotspot().x();
    custom_cursor_info.hotspot.y = ui_cursor.custom_hotspot().y();
    custom_cursor_info.image_scale_factor = ui_cursor.image_scale_factor();
    custom_cursor_info.buffer = ui_cursor.custom_bitmap().getPixels();
    custom_cursor_info.size.width = ui_cursor.custom_bitmap().width();
    custom_cursor_info.size.height = ui_cursor.custom_bitmap().height();
  }

  bool handled = false;

#if defined(USE_AURA)
  wm::CursorLoader cursor_loader;
  scoped_refptr<ui::PlatformCursor> platform_cursor;
  CefCursorHandle native_cursor = kNullCursorHandle;

  ui::Cursor loaded_cursor = ui_cursor;

  if (ui_cursor.type() == ui::mojom::CursorType::kCustom) {
    platform_cursor = ui::CursorFactory::GetInstance()->CreateImageCursor(
        ui::mojom::CursorType::kCustom, ui_cursor.custom_bitmap(),
        ui_cursor.custom_hotspot());
  } else {
    const auto& screen_info = GetScreenInfo(browser);
    cursor_loader.SetDisplayData(
        OrientationAngleToRotation(screen_info.orientation_angle),
        screen_info.device_scale_factor);

    // Attempts to load the cursor via the platform or from pak resources.
    cursor_loader.SetPlatformCursor(&loaded_cursor);
    platform_cursor = loaded_cursor.platform();
  }

  if (platform_cursor) {
    native_cursor = cursor_util::ToCursorHandle(platform_cursor);
  }

  handled = handler->OnCursorChange(browser, native_cursor, cursor_type,
                                    custom_cursor_info);
#elif BUILDFLAG(IS_MAC)
  // |web_cursor| owns the resulting |native_cursor|.
  content::WebCursor web_cursor(ui_cursor);
  CefCursorHandle native_cursor = web_cursor.GetNativeCursor();
  handled = handler->OnCursorChange(browser, native_cursor, cursor_type,
                                    custom_cursor_info);
#else
  NOTIMPLEMENTED();
#endif

  return handled;
}

}  // namespace cursor_util
