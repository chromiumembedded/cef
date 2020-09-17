// Copyright 2020 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_FEATURES_RUNTIME_CHECKS_H_
#define CEF_LIBCEF_FEATURES_RUNTIME_CHECKS_H_
#pragma once

#include "base/logging.h"
#include "cef/libcef/features/runtime.h"

#define REQUIRE_ALLOY_RUNTIME() \
  CHECK(cef::IsAlloyRuntimeEnabled()) << "Alloy runtime is required"

#define REQUIRE_CHROME_RUNTIME() \
  CHECK(cef::IsChromeRuntimeEnabled()) << "Chrome runtime is required"

#endif  // CEF_LIBCEF_FEATURES_RUNTIME_CHECKS_H_
