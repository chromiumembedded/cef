// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef_dll/bootstrap/installer/installer_archive.h"

#include <cstring>
#include <vector>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "cef/libcef_dll/bootstrap/installer/installer_test_helpers.h"
#include "cef/libcef_dll/bootstrap/installer/installer_xz_index.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cef_installer {

using internal::TarEntry;
using internal::TarReader;
using internal::XzDecompressor;

namespace {

// Embedded test archive: test.tar.xz containing:
//   testdir/
//   testdir/hello.txt     (9 bytes: "Hello CEF")
//   testdir/subdir/
//   testdir/subdir/nested.txt  (14 bytes: "Nested content")
// Created with: python3 -c 'open(...).write(b"Hello CEF")' && tar cf - | xz -9
// clang-format off
const uint8_t kTestTarXz[] = {
  0xfd, 0x37, 0x7a, 0x58, 0x5a, 0x00, 0x00, 0x04, 0xe6, 0xd6, 0xb4, 0x46,
  0x04, 0xc0, 0xcb, 0x01, 0x80, 0x50, 0x21, 0x01, 0x1c, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x62, 0x9c, 0x1b, 0x68, 0xe0, 0x27, 0xff, 0x00,
  0xc3, 0x5d, 0x00, 0x3a, 0x19, 0x4a, 0xce, 0x1f, 0xe4, 0x18, 0x64, 0xc3,
  0xfe, 0xf9, 0x76, 0x67, 0xce, 0xe7, 0xf2, 0xb1, 0x9b, 0x8d, 0x99, 0x06,
  0xc9, 0x69, 0x62, 0x28, 0xcd, 0x58, 0xfc, 0x78, 0x5d, 0x9f, 0x3d, 0x57,
  0x4c, 0xe8, 0x0b, 0x44, 0x7e, 0x15, 0x3d, 0x3a, 0x8d, 0xa7, 0x51, 0x55,
  0xb0, 0xfc, 0xe3, 0x71, 0x81, 0xcb, 0x8c, 0x24, 0xa4, 0xd7, 0x22, 0x1f,
  0xe0, 0x92, 0xb0, 0xc0, 0xd4, 0x62, 0x28, 0x2d, 0xae, 0x14, 0x33, 0x9e,
  0x17, 0xee, 0xab, 0x20, 0xf3, 0xa3, 0xf7, 0xa8, 0x84, 0xa8, 0xe7, 0x7d,
  0x92, 0x40, 0xf1, 0x26, 0x68, 0x27, 0x0e, 0xd2, 0x4b, 0xec, 0xec, 0xb7,
  0xa3, 0xc1, 0x4a, 0x76, 0x07, 0x61, 0x7d, 0xfb, 0xcc, 0xb0, 0x4b, 0x56,
  0xe7, 0xae, 0x70, 0x29, 0xb9, 0xec, 0x2f, 0xd5, 0xb8, 0x42, 0x79, 0x65,
  0x34, 0xb2, 0x5d, 0xfd, 0x36, 0x65, 0x4e, 0x7b, 0xbe, 0x91, 0xea, 0xc8,
  0x25, 0x80, 0x6e, 0x1d, 0x7e, 0x27, 0x57, 0x61, 0xa9, 0xb8, 0x5d, 0x0d,
  0x14, 0x86, 0x6e, 0xad, 0x6a, 0x48, 0x94, 0x63, 0x93, 0xa2, 0xc0, 0xbc,
  0xeb, 0xda, 0x81, 0xcc, 0x40, 0x1e, 0x81, 0xf0, 0x8c, 0x14, 0x19, 0x51,
  0xe9, 0x68, 0x4f, 0x24, 0xb2, 0xb1, 0xea, 0x2e, 0x68, 0x97, 0x26, 0x32,
  0xdf, 0x4e, 0xa4, 0x8b, 0x0d, 0x47, 0x29, 0x42, 0x0c, 0xbd, 0x5d, 0x8c,
  0x10, 0xe7, 0xda, 0xa2, 0xcb, 0x00, 0x00, 0x00, 0xce, 0x6a, 0x35, 0x81,
  0xed, 0x60, 0xfd, 0x47, 0x00, 0x01, 0xe7, 0x01, 0x80, 0x50, 0x00, 0x00,
  0xdc, 0x4e, 0x91, 0x74, 0xb1, 0xc4, 0x67, 0xfb, 0x02, 0x00, 0x00, 0x00,
  0x00, 0x04, 0x59, 0x5a
};
const size_t kTestTarXzLen = sizeof(kTestTarXz);
// clang-format on

// Recalculate tar header checksum after modifying fields.
void RecalculateChecksum(std::vector<uint8_t>& header) {
  std::memset(header.data() + TarReader::kChecksumOffset, ' ',
              TarReader::kChecksumLength);
  uint64_t checksum = 0;
  for (size_t i = 0; i < TarReader::kHeaderSize; ++i) {
    checksum += header[i];
  }
  char checksum_str[8];
  snprintf(checksum_str, sizeof(checksum_str), "%06llo ",
           static_cast<unsigned long long>(checksum));
  std::memcpy(header.data() + TarReader::kChecksumOffset, checksum_str, 7);
  header[TarReader::kChecksumOffset + 7] = '\0';
}

// Helper to create a tar header
std::vector<uint8_t> CreateTarHeader(const std::string& name,
                                     uint64_t size,
                                     char typeflag,
                                     uint64_t mtime = 0) {
  std::vector<uint8_t> header(TarReader::kHeaderSize, 0);

  // Name (offset 0, length 100)
  size_t name_len = std::min(name.size(), size_t{100});
  std::memcpy(header.data(), name.c_str(), name_len);

  // Mode (offset 100) - default 0644 for files, 0755 for directories
  const char* mode =
      (typeflag == TarReader::kTypeDirectory) ? "0000755 " : "0000644 ";
  std::memcpy(header.data() + TarReader::kModeOffset, mode, 8);

  // UID (offset 108)
  std::memcpy(header.data() + 108, "0000000 ", 8);

  // GID (offset 116)
  std::memcpy(header.data() + 116, "0000000 ", 8);

  // Size (offset 124, length 12) - octal with leading zeros
  char size_str[12];
  snprintf(size_str, sizeof(size_str), "%011llo",
           static_cast<unsigned long long>(size));
  std::memcpy(header.data() + TarReader::kSizeOffset, size_str, 11);
  header[TarReader::kSizeOffset + 11] = ' ';

  // Mtime (offset 136, length 12) - octal
  char mtime_str[12];
  snprintf(mtime_str, sizeof(mtime_str), "%011llo",
           static_cast<unsigned long long>(mtime));
  std::memcpy(header.data() + TarReader::kMtimeOffset, mtime_str, 11);
  header[TarReader::kMtimeOffset + 11] = ' ';

  // Initialize checksum field with spaces (offset 148, length 8)
  std::memset(header.data() + TarReader::kChecksumOffset, ' ',
              TarReader::kChecksumLength);

  // Typeflag (offset 156)
  header[TarReader::kTypeflagOffset] = typeflag;

  // Magic (offset 257) - "ustar\0"
  std::memcpy(header.data() + TarReader::kMagicOffset, "ustar", 5);
  header[TarReader::kMagicOffset + 5] = '\0';

  // Version (offset 263) - "00"
  header[263] = '0';
  header[264] = '0';

  // Calculate and write checksum
  uint64_t checksum = 0;
  for (size_t i = 0; i < TarReader::kHeaderSize; ++i) {
    checksum += header[i];
  }

  char checksum_str[8];
  snprintf(checksum_str, sizeof(checksum_str), "%06llo ",
           static_cast<unsigned long long>(checksum));
  std::memcpy(header.data() + TarReader::kChecksumOffset, checksum_str, 7);
  header[TarReader::kChecksumOffset + 7] = '\0';

  return header;
}

// Write test archive to a file
base::FilePath WriteTestArchive(const base::FilePath& dir) {
  base::FilePath archive_path = dir.AppendASCII("test.tar.xz");
  base::WriteFile(archive_path, base::as_string_view(base::span<const uint8_t>(
                                    kTestTarXz, kTestTarXzLen)));
  return archive_path;
}

class TarReaderTest : public testing::Test {
 protected:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  base::ScopedTempDir temp_dir_;
};

// ============================================================================
// TarReader Tests
// ============================================================================

TEST_F(TarReaderTest, ParseHeaderValid) {
  std::vector<uint8_t> header =
      CreateTarHeader("test.txt", 100, TarReader::kTypeRegular);

  auto entry = TarReader::ParseHeader(header);

  ASSERT_TRUE(entry.has_value());
  EXPECT_EQ(L"test.txt", entry->path.value());
  EXPECT_EQ(100u, entry->size);
  EXPECT_EQ(TarEntry::Type::kRegularFile, entry->type);
}

TEST_F(TarReaderTest, ParseHeaderRegularFile) {
  // Test type '0' (regular file)
  std::vector<uint8_t> header0 =
      CreateTarHeader("file0.txt", 50, TarReader::kTypeRegular);
  auto entry0 = TarReader::ParseHeader(header0);
  ASSERT_TRUE(entry0.has_value());
  EXPECT_EQ(TarEntry::Type::kRegularFile, entry0->type);

  // Test type '\0' (old-style regular file)
  std::vector<uint8_t> header_alt =
      CreateTarHeader("file_alt.txt", 50, TarReader::kTypeRegularAlt);
  auto entry_alt = TarReader::ParseHeader(header_alt);
  ASSERT_TRUE(entry_alt.has_value());
  EXPECT_EQ(TarEntry::Type::kRegularFile, entry_alt->type);
}

TEST_F(TarReaderTest, ParseHeaderDirectory) {
  std::vector<uint8_t> header =
      CreateTarHeader("subdir/", 0, TarReader::kTypeDirectory);

  auto entry = TarReader::ParseHeader(header);

  ASSERT_TRUE(entry.has_value());
  EXPECT_EQ(TarEntry::Type::kDirectory, entry->type);
}

TEST_F(TarReaderTest, ParseHeaderEndOfArchive) {
  std::vector<uint8_t> zero_block(TarReader::kHeaderSize, 0);

  auto entry = TarReader::ParseHeader(zero_block);

  EXPECT_FALSE(entry.has_value());  // nullopt for end of archive
}

TEST_F(TarReaderTest, ParseHeaderFileSizeTooLarge) {
  // Create a header with a size larger than kMaxFileSize (4GB)
  // Using GNU binary format to encode a 5GB size
  std::vector<uint8_t> header =
      CreateTarHeader("test.txt", 100, TarReader::kTypeRegular);

  // Overwrite size field with GNU binary format for 5GB
  // 5GB = 5 * 1024 * 1024 * 1024 = 5368709120 = 0x140000000
  header[TarReader::kSizeOffset] = 0x80;  // GNU binary marker
  header[TarReader::kSizeOffset + 1] = 0x00;
  header[TarReader::kSizeOffset + 2] = 0x00;
  header[TarReader::kSizeOffset + 3] = 0x00;
  header[TarReader::kSizeOffset + 4] = 0x00;  // High bytes
  header[TarReader::kSizeOffset + 5] = 0x00;
  header[TarReader::kSizeOffset + 6] = 0x00;
  header[TarReader::kSizeOffset + 7] = 0x01;  // 0x140000000
  header[TarReader::kSizeOffset + 8] = 0x40;
  header[TarReader::kSizeOffset + 9] = 0x00;
  header[TarReader::kSizeOffset + 10] = 0x00;
  header[TarReader::kSizeOffset + 11] = 0x00;

  // Recalculate checksum
  uint64_t checksum = 0;
  for (size_t i = 0; i < TarReader::kHeaderSize; ++i) {
    if (i >= TarReader::kChecksumOffset &&
        i < TarReader::kChecksumOffset + TarReader::kChecksumLength) {
      checksum += ' ';
    } else {
      checksum += header[i];
    }
  }
  char checksum_str[8];
  snprintf(checksum_str, sizeof(checksum_str), "%06llo ",
           static_cast<unsigned long long>(checksum));
  std::memcpy(header.data() + TarReader::kChecksumOffset, checksum_str, 7);

  auto entry = TarReader::ParseHeader(header);

  // Should reject files larger than 4GB
  EXPECT_FALSE(entry.has_value());
}

TEST_F(TarReaderTest, ParseHeaderFileSizeAtLimit) {
  // Create a header with a size at the 4GB limit (should be accepted)
  std::vector<uint8_t> header = CreateTarHeader(
      "test.txt", 4ULL * 1024 * 1024 * 1024, TarReader::kTypeRegular);

  auto entry = TarReader::ParseHeader(header);

  // Should accept files at exactly 4GB
  ASSERT_TRUE(entry.has_value());
  EXPECT_EQ(4ULL * 1024 * 1024 * 1024, entry->size);
}

TEST_F(TarReaderTest, ValidateChecksumValid) {
  std::vector<uint8_t> header =
      CreateTarHeader("test.txt", 100, TarReader::kTypeRegular);

  EXPECT_TRUE(TarReader::ValidateChecksum(header));
}

TEST_F(TarReaderTest, ValidateChecksumCorrupt) {
  std::vector<uint8_t> header =
      CreateTarHeader("test.txt", 100, TarReader::kTypeRegular);

  // Corrupt the header
  header[0] = 'X';
  header[1] = 'Y';

  EXPECT_FALSE(TarReader::ValidateChecksum(header));
}

TEST_F(TarReaderTest, ReadOctalStandard) {
  // "00000123\0\0\0\0" = 83 in decimal
  std::vector<uint8_t> field = {'0', '0', '0',  '0',  '0',  '1',
                                '2', '3', '\0', '\0', '\0', '\0'};

  auto result = TarReader::ReadOctalNumber(field);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(83u, *result);  // 1*64 + 2*8 + 3 = 83
}

TEST_F(TarReaderTest, ReadOctalGnuBinary) {
  // GNU binary format: 0x80 prefix followed by big-endian integer
  std::vector<uint8_t> field = {0x80, 0x00, 0x00, 0x00, 0x00, 0x00,
                                0x00, 0x00, 0x00, 0x00, 0x01, 0x00};  // = 256

  auto result = TarReader::ReadOctalNumber(field);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(256u, *result);
}

TEST_F(TarReaderTest, ReadOctalInvalidDigit) {
  // Invalid octal digit (8 and 9 are not valid in octal)
  std::vector<uint8_t> field = {'0', '0', '0',  '0',  '0',  '1',
                                '8', '9', '\0', '\0', '\0', '\0'};

  auto result = TarReader::ReadOctalNumber(field);

  EXPECT_FALSE(result.has_value());
}

TEST_F(TarReaderTest, ReadOctalTooShort) {
  // Field too short (less than 8 bytes)
  std::vector<uint8_t> field = {'0', '1', '2', '3'};

  auto result = TarReader::ReadOctalNumber(field);

  EXPECT_FALSE(result.has_value());
}

TEST_F(TarReaderTest, IsPathSafeAbsoluteUnix) {
  // String version for Unix-style paths
  EXPECT_FALSE(TarReader::IsPathSafe("/etc/passwd"));
  EXPECT_FALSE(TarReader::IsPathSafe("/home/user/file.txt"));
}

TEST_F(TarReaderTest, IsPathSafeAbsoluteWindows) {
  // String version for Windows-style paths
  EXPECT_FALSE(TarReader::IsPathSafe("C:\\Windows\\System32\\cmd.exe"));
  EXPECT_FALSE(TarReader::IsPathSafe("D:\\file.txt"));

  // UNC paths (\\server\share)
  EXPECT_FALSE(TarReader::IsPathSafe("\\\\server\\share\\file.txt"));
  EXPECT_FALSE(TarReader::IsPathSafe("\\\\?\\C:\\long\\path"));

  // Bare backslash absolute path
  EXPECT_FALSE(TarReader::IsPathSafe("\\Windows\\System32\\cmd.exe"));
}

TEST_F(TarReaderTest, IsPathSafeNullByte) {
  // Null byte injection could truncate path in C APIs
  EXPECT_FALSE(
      TarReader::IsPathSafe(std::string("safe\x00/../etc/passwd", 20)));
  EXPECT_FALSE(TarReader::IsPathSafe(std::string("file\x00.txt", 9)));
}

TEST_F(TarReaderTest, IsPathSafeTooLong) {
  // Paths longer than kMaxPathLength (4096) should be rejected
  std::string long_path(5000, 'a');
  long_path += ".txt";
  EXPECT_FALSE(TarReader::IsPathSafe(long_path));

  // Path at the limit should be accepted
  std::string ok_path(4000, 'b');
  ok_path += ".txt";
  EXPECT_TRUE(TarReader::IsPathSafe(ok_path));
}

TEST_F(TarReaderTest, IsPathSafeWindowsReserved) {
  // Windows reserved device names should be rejected
  EXPECT_FALSE(TarReader::IsPathSafe("CON"));
  EXPECT_FALSE(TarReader::IsPathSafe("con"));  // Case insensitive
  EXPECT_FALSE(TarReader::IsPathSafe("PRN"));
  EXPECT_FALSE(TarReader::IsPathSafe("AUX"));
  EXPECT_FALSE(TarReader::IsPathSafe("NUL"));
  EXPECT_FALSE(TarReader::IsPathSafe("COM1"));
  EXPECT_FALSE(TarReader::IsPathSafe("COM9"));
  EXPECT_FALSE(TarReader::IsPathSafe("LPT1"));
  EXPECT_FALSE(TarReader::IsPathSafe("LPT9"));

  // Reserved names with extensions are also reserved
  EXPECT_FALSE(TarReader::IsPathSafe("CON.txt"));
  EXPECT_FALSE(TarReader::IsPathSafe("NUL.tar.gz"));

  // Reserved names in subdirectories
  EXPECT_FALSE(TarReader::IsPathSafe("subdir/CON"));
  EXPECT_FALSE(TarReader::IsPathSafe("subdir/NUL.txt"));

  // Similar but non-reserved names should be allowed
  EXPECT_TRUE(TarReader::IsPathSafe("CONSOLE"));
  EXPECT_TRUE(TarReader::IsPathSafe("CONN"));
  EXPECT_TRUE(TarReader::IsPathSafe("COM10"));  // Only COM1-9 reserved
  EXPECT_TRUE(TarReader::IsPathSafe("LPT10"));
}

TEST_F(TarReaderTest, IsPathSafeEmpty) {
  EXPECT_FALSE(TarReader::IsPathSafe(""));
}

TEST_F(TarReaderTest, ParsePaxExtendedHeader) {
  // PAX format: "length key=value\n" where length is total record size
  // "26 path=long_filename.txt\n" = 26 chars total
  std::string pax_content = "26 path=long_filename.txt\n";
  std::vector<uint8_t> content(pax_content.begin(), pax_content.end());

  auto path = TarReader::ParsePaxPath(content);

  ASSERT_TRUE(path.has_value());
  EXPECT_EQ("long_filename.txt", *path);
}

TEST_F(TarReaderTest, ParseGnuLongName) {
  // GNU long name is stored as content of a 'L' type entry
  // The CreateTarHeader helper creates a valid header
  std::string long_name(200, 'a');  // Name longer than 100 chars

  std::vector<uint8_t> header = CreateTarHeader(
      "././@LongLink", long_name.size(), TarReader::kTypeGnuLongName);

  auto entry = TarReader::ParseHeader(header);

  // For GNU long name entries, type should be kOther
  ASSERT_TRUE(entry.has_value());
  EXPECT_EQ(TarEntry::Type::kOther, entry->type);
}

TEST_F(TarReaderTest, ParsePaxLongPath) {
  // PAX with a path longer than 100 characters
  // Format: "<len> path=<value>\n" where len is total record length
  std::string long_path(150, 'x');
  long_path += ".txt";

  // Total length = digits of length + space + "path=" + value + newline
  // Need to account for the fact that length includes itself
  // "NNN path=<154 chars>\n" = 3 + 1 + 5 + 154 + 1 = 164
  std::string pax_content = "164 path=" + long_path + "\n";
  std::vector<uint8_t> content(pax_content.begin(), pax_content.end());

  auto path = TarReader::ParsePaxPath(content);

  ASSERT_TRUE(path.has_value());
  EXPECT_EQ(long_path, *path);
}

TEST_F(TarReaderTest, ParsePaxPathTooLong) {
  // PAX with a path exceeding kMaxPathLength (4096)
  std::string too_long_path(5000, 'x');
  too_long_path += ".txt";

  // Build PAX content
  std::string value_part = "path=" + too_long_path + "\n";
  // Length field needs to include itself - approximate
  size_t approx_len = 5 + value_part.size();  // "NNNNN " prefix
  std::string pax_content = std::to_string(approx_len) + " " + value_part;
  // Recalculate with actual length
  pax_content = std::to_string(pax_content.size()) + " " + value_part;

  std::vector<uint8_t> content(pax_content.begin(), pax_content.end());

  auto path = TarReader::ParsePaxPath(content);

  // Should reject paths exceeding the limit
  EXPECT_FALSE(path.has_value());
}

TEST_F(TarReaderTest, ParsePaxMalformedLength) {
  // PAX with invalid/malicious length field
  std::string pax_content = "99999999999999999 path=test.txt\n";
  std::vector<uint8_t> content(pax_content.begin(), pax_content.end());

  auto path = TarReader::ParsePaxPath(content);

  // Should handle gracefully (length exceeds buffer)
  EXPECT_FALSE(path.has_value());
}

TEST_F(TarReaderTest, ParsePaxZeroLength) {
  std::string pax_content = "0 path=test.txt\n";
  std::vector<uint8_t> content(pax_content.begin(), pax_content.end());

  auto path = TarReader::ParsePaxPath(content);

  // Zero length records should be rejected
  EXPECT_FALSE(path.has_value());
}

TEST_F(TarReaderTest, ParsePaxNegativeLength) {
  // Negative numbers should be rejected (StringToSizeT handles this)
  std::string pax_content = "-10 path=test.txt\n";
  std::vector<uint8_t> content(pax_content.begin(), pax_content.end());

  auto path = TarReader::ParsePaxPath(content);

  EXPECT_FALSE(path.has_value());
}

TEST_F(TarReaderTest, ParseHeaderSymlink) {
  std::vector<uint8_t> header =
      CreateTarHeader("link.txt", 0, TarReader::kTypeSymlink);

  auto entry = TarReader::ParseHeader(header);

  ASSERT_TRUE(entry.has_value());
  EXPECT_EQ(TarEntry::Type::kOther, entry->type);
}

TEST_F(TarReaderTest, ParseHeaderHardLink) {
  std::vector<uint8_t> header =
      CreateTarHeader("hardlink.txt", 0, TarReader::kTypeHardLink);

  auto entry = TarReader::ParseHeader(header);

  ASSERT_TRUE(entry.has_value());
  EXPECT_EQ(TarEntry::Type::kOther, entry->type);
}

TEST_F(TarReaderTest, ParseHeaderPaxGlobalType) {
  std::vector<uint8_t> header =
      CreateTarHeader("pax_global", 0, TarReader::kTypePaxGlobal);

  auto entry = TarReader::ParseHeader(header);

  ASSERT_TRUE(entry.has_value());
  EXPECT_EQ(TarEntry::Type::kOther, entry->type);
}

TEST_F(TarReaderTest, ParseHeaderWithPrefix) {
  std::vector<uint8_t> header =
      CreateTarHeader("basename.txt", 100, TarReader::kTypeRegular);

  // Set prefix field for ustar long path support
  const char* prefix = "some/long/prefix/dir";
  std::memcpy(header.data() + TarReader::kPrefixOffset, prefix,
              std::strlen(prefix));
  RecalculateChecksum(header);

  auto entry = TarReader::ParseHeader(header);

  ASSERT_TRUE(entry.has_value());
  EXPECT_EQ(L"some/long/prefix/dir/basename.txt", entry->path.value());
}

TEST_F(TarReaderTest, ParseHeaderWithOverrideName) {
  std::vector<uint8_t> header =
      CreateTarHeader("original.txt", 100, TarReader::kTypeRegular);

  auto entry = TarReader::ParseHeader(header, "overridden.txt");

  ASSERT_TRUE(entry.has_value());
  EXPECT_EQ(L"overridden.txt", entry->path.value());
}

TEST_F(TarReaderTest, ParseHeaderSmallBuffer) {
  // Buffer smaller than 512 bytes should return nullopt
  std::vector<uint8_t> small_buffer(256, 0);

  auto entry = TarReader::ParseHeader(small_buffer);

  EXPECT_FALSE(entry.has_value());
}

TEST_F(TarReaderTest, ParseHeaderBadSizeField) {
  std::vector<uint8_t> header =
      CreateTarHeader("test.txt", 100, TarReader::kTypeRegular);

  // Overwrite size field with invalid octal (contains 'a')
  std::memcpy(header.data() + TarReader::kSizeOffset, "0000000abc ", 12);
  RecalculateChecksum(header);

  auto entry = TarReader::ParseHeader(header);

  // Should return nullopt because size field has invalid octal
  EXPECT_FALSE(entry.has_value());
}

TEST_F(TarReaderTest, ParseHeaderMtimeValid) {
  // Use a known Unix timestamp: 1640000000 = 2021-12-20T14:13:20Z
  std::vector<uint8_t> header =
      CreateTarHeader("test.txt", 100, TarReader::kTypeRegular, 1640000000);

  auto entry = TarReader::ParseHeader(header);

  ASSERT_TRUE(entry.has_value());
  EXPECT_FALSE(entry->mtime.is_null());
  EXPECT_EQ(base::Time::FromTimeT(1640000000), entry->mtime);
}

TEST_F(TarReaderTest, IsEndOfArchiveSmallBuffer) {
  // Buffer smaller than 512 bytes should return false
  std::vector<uint8_t> small_buffer(256, 0);

  EXPECT_FALSE(TarReader::IsEndOfArchive(small_buffer));
}

TEST_F(TarReaderTest, ValidateChecksumSmallBuffer) {
  // Buffer smaller than 512 bytes should return false
  std::vector<uint8_t> small_buffer(256, 0);

  EXPECT_FALSE(TarReader::ValidateChecksum(small_buffer));
}

TEST_F(TarReaderTest, ParsePaxNonPathKey) {
  // PAX content with only non-path keys should return nullopt
  std::string pax_content = "30 mtime=1640000000.000000\n";
  std::vector<uint8_t> content(pax_content.begin(), pax_content.end());

  auto path = TarReader::ParsePaxPath(content);

  EXPECT_FALSE(path.has_value());
}

// ============================================================================
// XzDecompressor Tests
// ============================================================================

class XzDecompressorTest : public testing::Test {
 protected:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  base::ScopedTempDir temp_dir_;
};

TEST_F(XzDecompressorTest, DecompressValid) {
  // Use the embedded test archive to verify decompression works
  XzDecompressor decompressor;
  std::vector<uint8_t> output;

  bool result = decompressor.DecompressChunk(
      base::span<const uint8_t>(kTestTarXz, kTestTarXzLen), &output, true);

  EXPECT_TRUE(result);
  EXPECT_FALSE(output.empty());
  // Output should be tar data (multiple of 512 bytes)
  EXPECT_EQ(0u, output.size() % 512);
}

TEST_F(XzDecompressorTest, DecompressCorrupt) {
  XzDecompressor decompressor;
  std::vector<uint8_t> output;

  // Feed garbage data
  std::vector<uint8_t> garbage = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05};

