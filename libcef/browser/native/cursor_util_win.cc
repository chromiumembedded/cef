// Copyright 2020 The Chromium Embedded Framework Authors. Portions copyright
// 2012 The Chromium Authors. All rights reserved. Use of this source code is
// governed by a BSD-style license that can be found in the LICENSE file.

#include "libcef/browser/native/cursor_util.h"

#include "ui/base/win/win_cursor.h"

namespace cursor_util {

cef_cursor_handle_t ToCursorHandle(scoped_refptr<ui::PlatformCursor> cursor) {
  return ui::WinCursor::FromPlatformCursor(cursor)->hcursor();
}

}  // namespace cursor_util
