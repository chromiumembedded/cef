// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFTESTS_OS_RENDERING_UNITTEST_MAC_H_
#define CEF_TESTS_CEFTESTS_OS_RENDERING_UNITTEST_MAC_H_

#include "include/cef_base.h"

namespace osr_unittests {

CefWindowHandle GetFakeView();

// Prevent App Nap from coalescing the windowless timing test's timers.
class ScopedTimingActivity {
 public:
  ScopedTimingActivity();
  ~ScopedTimingActivity();

  ScopedTimingActivity(const ScopedTimingActivity&) = delete;
  ScopedTimingActivity& operator=(const ScopedTimingActivity&) = delete;

 private:
  void* activity_;
};

}  // namespace osr_unittests

#endif  // CEF_TESTS_CEFTESTS_OS_RENDERING_UNITTEST_MAC_H_