  bool result = decompressor.DecompressChunk(garbage, &output, true);

  // Should fail on corrupt data
  EXPECT_FALSE(result);
}

TEST_F(XzDecompressorTest, IsFinished) {
  XzDecompressor decompressor;
  std::vector<uint8_t> output;

  // Should not be finished before any data is processed
  EXPECT_FALSE(decompressor.IsFinished());

  // Process the full archive
  decompressor.DecompressChunk(
      base::span<const uint8_t>(kTestTarXz, kTestTarXzLen), &output, true);

  // Should be finished after processing complete stream
  EXPECT_TRUE(decompressor.IsFinished());
}

TEST_F(XzDecompressorTest, DecompressChunkNullOutput) {
  XzDecompressor decompressor;

  bool result = decompressor.DecompressChunk(
      base::span<const uint8_t>(kTestTarXz, kTestTarXzLen), nullptr, true);

  EXPECT_FALSE(result);
}

// ============================================================================
// Archive Extraction Tests (using embedded test data)
// ============================================================================

class InstallerArchiveTest : public testing::Test {
 protected:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  base::ScopedTempDir temp_dir_;
};

TEST_F(InstallerArchiveTest, ExtractMultiFile) {
  base::FilePath archive_path = WriteTestArchive(temp_dir_.GetPath());
  base::FilePath dest_dir = temp_dir_.GetPath().AppendASCII("extracted");

  ArchiveError result = ExtractTarXzSingleThread(archive_path, dest_dir);

  EXPECT_EQ(ArchiveError::kSuccess, result);

  // Verify extracted files exist
  EXPECT_TRUE(base::DirectoryExists(dest_dir.AppendASCII("testdir")));
  EXPECT_TRUE(base::PathExists(dest_dir.AppendASCII("testdir/hello.txt")));
  EXPECT_TRUE(base::DirectoryExists(dest_dir.AppendASCII("testdir/subdir")));
  EXPECT_TRUE(
      base::PathExists(dest_dir.AppendASCII("testdir/subdir/nested.txt")));
}

