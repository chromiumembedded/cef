// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_ARCHIVE_H_
#define CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_ARCHIVE_H_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/time/time.h"

namespace cef_installer {

// Security limits to prevent resource exhaustion attacks.
constexpr uint64_t kMaxFileSize = 4ULL * 1024 * 1024 * 1024;  // 4 GB
constexpr uint64_t kMaxTotalExtractionSize =
    10ULL * 1024 * 1024 * 1024;  // 10 GB
constexpr size_t kMaxPathLength = 4096;
constexpr size_t kMaxEntryCount = 100000;

// Error codes for archive operations.
enum class ArchiveError {
  kSuccess,
  kFileNotFound,          // Archive file doesn't exist
  kFileReadError,         // Could not read archive
  kInvalidFormat,         // Not a valid tar.xz file (xz decompression failed)
  kInvalidHeader,         // Tar header checksum mismatch or invalid
  kExtractionFailed,      // General extraction failure
  kDiskFull,              // Not enough space to extract
  kPathTraversal,         // Archive contains path traversal attack (../)
  kAbsolutePath,          // Archive contains absolute path
  kWriteError,            // Could not write extracted file
  kUnsupportedEntryType,  // Entry type not supported (e.g., device nodes)
  kCancelled,             // Extraction cancelled via callback
};

// Callback for extraction progress reporting.
// Parameters: bytes_processed, total_bytes
// Return false to cancel extraction.
using ExtractionProgressCallback =
    base::RepeatingCallback<bool(uint64_t, uint64_t)>;

// Configuration for extraction (single struct for both paths).
struct ExtractionConfig {
  // Background mode: minimizes system impact for non-interactive extraction
  // (e.g. scheduled updates). Always uses the streaming single-threaded path
  // (~128 KB memory) rather than the parallel path (~500 MB allocation).
  bool background_mode = false;

  // Max threads (0 = auto-detect from CPU count).
  // Only used when parallel extraction is possible (multi-block archive).
  int max_threads = 0;

  // Max memory per thread for decompression buffers (default 64MB).
  // Covers most LZMA2 dictionary sizes.
  size_t max_memory_per_thread = 64 * 1024 * 1024;
};

// Primary API - auto-selects parallel vs single-threaded based on archive.
// For multi-block XZ archives, uses parallel block decompression.
// For single-block archives, falls back to streaming single-threaded path.
//
// - Creates dest_dir if it doesn't exist
// - Extracts all files and directories preserving structure
// - Validates no path traversal (rejects entries with ".." components)
// - Validates no absolute paths (rejects entries starting with "/" or "C:\")
// - Preserves modification times
// - Archive format: xz/LZMA2-compressed tar (POSIX ustar format)
//
// Uses //third_party/lzma_sdk/src/C/Xz.h for xz decompression.
ArchiveError ExtractTarXz(const base::FilePath& archive_path,
                          const base::FilePath& dest_dir,
                          const ExtractionConfig& config = {},
                          ExtractionProgressCallback progress = {});

// Explicit single-threaded streaming extraction (for fallback).
// Also used by installer_parallel_xz.cc as fallback.
ArchiveError ExtractTarXzSingleThread(const base::FilePath& archive_path,
                                      const base::FilePath& dest_dir,
                                      ExtractionProgressCallback progress = {});

// Convert error code to human-readable string for logging.
const char* ArchiveErrorToString(ArchiveError error);

// Decompress a single XZ block (not a full XZ stream).
// |block_data| must start at the block header.
// |check_type| is the stream check type (0=none, 1=CRC32, 4=CRC64, 10=SHA256).
// |stream_flags| is the raw CXzStreamFlags value from the stream header/footer.
// |output| must be pre-sized to the expected uncompressed size (from XZ index).
// Returns true on success.
bool DecompressXzBlock(base::span<const uint8_t> block_data,
                       uint8_t check_type,
                       uint16_t stream_flags,
                       base::span<uint8_t> output);

// Extract tar archive from an in-memory buffer.
// Processes tar headers and extracts files to |dest_dir|.
ArchiveError ExtractTarFromBuffer(base::span<const uint8_t> tar_data,
                                  const base::FilePath& dest_dir,
                                  ExtractionProgressCallback progress = {});

// ============================================================================
// Internal helpers exposed for testing. Not part of the public API.
// ============================================================================
namespace internal {

// Test observation for the most recent top-level extraction request.
#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
void ResetLastExtractionBackgroundModeForTesting();
std::optional<bool> GetLastExtractionBackgroundModeForTesting();
#endif

// Information about a single entry in a tar archive.
struct TarEntry {
  enum class Type {
    kRegularFile,
    kDirectory,
    kOther,  // Symlinks, hard links, devices, etc. - skipped
  };

