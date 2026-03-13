// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef_dll/bootstrap/installer/installer_xz_index.h"

#include <cstring>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "cef/libcef_dll/bootstrap/installer/installer_crc.h"
#include "cef/libcef_dll/bootstrap/installer/installer_test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/lzma_sdk/src/C/7zCrc.h"

namespace cef_installer {

using internal::ReadMultiByteInt;

namespace {

base::File OpenTestFixture(const std::string& name) {
  base::FilePath path = test::GetTestDataPath().AppendASCII(name);
  return base::File(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
}

// Copy a fixture to a temp dir and flip a bit at the given offset.
// Returns the opened (read-only) corrupted file.
base::File CorruptFixtureAt(const std::string& fixture_name,
                            const base::FilePath& temp_dir,
                            int64_t offset_from_end,
                            const std::string& output_name) {
  base::FilePath src = test::GetTestDataPath().AppendASCII(fixture_name);
  std::string content;
  if (!base::ReadFileToString(src, &content) || content.empty()) {
    return base::File();
  }
  size_t idx = content.size() + offset_from_end;  // offset_from_end is negative
  content[idx] ^= 0x01;                           // flip lowest bit
  base::FilePath dst = temp_dir.AppendASCII(output_name);
  if (!base::WriteFile(dst, content)) {
    return base::File();
  }
  return base::File(dst, base::File::FLAG_OPEN | base::File::FLAG_READ);
}

TEST(InstallerXzIndexTest, ParseMultiBlockArchive) {
  base::File file = OpenTestFixture("multi_block_4.tar.xz");
  ASSERT_TRUE(file.IsValid());

  auto info = ParseXzIndex(file);
  ASSERT_TRUE(info.has_value());
  EXPECT_GE(info->blocks.size(), 4u);
  EXPECT_GT(info->total_uncompressed_size, 0u);

  // Verify block offsets are monotonically increasing and start after header.
  uint64_t prev_end = 12;  // Stream header size
  for (const auto& block : info->blocks) {
    EXPECT_EQ(block.compressed_offset, prev_end);
    EXPECT_GT(block.compressed_size, 0u);
    EXPECT_GT(block.uncompressed_size, 0u);
    prev_end = block.compressed_offset + block.compressed_size;
  }
}

TEST(InstallerXzIndexTest, ParseSingleBlockArchive) {
  base::File file = OpenTestFixture("single_block.tar.xz");
  ASSERT_TRUE(file.IsValid());

  auto info = ParseXzIndex(file);
  ASSERT_TRUE(info.has_value());
  EXPECT_EQ(info->blocks.size(), 1u);
  EXPECT_GT(info->total_uncompressed_size, 0u);
  EXPECT_EQ(info->blocks[0].compressed_offset, 12u);
}

TEST(InstallerXzIndexTest, ParseInvalidFile) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // Write garbage data.
  base::FilePath path = temp_dir.GetPath().AppendASCII("not_xz.bin");
  const std::string garbage(128, 'X');
  ASSERT_TRUE(base::WriteFile(path, garbage));

  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  ASSERT_TRUE(file.IsValid());

  auto info = ParseXzIndex(file);
  EXPECT_FALSE(info.has_value());
}

TEST(InstallerXzIndexTest, ReadMultiByteIntSingleByte) {
  // Value 0: single byte 0x00
  std::vector<uint8_t> data = {0x00};
  size_t pos = 0;
  auto val = ReadMultiByteInt(data, pos);
  ASSERT_TRUE(val.has_value());
  EXPECT_EQ(*val, 0u);
  EXPECT_EQ(pos, 1u);

  // Value 127: single byte 0x7F
  data = {0x7F};
  pos = 0;
  val = ReadMultiByteInt(data, pos);
  ASSERT_TRUE(val.has_value());
  EXPECT_EQ(*val, 127u);
}

TEST(InstallerXzIndexTest, ReadMultiByteIntMultiByte) {
  // Value 128: 0x80 0x01 (continuation bit set on first byte)
  std::vector<uint8_t> data = {0x80, 0x01};
  size_t pos = 0;
  auto val = ReadMultiByteInt(data, pos);
  ASSERT_TRUE(val.has_value());
  EXPECT_EQ(*val, 128u);
  EXPECT_EQ(pos, 2u);

  // Value 16384: 0x80 0x80 0x01
  data = {0x80, 0x80, 0x01};
  pos = 0;
  val = ReadMultiByteInt(data, pos);
  ASSERT_TRUE(val.has_value());
  EXPECT_EQ(*val, 16384u);
  EXPECT_EQ(pos, 3u);
}

TEST(InstallerXzIndexTest, ReadMultiByteIntTruncated) {
  // Continuation bit set but no more data.
  std::vector<uint8_t> data = {0x80};
  size_t pos = 0;
  auto val = ReadMultiByteInt(data, pos);
  EXPECT_FALSE(val.has_value());
}

TEST(InstallerXzIndexTest, ReadMultiByteIntEmpty) {
  std::vector<uint8_t> data;
  size_t pos = 0;
  auto val = ReadMultiByteInt(data, pos);
  EXPECT_FALSE(val.has_value());
}

TEST(InstallerXzIndexTest, FileTooSmall) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // File smaller than header + footer (24 bytes minimum).
  base::FilePath path = temp_dir.GetPath().AppendASCII("tiny.bin");
  const std::string tiny(10, '\0');
  ASSERT_TRUE(base::WriteFile(path, tiny));

  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  ASSERT_TRUE(file.IsValid());

