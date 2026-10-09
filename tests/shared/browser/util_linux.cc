// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/shared/browser/util_linux.h"

#include <cstdlib>
#include <cstring>

#include "tests/shared/common/client_switches.h"

namespace client {

bool IsRunningOnWayland(CefRefPtr<CefCommandLine> command_line) {
  if (command_line->HasSwitch(switches::kOzonePlatform)) {
    return command_line->GetSwitchValue(switches::kOzonePlatform) == "wayland";
  }

  // Same default as ui::SetOzonePlatformForLinuxIfNeeded().
  const char* session_type = getenv("XDG_SESSION_TYPE");
  return session_type && strcmp(session_type, "wayland") == 0;
}

}  // namespace client
