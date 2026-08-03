// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "cef/include/internal/cef_types_linux.h"
#include "cef/libcef/browser/context.h"
#include "ui/base/ozone_buildflags.h"

#if BUILDFLAG(SUPPORTS_OZONE_WAYLAND)
#include "ui/ozone/platform/wayland/wayland_embedder.h"
#endif

CEF_EXPORT void cef_set_wayland_display(cef_wayland_display_handle_t display) {
  if (CONTEXT_STATE_VALID()) {
    LOG(ERROR) << "cef_set_wayland_display() must be called before "
                  "CefInitialize(); Chromium creates its Wayland connection "
                  "while initializing Ozone";
    return;
  }

#if BUILDFLAG(SUPPORTS_OZONE_WAYLAND)
  // Ozone only reads this when it starts up, which happens inside
  // CefInitialize, so recording it now is both necessary and sufficient.
  ui::SetExternalWaylandDisplay(static_cast<wl_display*>(display));
#endif
}

CEF_EXPORT cef_wayland_display_handle_t cef_get_wayland_display() {
#if BUILDFLAG(SUPPORTS_OZONE_WAYLAND)
  return ui::GetWaylandDisplay();
#else
  return nullptr;
#endif
}
