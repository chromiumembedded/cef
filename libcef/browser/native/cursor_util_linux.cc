// Copyright 2020 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/native/cursor_util.h"

#include "ui/base/cursor/cursor_factory.h"

#if defined(USE_X11)
#include "ui/base/x/x11_cursor.h"
#endif

namespace cursor_util {

cef_cursor_handle_t GetPlatformCursor(ui::mojom::CursorType type) {
  auto cursor = ui::CursorFactory::GetInstance()->GetDefaultCursor(type);
  if (cursor) {
    return ToCursorHandle(*cursor);
  }
  return 0;
}

cef_cursor_handle_t ToCursorHandle(ui::PlatformCursor cursor) {
#if defined(USE_X11)
  // See https://crbug.com/1029142 for background.
  return static_cast<cef_cursor_handle_t>(
      static_cast<ui::X11Cursor*>(cursor)->xcursor());
#else
  return cursor;
#endif
}

}  // namespace cursor_util