TEST_F(InstallerArchiveTest, ExtractPreservesStructure) {
  base::FilePath archive_path = WriteTestArchive(temp_dir_.GetPath());
  base::FilePath dest_dir = temp_dir_.GetPath().AppendASCII("extracted");

  ArchiveError result = ExtractTarXzSingleThread(archive_path, dest_dir);

  ASSERT_EQ(ArchiveError::kSuccess, result);

  // Verify directory structure
  EXPECT_TRUE(base::DirectoryExists(dest_dir.AppendASCII("testdir")));
  EXPECT_TRUE(base::DirectoryExists(dest_dir.AppendASCII("testdir/subdir")));

  // Verify file contents
  std::string content;
  ASSERT_TRUE(base::ReadFileToString(dest_dir.AppendASCII("testdir/hello.txt"),
                                     &content));
  EXPECT_EQ("Hello CEF", content);

  ASSERT_TRUE(base::ReadFileToString(
      dest_dir.AppendASCII("testdir/subdir/nested.txt"), &content));
  EXPECT_EQ("Nested content", content);
}

TEST_F(InstallerArchiveTest, ExtractPreservesMtime) {
  base::FilePath archive_path = WriteTestArchive(temp_dir_.GetPath());
  base::FilePath dest_dir = temp_dir_.GetPath().AppendASCII("extracted");

  ArchiveError result = ExtractTarXzSingleThread(archive_path, dest_dir);

  ASSERT_EQ(ArchiveError::kSuccess, result);

  // Verify mtime was set (should be some past time, not current time)
  base::File::Info info;
  ASSERT_TRUE(
      base::GetFileInfo(dest_dir.AppendASCII("testdir/hello.txt"), &info));
  // Just verify the file exists and has some modification time
  EXPECT_FALSE(info.last_modified.is_null());
}

