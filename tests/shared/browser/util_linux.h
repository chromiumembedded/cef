// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_SHARED_BROWSER_UTIL_LINUX_H_
#define CEF_TESTS_SHARED_BROWSER_UTIL_LINUX_H_
#pragma once

#include "include/cef_command_line.h"

namespace client {

// Returns true if Chromium will select the Wayland Ozone platform for the
// browser process with |command_line|. Mirrors the selection logic in
// ui/linux/display_server_utils.cc. Safe to call before CefInitialize.
bool IsRunningOnWayland(CefRefPtr<CefCommandLine> command_line);

}  // namespace client

#endif  // CEF_TESTS_SHARED_BROWSER_UTIL_LINUX_H_
