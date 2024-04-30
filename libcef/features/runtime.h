// Copyright 2020 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_FEATURES_RUNTIME_H_
#define CEF_LIBCEF_FEATURES_RUNTIME_H_
#pragma once

#include "cef/libcef/features/features.h"

namespace cef {

#if BUILDFLAG(ENABLE_CEF)

inline bool IsCefBuildEnabled() {
  return true;
}

#if BUILDFLAG(ENABLE_ALLOY_BOOTSTRAP)
// True if CEF was initialized with the Alloy runtime.
bool IsAlloyRuntimeEnabled();
#endif

// True if CEF was initialized with the Chrome runtime.
bool IsChromeRuntimeEnabled();

// True if CEF crash reporting is enabled.
bool IsCrashReportingEnabled();

#else  // !BUILDFLAG(ENABLE_CEF)

inline bool IsCefBuildEnabled() {
  return false;
}
#if BUILDFLAG(ENABLE_ALLOY_BOOTSTRAP)
inline bool IsAlloyRuntimeEnabled() {
  return false;
}
#endif
inline bool IsChromeRuntimeEnabled() {
  return false;
}
inline bool IsCrashReportingEnabled() {
  return false;
}

#endif  // !BUILDFLAG(ENABLE_CEF)

}  // namespace cef

#endif  // CEF_LIBCEF_FEATURES_RUNTIME_H_
