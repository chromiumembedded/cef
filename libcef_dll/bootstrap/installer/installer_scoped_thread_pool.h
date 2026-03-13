// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_SCOPED_THREAD_POOL_H_
#define CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_SCOPED_THREAD_POOL_H_

#include <atomic>
#include <optional>

#include "base/at_exit.h"

namespace cef_installer {

// Creates and tears down a base::ThreadPoolInstance (and the AtExitManager it
// requires) when no pool exists yet. No-op when a pool is already running
// (e.g. Chromium is initialized). Thread-safe: multiple instances may coexist
// across threads — a static refcount ensures the last instance performs
// teardown.
class ScopedThreadPool {
 public:
  ScopedThreadPool();
  ~ScopedThreadPool();

  ScopedThreadPool(const ScopedThreadPool&) = delete;
  ScopedThreadPool& operator=(const ScopedThreadPool&) = delete;

 private:
  static std::atomic<int> refcount_;
  std::optional<base::AtExitManager> at_exit_;
  bool owns_pool_ = false;
};

}  // namespace cef_installer

#endif  // CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_SCOPED_THREAD_POOL_H_
