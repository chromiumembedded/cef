// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef_dll/bootstrap/installer/installer_crc.h"

#include "third_party/lzma_sdk/src/C/7zCrc.h"
#include "third_party/lzma_sdk/src/C/XzCrc64.h"

namespace cef_installer {

void EnsureCrcInitialized() {
  [[maybe_unused]] static bool initialized = [] {
    CrcGenerateTable();
    Crc64GenerateTable();
    return true;
  }();
}

}  // namespace cef_installer
