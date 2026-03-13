// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_XZ_INDEX_H_
#define CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_XZ_INDEX_H_

#include <cstdint>
#include <optional>
#include <vector>

#include "base/containers/span.h"
#include "base/files/file.h"

namespace cef_installer {

struct XzBlockInfo {
  uint64_t compressed_offset;  // Absolute offset in file to block header
  uint64_t compressed_size;    // Size of block including header/padding
  uint64_t uncompressed_size;  // Size after decompression
};

struct XzStreamInfo {
  std::vector<XzBlockInfo> blocks;
  uint64_t total_uncompressed_size = 0;
  // From stream flags: 0=none, 1=CRC32, 4=CRC64, 10=SHA256
  uint8_t check_type = 0;
};

// Parse XZ file to extract block information from the stream index.
// Returns nullopt if:
//   - File is not valid XZ
//   - File has no index (malformed)
//   - Footer or index CRC32 validation fails
// Note: Returns info even for single-block files; caller decides whether
// to use parallel extraction.
std::optional<XzStreamInfo> ParseXzIndex(base::File& file);

// ============================================================================
// Internal helpers exposed for testing. Not part of the public API.
// ============================================================================
namespace internal {

// Read an XZ multi-byte integer (7 bits per byte, bit 7 = continuation).
// Returns nullopt on overflow or truncation. Advances |pos| past bytes read.
std::optional<uint64_t> ReadMultiByteInt(base::span<const uint8_t> data,
                                         size_t& pos);

}  // namespace internal

}  // namespace cef_installer

#endif  // CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_XZ_INDEX_H_
