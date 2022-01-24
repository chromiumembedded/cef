// Copyright 2020 The Chromium Embedded Framework Authors. Portions copyright
// 2012 The Chromium Authors. All rights reserved. Use of this source code is
// governed by a BSD-style license that can be found in the LICENSE file.

#include "libcef/browser/native/cursor_util.h"

#include "libcef/browser/browser_host_base.h"

#include "content/common/cursors/webcursor.h"
#include "ui/base/cursor/cursor_factory.h"
#include "ui/base/cursor/mojom/cursor_type.mojom.h"

namespace cursor_util {

bool OnCursorChange(CefBrowserHostBase* browser, const ui::Cursor& ui_cursor) {
  auto client = browser->GetClient();
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
  CefCursorHandle platform_cursor;
  scoped_refptr<ui::PlatformCursor> image_cursor;

  if (ui_cursor.type() == ui::mojom::CursorType::kCustom) {
    image_cursor = ui::CursorFactory::GetInstance()->CreateImageCursor(
        ui::mojom::CursorType::kCustom, ui_cursor.custom_bitmap(),
        ui_cursor.custom_hotspot());
    platform_cursor = cursor_util::ToCursorHandle(image_cursor);
  } else {
    platform_cursor = cursor_util::GetPlatformCursor(ui_cursor.type());
  }

  handled = handler->OnCursorChange(browser, platform_cursor, cursor_type,
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
