// Copyright 2015 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/browser_platform_delegate.h"

#include <utility>

#include "libcef/browser/context.h"

#include "base/memory/ptr_util.h"
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

#if defined(USE_AURA)
#include "libcef/browser/views/browser_platform_delegate_views.h"
#endif

namespace {

std::unique_ptr<CefBrowserPlatformDelegateNative> CreateNativeDelegate(
    const CefWindowInfo& window_info,
    SkColor background_color) {
#if defined(OS_WIN)
  return base::MakeUnique<CefBrowserPlatformDelegateNativeWin>(
      window_info, background_color);
#elif defined(OS_MACOSX)
  return base::MakeUnique<CefBrowserPlatformDelegateNativeMac>(
      window_info, background_color);
#elif defined(OS_LINUX)
  return base::MakeUnique<CefBrowserPlatformDelegateNativeLinux>(
      window_info, background_color);
#endif
}

std::unique_ptr<CefBrowserPlatformDelegateOsr> CreateOSRDelegate(
    std::unique_ptr<CefBrowserPlatformDelegateNative> native_delegate) {
#if defined(OS_WIN)
  return base::MakeUnique<CefBrowserPlatformDelegateOsrWin>(
      std::move(native_delegate));
#elif defined(OS_MACOSX)
  return base::MakeUnique<CefBrowserPlatformDelegateOsrMac>(
      std::move(native_delegate));
#elif defined(OS_LINUX)
  return base::MakeUnique<CefBrowserPlatformDelegateOsrLinux>(
      std::move(native_delegate));
#endif
}

}  // namespace

// static
std::unique_ptr<CefBrowserPlatformDelegate> CefBrowserPlatformDelegate::Create(
    CefBrowserHostImpl::CreateParams& create_params) {
  const bool is_windowless =
      create_params.window_info &&
      create_params.window_info->windowless_rendering_enabled &&
      create_params.client &&
      create_params.client->GetRenderHandler().get();
  const SkColor background_color = CefContext::Get()->GetBackgroundColor(
      &create_params.settings, is_windowless ? STATE_ENABLED : STATE_DISABLED);

  if (create_params.window_info) {
    std::unique_ptr<CefBrowserPlatformDelegateNative> native_delegate =
        CreateNativeDelegate(*create_params.window_info.get(),
                             background_color);
    if (is_windowless)
      return CreateOSRDelegate(std::move(native_delegate));
    return std::move(native_delegate);
  }
#if defined(USE_AURA)
  else {
    // CefWindowInfo is not used in this case.
    std::unique_ptr<CefBrowserPlatformDelegateNative> native_delegate =
        CreateNativeDelegate(CefWindowInfo(), background_color);
    return base::MakeUnique<CefBrowserPlatformDelegateViews>(
        std::move(native_delegate),
        static_cast<CefBrowserViewImpl*>(create_params.browser_view.get()));
  }
#endif  // defined(USE_AURA)

  NOTREACHED();
  return nullptr;
}
