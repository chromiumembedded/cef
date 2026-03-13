// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef_dll/bootstrap/installer/installer_xz_index.h"

#include <cstring>

#include "base/containers/span.h"
#include "base/logging.h"
#include "base/numerics/byte_conversions.h"
#include "base/numerics/checked_math.h"
#include "cef/libcef_dll/bootstrap/installer/installer_crc.h"
#include "third_party/lzma_sdk/src/C/7zCrc.h"

namespace cef_installer {

namespace {

// XZ format constants.
constexpr size_t kStreamHeaderSize = 12;
constexpr size_t kStreamFooterSize = 12;
constexpr uint8_t kHeaderMagic[] = {0xFD, '7', 'z', 'X', 'Z', 0x00};
constexpr uint8_t kFooterMagic0 = 'Y';
constexpr uint8_t kFooterMagic1 = 'Z';
constexpr uint8_t kIndexIndicator = 0x00;

// Security limits.
constexpr uint64_t kMaxIndexSize = 1 * 1024 * 1024;  // 1 MB
constexpr uint64_t kMaxNumRecords = 10000;
constexpr uint64_t kMaxUncompressedSize = 10ULL * 1024 * 1024 * 1024;  // 10 GB

}  // namespace

namespace internal {

std::optional<uint64_t> ReadMultiByteInt(base::span<const uint8_t> data,
                                         size_t& pos) {
  uint64_t result = 0;
  int shift = 0;

  while (pos < data.size() && shift < 63) {
    uint8_t byte = data[pos++];
    result |= static_cast<uint64_t>(byte & 0x7F) << shift;
    if ((byte & 0x80) == 0) {
      return result;
    }
    shift += 7;
  }
  return std::nullopt;  // Overflow or truncated
}

}  // namespace internal

std::optional<XzStreamInfo> ParseXzIndex(base::File& file) {
  EnsureCrcInitialized();

  // 1. Get file size and validate minimum (header + footer).
  int64_t file_size = file.GetLength();
  if (file_size < static_cast<int64_t>(kStreamHeaderSize + kStreamFooterSize)) {
    return std::nullopt;
  }

  // 2. Read and validate stream header (first 12 bytes).
  uint8_t header[kStreamHeaderSize];
  if (file.Seek(base::File::FROM_BEGIN, 0) != 0) {
    return std::nullopt;
  }
  if (!file.ReadAtCurrentPosAndCheck(header)) {
    return std::nullopt;
  }

  // 2a. Validate header magic bytes.
  if (std::memcmp(header, kHeaderMagic, sizeof(kHeaderMagic)) != 0) {
    return std::nullopt;
  }

  // 2b. Validate header stream flags reserved bits.
  // Header bytes 6-7 are stream flags (same layout as footer bytes 8-9).
  if (header[6] != 0x00 || (header[7] & 0xF0) != 0x00) {
    return std::nullopt;
  }

  // 2c. Verify header CRC32 (bytes 8-11 cover stream flags at bytes 6-7).
  uint32_t header_crc_stored =
      base::U32FromLittleEndian(base::span<const uint8_t, 4u>(header + 8, 4u));
  uint32_t header_crc_computed = CrcCalc(header + 6, 2);
  if (header_crc_stored != header_crc_computed) {
    return std::nullopt;
  }

  // 3. Read stream footer (last 12 bytes).
  uint8_t footer[kStreamFooterSize];
  if (file.Seek(base::File::FROM_END,
                -static_cast<int64_t>(kStreamFooterSize)) < 0) {
    return std::nullopt;
  }
  if (!file.ReadAtCurrentPosAndCheck(footer)) {
    return std::nullopt;
  }

  // 4. Validate footer magic "YZ" at bytes 10-11.
  if (footer[10] != kFooterMagic0 || footer[11] != kFooterMagic1) {
    return std::nullopt;
  }

  // 4. Verify footer CRC32 (covers bytes 4-9: backward_size + stream_flags).
  uint32_t footer_crc_stored =
      base::U32FromLittleEndian(base::span<const uint8_t, 4u>(footer, 4u));
  uint32_t footer_crc_computed = CrcCalc(footer + 4, 6);
  if (footer_crc_stored != footer_crc_computed) {
    return std::nullopt;
  }

  // 5. Get Backward Size -> Index size.
  uint32_t backward_size =
      base::U32FromLittleEndian(base::span<const uint8_t, 4u>(footer + 4, 4u));
  uint64_t index_size = (static_cast<uint64_t>(backward_size) + 1) * 4;

  if (index_size > kMaxIndexSize) {
    LOG(WARNING) << "XZ index size " << index_size << " exceeds limit";
    return std::nullopt;
  }

  // 6. Calculate index offset and validate it doesn't overlap the header.
  int64_t index_offset = file_size - kStreamFooterSize - index_size;
  if (index_offset < static_cast<int64_t>(kStreamHeaderSize)) {
    return std::nullopt;
  }

  // 7. Read index data.
  std::vector<uint8_t> index_data(index_size);
  if (file.Seek(base::File::FROM_BEGIN, index_offset) != index_offset) {
    return std::nullopt;
  }
  if (!file.ReadAtCurrentPosAndCheck(index_data)) {
    return std::nullopt;
  }

  // 8. Verify index CRC32 (last 4 bytes of index cover everything before).
  if (index_size < 5) {
    return std::nullopt;  // Need at least indicator + CRC32
  }
  uint64_t index_crc_offset = index_size - 4;
  uint32_t index_crc_stored = base::U32FromLittleEndian(
      base::span<const uint8_t, 4u>(index_data.data() + index_crc_offset, 4u));
  uint32_t index_crc_computed =
      CrcCalc(index_data.data(), static_cast<size_t>(index_crc_offset));
  if (index_crc_stored != index_crc_computed) {
    return std::nullopt;
  }

  // 9. Validate Index Indicator (first byte must be 0x00).
  if (index_data[0] != kIndexIndicator) {
    return std::nullopt;
  }

  // 10. Read Number of Records (multi-byte integer).
  size_t pos = 1;
  auto num_records = internal::ReadMultiByteInt(index_data, pos);
  if (!num_records) {
    return std::nullopt;
  }
  if (*num_records > kMaxNumRecords) {
    LOG(WARNING) << "XZ record count " << *num_records << " exceeds limit";
    return std::nullopt;
  }

  // 11. Validate and extract stream flags (footer bytes 8-9).
  // Byte 8 is reserved (must be 0), byte 9 bits 4-7 are reserved (must be 0),
  // byte 9 bits 0-3 hold the check type.
  if (footer[8] != 0x00 || (footer[9] & 0xF0) != 0x00) {
    return std::nullopt;
  }

  // Header and footer stream flags must match (spec requirement).
  if (header[6] != footer[8] || header[7] != footer[9]) {
    return std::nullopt;
  }

  uint8_t check_type = footer[9] & 0x0F;

  // 12. Parse each index record.
  XzStreamInfo info;
  info.check_type = check_type;

  base::CheckedNumeric<uint64_t> block_offset = kStreamHeaderSize;
  base::CheckedNumeric<uint64_t> total_uncompressed = 0;

  for (uint64_t i = 0; i < *num_records; ++i) {
    auto unpadded_size = internal::ReadMultiByteInt(index_data, pos);
    auto uncompressed_size = internal::ReadMultiByteInt(index_data, pos);
    if (!unpadded_size || !uncompressed_size) {
      return std::nullopt;
    }
    if (*unpadded_size < 1) {
      return std::nullopt;
    }

    // Padded size = unpadded rounded up to 4-byte boundary.
    base::CheckedNumeric<uint64_t> checked_padded =
        base::CheckedNumeric<uint64_t>(*unpadded_size) + 3;
    if (!checked_padded.IsValid()) {
      return std::nullopt;
    }
    uint64_t padded_size =
        static_cast<uint64_t>(checked_padded.ValueOrDie()) & ~3ULL;

    if (!block_offset.IsValid()) {
      return std::nullopt;
    }

    XzBlockInfo block;
    block.compressed_offset = block_offset.ValueOrDie();
    block.compressed_size = padded_size;
    block.uncompressed_size = *uncompressed_size;
    info.blocks.push_back(block);

    block_offset += padded_size;
    total_uncompressed += *uncompressed_size;

    if (!block_offset.IsValid() || !total_uncompressed.IsValid()) {
      return std::nullopt;
    }
  }

  // 13. Verify block offsets are consistent with file layout.
  // The last block must end exactly at the index start.
  if (static_cast<uint64_t>(block_offset.ValueOrDie()) !=
      static_cast<uint64_t>(index_offset)) {
    return std::nullopt;
  }

  // 14. Cap total uncompressed size as defense-in-depth.
  uint64_t total_unc = static_cast<uint64_t>(total_uncompressed.ValueOrDie());
  if (total_unc > kMaxUncompressedSize) {
    LOG(WARNING) << "XZ total uncompressed size " << total_unc
                 << " exceeds limit";
    return std::nullopt;
  }

  info.total_uncompressed_size = total_unc;
  return info;
}

}  // namespace cef_installer
