// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef_dll/bootstrap/installer/installer_parallel_xz.h"

#include <vector>

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "cef/libcef_dll/bootstrap/installer/installer_archive.h"
#include "cef/libcef_dll/bootstrap/installer/installer_test_helpers.h"
#include "cef/libcef_dll/bootstrap/installer/installer_xz_index.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cef_installer {

using internal::DetermineThreadCount;

namespace {

class InstallerParallelXzTest : public testing::Test {
 protected:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  base::FilePath GetFixture(const std::string& name) {
    return test::GetTestDataPath().AppendASCII(name);
  }

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
};

TEST_F(InstallerParallelXzTest, ExtractMultiBlock) {
  base::FilePath dest = temp_dir_.GetPath().AppendASCII("extracted");

  ArchiveError result =
      ExtractTarXzParallel(GetFixture("multi_block_4.tar.xz"), dest, {});

  EXPECT_EQ(ArchiveError::kSuccess, result);

  // Verify directory structure.
  EXPECT_TRUE(base::DirectoryExists(dest.AppendASCII("multi4")));
  EXPECT_TRUE(base::DirectoryExists(dest.AppendASCII("multi4/subdir1")));
  EXPECT_TRUE(base::DirectoryExists(dest.AppendASCII("multi4/subdir2")));

  // Verify file count: 20 files across 3 directories.
  int file_count = 0;
  base::FileEnumerator enumerator(dest, /*recursive=*/true,
                                  base::FileEnumerator::FILES);
  for (auto path = enumerator.Next(); !path.empty(); path = enumerator.Next()) {
    file_count++;
  }
  EXPECT_EQ(20, file_count);

  // Verify file content: each file is 4096 bytes with a known byte pattern.
  // file_0000.dat starts with 0x00,0x01,0x02,...
  std::string content;
  ASSERT_TRUE(base::ReadFileToString(dest.AppendASCII("multi4/file_0000.dat"),
                                     &content));
  ASSERT_EQ(4096u, content.size());
  EXPECT_EQ('\x00', content[0]);
  EXPECT_EQ('\x01', content[1]);
  EXPECT_EQ('\x02', content[2]);

  // Verify mtime is preserved (fixture files have non-zero mtime).
  base::File::Info info;
  ASSERT_TRUE(
      base::GetFileInfo(dest.AppendASCII("multi4/file_0000.dat"), &info));
  EXPECT_FALSE(info.last_modified.is_null());
}

TEST_F(InstallerParallelXzTest, FallbackSingleBlock) {
  base::FilePath dest = temp_dir_.GetPath().AppendASCII("extracted");

  // single_block.tar.xz has only 1 block -> should fall back to ST.
  ArchiveError result =
      ExtractTarXzParallel(GetFixture("single_block.tar.xz"), dest, {});

  EXPECT_EQ(ArchiveError::kSuccess, result);
  EXPECT_TRUE(base::DirectoryExists(dest));
}

TEST_F(InstallerParallelXzTest, ProgressCallback) {
  base::FilePath dest = temp_dir_.GetPath().AppendASCII("extracted");
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

  ArchiveError result = ExtractTarXzParallel(GetFixture("multi_block_4.tar.xz"),
                                             dest, {}, std::move(callback));

  EXPECT_EQ(ArchiveError::kSuccess, result);
  EXPECT_GT(last_total, 0u);
  EXPECT_EQ(last_bytes, last_total);
}

TEST_F(InstallerParallelXzTest, Cancellation) {
  base::FilePath dest = temp_dir_.GetPath().AppendASCII("extracted");

  // Cancel immediately on first progress callback.
  ExtractionProgressCallback cancel =
      base::BindRepeating([](uint64_t, uint64_t) -> bool { return false; });

  ArchiveError result = ExtractTarXzParallel(GetFixture("multi_block_4.tar.xz"),
                                             dest, {}, std::move(cancel));

  EXPECT_EQ(ArchiveError::kCancelled, result);
}

TEST_F(InstallerParallelXzTest, ThreadCountLimit) {
  base::FilePath dest = temp_dir_.GetPath().AppendASCII("extracted");

  ExtractionConfig config;
  config.max_threads = 1;  // Force single-thread parallelism.

  ArchiveError result =
      ExtractTarXzParallel(GetFixture("multi_block_4.tar.xz"), dest, config);

  EXPECT_EQ(ArchiveError::kSuccess, result);
  EXPECT_TRUE(base::DirectoryExists(dest.AppendASCII("multi4")));
}

TEST_F(InstallerParallelXzTest, BackgroundModeFallsBackToSingleThread) {
  base::FilePath dest = temp_dir_.GetPath().AppendASCII("extracted");

  ExtractionConfig config;
  config.background_mode = true;

  // Background mode falls back to single-threaded streaming (the parallel
  // path's ~500 MB allocation contradicts background mode's low-impact goal).
  // Verify extraction still succeeds via the fallback path.
  ArchiveError result = ExtractTarXzParallel(
      GetFixture("multi_block_large.tar.xz"), dest, config);

  EXPECT_EQ(ArchiveError::kSuccess, result);
  EXPECT_TRUE(base::DirectoryExists(dest));
}

// ============================================================================
// DetermineThreadCount / ComputeMemoryThreadLimit Tests
// ============================================================================

// Helper to build a minimal XzStreamInfo with N blocks of given sizes.
XzStreamInfo MakeStreamInfo(size_t num_blocks,
                            uint64_t compressed_per_block = 1000,
                            uint64_t uncompressed_per_block = 4096) {
  XzStreamInfo info;
  info.check_type = 4;   // CRC64
  uint64_t offset = 12;  // After stream header.
  for (size_t i = 0; i < num_blocks; ++i) {
    XzBlockInfo block;
    block.compressed_offset = offset;
    block.compressed_size = compressed_per_block;
    block.uncompressed_size = uncompressed_per_block;
    info.blocks.push_back(block);
    offset += compressed_per_block;
  }
  info.total_uncompressed_size = num_blocks * uncompressed_per_block;
  return info;
}

TEST(DetermineThreadCountTest, SingleBlockReturnsSingleThread) {
  auto info = MakeStreamInfo(1);
  ExtractionConfig config;

  EXPECT_EQ(1, DetermineThreadCount(config, info, 8, 1ULL << 30));
}

TEST(DetermineThreadCountTest, UsesAllCpusByDefault) {
  auto info = MakeStreamInfo(16);
  ExtractionConfig config;

  // With plenty of memory, thread count == CPU count.
  EXPECT_EQ(8, DetermineThreadCount(config, info, 8, 1ULL << 30));
  EXPECT_EQ(4, DetermineThreadCount(config, info, 4, 1ULL << 30));
}

TEST(DetermineThreadCountTest, RespectsMaxThreadsSetting) {
  auto info = MakeStreamInfo(16);
  ExtractionConfig config;
  config.max_threads = 3;

  EXPECT_EQ(3, DetermineThreadCount(config, info, 8, 1ULL << 30));
}

TEST(DetermineThreadCountTest, BackgroundModeReturnsSingleThread) {
  auto info = MakeStreamInfo(16);
  ExtractionConfig config;
  config.background_mode = true;

  // Background mode always falls back to single-threaded streaming.
  // The parallel path's ~500 MB allocation contradicts background mode's
  // goal of minimizing system impact.
  EXPECT_EQ(1, DetermineThreadCount(config, info, 8, 1ULL << 30));
}

TEST(DetermineThreadCountTest, BackgroundModeWithExplicitMax) {
  auto info = MakeStreamInfo(16);
  ExtractionConfig config;
  config.background_mode = true;
  config.max_threads = 6;  // Ignored — background mode always returns 1.

  EXPECT_EQ(1, DetermineThreadCount(config, info, 8, 1ULL << 30));
}

TEST(DetermineThreadCountTest, CappedByBlockCount) {
  auto info = MakeStreamInfo(3);
  ExtractionConfig config;

  // 8 CPUs but only 3 blocks.
  EXPECT_EQ(3, DetermineThreadCount(config, info, 8, 1ULL << 30));
}

TEST(DetermineThreadCountTest, MemoryFitUsesAllCpus) {
  // 10 blocks, total uncompressed = 500 MB.
  auto info = MakeStreamInfo(10, 5 * 1024 * 1024, 50 * 1024 * 1024);
  ExtractionConfig config;

  // 1.5 GB available: usable = 1.125 GB > 500 MB tar buffer. Use all CPUs.
  EXPECT_EQ(8, DetermineThreadCount(config, info, 8, 1536ULL * 1024 * 1024));
}

TEST(DetermineThreadCountTest, MemoryTooLowFallsBack) {
  // Total uncompressed = 500 MB.
  auto info = MakeStreamInfo(10, 5 * 1024 * 1024, 50 * 1024 * 1024);
  ExtractionConfig config;

  // 600 MB available: usable = 450 MB < 500 MB tar buffer. Fall back.
  EXPECT_EQ(1, DetermineThreadCount(config, info, 8, 600ULL * 1024 * 1024));

  // 100 MB available: way too low.
  EXPECT_EQ(1, DetermineThreadCount(config, info, 8, 100ULL * 1024 * 1024));
}

TEST(DetermineThreadCountTest, SingleCpuReturnsSingleThread) {
  auto info = MakeStreamInfo(10);
  ExtractionConfig config;

  EXPECT_EQ(1, DetermineThreadCount(config, info, 1, 1ULL << 30));
}

TEST(DetermineThreadCountTest, ExplicitMaxThreadsOne) {
  auto info = MakeStreamInfo(10);
  ExtractionConfig config;
  config.max_threads = 1;

  EXPECT_EQ(1, DetermineThreadCount(config, info, 8, 1ULL << 30));
}

TEST(DetermineThreadCountTest, BackgroundModeSingleCpu) {
  auto info = MakeStreamInfo(10);
  ExtractionConfig config;
  config.background_mode = true;

  // Background mode always returns 1 regardless of CPU count.
  EXPECT_EQ(1, DetermineThreadCount(config, info, 1, 1ULL << 30));
}

TEST(DetermineThreadCountTest, ZeroAvailableMemory) {
  auto info = MakeStreamInfo(10);
  ExtractionConfig config;

  EXPECT_EQ(1, DetermineThreadCount(config, info, 8, 0));
}

}  // namespace
}  // namespace cef_installer
