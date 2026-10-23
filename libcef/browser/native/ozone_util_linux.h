// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_NATIVE_OZONE_UTIL_LINUX_H_
#define CEF_LIBCEF_BROWSER_NATIVE_OZONE_UTIL_LINUX_H_
#pragma once

#include <string_view>

namespace cef_ozone {

// Name of the Ozone platform that was selected at runtime ("x11", "wayland",
// "headless", ...). A single libcef binary may be built with multiple Ozone
// platforms compiled in, so build flags alone are not sufficient to determine
// which windowing code path is valid. Must be called after Ozone has been
// initialized.
std::string_view PlatformName();

// True if the active Ozone platform is X11.
bool IsX11();

// True if the active Ozone platform is Wayland.
bool IsWayland();

// True if the active Ozone platform can host a browser window inside a native
// window owned by the client application (CefWindowInfo::SetAsChild on X11,
// the three-argument SetAsChild on Wayland).
bool SupportsNativeWindowedBrowsers();

}  // namespace cef_ozone

#endif  // CEF_LIBCEF_BROWSER_NATIVE_OZONE_UTIL_LINUX_H_