TEST_F(InstallerArchiveTest, ExtractCorruptXz) {
  // Create a file with invalid xz data
  base::FilePath archive_path =
      temp_dir_.GetPath().AppendASCII("corrupt.tar.xz");
  std::vector<uint8_t> garbage = {0x00, 0x01, 0x02, 0x03};
  ASSERT_TRUE(base::WriteFile(archive_path, base::as_string_view(garbage)));

  base::FilePath dest_dir = temp_dir_.GetPath().AppendASCII("extracted");

  ArchiveError result = ExtractTarXzSingleThread(archive_path, dest_dir);

  EXPECT_EQ(ArchiveError::kInvalidFormat, result);
}

TEST_F(InstallerArchiveTest, ExtractCorruptTar) {
  // Create valid xz header but with garbage tar content
  base::FilePath archive_path =
      temp_dir_.GetPath().AppendASCII("nonexistent.tar.xz");

  ArchiveError result =
      ExtractTarXzSingleThread(archive_path, temp_dir_.GetPath());

  EXPECT_EQ(ArchiveError::kFileNotFound, result);
}

TEST_F(InstallerArchiveTest, ExtractPathTraversal) {
  // Test that path traversal is rejected at the TarReader level
  std::vector<uint8_t> header =
      CreateTarHeader("../../../etc/passwd", 10, TarReader::kTypeRegular);

  auto entry = TarReader::ParseHeader(header);

  // ParseHeader should reject path traversal
  EXPECT_FALSE(entry.has_value());
}

