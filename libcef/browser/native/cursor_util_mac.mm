// Copyright 2023 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "include/internal/cef_types_mac.h"
#include "libcef/browser/native/cursor_util.h"

#import "ui/base/cocoa/cursor_utils.h"

namespace cursor_util {

namespace {

class ScopedCursorHandleImpl : public ScopedCursorHandle {
 public:
  explicit ScopedCursorHandleImpl(NSCursor* native_cursor) {
    if (native_cursor) {
      cursor_ = native_cursor;
    }
  }

  cef_cursor_handle_t GetCursorHandle() override {
    return CAST_NSCURSOR_TO_CEF_CURSOR_HANDLE(cursor_);
  }

 private:
  NSCursor* __strong cursor_;
};

}  // namespace

// static
std::unique_ptr<ScopedCursorHandle> ScopedCursorHandle::Create(
    CefRefPtr<CefBrowser> /*browser*/,
    const ui::Cursor& ui_cursor) {
  return std::make_unique<ScopedCursorHandleImpl>(
      ui::GetNativeCursor(ui_cursor));
}

}  // namespace cursor_util
