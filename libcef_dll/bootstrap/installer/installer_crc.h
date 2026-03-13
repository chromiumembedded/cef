// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_CRC_H_
#define CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_CRC_H_

namespace cef_installer {

// One-time initialization of CRC32 and CRC64 lookup tables (lzma_sdk).
// Safe to call multiple times; subsequent calls are no-ops.
// Must be called before CrcCalc() or any XZ decompression.
void EnsureCrcInitialized();

}  // namespace cef_installer

#endif  // CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_CRC_H_
