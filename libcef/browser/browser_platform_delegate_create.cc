// Copyright 2015 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/browser_platform_delegate.h"

#include <utility>

#include "build/build_config.h"

#if defined(OS_WIN)
#include "libcef/browser/native/browser_platform_delegate_native_win.h"
#include "libcef/browser/osr/browser_platform_delegate_osr_win.h"
#elif defined(OS_MACOSX)
#include "libcef/browser/native/browser_platform_delegate_native_mac.h"
#include "libcef/browser/osr/browser_platform_delegate_osr_mac.h"
#elif defined(OS_LINUX)
#include "libcef/browser/native/browser_platform_delegate_native_linux.h"
#include "libcef/browser/osr/browser_platform_delegate_osr_linux.h"
#else
#error A delegate implementation is not available for your platform.
#endif

namespace {

scoped_ptr<CefBrowserPlatformDelegateNative> CreateNativeDelegate(
    const CefWindowInfo& window_info) {
#if defined(OS_WIN)
  return make_scoped_ptr(new CefBrowserPlatformDelegateNativeWin(window_info));
#elif defined(OS_MACOSX)
  return make_scoped_ptr(new CefBrowserPlatformDelegateNativeMac(window_info));
#elif defined(OS_LINUX)
  return make_scoped_ptr(
      new CefBrowserPlatformDelegateNativeLinux(window_info));
#endif
}

scoped_ptr<CefBrowserPlatformDelegateOsr> CreateOSRDelegate(
    scoped_ptr<CefBrowserPlatformDelegateNative> native_delegate) {
#if defined(OS_WIN)
  return make_scoped_ptr(
        new CefBrowserPlatformDelegateOsrWin(std::move(native_delegate)));
#elif defined(OS_MACOSX)
  return make_scoped_ptr(
        new CefBrowserPlatformDelegateOsrMac(std::move(native_delegate)));
#elif defined(OS_LINUX)
  return make_scoped_ptr(
        new CefBrowserPlatformDelegateOsrLinux(std::move(native_delegate)));
#endif
}

}  // namespace

// static
scoped_ptr<CefBrowserPlatformDelegate> CefBrowserPlatformDelegate::Create(
    const CefWindowInfo& window_info,
    const CefBrowserSettings& settings,
    CefRefPtr<CefClient> client) {
  scoped_ptr<CefBrowserPlatformDelegateNative> native_delegate =
      CreateNativeDelegate(window_info);
  if (window_info.windowless_rendering_enabled &&
      client->GetRenderHandler().get()) {
    return CreateOSRDelegate(std::move(native_delegate));
  }
  return std::move(native_delegate);
}
