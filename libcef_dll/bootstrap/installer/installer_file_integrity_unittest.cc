// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef_dll/bootstrap/installer/installer_file_integrity.h"

#include <windows.h>

#include <atomic>
#include <thread>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "cef/libcef_dll/bootstrap/installer/installer_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cef_installer {
namespace {

class InstallerFileIntegrityTest : public testing::Test {
 protected:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  void TearDown() override { SetConditionalDeleteHookForTesting({}); }

  base::FilePath TestPath(const std::string& name) {
    return temp_dir_.GetPath().AppendASCII(name);
  }

  base::FilePath ResolvedParent(const base::FilePath& path) {
    std::optional<base::FilePath> resolved =
        GetSafeDirectoryResolvedPath(path.DirName());
    EXPECT_TRUE(resolved);
    return resolved.value_or(base::FilePath());
  }

  base::ScopedTempDir temp_dir_;
};

// ============================================================================
// WriteFileWithIntegrity Tests
// ============================================================================

TEST_F(InstallerFileIntegrityTest, WriteCreatesFile) {
  base::FilePath path = TestPath("test.dat");

  EXPECT_TRUE(WriteFileWithIntegrity(path, "hello"));
  EXPECT_TRUE(base::PathExists(path));

  // File should be content + 16-byte footer.
  std::optional<int64_t> size = base::GetFileSize(path);
  ASSERT_TRUE(size.has_value());
  EXPECT_EQ(5 + 16, size.value());
}

TEST_F(InstallerFileIntegrityTest, WriteEmptyContent) {
  base::FilePath path = TestPath("empty.dat");

  EXPECT_TRUE(WriteFileWithIntegrity(path, ""));
  EXPECT_TRUE(base::PathExists(path));

  // File should be just the 16-byte footer.
  std::optional<int64_t> size = base::GetFileSize(path);
  ASSERT_TRUE(size.has_value());
  EXPECT_EQ(16, size.value());
}

TEST_F(InstallerFileIntegrityTest, OverwriteIsAtomicRoundTrip) {
  base::FilePath path = TestPath("overwrite.dat");
  ASSERT_TRUE(WriteFileWithIntegrity(path, "old"));
  ASSERT_TRUE(WriteFileWithIntegrity(path, "new"));

  std::string content;
  EXPECT_EQ(IntegrityResult::kSuccess, ReadFileWithIntegrity(path, &content));
  EXPECT_EQ("new", content);
}

TEST_F(InstallerFileIntegrityTest, ReplaceFailurePreservesPriorFile) {
  base::FilePath path = TestPath("locked.dat");
  ASSERT_TRUE(WriteFileWithIntegrity(path, "prior"));
  {
    base::File lock(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
    ASSERT_TRUE(lock.IsValid());
    EXPECT_FALSE(WriteFileWithIntegrity(path, "replacement"));
  }

  std::string content;
  EXPECT_EQ(IntegrityResult::kSuccess, ReadFileWithIntegrity(path, &content));
  EXPECT_EQ("prior", content);
}

TEST_F(InstallerFileIntegrityTest, ConcurrentWritersPublishWholeFile) {
  base::FilePath path = TestPath("concurrent.dat");
  std::vector<std::string> payloads;
  std::vector<std::thread> writers;
  std::atomic<int> successes = 0;
  for (int i = 0; i < 8; ++i) {
    payloads.push_back(std::string(4096, static_cast<char>('A' + i)));
  }
  for (const auto& payload : payloads) {
    writers.emplace_back([&, payload] {
      if (WriteFileWithIntegrity(path, payload)) {
        ++successes;
      }
    });
  }
  for (auto& writer : writers) {
    writer.join();
  }
  EXPECT_GT(successes.load(), 0);

  std::string content;
  ASSERT_EQ(IntegrityResult::kSuccess, ReadFileWithIntegrity(path, &content));
  EXPECT_NE(payloads.end(),
            std::find(payloads.begin(), payloads.end(), content));
}

// ============================================================================
// ReadFileWithIntegrity Tests
// ============================================================================

TEST_F(InstallerFileIntegrityTest, RoundTrip) {
  base::FilePath path = TestPath("roundtrip.dat");
  std::string original = "Hello, CEF Installer!";

  ASSERT_TRUE(WriteFileWithIntegrity(path, original));

  std::string content;
  IntegrityResult result = ReadFileWithIntegrity(path, &content);

  EXPECT_EQ(IntegrityResult::kSuccess, result);
  EXPECT_EQ(original, content);
}

TEST_F(InstallerFileIntegrityTest, RoundTripEmptyContent) {
  base::FilePath path = TestPath("empty_roundtrip.dat");

  ASSERT_TRUE(WriteFileWithIntegrity(path, ""));

  std::string content;
  IntegrityResult result = ReadFileWithIntegrity(path, &content);

  EXPECT_EQ(IntegrityResult::kSuccess, result);
  EXPECT_TRUE(content.empty());
}

TEST_F(InstallerFileIntegrityTest, RoundTripLargeContent) {
  base::FilePath path = TestPath("large.dat");
  std::string original(100 * 1024, 'X');  // 100 KB

  ASSERT_TRUE(WriteFileWithIntegrity(path, original));

  std::string content;
  IntegrityResult result = ReadFileWithIntegrity(path, &content);

  EXPECT_EQ(IntegrityResult::kSuccess, result);
  EXPECT_EQ(original, content);
}

TEST_F(InstallerFileIntegrityTest, RoundTripJsonContent) {
  base::FilePath path = TestPath("manifest.cache");
  std::string json = R"({"version":"137.3.5","file":"cef_137.3.5.tar.xz"})";

  ASSERT_TRUE(WriteFileWithIntegrity(path, json));

  std::string content;
  IntegrityResult result = ReadFileWithIntegrity(path, &content);

  EXPECT_EQ(IntegrityResult::kSuccess, result);
  EXPECT_EQ(json, content);
}

TEST_F(InstallerFileIntegrityTest, RoundTripBinaryContent) {
  base::FilePath path = TestPath("binary.dat");
  // Content with null bytes and non-printable characters.
  std::string original = std::string("\x00\x01\x02\xff\xfe\xfd", 6);

  ASSERT_TRUE(WriteFileWithIntegrity(path, original));

  std::string content;
  IntegrityResult result = ReadFileWithIntegrity(path, &content);

  EXPECT_EQ(IntegrityResult::kSuccess, result);
  EXPECT_EQ(original, content);
}

TEST_F(InstallerFileIntegrityTest, ReadNonExistentFile) {
  base::FilePath path = TestPath("nonexistent.dat");

  std::string content;
  IntegrityResult result = ReadFileWithIntegrity(path, &content);

  EXPECT_EQ(IntegrityResult::kFileNotFound, result);
}

TEST_F(InstallerFileIntegrityTest, ReadLegacyFileNoFooter) {
  base::FilePath path = TestPath("legacy.dat");
  std::string original = "legacy content without footer";
  ASSERT_TRUE(base::WriteFile(path, original));

  std::string content;
  IntegrityResult result = ReadFileWithIntegrity(path, &content);

  EXPECT_EQ(IntegrityResult::kSuccessNoFooter, result);
  EXPECT_EQ(original, content);
}

TEST_F(InstallerFileIntegrityTest, ReadSmallLegacyFile) {
  base::FilePath path = TestPath("tiny.dat");
  // File smaller than 16-byte footer — always treated as legacy.
  std::string original = "tiny";
  ASSERT_TRUE(base::WriteFile(path, original));

  std::string content;
  IntegrityResult result = ReadFileWithIntegrity(path, &content);

  EXPECT_EQ(IntegrityResult::kSuccessNoFooter, result);
  EXPECT_EQ(original, content);
}

TEST_F(InstallerFileIntegrityTest, ReadEmptyFile) {
  base::FilePath path = TestPath("empty.dat");
  ASSERT_TRUE(base::WriteFile(path, ""));

  std::string content;
  IntegrityResult result = ReadFileWithIntegrity(path, &content);

  EXPECT_EQ(IntegrityResult::kSuccessNoFooter, result);
  EXPECT_TRUE(content.empty());
}

// ============================================================================
// Corruption Detection (Doom Pattern)
// ============================================================================

TEST_F(InstallerFileIntegrityTest, CorruptedContentDetected) {
  base::FilePath path = TestPath("corrupt.dat");
  ASSERT_TRUE(WriteFileWithIntegrity(path, "original content"));
  ASSERT_TRUE(base::PathExists(path));

  // Corrupt the content by overwriting the first byte.
  std::string raw;
  ASSERT_TRUE(base::ReadFileToString(path, &raw));
  raw[0] = ~raw[0];  // Flip bits in first byte
  ASSERT_TRUE(base::WriteFile(path, raw));

  std::string content;
  IntegrityResult result = ReadFileWithIntegrity(path, &content);

  EXPECT_EQ(IntegrityResult::kIntegrityMismatch, result);
  // File should be doomed (deleted).
  EXPECT_FALSE(base::PathExists(path));
}

TEST_F(InstallerFileIntegrityTest, CorruptedCrcDetected) {
  base::FilePath path = TestPath("bad_crc.dat");
  ASSERT_TRUE(WriteFileWithIntegrity(path, "test data"));

  // Corrupt the CRC32 field (bytes at offset content_size).
  std::string raw;
  ASSERT_TRUE(base::ReadFileToString(path, &raw));
  size_t crc_offset = raw.size() - 16;  // CRC32 is first 4 bytes of footer
  raw[crc_offset] = ~raw[crc_offset];
  ASSERT_TRUE(base::WriteFile(path, raw));

  std::string content;
  IntegrityResult result = ReadFileWithIntegrity(path, &content);

  EXPECT_EQ(IntegrityResult::kIntegrityMismatch, result);
  EXPECT_FALSE(base::PathExists(path));
}

TEST_F(InstallerFileIntegrityTest, CorruptedCrcCanBePreserved) {
  base::FilePath path = TestPath("preserved_bad_crc.dat");
  ASSERT_TRUE(WriteFileWithIntegrity(path, "test data"));

  std::string raw;
  ASSERT_TRUE(base::ReadFileToString(path, &raw));
  raw[0] = ~raw[0];
  ASSERT_TRUE(base::WriteFile(path, raw));

  std::string content;
  EXPECT_EQ(IntegrityResult::kIntegrityMismatch,
            ReadFileWithIntegrity(path, &content,
                                  IntegrityMismatchAction::kPreserve));
  std::string unchanged;
  ASSERT_TRUE(base::ReadFileToString(path, &unchanged));
  EXPECT_EQ(raw, unchanged);
}

TEST_F(InstallerFileIntegrityTest,
       ConditionalDeletePreservesAtomicReplacementAfterComparison) {
  base::FilePath path = TestPath("conditional_delete.dat");
  ASSERT_TRUE(WriteFileWithIntegrity(path, "old"));
  SetConditionalDeleteHookForTesting(
      base::BindRepeating([](const base::FilePath& replacement_path) {
        ASSERT_TRUE(WriteFileWithIntegrity(replacement_path, "new"));
      }));

  EXPECT_EQ(ConditionalDeleteResult::kDeleted,
            DeleteFileWithIntegrityIfMatching(path, "old", true,
                                              ResolvedParent(path)));

  std::string content;
  EXPECT_EQ(IntegrityResult::kSuccess, ReadFileWithIntegrity(path, &content));
  EXPECT_EQ("new", content);
}

TEST_F(InstallerFileIntegrityTest, ConditionalDeletePreservesChangedPayload) {
  base::FilePath path = TestPath("conditional_changed.dat");
  ASSERT_TRUE(WriteFileWithIntegrity(path, "new"));

  EXPECT_EQ(ConditionalDeleteResult::kChanged,
            DeleteFileWithIntegrityIfMatching(path, "old", true,
                                              ResolvedParent(path)));
  EXPECT_TRUE(base::PathExists(path));
}

TEST_F(InstallerFileIntegrityTest,
       ConditionalDeleteRejectsUnexpectedLengthBeforeReading) {
  base::FilePath path = TestPath("conditional_oversized.dat");
  ASSERT_TRUE(
      base::WriteFile(path, std::string(kMaxLaunchStateFileSize + 1024, 'x')));

  EXPECT_EQ(ConditionalDeleteResult::kChanged,
            DeleteFileWithIntegrityIfMatching(path, "old", true,
                                              ResolvedParent(path)));
  EXPECT_TRUE(base::PathExists(path));
}

TEST_F(InstallerFileIntegrityTest, ConditionalDeleteDoesNotFollowReparsePoint) {
  base::FilePath target = TestPath("conditional_target.dat");
  base::FilePath link = TestPath("conditional_link.dat");
  ASSERT_TRUE(WriteFileWithIntegrity(target, "old"));
  constexpr DWORD kAllowUnprivilegedCreate = 0x2;
  if (!::CreateSymbolicLinkW(link.value().c_str(), target.value().c_str(),
                             kAllowUnprivilegedCreate)) {
    GTEST_SKIP() << "Symbolic-link creation is unavailable";
  }

  EXPECT_EQ(ConditionalDeleteResult::kChanged,
            DeleteFileWithIntegrityIfMatching(link, "old", true,
                                              ResolvedParent(link)));
  EXPECT_TRUE(base::PathExists(target));
}

TEST_F(InstallerFileIntegrityTest,
       ConditionalReparseDeleteRemovesLinkAndPreservesTarget) {
  base::FilePath target = TestPath("reparse_delete_target.dat");
  base::FilePath link = TestPath("reparse_delete_link.dat");
  ASSERT_TRUE(WriteFileWithIntegrity(target, "target"));
  constexpr DWORD kAllowUnprivilegedCreate = 0x2;
  if (!::CreateSymbolicLinkW(link.value().c_str(), target.value().c_str(),
                             kAllowUnprivilegedCreate)) {
    GTEST_SKIP() << "Symbolic-link creation is unavailable";
  }

  EXPECT_EQ(ConditionalDeleteResult::kDeleted,
            DeleteFileIfReparsePoint(link, ResolvedParent(link)));
  EXPECT_FALSE(base::PathExists(link));
  std::string content;
  EXPECT_EQ(IntegrityResult::kSuccess, ReadFileWithIntegrity(target, &content));
  EXPECT_EQ("target", content);
}

TEST_F(InstallerFileIntegrityTest,
       ConditionalReparseDeletePreservesConcurrentReplacement) {
  base::FilePath target = TestPath("reparse_race_target.dat");
  base::FilePath link = TestPath("reparse_race_link.dat");
  ASSERT_TRUE(WriteFileWithIntegrity(target, "target"));
  constexpr DWORD kAllowUnprivilegedCreate = 0x2;
  if (!::CreateSymbolicLinkW(link.value().c_str(), target.value().c_str(),
                             kAllowUnprivilegedCreate)) {
    GTEST_SKIP() << "Symbolic-link creation is unavailable";
  }
  SetConditionalDeleteHookForTesting(
      base::BindRepeating([](const base::FilePath& hook_path) {
        ASSERT_TRUE(base::DeleteFile(hook_path));
        ASSERT_TRUE(WriteFileWithIntegrity(hook_path, "replacement"));
      }));

  EXPECT_EQ(ConditionalDeleteResult::kDeleted,
            DeleteFileIfReparsePoint(link, ResolvedParent(link)));
  std::string content;
  EXPECT_EQ(IntegrityResult::kSuccess, ReadFileWithIntegrity(link, &content));
  EXPECT_EQ("replacement", content);
  EXPECT_TRUE(base::PathExists(target));
}

TEST_F(InstallerFileIntegrityTest,
       ConditionalDeleteRejectsRedirectedParentDirectory) {
  base::FilePath parent = TestPath("launch");
  base::FilePath target = TestPath("redirect_target");
  base::FilePath path = parent.Append(L"evidence");
  ASSERT_TRUE(base::CreateDirectory(parent));
  ASSERT_TRUE(WriteFileWithIntegrity(path, "old"));
  std::optional<base::FilePath> expected_parent =
      GetSafeDirectoryResolvedPath(parent);
  ASSERT_TRUE(expected_parent);

  ASSERT_TRUE(base::DeletePathRecursively(parent));
  ASSERT_TRUE(base::CreateDirectory(target));
  base::FilePath redirected_file = target.Append(L"evidence");
  ASSERT_TRUE(WriteFileWithIntegrity(redirected_file, "old"));
  constexpr DWORD kDirectorySymlinkWithUnprivilegedCreate = 0x3;
  if (!::CreateSymbolicLinkW(parent.value().c_str(), target.value().c_str(),
                             kDirectorySymlinkWithUnprivilegedCreate)) {
    GTEST_SKIP() << "Directory symbolic-link creation is unavailable";
  }

  EXPECT_EQ(
      ConditionalDeleteResult::kChanged,
      DeleteFileWithIntegrityIfMatching(path, "old", true, *expected_parent));
  EXPECT_TRUE(base::PathExists(redirected_file));
  EXPECT_TRUE(::RemoveDirectoryW(parent.value().c_str()));
}

TEST_F(InstallerFileIntegrityTest, TruncatedFileWithFooterMagic) {
  // A file that has exactly 16 bytes (just a footer, no content).
  // If the magic matches, it should verify the CRC of empty content.
  base::FilePath path = TestPath("footer_only.dat");
  ASSERT_TRUE(WriteFileWithIntegrity(path, ""));

  std::string content;
  IntegrityResult result = ReadFileWithIntegrity(path, &content);

  EXPECT_EQ(IntegrityResult::kSuccess, result);
  EXPECT_TRUE(content.empty());
}

// ============================================================================
// IntegrityResultToString Tests
// ============================================================================

TEST_F(InstallerFileIntegrityTest, ResultToString) {
  EXPECT_STREQ("Success (verified)",
               IntegrityResultToString(IntegrityResult::kSuccess));
  EXPECT_STREQ("Success (no integrity footer)",
               IntegrityResultToString(IntegrityResult::kSuccessNoFooter));
  EXPECT_STREQ("File not found",
               IntegrityResultToString(IntegrityResult::kFileNotFound));
  EXPECT_STREQ("Read error",
               IntegrityResultToString(IntegrityResult::kReadError));
  EXPECT_STREQ("Integrity mismatch",
               IntegrityResultToString(IntegrityResult::kIntegrityMismatch));
}

}  // namespace
}  // namespace cef_installer
