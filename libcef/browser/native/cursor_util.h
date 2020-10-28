// Copyright 2020 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_NATIVE_CURSOR_UTIL_H_
#define CEF_LIBCEF_BROWSER_NATIVE_CURSOR_UTIL_H_

#include "include/internal/cef_types.h"

#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-forward.h"

class CefBrowserHostBase;

namespace cursor_util {

#if defined(USE_AURA)
cef_cursor_handle_t GetPlatformCursor(ui::mojom::CursorType type);
cef_cursor_handle_t ToCursorHandle(ui::PlatformCursor cursor);
#endif  // defined(USE_AURA)

// Returns true if the client handled the cursor change.
bool OnCursorChange(CefBrowserHostBase* browser, const ui::Cursor& ui_cursor);

}  // namespace cursor_util

#endif  // CEF_LIBCEF_BROWSER_NATIVE_CURSOR_UTIL_H_
