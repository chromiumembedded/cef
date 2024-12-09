// Copyright (c) 2024 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_COMMON_API_VERSION_UTIL_H_
#define CEF_LIBCEF_COMMON_API_VERSION_UTIL_H_
#pragma once

#include "base/logging.h"
#include "base/notreached.h"
#include "cef/include/cef_api_hash.h"

#define _CEF_RA_CMP(v, op) (cef_api_version() op v)

#define _CEF_RA_LT(v) _CEF_RA_CMP(v, <)
#define _CEF_RA_GE(v) _CEF_RA_CMP(v, >=)

#define _CEF_RA_CHECK(check) \
  check << __func__ << " called for invalid API version " << cef_api_version()

// Helpers for fatally asserting (with [[noreturn]]) that the configured
// CEF API version matches expectations.

// Assert that the specified version-related condition is true.
#define CEF_API_ASSERT(condition) _CEF_RA_CHECK(LOG_IF(FATAL, (condition)))

// Annotate should-be unreachable version-related code.
#define CEF_API_NOTREACHED() _CEF_RA_CHECK(NOTREACHED())

// Assert that API was added in the specified version.
// Pair with CEF_API_ADDED usage in CEF header files.
#define CEF_API_REQUIRE_ADDED(v) CEF_API_ASSERT(_CEF_RA_LT(v))

// Assert that API was removed in the specified version.
// Pair with CEF_API_REMOVED usage in CEF header files.
#define CEF_API_REQUIRE_REMOVED(v) CEF_API_ASSERT(_CEF_RA_GE(v))

// Assert that API exists only in the specified version range.
// Pair with CEF_API_RANGE usage in CEF header files.
#define CEF_API_REQUIRE_RANGE(va, vr) \
  CEF_API_ASSERT(_CEF_RA_LT(va) || _CEF_RA_GE(vr))

// Helpers for testing the configured CEF API version in conditionals.

// True if API was added in the specified version.
#define CEF_API_IS_ADDED(v) _CEF_RA_GE(v)

// True if API was removed in the specified version.
#define CEF_API_IS_REMOVED(v) _CEF_RA_LT(v)

// True if API exists only in the specified version range.
#define CEF_API_IS_RANGE(va, vr) (_CEF_RA_GE(va) && _CEF_RA_LT(vr))

#endif  // CEF_LIBCEF_COMMON_API_VERSION_UTIL_H_