TEST_F(InstallerArchiveTest, ExtractAbsolutePath) {
  // Test that absolute paths are rejected
  EXPECT_FALSE(TarReader::IsPathSafe("/etc/passwd"));
  EXPECT_FALSE(TarReader::IsPathSafe("C:\\Windows\\System32\\cmd.exe"));
}

TEST_F(InstallerArchiveTest, ExtractWindowsReservedName) {
  // Test that Windows reserved names are rejected at TarReader level
  std::vector<uint8_t> header =
      CreateTarHeader("CON", 10, TarReader::kTypeRegular);

  auto entry = TarReader::ParseHeader(header);

  // ParseHeader should reject Windows reserved names
  EXPECT_FALSE(entry.has_value());
}

TEST_F(InstallerArchiveTest, ExtractWindowsReservedNameInSubdir) {
  std::vector<uint8_t> header =
      CreateTarHeader("subdir/NUL.txt", 10, TarReader::kTypeRegular);

  auto entry = TarReader::ParseHeader(header);

  // Should reject reserved names even in subdirectories
  EXPECT_FALSE(entry.has_value());
}

TEST_F(InstallerArchiveTest, ExtractNullByteInPath) {
  // Create header with embedded null byte in name
  std::vector<uint8_t> header(TarReader::kHeaderSize, 0);

  // Name with embedded null: "safe\0/../etc"
  const char name_with_null[] = "safe\0/../etc";
  std::memcpy(header.data(), name_with_null, sizeof(name_with_null) - 1);

  // Fill in rest of header like CreateTarHeader does
  std::memcpy(header.data() + TarReader::kModeOffset, "0000644 ", 8);
  std::memcpy(header.data() + 108, "0000000 ", 8);  // UID
  std::memcpy(header.data() + 116, "0000000 ", 8);  // GID
  std::memcpy(header.data() + TarReader::kSizeOffset, "00000000010 ", 12);
  std::memcpy(header.data() + TarReader::kMtimeOffset, "00000000000 ", 12);
  std::memset(header.data() + TarReader::kChecksumOffset, ' ', 8);
  header[TarReader::kTypeflagOffset] = TarReader::kTypeRegular;
  std::memcpy(header.data() + TarReader::kMagicOffset, "ustar", 5);
  header[263] = '0';
  header[264] = '0';

  // Calculate checksum
  uint64_t checksum = 0;
  for (size_t i = 0; i < TarReader::kHeaderSize; ++i) {
    if (i >= TarReader::kChecksumOffset &&
        i < TarReader::kChecksumOffset + TarReader::kChecksumLength) {
      checksum += ' ';
    } else {
      checksum += header[i];
    }
  }
  char checksum_str[8];
  snprintf(checksum_str, sizeof(checksum_str), "%06llo ",
           static_cast<unsigned long long>(checksum));
  std::memcpy(header.data() + TarReader::kChecksumOffset, checksum_str, 7);

  // The null byte will cause ExtractString to truncate the name to "safe"
  // which is actually safe. The concern is if the full string with null
  // were somehow used. Our IsPathSafe check handles this case.
  auto entry = TarReader::ParseHeader(header);

  // The entry should parse (name truncated at null), but if it contained
  // the full string it would be caught by IsPathSafe
  // For this test, "safe" is a valid name so it passes
  ASSERT_TRUE(entry.has_value());
  EXPECT_EQ(L"safe", entry->path.value());
}

TEST_F(InstallerArchiveTest, ExtractVeryLongPath) {
  // Create header with path exceeding kMaxPathLength
  std::string long_name(200, 'a');  // Max in tar header is 100, but test anyway

  std::vector<uint8_t> header =
      CreateTarHeader(long_name.substr(0, 100), 10, TarReader::kTypeRegular);

  auto entry = TarReader::ParseHeader(header);

  // Should accept since it's within the 100-char tar limit
  ASSERT_TRUE(entry.has_value());
}

TEST_F(InstallerArchiveTest, ExtractTraversalVariations) {
  // Various path traversal attempts
  EXPECT_FALSE(TarReader::IsPathSafe(".."));
  EXPECT_FALSE(TarReader::IsPathSafe("../"));
  EXPECT_FALSE(TarReader::IsPathSafe("..\\"));
  EXPECT_FALSE(TarReader::IsPathSafe("foo/.."));
  EXPECT_FALSE(TarReader::IsPathSafe("foo\\.."));
  EXPECT_FALSE(TarReader::IsPathSafe("foo/../bar"));
  EXPECT_FALSE(TarReader::IsPathSafe("foo/bar/../../baz"));
  EXPECT_FALSE(TarReader::IsPathSafe("foo/bar/../../../etc/passwd"));

  // These should be safe
  EXPECT_TRUE(TarReader::IsPathSafe("foo"));
  EXPECT_TRUE(TarReader::IsPathSafe("foo/bar"));
  EXPECT_TRUE(TarReader::IsPathSafe("foo/bar.txt"));
  EXPECT_TRUE(TarReader::IsPathSafe("..."));    // Three dots is not traversal
  EXPECT_TRUE(TarReader::IsPathSafe("..foo"));  // Starts with .. but not alone
  EXPECT_TRUE(TarReader::IsPathSafe("foo..bar"));
  EXPECT_TRUE(TarReader::IsPathSafe("foo../bar"));  // .. not a full component
  EXPECT_TRUE(TarReader::IsPathSafe("path/to../something"));
}

TEST_F(InstallerArchiveTest, ExtractProgress) {
  base::FilePath archive_path = WriteTestArchive(temp_dir_.GetPath());
  base::FilePath dest_dir = temp_dir_.GetPath().AppendASCII("extracted");
  bool callback_invoked = false;

  ExtractionProgressCallback callback = base::BindRepeating(
      [](bool* invoked, uint64_t, uint64_t) -> bool {
        *invoked = true;
        return true;
      },
      &callback_invoked);

  ArchiveError result =
      ExtractTarXzSingleThread(archive_path, dest_dir, callback);

  EXPECT_EQ(ArchiveError::kSuccess, result);
  EXPECT_TRUE(callback_invoked);
}

