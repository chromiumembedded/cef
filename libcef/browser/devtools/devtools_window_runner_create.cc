// Copyright 2024 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef/browser/chrome/chrome_devtools_window_runner.h"
#include "cef/libcef/browser/devtools/devtools_window_runner.h"

// static
std::unique_ptr<CefDevToolsWindowRunner> CefDevToolsWindowRunner::Create() {
  return std::make_unique<ChromeDevToolsWindowRunner>();
}
