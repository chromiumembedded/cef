// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_RESULT_JSON_H_
#define CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_RESULT_JSON_H_

#include "base/values.h"

namespace cef_installer {

struct Result;

namespace internal {

// Builds the normalized dictionary used by Result::ToJson() and additive
// result transports. Field meanings and omission rules belong here so the
// transports cannot drift.
base::DictValue BuildResultJsonValue(const Result& result);

}  // namespace internal
}  // namespace cef_installer

#endif  // CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_RESULT_JSON_H_
