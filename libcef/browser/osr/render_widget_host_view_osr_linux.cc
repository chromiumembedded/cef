// Copyright (c) 2014 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/osr/render_widget_host_view_osr.h"

#include "ui/base/cursor/cursor_factory.h"

#if defined(USE_X11)
#include "ui/base/x/x11_cursor.h"
#endif

CefCursorHandle CefRenderWidgetHostViewOSR::GetPlatformCursor(
    ui::mojom::CursorType type) {
  auto cursor = ui::CursorFactory::GetInstance()->GetDefaultCursor(type);
  if (cursor) {
    return ToCursorHandle(*cursor);
  }
  return 0;
}

CefCursorHandle CefRenderWidgetHostViewOSR::ToCursorHandle(
    ui::PlatformCursor cursor) {
#if defined(USE_X11)
  // See https://crbug.com/1029142 for background.
  return static_cast<CefCursorHandle>(
      static_cast<ui::X11Cursor*>(cursor)->xcursor());
#else
  return cursor;
#endif
}