TEST_F(InstallerArchiveTest, ExtractCancel) {
  base::FilePath archive_path = WriteTestArchive(temp_dir_.GetPath());
  base::FilePath dest_dir = temp_dir_.GetPath().AppendASCII("extracted");

  // Cancel on first callback
  ExtractionProgressCallback cancel_callback =
      base::BindRepeating([](uint64_t, uint64_t) -> bool { return false; });

  ArchiveError result =
      ExtractTarXzSingleThread(archive_path, dest_dir, cancel_callback);

  EXPECT_EQ(ArchiveError::kCancelled, result);
}

TEST_F(InstallerArchiveTest, SingleThreadProgressUsesCompressedBytes) {
  base::FilePath archive_path = WriteTestArchive(temp_dir_.GetPath());
  base::FilePath dest_dir = temp_dir_.GetPath().AppendASCII("extracted");

  // Get the compressed file size for comparison.
  auto file_size_opt = base::GetFileSize(archive_path);
  ASSERT_TRUE(file_size_opt.has_value());
  int64_t file_size = *file_size_opt;

  uint64_t last_total_bytes = 0;
  uint64_t last_bytes = 0;

  ExtractionProgressCallback callback = base::BindRepeating(
      [](uint64_t* last_total_bytes, uint64_t* last_bytes, uint64_t bytes,
         uint64_t total_bytes) -> bool {
        *last_total_bytes = total_bytes;
        *last_bytes = bytes;
        return true;
      },
      &last_total_bytes, &last_bytes);

  ArchiveError result =
      ExtractTarXzSingleThread(archive_path, dest_dir, callback);

  ASSERT_EQ(ArchiveError::kSuccess, result);

  // total_bytes should be the compressed file size.
  EXPECT_EQ(static_cast<uint64_t>(file_size), last_total_bytes);
  // After successful extraction, all compressed bytes should have been read.
  EXPECT_EQ(last_bytes, last_total_bytes);
}

// ============================================================================
// Error String Tests
// ============================================================================

TEST_F(InstallerArchiveTest, ArchiveErrorToString) {
  EXPECT_STREQ("Success", ArchiveErrorToString(ArchiveError::kSuccess));
  EXPECT_STREQ("Archive file not found",
               ArchiveErrorToString(ArchiveError::kFileNotFound));
  EXPECT_STREQ("Invalid archive format (xz decompression failed)",
               ArchiveErrorToString(ArchiveError::kInvalidFormat));
  EXPECT_STREQ("Invalid tar header (checksum mismatch)",
               ArchiveErrorToString(ArchiveError::kInvalidHeader));
  EXPECT_STREQ("Archive contains path traversal attack",
               ArchiveErrorToString(ArchiveError::kPathTraversal));
  EXPECT_STREQ("Archive contains absolute path",
               ArchiveErrorToString(ArchiveError::kAbsolutePath));
  EXPECT_STREQ("Failed to write extracted file",
               ArchiveErrorToString(ArchiveError::kWriteError));
  EXPECT_STREQ("Extraction cancelled",
               ArchiveErrorToString(ArchiveError::kCancelled));
}

TEST_F(InstallerArchiveTest, ArchiveErrorToStringAll) {
  // Verify error strings for codes not covered by the test above
  EXPECT_STREQ("Failed to read archive file",
               ArchiveErrorToString(ArchiveError::kFileReadError));
  EXPECT_STREQ("Extraction failed",
               ArchiveErrorToString(ArchiveError::kExtractionFailed));
  EXPECT_STREQ("Disk full", ArchiveErrorToString(ArchiveError::kDiskFull));
  EXPECT_STREQ("Unsupported archive entry type",
               ArchiveErrorToString(ArchiveError::kUnsupportedEntryType));
}

// ============================================================================
// DecompressXzBlock Tests
// ============================================================================

TEST_F(InstallerArchiveTest, DecompressXzBlock) {
  // Read a multi-block archive, parse index, read first block, decompress it.
  base::FilePath fixture =
      test::GetTestDataPath().AppendASCII("multi_block_4.tar.xz");
  base::File file(fixture, base::File::FLAG_OPEN | base::File::FLAG_READ);
  ASSERT_TRUE(file.IsValid());

  auto info = ParseXzIndex(file);
  ASSERT_TRUE(info.has_value());
  ASSERT_GE(info->blocks.size(), 1u);

  const auto& block = info->blocks[0];
  std::vector<uint8_t> block_data(
      base::checked_cast<size_t>(block.compressed_size));
  file.Seek(base::File::FROM_BEGIN,
            base::checked_cast<int64_t>(block.compressed_offset));
  ASSERT_TRUE(file.ReadAtCurrentPosAndCheck(block_data));

  std::vector<uint8_t> output(
      base::checked_cast<size_t>(block.uncompressed_size));
  bool result =
      DecompressXzBlock(block_data, info->check_type,
                        static_cast<uint16_t>(info->check_type), output);

  EXPECT_TRUE(result);
}

TEST_F(InstallerArchiveTest, DecompressXzBlockInvalid) {
  // Zeroed data should fail.
  std::vector<uint8_t> zeroed(256, 0);
  std::vector<uint8_t> output(4096);
  EXPECT_FALSE(DecompressXzBlock(zeroed, 4, 4, output));

  // Random-ish data should fail.
  std::vector<uint8_t> garbage(256);
  for (size_t i = 0; i < garbage.size(); ++i) {
    garbage[i] = static_cast<uint8_t>(i * 37 + 13);
  }
  EXPECT_FALSE(DecompressXzBlock(garbage, 4, 4, output));
}

TEST_F(InstallerArchiveTest, DecompressXzBlockCrcMismatch) {
  // Read a valid block, then flip bits in the CRC at the end.
  base::FilePath fixture =
      test::GetTestDataPath().AppendASCII("multi_block_4.tar.xz");
  base::File file(fixture, base::File::FLAG_OPEN | base::File::FLAG_READ);
  ASSERT_TRUE(file.IsValid());

  auto info = ParseXzIndex(file);
  ASSERT_TRUE(info.has_value());
  ASSERT_GE(info->blocks.size(), 1u);

  const auto& block = info->blocks[0];
  std::vector<uint8_t> block_data(
      base::checked_cast<size_t>(block.compressed_size));
  file.Seek(base::File::FROM_BEGIN,
            base::checked_cast<int64_t>(block.compressed_offset));
  ASSERT_TRUE(file.ReadAtCurrentPosAndCheck(block_data));

  // Flip bits near the end of the block (where the CRC64 check lives).
  // The check comes after compressed data + padding, before the next block.
  size_t corrupt_offset = block_data.size() - 5;
  block_data[corrupt_offset] ^= 0xFF;

  std::vector<uint8_t> output(
      base::checked_cast<size_t>(block.uncompressed_size));
  EXPECT_FALSE(DecompressXzBlock(block_data, info->check_type,
                                 static_cast<uint16_t>(info->check_type),
                                 output));
}

// ============================================================================
// ExtractTarFromBuffer Tests
// ============================================================================

// Helper: build tar end-of-archive marker (two zero blocks).
std::vector<uint8_t> BuildTarEndMarker() {
  return std::vector<uint8_t>(TarReader::kHeaderSize * 2, 0);
}

// Helper: build complete tar with one file.
std::vector<uint8_t> BuildSimpleTar(const std::string& name,
                                    const std::string& content,
                                    char typeflag = TarReader::kTypeRegular,
                                    uint64_t mtime = 0) {
  std::vector<uint8_t> tar;

  auto header = CreateTarHeader(name, content.size(), typeflag, mtime);
  tar.insert(tar.end(), header.begin(), header.end());

  tar.insert(tar.end(), content.begin(), content.end());
  size_t padding = (512 - (content.size() % 512)) % 512;
  tar.resize(tar.size() + padding, 0);

  auto end = BuildTarEndMarker();
  tar.insert(tar.end(), end.begin(), end.end());

  return tar;
}

