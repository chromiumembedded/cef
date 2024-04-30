// Copyright 2024 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_CHROME_CHROME_DEVTOOLS_WINDOW_RUNNER_H_
#define CEF_LIBCEF_BROWSER_CHROME_CHROME_DEVTOOLS_WINDOW_RUNNER_H_
#pragma once

#include "base/memory/weak_ptr.h"
#include "cef/libcef/browser/devtools/devtools_window_runner.h"

class ChromeBrowserHostImpl;

// Creates and runs a DevTools window instance. Only accessed on the UI thread.
class ChromeDevToolsWindowRunner : public CefDevToolsWindowRunner {
 public:
  ChromeDevToolsWindowRunner() = default;

  ChromeDevToolsWindowRunner(const ChromeDevToolsWindowRunner&) = delete;
  ChromeDevToolsWindowRunner& operator=(const ChromeDevToolsWindowRunner&) =
      delete;

  // CefDevToolsWindowRunner methods:
  void ShowDevTools(CefBrowserHostBase* opener,
                    std::unique_ptr<CefShowDevToolsParams> params) override;
  void CloseDevTools() override;
  bool HasDevTools() override;

  std::unique_ptr<CefShowDevToolsParams> TakePendingParams();

  void SetDevToolsBrowserHost(
      base::WeakPtr<ChromeBrowserHostImpl> browser_host);

 private:
  std::unique_ptr<CefShowDevToolsParams> pending_params_;

  base::WeakPtr<ChromeBrowserHostImpl> browser_host_;
};

#endif  // CEF_LIBCEF_BROWSER_CHROME_CHROME_DEVTOOLS_WINDOW_RUNNER_H_
