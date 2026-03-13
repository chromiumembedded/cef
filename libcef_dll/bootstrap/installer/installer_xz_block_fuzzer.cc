// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "base/containers/span.h"
#include "cef/libcef_dll/bootstrap/installer/installer_archive.h"

extern "C" {
#include "third_party/lzma_sdk/src/C/7zCrc.h"
#include "third_party/lzma_sdk/src/C/XzCrc64.h"
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  [[maybe_unused]] static bool initialized = []() {
    CrcGenerateTable();
    Crc64GenerateTable();
    return true;
  }();

  // Need at least 3 bytes: check_type, stream_flags (2 bytes), then block data.
  if (size < 3) {
    return 0;
  }

  uint8_t check_type = data[0] % 11;  // Map to valid range: 0,1,4,10
  uint16_t stream_flags =
      static_cast<uint16_t>(data[1]) | (static_cast<uint16_t>(data[2]) << 8);

  // Fixed 64 KB output buffer — DecompressXzBlock will fail gracefully
  // if the actual uncompressed size doesn't match.
  std::vector<uint8_t> output(64 * 1024);

  cef_installer::DecompressXzBlock(
      base::span<const uint8_t>(data + 3, size - 3), check_type, stream_flags,
      base::span<uint8_t>(output));

  return 0;
}
