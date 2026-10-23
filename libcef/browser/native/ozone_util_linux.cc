// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef/browser/native/ozone_util_linux.h"

#include "ui/base/ozone_buildflags.h"
#include "ui/ozone/platform_selection.h"

namespace cef_ozone {

std::string_view PlatformName() {
  // ui::GetOzonePlatformName() reports the platform actually selected for this
  // process, unlike the SUPPORTS_OZONE_* build flags, which only say what was
  // compiled in. DesktopWindowTreeHostLinux::AddAdditionalInitProperties()
  // consults the same function.
  const char* name = ui::GetOzonePlatformName();
  return name ? std::string_view(name) : std::string_view();
}

bool IsX11() {
#if BUILDFLAG(SUPPORTS_OZONE_X11)
  return PlatformName() == "x11";
#else
  return false;
#endif
}

bool IsWayland() {
#if BUILDFLAG(SUPPORTS_OZONE_WAYLAND)
  return PlatformName() == "wayland";
#else
  return false;
#endif
}

bool SupportsNativeWindowedBrowsers() {
  return IsX11() || IsWayland();
}

}  // namespace cef_ozone
