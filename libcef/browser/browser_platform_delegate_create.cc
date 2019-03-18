// Copyright 2015 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/browser_platform_delegate.h"

#include <utility>

#include "libcef/browser/context.h"

#include "base/memory/ptr_util.h"
#include "build/build_config.h"

#include "libcef/browser/extensions/browser_platform_delegate_background.h"

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
    SkColor background_color,
    bool use_shared_texture,
    bool use_external_begin_frame) {
#if defined(OS_WIN)
  return std::make_unique<CefBrowserPlatformDelegateNativeWin>(
      window_info, background_color, use_shared_texture,
      use_external_begin_frame);
#elif defined(OS_MACOSX)
  return std::make_unique<CefBrowserPlatformDelegateNativeMac>(
      window_info, background_color);
#elif defined(OS_LINUX)
  return std::make_unique<CefBrowserPlatformDelegateNativeLinux>(
      window_info, background_color, use_external_begin_frame);
#endif
}

std::unique_ptr<CefBrowserPlatformDelegateOsr> CreateOSRDelegate(
    std::unique_ptr<CefBrowserPlatformDelegateNative> native_delegate) {
#if defined(OS_WIN)
  return std::make_unique<CefBrowserPlatformDelegateOsrWin>(
      std::move(native_delegate));
#elif defined(OS_MACOSX)
  return std::make_unique<CefBrowserPlatformDelegateOsrMac>(
      std::move(native_delegate));
#elif defined(OS_LINUX)
  return std::make_unique<CefBrowserPlatformDelegateOsrLinux>(
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
      create_params.client && create_params.client->GetRenderHandler().get();
  const SkColor background_color = CefContext::Get()->GetBackgroundColor(
      &create_params.settings, is_windowless ? STATE_ENABLED : STATE_DISABLED);

  bool use_shared_texture = false;
  bool use_external_begin_frame = false;

  if (is_windowless) {
    use_shared_texture = create_params.window_info &&
                         create_params.window_info->shared_texture_enabled;

    use_external_begin_frame =
        create_params.window_info &&
        create_params.window_info->external_begin_frame_enabled;
  }

  if (create_params.window_info) {
    std::unique_ptr<CefBrowserPlatformDelegateNative> native_delegate =
        CreateNativeDelegate(*create_params.window_info.get(), background_color,
                             use_shared_texture, use_external_begin_frame);
    if (is_windowless)
      return CreateOSRDelegate(std::move(native_delegate));
    return std::move(native_delegate);
  } else if (create_params.extension_host_type ==
             extensions::VIEW_TYPE_EXTENSION_BACKGROUND_PAGE) {
    // Creating a background extension host without a window.
    std::unique_ptr<CefBrowserPlatformDelegateNative> native_delegate =
        CreateNativeDelegate(CefWindowInfo(), background_color,
                             use_shared_texture, use_external_begin_frame);
    return std::make_unique<CefBrowserPlatformDelegateBackground>(
        std::move(native_delegate));
  }
#if defined(USE_AURA)
  else {
    // CefWindowInfo is not used in this case.
    std::unique_ptr<CefBrowserPlatformDelegateNative> native_delegate =
        CreateNativeDelegate(CefWindowInfo(), background_color,
                             use_shared_texture, use_external_begin_frame);
    return std::make_unique<CefBrowserPlatformDelegateViews>(
        std::move(native_delegate),
        static_cast<CefBrowserViewImpl*>(create_params.browser_view.get()));
  }
#endif  // defined(USE_AURA)

  NOTREACHED();
  return nullptr;
}
