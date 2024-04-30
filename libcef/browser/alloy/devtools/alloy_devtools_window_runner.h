// Copyright 2024 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_ALLOY_DEVTOOLS_ALLOY_DEVTOOLS_WINDOW_RUNNER_H_
#define CEF_LIBCEF_BROWSER_ALLOY_DEVTOOLS_ALLOY_DEVTOOLS_WINDOW_RUNNER_H_
#pragma once

#include "base/memory/weak_ptr.h"
#include "cef/libcef/browser/devtools/devtools_window_runner.h"

class CefDevToolsFrontend;

// Creates and runs a DevTools window instance. Only accessed on the UI thread.
class AlloyDevToolsWindowRunner : public CefDevToolsWindowRunner {
 public:
  AlloyDevToolsWindowRunner() = default;

  AlloyDevToolsWindowRunner(const AlloyDevToolsWindowRunner&) = delete;
  AlloyDevToolsWindowRunner& operator=(const AlloyDevToolsWindowRunner&) =
      delete;

  // CefDevToolsWindowRunner methods:
  void ShowDevTools(CefBrowserHostBase* opener,
                    std::unique_ptr<CefShowDevToolsParams> params) override;
  void CloseDevTools() override;
  bool HasDevTools() override;

 private:
  void OnFrontEndDestroyed();

  // CefDevToolsFrontend will delete itself when the frontend WebContents is
  // destroyed.
  CefDevToolsFrontend* devtools_frontend_ = nullptr;

  base::WeakPtrFactory<AlloyDevToolsWindowRunner> weak_ptr_factory_{this};
};

#endif  // CEF_LIBCEF_BROWSER_ALLOY_DEVTOOLS_ALLOY_DEVTOOLS_WINDOW_RUNNER_H_
