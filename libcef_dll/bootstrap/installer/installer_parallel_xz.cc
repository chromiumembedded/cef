// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef_dll/bootstrap/installer/installer_parallel_xz.h"

#include <windows.h>

#include <algorithm>
#include <atomic>
#include <vector>

#include "base/files/file.h"
#include "base/files/memory_mapped_file.h"
#include "base/logging.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/process/memory.h"
#include "base/system/sys_info.h"
#include "base/task/post_job.h"
#include "base/task/task_traits.h"
#include "cef/libcef_dll/bootstrap/installer/installer_crc.h"
#include "cef/libcef_dll/bootstrap/installer/installer_scoped_thread_pool.h"
#include "cef/libcef_dll/bootstrap/installer/installer_xz_index.h"

namespace cef_installer {

namespace {

// Shared state for parallel block decompression workers.
// The input archive is memory-mapped (read-only, shared across workers).
// Workers decompress directly into non-overlapping regions of a single
// pre-allocated output buffer.
struct DecompressState {
  // Memory-mapped archive data (read-only, shared across all workers).
  base::span<const uint8_t> archive_data;
  const XzStreamInfo* stream_info = nullptr;
  size_t max_memory_per_thread = 0;
  int max_threads = 1;

  // Pre-allocated output buffer. Each block writes to its own slice.
  uint8_t* tar_data = nullptr;
  // Byte offset where each block's output starts within tar_data.
  const std::vector<size_t>* block_offsets = nullptr;

  std::atomic<size_t> next_block{0};
  std::atomic<size_t> completed_blocks{0};
  std::atomic<bool> has_error{false};

  size_t total_blocks() const { return stream_info->blocks.size(); }

  static size_t MaxConcurrency(DecompressState* state,
                               size_t /*worker_count*/) {
    if (state->has_error.load(std::memory_order_relaxed)) {
      return 0;
    }
    size_t claimed = state->next_block.load(std::memory_order_relaxed);
    if (claimed >= state->total_blocks()) {
      return 0;
    }
    size_t remaining = state->total_blocks() - claimed;
    return std::min(remaining, static_cast<size_t>(state->max_threads));
  }

