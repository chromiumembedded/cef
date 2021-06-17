// Copyright (c) 2019 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef_dll/shutdown_checker.h"

#include <atomic>

#include "include/base/cef_logging.h"

namespace shutdown_checker {

#if DCHECK_IS_ON()

namespace {

std::atomic_bool g_cef_shutdown{false};

bool IsCefShutdown() {
  return g_cef_shutdown.load();
}

void SetCefShutdown() {
  g_cef_shutdown.store(true);
}

}  // namespace

void AssertNotShutdown() {
  DCHECK(!IsCefShutdown())
      << "Object reference incorrectly held at CefShutdown";
}

void SetIsShutdown() {
  DCHECK(!IsCefShutdown());
  SetCefShutdown();
}

#else  // !DCHECK_IS_ON()

void AssertNotShutdown() {}

#endif  // !DCHECK_IS_ON()

}  // namespace shutdown_checker
