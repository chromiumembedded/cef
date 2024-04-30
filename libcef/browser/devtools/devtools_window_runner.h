// Copyright 2024 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_DEVTOOLS_DEVTOOLS_WINDOW_RUNNER_H_
#define CEF_LIBCEF_BROWSER_DEVTOOLS_DEVTOOLS_WINDOW_RUNNER_H_
#pragma once

#include <memory>

#include "cef/include/cef_client.h"

class CefBrowserHostBase;

// Parameters passed to ShowDevTools.
struct CefShowDevToolsParams {
  CefShowDevToolsParams(const CefWindowInfo& windowInfo,
                        CefRefPtr<CefClient> client,
                        const CefBrowserSettings& settings,
                        const CefPoint& inspect_element_at)
      : window_info_(windowInfo),
        client_(client),
        settings_(settings),
        inspect_element_at_(inspect_element_at) {}

  CefWindowInfo window_info_;
  CefRefPtr<CefClient> client_;
  CefBrowserSettings settings_;
  CefPoint inspect_element_at_;
};

// Creates and runs a DevTools window instance. Only accessed on the UI thread.
class CefDevToolsWindowRunner {
 public:
  // Creates the appropriate runner type based on the current runtime.
  static std::unique_ptr<CefDevToolsWindowRunner> Create();

  // See documentation on CefBrowserHost methods of the same name.
  virtual void ShowDevTools(CefBrowserHostBase* opener,
                            std::unique_ptr<CefShowDevToolsParams> params) = 0;
  virtual void CloseDevTools() = 0;
  virtual bool HasDevTools() = 0;

  virtual ~CefDevToolsWindowRunner() = default;
};

#endif  // CEF_LIBCEF_BROWSER_DEVTOOLS_DEVTOOLS_WINDOW_RUNNER_H_
