// Copyright 2024 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/devtools/devtools_window_runner.h"

#include "libcef/browser/alloy/devtools/alloy_devtools_window_runner.h"
#include "libcef/browser/chrome/chrome_devtools_window_runner.h"
#include "libcef/features/runtime.h"

// static
std::unique_ptr<CefDevToolsWindowRunner> CefDevToolsWindowRunner::Create() {
  if (cef::IsChromeRuntimeEnabled()) {
    return std::make_unique<ChromeDevToolsWindowRunner>();
  }
  return std::make_unique<AlloyDevToolsWindowRunner>();
}