  static void Worker(DecompressState* state, base::JobDelegate* delegate) {
    while (!delegate->ShouldYield() &&
           !state->has_error.load(std::memory_order_relaxed)) {
      size_t idx = state->next_block.fetch_add(1, std::memory_order_relaxed);
      if (idx >= state->total_blocks()) {
        return;
      }

      const XzBlockInfo& block = state->stream_info->blocks[idx];

      // Validate per-block sizes against INT_MAX.
      if (block.compressed_size > static_cast<uint64_t>(INT_MAX) ||
          block.uncompressed_size > static_cast<uint64_t>(INT_MAX)) {
        state->has_error.store(true, std::memory_order_relaxed);
        return;
      }

      // Check uncompressed size against memory limit.
      if (block.uncompressed_size > state->max_memory_per_thread) {
        state->has_error.store(true, std::memory_order_relaxed);
        return;
      }

      // Validate block range is within the mapped file.
      size_t block_end = base::checked_cast<size_t>(block.compressed_offset +
                                                    block.compressed_size);
      if (block_end > state->archive_data.size()) {
        state->has_error.store(true, std::memory_order_relaxed);
        return;
      }

      // Get compressed data directly from the memory-mapped archive.
      base::span<const uint8_t> compressed = state->archive_data.subspan(
          base::checked_cast<size_t>(block.compressed_offset),
          base::checked_cast<size_t>(block.compressed_size));

      // Decompress directly into the pre-allocated tar_data at this
      // block's offset. Each block writes to a non-overlapping region,
      // so no locking is needed.
      size_t out_offset = (*state->block_offsets)[idx];
      base::span<uint8_t> out_span(
          state->tar_data + out_offset,
          base::checked_cast<size_t>(block.uncompressed_size));
      if (!DecompressXzBlock(
              compressed, state->stream_info->check_type,
              static_cast<uint16_t>(state->stream_info->check_type),
              out_span)) {
        state->has_error.store(true, std::memory_order_relaxed);
        return;
      }

      state->completed_blocks.fetch_add(1, std::memory_order_relaxed);
    }
  }
};

}  // namespace

namespace internal {

int DetermineThreadCount(const ExtractionConfig& config,
                         const XzStreamInfo& stream_info,
                         int num_cpus,
                         uint64_t available_memory) {
  if (stream_info.blocks.size() <= 1) {
    return 1;
  }

  // With memory-mapped input and direct-to-buffer decompression, the only
  // significant allocation is the tar_data output buffer. If it won't fit
  // in 75% of available memory, fall back to single-threaded streaming.
  const uint64_t usable = available_memory * 3 / 4;
  if (stream_info.total_uncompressed_size > usable) {
    return 1;
  }

  // Background mode falls back to single-threaded streaming. The parallel
  // path allocates ~total_uncompressed_size in RAM (~500 MB for typical CEF
  // archives), which contradicts background mode's goal of minimizing system
  // impact. The modest speedup from 2 threads (~1.5-1.7x) does not justify
  // the ~4000x increase in memory usage vs streaming (~128 KB).
  if (config.background_mode) {
    return 1;
  }

  int max_threads = config.max_threads;
  if (max_threads <= 0) {
    max_threads = num_cpus;
  }

  max_threads =
      std::min(max_threads, static_cast<int>(stream_info.blocks.size()));

  return std::max(max_threads, 1);
}

}  // namespace internal

ArchiveError ExtractTarXzParallel(const base::FilePath& archive_path,
                                  const base::FilePath& dest_dir,
                                  const ExtractionConfig& config,
                                  ExtractionProgressCallback progress) {
  // 1. Parse index to get block info.
  base::File file(archive_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid()) {
    return ArchiveError::kFileNotFound;
  }

  auto stream_info = ParseXzIndex(file);
  file.Close();

  if (!stream_info || stream_info->blocks.size() <= 1) {
    // Fall back to single-threaded for single-block or unparseable archives.
    return ExtractTarXzSingleThread(archive_path, dest_dir,
                                    std::move(progress));
  }

  // Validate total uncompressed size before allocation.
  if (stream_info->total_uncompressed_size > kMaxTotalExtractionSize) {
    return ArchiveError::kExtractionFailed;
  }

  // Initialize CRC tables on main thread before worker dispatch.
  EnsureCrcInitialized();

  // 2. Determine thread count and priority.
  // Query available memory directly via Win32 instead of
  // base::SysInfo::AmountOfAvailablePhysicalMemory(), which internally calls
  // CommandLine::ForCurrentProcess(). The bootstrap does not initialize the
  // CommandLine singleton.
  MEMORYSTATUSEX mem_status = {sizeof(mem_status)};
  const uint64_t available_memory =
      ::GlobalMemoryStatusEx(&mem_status) ? mem_status.ullAvailPhys : 0;
  int max_threads = internal::DetermineThreadCount(
      config, *stream_info, base::SysInfo::NumberOfProcessors(),
      available_memory);

  // With only 1 thread, fall back to the streaming single-threaded path
  // which is more memory-efficient (doesn't buffer the entire tar in RAM).
  if (max_threads <= 1) {
    return ExtractTarXzSingleThread(archive_path, dest_dir,
                                    std::move(progress));
  }

  // 3. Memory-map the archive file (read-only). Workers read compressed
  // block data directly from the mapping — no per-worker file I/O.
  base::MemoryMappedFile mapped_archive;
  if (!mapped_archive.Initialize(archive_path)) {
    return ExtractTarXzSingleThread(archive_path, dest_dir,
                                    std::move(progress));
  }

  // Background mode always falls back to single-threaded in
  // DetermineThreadCount, so we only reach here in foreground mode.
  base::TaskPriority priority = base::TaskPriority::USER_BLOCKING;

  // 4. Pre-allocate a single output buffer and compute per-block offsets.
  // Use UncheckedMalloc so that allocation failure returns gracefully
  // instead of crashing (exceptions are disabled in Chromium). On low-memory
  // systems, fall back to the streaming single-threaded path.
  const size_t total_size =
      base::checked_cast<size_t>(stream_info->total_uncompressed_size);
  void* tar_raw = nullptr;
  if (!base::UncheckedMalloc(total_size, &tar_raw) || !tar_raw) {
    return ExtractTarXzSingleThread(archive_path, dest_dir,
                                    std::move(progress));
  }
  std::unique_ptr<uint8_t, base::UncheckedFreeDeleter> tar_data(
      static_cast<uint8_t*>(tar_raw));
  auto tar_span = base::span(tar_data.get(), total_size);

  std::vector<size_t> block_offsets(stream_info->blocks.size());
  size_t offset = 0;
  for (size_t i = 0; i < stream_info->blocks.size(); ++i) {
    block_offsets[i] = offset;
    offset +=
        base::checked_cast<size_t>(stream_info->blocks[i].uncompressed_size);
  }

  // 5. Decompress blocks in parallel using PostJob.
  DecompressState state;
  state.archive_data = mapped_archive.bytes();
  state.stream_info = &*stream_info;
  state.max_memory_per_thread = config.max_memory_per_thread;
  state.max_threads = max_threads;
  state.tar_data = tar_data.get();
  state.block_offsets = &block_offsets;

  // Ensure a ThreadPool exists for PostJob. Created here (rather than at
  // a higher level) so callers don't need to manage pool lifecycle.
  ScopedThreadPool thread_pool;

  // MUST_USE_FOREGROUND is required so that Join() can safely upgrade
  // the priority of BEST_EFFORT tasks without hitting a DCHECK.
  base::JobHandle handle = base::PostJob(
      FROM_HERE,
      {priority, base::MayBlock(), base::ThreadPolicy::MUST_USE_FOREGROUND},
      base::BindRepeating(&DecompressState::Worker, base::Unretained(&state)),
      base::BindRepeating(&DecompressState::MaxConcurrency,
                          base::Unretained(&state)));

  handle.Join();

  if (state.has_error.load(std::memory_order_relaxed)) {
    return ArchiveError::kInvalidFormat;
  }

  // 6. Extract tar from the single buffer.
  return ExtractTarFromBuffer(tar_span, dest_dir, std::move(progress));
}

}  // namespace cef_installer
