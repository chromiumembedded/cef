// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_VALIDATION_H_
#define CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_VALIDATION_H_

#include <string>

namespace cef_installer {

// Validate that an ABI hash contains only hex characters [0-9a-fA-F] and is
// within kMaxAbiHashLength. ABI hashes are derived from CEF_API_HASH_UNIVERSAL
// at compile time and are always hex strings, but we validate defensively
// because the value is interpolated into CDN URL paths (BuildAbiHashUrl).
// Rejecting non-hex characters prevents path traversal (../), query injection
// (?), fragment injection (#), and other URL metacharacter attacks.
bool IsValidAbiHash(const std::string& hash);

}  // namespace cef_installer

#endif  // CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_VALIDATION_H_
