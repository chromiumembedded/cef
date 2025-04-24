// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFCLIENT_BROWSER_OSR_RENDERER_SETTINGS_H_
#define CEF_TESTS_CEFCLIENT_BROWSER_OSR_RENDERER_SETTINGS_H_
#pragma once

#include "include/internal/cef_types.h"

namespace client {

struct OsrRendererSettings {
  OsrRendererSettings() = default;

  // If true draw a border around update rectangles.
  bool show_update_rect = false;

  // If true return real screen bounds from GetRootScreenRect/GetScreenInfo.
  // - Allows window.outerWidth/Height and window.screenX/Y to return correct
  //   values.
  // - Allows JavaScript window.moveTo/By() and window.resizeTo/By() to provide
  //   bounds that include the window frame.
  // - Causes HTML select popups to be cropped (limitation of cefclient impl).
  bool real_screen_bounds = true;

  // Background color. Enables transparency if the alpha component is 0.
  cef_color_t background_color = 0;

  // Render using shared textures. Supported on Windows only via D3D11.
  bool shared_texture_enabled = false;

  // Client implements a BeginFrame timer by calling
  // CefBrowserHost::SendExternalBeginFrame at the specified frame rate.
  bool external_begin_frame_enabled = false;
  int begin_frame_rate = 0;
};

}  // namespace client

#endif  // CEF_TESTS_CEFCLIENT_BROWSER_OSR_RENDERER_SETTINGS_H_
