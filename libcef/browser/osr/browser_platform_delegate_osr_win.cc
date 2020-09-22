// Copyright 2015 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/osr/browser_platform_delegate_osr_win.h"

#include <utility>

CefBrowserPlatformDelegateOsrWin::CefBrowserPlatformDelegateOsrWin(
    std::unique_ptr<CefBrowserPlatformDelegateNative> native_delegate,
    bool use_shared_texture,
    bool use_external_begin_frame)
    : CefBrowserPlatformDelegateOsr(std::move(native_delegate),
                                    use_shared_texture,
                                    use_external_begin_frame) {}

CefWindowHandle CefBrowserPlatformDelegateOsrWin::GetHostWindowHandle() const {
  return native_delegate_->window_info().parent_window;
}