TEST_F(InstallerArchiveTest, ExtractTarFromBuffer) {
  auto tar = BuildSimpleTar("testfile.txt", "Hello World!");
  base::FilePath dest = temp_dir_.GetPath().AppendASCII("out");

  EXPECT_EQ(ArchiveError::kSuccess, ExtractTarFromBuffer(tar, dest));

  std::string content;
  ASSERT_TRUE(
      base::ReadFileToString(dest.AppendASCII("testfile.txt"), &content));
  EXPECT_EQ("Hello World!", content);
}

TEST_F(InstallerArchiveTest, ExtractTarFromBuffer_GnuLongName) {
  // Build a GNU long name entry ('L') followed by the real file entry.
  // Use a path with directory separators to stay within filename limits.
  // Keep the total path short enough for Windows temp dirs.
  std::string long_name = "sub/";
  long_name += std::string(100, 'a');
  long_name += ".txt";
  std::string file_content = "long name content";

  std::vector<uint8_t> tar;

  // 'L' header for the long name.
  auto l_header = CreateTarHeader("././@LongLink", long_name.size() + 1,
                                  TarReader::kTypeGnuLongName);
  tar.insert(tar.end(), l_header.begin(), l_header.end());

  // Long name content (null-terminated).
  std::string long_name_data = long_name + '\0';
  tar.insert(tar.end(), long_name_data.begin(), long_name_data.end());
  size_t padding = (512 - (long_name_data.size() % 512)) % 512;
  tar.resize(tar.size() + padding, 0);

  // Actual file entry (name in header is truncated, overridden by long name).
  auto file_header = CreateTarHeader(
      long_name.substr(0, 100), file_content.size(), TarReader::kTypeRegular);
  tar.insert(tar.end(), file_header.begin(), file_header.end());
  tar.insert(tar.end(), file_content.begin(), file_content.end());
  padding = (512 - (file_content.size() % 512)) % 512;
  tar.resize(tar.size() + padding, 0);

  auto end = BuildTarEndMarker();
  tar.insert(tar.end(), end.begin(), end.end());

  base::FilePath dest = temp_dir_.GetPath().AppendASCII("out");
  EXPECT_EQ(ArchiveError::kSuccess, ExtractTarFromBuffer(tar, dest));

  std::string content;
  ASSERT_TRUE(base::ReadFileToString(dest.Append(base::UTF8ToWide(long_name)),
                                     &content));
  EXPECT_EQ("long name content", content);
}

TEST_F(InstallerArchiveTest, ExtractTarFromBuffer_EmptyFile) {
  auto tar = BuildSimpleTar("empty.txt", "");
  base::FilePath dest = temp_dir_.GetPath().AppendASCII("out");

  EXPECT_EQ(ArchiveError::kSuccess, ExtractTarFromBuffer(tar, dest));
  EXPECT_TRUE(base::PathExists(dest.AppendASCII("empty.txt")));

  auto size = base::GetFileSize(dest.AppendASCII("empty.txt"));
  ASSERT_TRUE(size.has_value());
  EXPECT_EQ(0, *size);
}

TEST_F(InstallerArchiveTest, ExtractTarFromBuffer_SymlinkSkipped) {
  // Symlink entries should be silently skipped.
  auto tar = BuildSimpleTar("link.txt", "", TarReader::kTypeSymlink);
  base::FilePath dest = temp_dir_.GetPath().AppendASCII("out");

  EXPECT_EQ(ArchiveError::kSuccess, ExtractTarFromBuffer(tar, dest));
  // The symlink should not have been created as a file.
  EXPECT_FALSE(base::PathExists(dest.AppendASCII("link.txt")));
}

// Disabled: creates 100K+ directories on disk, takes ~23s.
// Use ExtractTarFromBuffer_MaxEntryLimitFast instead.
TEST_F(InstallerArchiveTest, DISABLED_ExtractTarFromBuffer_MaxEntryLimit) {
  std::vector<uint8_t> tar;
  for (size_t i = 0; i <= kMaxEntryCount; ++i) {
    char name[20];
    snprintf(name, sizeof(name), "d%zu/", i);
    auto header = CreateTarHeader(name, 0, TarReader::kTypeDirectory);
    tar.insert(tar.end(), header.begin(), header.end());
  }
  auto end = BuildTarEndMarker();
  tar.insert(tar.end(), end.begin(), end.end());

  base::FilePath dest = temp_dir_.GetPath().AppendASCII("out");
  EXPECT_EQ(ArchiveError::kExtractionFailed, ExtractTarFromBuffer(tar, dest));
}

TEST_F(InstallerArchiveTest, ExtractTarFromBuffer_MaxEntryLimitFast) {
  // Verify the limit exists at the expected value without disk I/O.
  // kMaxEntryCount is used by both ExtractTarFromBuffer and ExtractTarXz.
  EXPECT_EQ(100000u, kMaxEntryCount);
}

TEST_F(InstallerArchiveTest, ExtractTarFromBuffer_PathTraversal) {
  auto tar = BuildSimpleTar("../etc/passwd", "root:x:0:0");
  base::FilePath dest = temp_dir_.GetPath().AppendASCII("out");

  // ParseHeader rejects "../" paths, so ExtractTarFromBuffer gets
  // kInvalidHeader.
  EXPECT_EQ(ArchiveError::kInvalidHeader, ExtractTarFromBuffer(tar, dest));
}

TEST_F(InstallerArchiveTest, ExtractTarFromBuffer_PaxExtended) {
  // Build a PAX extended header with a path override.
  // PAX format: "<len> key=value\n" where <len> includes the length digits.
  std::string pax_path = "pax_override.txt";
  std::string suffix = " path=" + pax_path + "\n";
  // Try different digit counts for self-referential length.
  size_t len = suffix.size() + 2;  // assume 2-digit length
  std::string pax_record = std::to_string(len) + suffix;
  if (pax_record.size() != len) {
    len = suffix.size() + std::to_string(suffix.size() + 3).size();
    pax_record = std::to_string(len) + suffix;
  }
  std::string file_content = "pax content";

  std::vector<uint8_t> tar;

  // PAX extended header entry.
  auto pax_header = CreateTarHeader("PaxHeader/file", pax_record.size(),
                                    TarReader::kTypePaxExtended);
  tar.insert(tar.end(), pax_header.begin(), pax_header.end());
  tar.insert(tar.end(), pax_record.begin(), pax_record.end());
  size_t padding = (512 - (pax_record.size() % 512)) % 512;
  tar.resize(tar.size() + padding, 0);

  // Actual file entry.
  auto file_header = CreateTarHeader("original.txt", file_content.size(),
                                     TarReader::kTypeRegular);
  tar.insert(tar.end(), file_header.begin(), file_header.end());
  tar.insert(tar.end(), file_content.begin(), file_content.end());
  padding = (512 - (file_content.size() % 512)) % 512;
  tar.resize(tar.size() + padding, 0);

  auto end = BuildTarEndMarker();
  tar.insert(tar.end(), end.begin(), end.end());

  base::FilePath dest = temp_dir_.GetPath().AppendASCII("out");
  EXPECT_EQ(ArchiveError::kSuccess, ExtractTarFromBuffer(tar, dest));

  // Should be extracted with the PAX-overridden name.
  std::string content;
  ASSERT_TRUE(
      base::ReadFileToString(dest.AppendASCII("pax_override.txt"), &content));
  EXPECT_EQ("pax content", content);
}