  auto info = ParseXzIndex(file);
  EXPECT_FALSE(info.has_value());
}

TEST(InstallerXzIndexTest, CheckTypeIsParsed) {
  base::File file = OpenTestFixture("multi_block_4.tar.xz");
  ASSERT_TRUE(file.IsValid());

  auto info = ParseXzIndex(file);
  ASSERT_TRUE(info.has_value());
  // Our test fixtures use CRC-64 (check_type = 4).
  EXPECT_EQ(info->check_type, 4u);
}

TEST(InstallerXzIndexTest, CorruptedHeaderMagic) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // Flip a bit in the header magic (byte 0, the 0xFD byte).
  base::FilePath src =
      test::GetTestDataPath().AppendASCII("multi_block_4.tar.xz");
  std::string content;
  ASSERT_TRUE(base::ReadFileToString(src, &content));
  content[0] ^= 0x01;
  base::FilePath dst = temp_dir.GetPath().AppendASCII("bad_header_magic.xz");
  ASSERT_TRUE(base::WriteFile(dst, content));

  base::File file(dst, base::File::FLAG_OPEN | base::File::FLAG_READ);
  ASSERT_TRUE(file.IsValid());
  EXPECT_FALSE(ParseXzIndex(file).has_value());
}

TEST(InstallerXzIndexTest, CorruptedHeaderCrc) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // Flip a bit in header stream flags (byte 7) to invalidate header CRC32.
  base::FilePath src =
      test::GetTestDataPath().AppendASCII("multi_block_4.tar.xz");
  std::string content;
  ASSERT_TRUE(base::ReadFileToString(src, &content));
  content[7] ^= 0x01;
  base::FilePath dst = temp_dir.GetPath().AppendASCII("bad_header_crc.xz");
  ASSERT_TRUE(base::WriteFile(dst, content));

  base::File file(dst, base::File::FLAG_OPEN | base::File::FLAG_READ);
  ASSERT_TRUE(file.IsValid());
  EXPECT_FALSE(ParseXzIndex(file).has_value());
}

TEST(InstallerXzIndexTest, CorruptedFooterMagic) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // Flip a bit in footer magic 'Y' (second-to-last byte).
  base::File file = CorruptFixtureAt("multi_block_4.tar.xz", temp_dir.GetPath(),
                                     -2, "bad_footer_magic.xz");
  ASSERT_TRUE(file.IsValid());
  EXPECT_FALSE(ParseXzIndex(file).has_value());
}

TEST(InstallerXzIndexTest, CorruptedFooterCrc) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // Flip a bit in backward_size field (footer byte 4, i.e. -8 from end)
  // to invalidate footer CRC32.
  base::File file = CorruptFixtureAt("multi_block_4.tar.xz", temp_dir.GetPath(),
                                     -8, "bad_footer_crc.xz");
  ASSERT_TRUE(file.IsValid());
  EXPECT_FALSE(ParseXzIndex(file).has_value());
}

TEST(InstallerXzIndexTest, CorruptedIndexCrc) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // The index sits just before the 12-byte footer. Corrupt a byte inside
  // the index body (well before the index CRC at the end of the index).
  // Footer is 12 bytes; use -20 to land inside the index data.
  base::File file = CorruptFixtureAt("multi_block_4.tar.xz", temp_dir.GetPath(),
                                     -20, "bad_index_crc.xz");
  ASSERT_TRUE(file.IsValid());
  EXPECT_FALSE(ParseXzIndex(file).has_value());
}

