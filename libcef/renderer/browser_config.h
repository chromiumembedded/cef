// Copyright 2025 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef CEF_LIBCEF_RENDERER_BROWSER_CONFIG_H_
#define CEF_LIBCEF_RENDERER_BROWSER_CONFIG_H_
#pragma once

namespace cef {

// CefBrowser configuration determined prior to CefBrowserHost creation (in
// CefBrowserPlatformDelegate::GetBrowserConfig) and passed to the renderer
// process via the GetNewBrowserInfo Mojo request.
struct BrowserConfig {
  bool is_windowless;
  bool print_preview_enabled;
  bool move_pip_enabled;
};

}  // namespace cef

#endif  // CEF_LIBCEF_RENDERER_BROWSER_CONFIG_H_