  base::FilePath path;  // Relative path within archive
  uint64_t size = 0;    // File size in bytes
  base::Time mtime;     // Last modification time
  Type type = Type::kOther;
};

// TarReader parses tar headers and validates entries.
// Reference: chrome/services/file_util/single_file_tar_reader.cc
class TarReader {
 public:
  // Tar header constants (POSIX ustar format)
  // https://www.gnu.org/software/tar/manual/html_node/Standard.html
  static constexpr size_t kHeaderSize = 512;
  static constexpr size_t kNameOffset = 0;
  static constexpr size_t kNameLength = 100;
  static constexpr size_t kModeOffset = 100;
  static constexpr size_t kSizeOffset = 124;
  static constexpr size_t kSizeLength = 12;
  static constexpr size_t kMtimeOffset = 136;
  static constexpr size_t kMtimeLength = 12;
  static constexpr size_t kChecksumOffset = 148;
  static constexpr size_t kChecksumLength = 8;
  static constexpr size_t kTypeflagOffset = 156;
  static constexpr size_t kMagicOffset = 257;
  static constexpr size_t kMagicLength = 6;
  static constexpr size_t kPrefixOffset = 345;
  static constexpr size_t kPrefixLength = 155;

  // Type flag values
  static constexpr char kTypeRegular = '0';
  static constexpr char kTypeRegularAlt = '\0';  // Old-style tar
  static constexpr char kTypeHardLink = '1';
  static constexpr char kTypeSymlink = '2';
  static constexpr char kTypeDirectory = '5';
  static constexpr char kTypePaxExtended = 'x';  // PAX extended header
  static constexpr char kTypePaxGlobal = 'g';    // PAX global header
  static constexpr char kTypeGnuLongName = 'L';  // GNU long filename
  static constexpr char kTypeGnuLongLink = 'K';  // GNU long linkname

  // Parse a tar header, returns nullopt if invalid or end-of-archive (zero
  // block).
  static std::optional<TarEntry> ParseHeader(
      base::span<const uint8_t> header,
      const std::string& override_name = std::string());

  // Check if header is an end-of-archive marker (512 zero bytes).
  static bool IsEndOfArchive(base::span<const uint8_t> header);

  // Validate header checksum.
  static bool ValidateChecksum(base::span<const uint8_t> header);

  // Read octal number from header field (handles GNU extension for large
  // files). Reference: SingleFileTarReader::ReadOctalNumber
  static std::optional<uint64_t> ReadOctalNumber(
      base::span<const uint8_t> field);

  // Validate path is safe (no traversal, not absolute, no reserved names).
  static bool IsPathSafe(const std::string& path_str);

  // Parse PAX extended header content to extract path override.
  // PAX format: "length key=value\n"
  static std::optional<std::string> ParsePaxPath(
      base::span<const uint8_t> content);
};

// XzDecompressor decompresses xz/LZMA2 streams.
// Reference: chrome/services/file_util/single_file_tar_xz_file_extractor.cc
class XzDecompressor {
 public:
  XzDecompressor();
  ~XzDecompressor();

  XzDecompressor(const XzDecompressor&) = delete;
  XzDecompressor& operator=(const XzDecompressor&) = delete;

  // Decompress a chunk of xz data.
  // Returns true on success, false on error.
  // Appends decompressed data to |output|.
  // Call repeatedly until IsFinished() returns true.
  bool DecompressChunk(base::span<const uint8_t> input,
                       std::vector<uint8_t>* output,
                       bool input_finished = false);

  // Returns true when the xz stream is completely decompressed.
  bool IsFinished() const;

 private:
  // Opaque pointer to CXzUnpacker (avoid including lzma_sdk headers here)
  void* state_ = nullptr;
  void* alloc_ = nullptr;
};

}  // namespace internal

}  // namespace cef_installer

#endif  // CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_ARCHIVE_H_