TEST(InstallerXzIndexTest, TruncatedFile) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // Take a valid fixture and truncate it to just the header + a few bytes.
  base::FilePath src =
      test::GetTestDataPath().AppendASCII("multi_block_4.tar.xz");
  std::string content;
  ASSERT_TRUE(base::ReadFileToString(src, &content));
  ASSERT_GT(content.size(), 30u);
  content.resize(30);  // Header (12) + 18 bytes — no valid footer.
  base::FilePath dst = temp_dir.GetPath().AppendASCII("truncated.xz");
  ASSERT_TRUE(base::WriteFile(dst, content));

  base::File file(dst, base::File::FLAG_OPEN | base::File::FLAG_READ);
  ASSERT_TRUE(file.IsValid());
  EXPECT_FALSE(ParseXzIndex(file).has_value());
}

// ============================================================================
// Synthetic XZ file builder for testing post-CRC validation branches.
//
// Builds a minimal valid XZ file, then allows mutation of specific fields
// while keeping CRCs consistent. This lets us reach branches that come after
// the CRC checks (index indicator, record count, reserved bits, etc).
// ============================================================================

// Write a little-endian uint32 into a buffer.
void PutLE32(uint8_t* buf, uint32_t val) {
  std::memcpy(buf, &val, 4);
}

// Build a synthetic XZ file with one block record in the index.
// The file has no actual block data — just valid header, index, and footer
// structures that ParseXzIndex can parse.
//
// Parameters let callers inject invalid values into specific fields:
//   header_flags: 2-byte stream flags for header (default: {0x00, 0x04} =
//   CRC64) footer_flags: 2-byte stream flags for footer (default: {0x00, 0x04}
//   = CRC64) index_indicator: first byte of index (should be 0x00)
//   unpadded_size: block unpadded size in index record
//   uncompressed_size: block uncompressed size in index record
struct SyntheticXzParams {
  uint8_t header_flags[2] = {0x00, 0x04};  // CRC64
  uint8_t footer_flags[2] = {0x00, 0x04};  // CRC64
  uint8_t index_indicator = 0x00;
  uint8_t num_records = 1;
  uint64_t unpadded_size = 100;
  uint64_t uncompressed_size = 200;
};

// Encode a multi-byte int (XZ variable-length integer).
void EncodeMultiByte(std::vector<uint8_t>& out, uint64_t val) {
  do {
    uint8_t byte = val & 0x7F;
    val >>= 7;
    if (val != 0) {
      byte |= 0x80;
    }
    out.push_back(byte);
  } while (val != 0);
}

base::File BuildSyntheticXz(const base::FilePath& dir,
                            const std::string& name,
                            const SyntheticXzParams& params) {
  EnsureCrcInitialized();

  // Build the index body (without CRC).
  std::vector<uint8_t> index_body;
  index_body.push_back(params.index_indicator);
  EncodeMultiByte(index_body, params.num_records);
  for (int i = 0; i < params.num_records; ++i) {
    EncodeMultiByte(index_body, params.unpadded_size);
    EncodeMultiByte(index_body, params.uncompressed_size);
  }
  // Pad to 4-byte boundary (index size must be multiple of 4 including
  // indicator, records, padding, and CRC).
  while ((index_body.size() + 4) % 4 != 0) {  // +4 for CRC at end
    index_body.push_back(0x00);
  }
  // Append index CRC32.
  uint32_t index_crc = CrcCalc(index_body.data(), index_body.size());
  size_t crc_pos = index_body.size();
  index_body.resize(index_body.size() + 4);
  PutLE32(index_body.data() + crc_pos, index_crc);

  uint64_t index_size = index_body.size();
  // backward_size = (index_size / 4) - 1
  uint32_t backward_size = static_cast<uint32_t>(index_size / 4) - 1;

  // Build header: magic(6) + stream_flags(2) + header_crc(4) = 12 bytes.
  uint8_t header[12];
  header[0] = 0xFD;
  header[1] = '7';
  header[2] = 'z';
  header[3] = 'X';
  header[4] = 'Z';
  header[5] = 0x00;
  header[6] = params.header_flags[0];
  header[7] = params.header_flags[1];
  uint32_t hdr_crc = CrcCalc(header + 6, 2);
  PutLE32(header + 8, hdr_crc);

  // Build footer: crc(4) + backward_size(4) + stream_flags(2) + "YZ"(2).
  uint8_t footer[12];
  PutLE32(footer + 4, backward_size);
  footer[8] = params.footer_flags[0];
  footer[9] = params.footer_flags[1];
  footer[10] = 'Y';
  footer[11] = 'Z';
  uint32_t ftr_crc = CrcCalc(footer + 4, 6);
  PutLE32(footer, ftr_crc);

  // We need filler between header and index to make block offsets work.
  // The index records say there's a block of unpadded_size starting at
  // offset 12 (after header). The padded size = (unpadded + 3) & ~3.
  // The index must start right after the last block's padded end.
  uint64_t padded = (params.unpadded_size + 3) & ~3ULL;
  uint64_t filler_size = padded * params.num_records;

  // Assemble the file.
  std::vector<uint8_t> file_data;
  file_data.insert(file_data.end(), header, header + 12);
  file_data.resize(file_data.size() + filler_size, 0x00);  // block filler
  file_data.insert(file_data.end(), index_body.begin(), index_body.end());
  file_data.insert(file_data.end(), footer, footer + 12);

  base::FilePath path = dir.AppendASCII(name);
  base::WriteFile(path, base::as_string_view(file_data));
  return base::File(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
}

TEST(InstallerXzIndexTest, SyntheticValidBaseline) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // A synthetic file with default params should parse successfully.
  SyntheticXzParams params;
  base::File file = BuildSyntheticXz(temp_dir.GetPath(), "valid.xz", params);
  ASSERT_TRUE(file.IsValid());

  auto info = ParseXzIndex(file);
  ASSERT_TRUE(info.has_value());
  EXPECT_EQ(1u, info->blocks.size());
  EXPECT_EQ(4u, info->check_type);  // CRC64
  EXPECT_EQ(200u, info->total_uncompressed_size);
}

