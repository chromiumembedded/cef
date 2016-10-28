// Copyright 2015 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/osr/browser_platform_delegate_osr_mac.h"

#include <utility>

#include "libcef/browser/browser_host_impl.h"
#include "libcef/browser/osr/render_widget_host_view_osr.h"

CefBrowserPlatformDelegateOsrMac::CefBrowserPlatformDelegateOsrMac(
    std::unique_ptr<CefBrowserPlatformDelegateNative> native_delegate)
    : CefBrowserPlatformDelegateOsr(std::move(native_delegate)) {
}

CefWindowHandle CefBrowserPlatformDelegateOsrMac::GetHostWindowHandle() const {
  return native_delegate_->window_info().parent_view;
}

