// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_PARALLEL_XZ_H_
#define CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_PARALLEL_XZ_H_

#include "base/files/file_path.h"
#include "cef/libcef_dll/bootstrap/installer/installer_archive.h"
#include "cef/libcef_dll/bootstrap/installer/installer_xz_index.h"

namespace cef_installer {

// =============================================================================
// Thread and memory model
// =============================================================================
//
// Parallel extraction decompresses XZ blocks on multiple threads, then
// extracts the resulting tar data on the calling thread. The thread count
// is chosen dynamically based on CPU count, available physical memory,
// block count, and ExtractionConfig settings.
//
// Memory layout during parallel decompression:
//
//   Shared (allocated once on caller):
//     tar_data buffer = total_uncompressed_size bytes
//       Pre-allocated before workers start. Each worker writes into a
//       non-overlapping slice at a pre-computed offset, so no locking
//       is needed.
//
//   Per-worker: no allocation. Compressed data is read directly from
//     the memory-mapped archive. Decompression writes directly into
//     the tar_data slice. The LZMA2 decoder allocates internal state
//     (~64 KB) which is freed after each block.
//
// Peak memory ≈ total_uncompressed_size (tar_data buffer)
//   + archive file size (memory-mapped, managed by OS pager)
//   + N * ~64 KB (LZMA2 decoder state per worker)
//
// Thread selection (see DetermineThreadCount):
//   1. If single block or background_mode, return 1. Background mode
//      always uses single-threaded streaming because the parallel path
//      allocates ~total_uncompressed_size in RAM (~500 MB for typical
//      CEF archives). The modest speedup from 2 threads (~1.5-1.7x)
//      does not justify the ~4000x memory increase vs streaming (~128 KB)
//      in a mode designed to minimize system impact.
//   2. If total_uncompressed_size > 75% of available physical memory,
//      return 1. The tar_data buffer is the only significant allocation
//      (workers decompress directly into it, and compressed data is
//      memory-mapped), so this is a simple fits-or-doesn't check.
//   3. Start with CPU count (or config.max_threads if explicitly set).
//   4. Cap to block count (no benefit from more threads than blocks).
//   5. If the result is 1, fall back to the streaming single-threaded path
//      which is more memory-efficient because it never buffers the entire
//      decompressed tar in memory.
//
// =============================================================================
// Why buffer-all is the right approach
// =============================================================================
//
// An alternative would pipeline decompression with tar extraction to reduce
// peak memory from total_uncompressed_size to ~thread_count * max_block_size.
// We chose the simpler buffer-all approach because:
//
// 1. The "sweet spot" where pipelining helps is vanishingly narrow. For a
//    typical 500 MB CEF archive, parallel needs ~667 MB free (75% threshold).
//    Pipelining lowers this to ~256 MB. But systems with 4+ cores and only
//    256-667 MB free are rare — even budget machines ship with 4-8 GB.
//
// 2. The fallback is already correct. When DetermineThreadCount returns 1
//    (or UncheckedMalloc fails), we fall back to ExtractTarXzSingleThread
//    which streams with ~128 KB. The worst case is slower, not broken.
//
// 3. Memory fragmentation is not a concern on x64. malloc allocates virtual
//    memory, and x64 provides 128 TB+ of virtual address space. A 500 MB
//    contiguous virtual allocation always succeeds if sufficient physical
//    memory + page file exists — the OS maps it to scattered physical pages
//    transparently. (This would matter on 32-bit, but CEF targets primarily
//    x64.)
//
// 4. Memory-constrained systems tend to also be CPU-constrained, reducing
//    the parallelism benefit. Disk I/O is more likely the bottleneck.
//
// If telemetry shows DetermineThreadCount frequently returning 1 on
// multi-core systems, the cheapest first step is lowering the 75% memory
// threshold, not building a pipeline.
//
// =============================================================================

// Extract tar.xz using parallel block decompression.
// Falls back to single-threaded if:
//   - Archive has only one block
//   - Index parsing fails
//   - DetermineThreadCount returns 1 (memory/CPU constraints)
//
// Uses ExtractionConfig from installer_archive.h:
//   - background_mode: fall back to single-threaded streaming
//   - max_threads: limit concurrent decompression (0 = auto)
//   - max_memory_per_thread: per-block size limit for decompression (default
//     64MB). Blocks exceeding this are rejected to prevent OOM from
//     crafted archives.
ArchiveError ExtractTarXzParallel(const base::FilePath& archive_path,
                                  const base::FilePath& dest_dir,
                                  const ExtractionConfig& config = {},
                                  ExtractionProgressCallback progress = {});

// ============================================================================
// Internal helpers exposed for testing. Not part of the public API.
// ============================================================================
namespace internal {

// Determine the number of threads to use for parallel extraction.
// Returns 1 if the caller should fall back to single-threaded extraction,
// or >= 2 for parallel.
//
// Both num_cpus and available_memory are passed explicitly (rather than
// queried internally) so the function is deterministic and testable.
int DetermineThreadCount(const ExtractionConfig& config,
                         const XzStreamInfo& stream_info,
                         int num_cpus,
                         uint64_t available_memory);

}  // namespace internal

}  // namespace cef_installer

#endif  // CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_PARALLEL_XZ_H_
