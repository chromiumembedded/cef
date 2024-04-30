// Copyright 2024 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef/browser/chrome/chrome_devtools_window_runner.h"
#include "cef/libcef/browser/devtools/devtools_window_runner.h"
#include "cef/libcef/features/runtime.h"

#if BUILDFLAG(ENABLE_ALLOY_BOOTSTRAP)
#include "cef/libcef/browser/alloy/devtools/alloy_devtools_window_runner.h"
#endif

// static
std::unique_ptr<CefDevToolsWindowRunner> CefDevToolsWindowRunner::Create() {
#if BUILDFLAG(ENABLE_ALLOY_BOOTSTRAP)
  if (cef::IsAlloyRuntimeEnabled()) {
    return std::make_unique<AlloyDevToolsWindowRunner>();
  }
#endif
  return std::make_unique<ChromeDevToolsWindowRunner>();
}