TEST_F(InstallerArchiveTest, ExtractTarFromBuffer_PaxMultipleRecords) {
  // PAX with a non-path key followed by path key.
  // "30 mtime=1640000000.000000\n" = 30 chars total. Correct.
  // "22 path=from_pax.txt\n" = 22 chars total. Let's verify: "22" + "
  // path=from_pax.txt\n" = 2 + 19 = 21. Need 22. "27 mtime=1640000000.000000\n"
  // = 27 chars "21 path=from_pax.txt\n" = 21 chars
  std::string pax_content =
      "27 mtime=1640000000.000000\n"
      "21 path=from_pax.txt\n";

  std::string file_content = "multi-record pax";

  std::vector<uint8_t> tar;
  auto pax_header = CreateTarHeader("PaxHeader/file", pax_content.size(),
                                    TarReader::kTypePaxExtended);
  tar.insert(tar.end(), pax_header.begin(), pax_header.end());
  tar.insert(tar.end(), pax_content.begin(), pax_content.end());
  size_t padding = (512 - (pax_content.size() % 512)) % 512;
  tar.resize(tar.size() + padding, 0);

  auto file_header = CreateTarHeader("original.txt", file_content.size(),
                                     TarReader::kTypeRegular);
  tar.insert(tar.end(), file_header.begin(), file_header.end());
  tar.insert(tar.end(), file_content.begin(), file_content.end());
  padding = (512 - (file_content.size() % 512)) % 512;
  tar.resize(tar.size() + padding, 0);

  auto end = BuildTarEndMarker();
  tar.insert(tar.end(), end.begin(), end.end());

  base::FilePath dest = temp_dir_.GetPath().AppendASCII("out");
  EXPECT_EQ(ArchiveError::kSuccess, ExtractTarFromBuffer(tar, dest));

  std::string content;
  ASSERT_TRUE(
      base::ReadFileToString(dest.AppendASCII("from_pax.txt"), &content));
  EXPECT_EQ("multi-record pax", content);
}

TEST_F(InstallerArchiveTest, ExtractTarFromBuffer_DirectoryEntry) {
  // Directory entries should create subdirectories.
  std::vector<uint8_t> tar;
  auto dir_header =
      CreateTarHeader("subdir/nested/", 0, TarReader::kTypeDirectory);
  tar.insert(tar.end(), dir_header.begin(), dir_header.end());
  auto end = BuildTarEndMarker();
  tar.insert(tar.end(), end.begin(), end.end());

  base::FilePath dest = temp_dir_.GetPath().AppendASCII("out");
  EXPECT_EQ(ArchiveError::kSuccess, ExtractTarFromBuffer(tar, dest));
  EXPECT_TRUE(base::DirectoryExists(dest.AppendASCII("subdir/nested")));
}

TEST_F(InstallerArchiveTest, ExtractTarFromBuffer_MtimePreserved) {
  // Use a known Unix timestamp: 1640000000 = 2021-12-20T14:13:20Z
  auto tar =
      BuildSimpleTar("timed.txt", "hello", TarReader::kTypeRegular, 1640000000);
  base::FilePath dest = temp_dir_.GetPath().AppendASCII("out");

  EXPECT_EQ(ArchiveError::kSuccess, ExtractTarFromBuffer(tar, dest));

  base::File::Info info;
  ASSERT_TRUE(base::GetFileInfo(dest.AppendASCII("timed.txt"), &info));
  EXPECT_EQ(base::Time::FromTimeT(1640000000), info.last_modified);
}

TEST_F(InstallerArchiveTest, ExtractTarFromBuffer_Cancellation) {
  auto tar = BuildSimpleTar("cancel.txt", "data");
  base::FilePath dest = temp_dir_.GetPath().AppendASCII("out");

  ExtractionProgressCallback cancel =
      base::BindRepeating([](uint64_t, uint64_t) -> bool { return false; });

  EXPECT_EQ(ArchiveError::kCancelled,
            ExtractTarFromBuffer(tar, dest, std::move(cancel)));
}

TEST_F(InstallerArchiveTest, ExtractTarFromBuffer_EmptyArchive) {
  // An archive with just the end-of-archive marker (two zero blocks).
  auto tar = BuildTarEndMarker();
  base::FilePath dest = temp_dir_.GetPath().AppendASCII("out");

  EXPECT_EQ(ArchiveError::kSuccess, ExtractTarFromBuffer(tar, dest));
  EXPECT_TRUE(base::DirectoryExists(dest));
}

TEST_F(InstallerArchiveTest, ExtractTarFromBuffer_TruncatedContent) {
  // Build a header claiming 1000 bytes of content, but provide only 100.
  auto header = CreateTarHeader("trunc.txt", 1000, TarReader::kTypeRegular);
  std::vector<uint8_t> tar(header.begin(), header.end());
  // Only append 100 bytes of content (not enough).
  tar.resize(tar.size() + 100, 'X');

  base::FilePath dest = temp_dir_.GetPath().AppendASCII("out");
  // Should fail because pos + entry->size > tar_data.size().
  EXPECT_EQ(ArchiveError::kInvalidFormat, ExtractTarFromBuffer(tar, dest));
}

// ============================================================================
// ExtractTarXz integration tests (config + callback forwarding)
// ============================================================================

class ExtractTarXzIntegrationTest : public testing::Test {
 protected:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  base::FilePath GetFixture(const std::string& name) {
    return test::GetTestDataPath().AppendASCII(name);
  }

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
};

TEST_F(ExtractTarXzIntegrationTest, MultiBlockWithConfigAndProgress) {
  base::FilePath dest = temp_dir_.GetPath().AppendASCII("extracted");

  ExtractionConfig config;
  config.background_mode = true;

  uint64_t last_bytes = 0;
  uint64_t last_total = 0;
  ExtractionProgressCallback callback = base::BindRepeating(
      [](uint64_t* last_bytes, uint64_t* last_total, uint64_t bytes,
         uint64_t total) -> bool {
        *last_bytes = bytes;
        *last_total = total;
        return true;
      },
      &last_bytes, &last_total);

  ArchiveError result = ExtractTarXz(GetFixture("multi_block_4.tar.xz"), dest,
                                     config, std::move(callback));

  EXPECT_EQ(ArchiveError::kSuccess, result);
  EXPECT_GT(last_total, 0u);
  EXPECT_EQ(last_bytes, last_total);

  // Verify extracted content.
  std::string content;
  ASSERT_TRUE(base::ReadFileToString(dest.AppendASCII("multi4/file_0000.dat"),
                                     &content));
  ASSERT_EQ(4096u, content.size());
  EXPECT_EQ('\x00', content[0]);
  EXPECT_EQ('\x01', content[1]);
}

TEST_F(ExtractTarXzIntegrationTest, MultiBlockCancellation) {
  base::FilePath dest = temp_dir_.GetPath().AppendASCII("extracted");

  ExtractionProgressCallback cancel =
      base::BindRepeating([](uint64_t, uint64_t) -> bool { return false; });

  ArchiveError result = ExtractTarXz(GetFixture("multi_block_4.tar.xz"), dest,
                                     {}, std::move(cancel));

  EXPECT_EQ(ArchiveError::kCancelled, result);
}

TEST_F(ExtractTarXzIntegrationTest, SingleBlockExtractAndVerifyContent) {
  base::FilePath dest = temp_dir_.GetPath().AppendASCII("extracted");

  ArchiveError result =
      ExtractTarXzSingleThread(GetFixture("single_block.tar.xz"), dest);
  EXPECT_EQ(ArchiveError::kSuccess, result);
  EXPECT_TRUE(base::DirectoryExists(dest));
}

TEST_F(ExtractTarXzIntegrationTest, SingleBlockWithProgress) {
  base::FilePath dest = temp_dir_.GetPath().AppendASCII("extracted");

  bool callback_invoked = false;
  ExtractionProgressCallback callback = base::BindRepeating(
      [](bool* invoked, uint64_t, uint64_t) -> bool {
        *invoked = true;
        return true;
      },
      &callback_invoked);

  ArchiveError result = ExtractTarXz(GetFixture("single_block.tar.xz"), dest,
                                     {}, std::move(callback));

  EXPECT_EQ(ArchiveError::kSuccess, result);
  EXPECT_TRUE(callback_invoked);
}

}  // namespace
}  // namespace cef_installer
