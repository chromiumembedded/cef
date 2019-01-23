// Copyright (c) 2019 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef_dll/shutdown_checker.h"

// For compatibility with older client compiler versions only use std::atomic
// on the library side.
#if defined(BUILDING_CEF_SHARED)
#include <atomic>
#else
#include "include/base/cef_atomic_ref_count.h"
#endif

#include "include/base/cef_logging.h"

namespace shutdown_checker {

#if DCHECK_IS_ON()

namespace {

#if defined(BUILDING_CEF_SHARED)

std::atomic_bool g_cef_shutdown{false};

bool IsCefShutdown() {
  return g_cef_shutdown.load();
}

void SetCefShutdown() {
  g_cef_shutdown.store(true);
}

#else  // !defined(BUILDING_CEF_SHARED)

base::AtomicRefCount g_cef_shutdown ATOMIC_DECLARATION;

bool IsCefShutdown() {
  return !base::AtomicRefCountIsZero(&g_cef_shutdown);
}

void SetCefShutdown() {
  base::AtomicRefCountInc(&g_cef_shutdown);
}

#endif  // !defined(BUILDING_CEF_SHARED)

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
