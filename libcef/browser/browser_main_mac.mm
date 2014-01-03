// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/browser_main.h"
#include "libcef/browser/context.h"

#include "content/browser/renderer_host/compositing_iosurface_shader_programs_mac.h"

void CefBrowserMainParts::PlatformInitialize() {
  const CefSettings& settings = CefContext::Get()->settings();
  if (CefColorGetA(settings.background_color) != 0) {
    const float r =
        static_cast<float>(CefColorGetR(settings.background_color)) / 255.0f;
    const float g =
        static_cast<float>(CefColorGetG(settings.background_color)) / 255.0f;
    const float b =
        static_cast<float>(CefColorGetB(settings.background_color)) / 255.0f;
    content::CompositingIOSurfaceShaderPrograms::SetBackgroundColor(r, g, b);
  }
}

void CefBrowserMainParts::PlatformCleanup() {
}
