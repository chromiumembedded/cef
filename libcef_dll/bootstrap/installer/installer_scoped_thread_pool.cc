// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef_dll/bootstrap/installer/installer_scoped_thread_pool.h"

#include "base/feature_list.h"
#include "base/task/thread_pool/thread_pool_instance.h"

namespace cef_installer {

// static
std::atomic<int> ScopedThreadPool::refcount_{0};

ScopedThreadPool::ScopedThreadPool() {
  if (refcount_.fetch_add(1, std::memory_order_acq_rel) == 0) {
    if (!base::ThreadPoolInstance::Get()) {
      at_exit_.emplace();
      base::ThreadPoolInstance::CreateAndStartWithDefaultParams("CefInstaller");
      owns_pool_ = true;
    }
  }
}

ScopedThreadPool::~ScopedThreadPool() {
  if (refcount_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
    if (owns_pool_) {
      base::ThreadPoolInstance::Get()->JoinForTesting();
      base::ThreadPoolInstance::Set(nullptr);
      at_exit_.reset();

      // Clear any leftover EarlyFeatureAccessTracker entries to avoid failures
      // if ContentMainInitialize it called later.
      base::FeatureList::ResetEarlyFeatureAccessTrackerForTesting();
    }
  }
}

}  // namespace cef_installer
