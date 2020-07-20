// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFCLIENT_BROWSER_OSR_RENDERER_SETTINGS_H_
#define CEF_TESTS_CEFCLIENT_BROWSER_OSR_RENDERER_SETTINGS_H_
#pragma once

#include "include/internal/cef_types.h"

namespace client {

struct OsrRendererSettings {
  OsrRendererSettings()
      : show_update_rect(false),
        background_color(0),
        shared_texture_enabled(false),
        external_begin_frame_enabled(false),
        begin_frame_rate(0) {}

  // If true draw a border around update rectangles.
  bool show_update_rect;

  // Background color. Enables transparency if the alpha component is 0.
  cef_color_t background_color;

  // Render using shared textures. Supported on Windows only via D3D11.
  bool shared_texture_enabled;

  // Client implements a BeginFrame timer by calling
  // CefBrowserHost::SendExternalBeginFrame at the specified frame rate.
  bool external_begin_frame_enabled;
  int begin_frame_rate;
};

}  // namespace client

#endif  // CEF_TESTS_CEFCLIENT_BROWSER_OSR_RENDERER_SETTINGS_H_