TEST(InstallerXzIndexTest, IndexIndicatorNonZero) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  SyntheticXzParams params;
  params.index_indicator = 0x01;  // Must be 0x00.
  base::File file =
      BuildSyntheticXz(temp_dir.GetPath(), "bad_indicator.xz", params);
  ASSERT_TRUE(file.IsValid());
  EXPECT_FALSE(ParseXzIndex(file).has_value());
}

TEST(InstallerXzIndexTest, FooterReservedBitsSet) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // Set reserved high nibble of footer byte 9 (bits 4-7 must be 0).
  SyntheticXzParams params;
  params.footer_flags[1] = 0xF4;  // CRC64 (0x04) + reserved bits set
  params.header_flags[1] = 0xF4;  // Must match to not fail earlier
  base::File file =
      BuildSyntheticXz(temp_dir.GetPath(), "reserved_bits.xz", params);
  ASSERT_TRUE(file.IsValid());
  EXPECT_FALSE(ParseXzIndex(file).has_value());
}

TEST(InstallerXzIndexTest, StreamFlagsMismatchHeaderFooter) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // Header says CRC32 (0x01), footer says CRC64 (0x04).
  SyntheticXzParams params;
  params.header_flags[1] = 0x01;  // CRC32
  params.footer_flags[1] = 0x04;  // CRC64
  base::File file =
      BuildSyntheticXz(temp_dir.GetPath(), "flags_mismatch.xz", params);
  ASSERT_TRUE(file.IsValid());
  EXPECT_FALSE(ParseXzIndex(file).has_value());
}

TEST(InstallerXzIndexTest, BlockOffsetMismatchWithIndex) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // Build a valid synthetic file, then remove filler bytes so the index
  // records claim block sizes that don't match the actual file layout.
  SyntheticXzParams params;
  base::FilePath src_path = temp_dir.GetPath().AppendASCII("offset_src.xz");
  BuildSyntheticXz(temp_dir.GetPath(), "offset_src.xz", params);

  std::string content;
  ASSERT_TRUE(base::ReadFileToString(src_path, &content));
  ASSERT_GT(content.size(), 30u);
  content.erase(12, 4);  // Remove 4 bytes of filler after header.
  base::FilePath dst = temp_dir.GetPath().AppendASCII("offset_bad.xz");
  ASSERT_TRUE(base::WriteFile(dst, content));

  base::File file(dst, base::File::FLAG_OPEN | base::File::FLAG_READ);
  ASSERT_TRUE(file.IsValid());
  EXPECT_FALSE(ParseXzIndex(file).has_value());
}

TEST(InstallerXzIndexTest, ZeroRecordsInIndex) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  SyntheticXzParams params;
  params.num_records = 0;
  params.unpadded_size = 100;  // Doesn't matter, no records.
  base::File file =
      BuildSyntheticXz(temp_dir.GetPath(), "zero_records.xz", params);
  ASSERT_TRUE(file.IsValid());

  auto info = ParseXzIndex(file);
  // Zero records is valid per XZ spec (empty stream), but block_offset (12)
  // must equal index_offset. With 0 records and 0 filler, index starts at 12.
  ASSERT_TRUE(info.has_value());
  EXPECT_EQ(0u, info->blocks.size());
  EXPECT_EQ(0u, info->total_uncompressed_size);
}

}  // namespace
}  // namespace cef_installer
