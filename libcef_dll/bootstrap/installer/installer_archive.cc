// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef_dll/bootstrap/installer/installer_archive.h"

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <cstring>

#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "cef/libcef_dll/bootstrap/installer/installer_crc.h"
#include "cef/libcef_dll/bootstrap/installer/installer_parallel_xz.h"
#include "cef/libcef_dll/bootstrap/installer/installer_xz_index.h"
#include "third_party/lzma_sdk/src/C/Xz.h"

namespace cef_installer {

namespace {

constexpr size_t kXzBufferSize = 64 * 1024;  // 64 KB
constexpr size_t kTarBufferSize = 64 * 1024;
constexpr size_t kMaxDecompressedSize = 512 * 1024 * 1024;  // 512 MB
#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
std::atomic<int> g_last_extraction_background_mode{-1};
#endif

#if BUILDFLAG(IS_WIN)
// Check if a filename component is a Windows reserved name.
// Reserved names: CON, PRN, AUX, NUL, COM1-COM9, LPT1-LPT9
// These are reserved regardless of extension (e.g., CON.txt is also reserved).
bool IsWindowsReservedName(const std::string& component) {
  if (component.empty()) {
    return false;
  }

  // Extract the base name (before any extension)
  std::string base_name = component;
  size_t dot_pos = component.find('.');
  if (dot_pos != std::string::npos) {
    base_name = component.substr(0, dot_pos);
  }

  // Convert to uppercase for comparison
  std::string upper;
  upper.reserve(base_name.size());
  for (char c : base_name) {
    upper += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  }

  // Check against reserved names
  static const char* const kReserved[] = {
      "CON",  "PRN",  "AUX",  "NUL",  "COM1", "COM2", "COM3", "COM4",
      "COM5", "COM6", "COM7", "COM8", "COM9", "LPT1", "LPT2", "LPT3",
      "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"};

  for (const char* reserved : kReserved) {
    if (upper == reserved) {
      return true;
    }
  }
  return false;
}

// Check if any component of a path contains a Windows reserved name.
bool PathContainsWindowsReservedName(const std::string& path) {
  size_t start = 0;
  while (start < path.size()) {
    size_t sep = path.find_first_of("/\\", start);
    std::string component;
    if (sep == std::string::npos) {
      component = path.substr(start);
      start = path.size();
    } else {
      component = path.substr(start, sep - start);
      start = sep + 1;
    }
    if (!component.empty() && IsWindowsReservedName(component)) {
      return true;
    }
  }
  return false;
}
#endif  // BUILDFLAG(IS_WIN)

// Extract null-terminated string_view from fixed-size buffer.
// The returned view points into |buffer|, so the caller must ensure
// the buffer outlives the view.
std::string_view ExtractString(base::span<const uint8_t> buffer,
                               size_t max_len) {
  size_t len = 0;
  while (len < max_len && len < buffer.size() && buffer[len] != 0) {
    len++;
  }
  return std::string_view(reinterpret_cast<const char*>(buffer.data()), len);
}

}  // namespace

namespace internal {

// ============================================================================
// TarReader implementation
// ============================================================================

// static
std::optional<uint64_t> TarReader::ReadOctalNumber(
    base::span<const uint8_t> field) {
  if (field.size() < 8u) {
    return std::nullopt;
  }

  base::CheckedNumeric<uint64_t> num = 0;

  // GNU tar extension: when the first byte is 0x80, the remaining bytes
  // are a big-endian integer.
  // https://www.gnu.org/software/tar/manual/html_node/Extensions.html
  if (field[0] == 0x80) {
    // Read the last 8 bytes as big-endian
    size_t start = field.size() - 8;
    for (size_t i = start; i < field.size(); ++i) {
      num <<= 8;
      num += field[i];
      if (!num.IsValid()) {
        return std::nullopt;  // Overflow
      }
    }
    return num.ValueOrDie();
  }

  // Standard octal format
  for (size_t i = 0; i < field.size(); ++i) {
    char c = static_cast<char>(field[i]);
    if (c == '\0' || c == ' ') {
      break;
    }
    if (c < '0' || c > '7') {
      return std::nullopt;  // Invalid octal digit
    }
    num *= 8;
    num += static_cast<uint64_t>(c - '0');
    if (!num.IsValid()) {
      return std::nullopt;  // Overflow
    }
  }
  return num.ValueOrDie();
}

// static
bool TarReader::ValidateChecksum(base::span<const uint8_t> header) {
  if (header.size() < kHeaderSize) {
    return false;
  }

  // Read the stored checksum
  auto checksum_field = header.subspan(kChecksumOffset, kChecksumLength);
  std::optional<uint64_t> stored_checksum = ReadOctalNumber(checksum_field);
  if (!stored_checksum) {
    return false;
  }

  // Calculate the checksum: sum of all bytes, treating checksum field as spaces
  uint64_t calculated = 0;
  for (size_t i = 0; i < kHeaderSize; ++i) {
    if (i >= kChecksumOffset && i < kChecksumOffset + kChecksumLength) {
      calculated += ' ';  // Checksum field treated as spaces
    } else {
      calculated += header[i];
    }
  }

  return calculated == *stored_checksum;
}

// static
bool TarReader::IsEndOfArchive(base::span<const uint8_t> header) {
  if (header.size() < kHeaderSize) {
    return false;
  }

  // End of archive is indicated by two 512-byte blocks of zeros.
  // We check for at least one zero block.
  for (size_t i = 0; i < kHeaderSize; ++i) {
    if (header[i] != 0) {
      return false;
    }
  }
  return true;
}

// static
bool TarReader::IsPathSafe(const std::string& path_str) {
  if (path_str.empty()) {
    return false;
  }

  // Check for null byte injection (could truncate path in C APIs)
  if (path_str.find('\0') != std::string::npos) {
    return false;
  }

  // Check for excessively long paths
  if (path_str.size() > kMaxPathLength) {
    return false;
  }

  // Check for absolute Unix path or UNC path (e.g., //server/share)
  if (path_str[0] == '/') {
    return false;
  }

  // Check for Windows backslash absolute/UNC path (e.g., \\server\share, \dir)
  if (path_str[0] == '\\') {
    return false;
  }

  // Check for absolute Windows path (e.g., C:\, D:\)
  if (path_str.size() >= 2 && std::isalpha(path_str[0]) &&
      (path_str[1] == ':')) {
    return false;
  }

  // Check for path traversal: reject ".." as any path component.
  {
    size_t start = 0;
    while (start <= path_str.size()) {
      size_t sep = path_str.find_first_of("/\\", start);
      std::string_view component(
          path_str.data() + start,
          (sep == std::string::npos ? path_str.size() : sep) - start);
      if (component == "..") {
        return false;
      }
      if (sep == std::string::npos) {
        break;
      }
      start = sep + 1;
    }
  }

#if BUILDFLAG(IS_WIN)
  // Check for Windows reserved filenames
  if (PathContainsWindowsReservedName(path_str)) {
    return false;
  }
#endif

  return true;
}

// static
std::optional<TarEntry> TarReader::ParseHeader(
    base::span<const uint8_t> header,
    const std::string& override_name) {
  if (header.size() < kHeaderSize) {
    return std::nullopt;
  }

  // Check for end of archive
  if (IsEndOfArchive(header)) {
    return std::nullopt;
  }

  // Validate checksum
  if (!ValidateChecksum(header)) {
    return std::nullopt;
  }

  TarEntry entry;

  // Get name (may use prefix for long names in ustar format)
  std::string name;
  if (!override_name.empty()) {
    name = override_name;
  } else {
    std::string_view prefix =
        ExtractString(header.subspan(kPrefixOffset), kPrefixLength);
    std::string_view base_name =
        ExtractString(header.subspan(kNameOffset), kNameLength);

    if (!prefix.empty()) {
      name.reserve(prefix.size() + 1 + base_name.size());
      name.append(prefix);
      name.push_back('/');
      name.append(base_name);
    } else {
      name = base_name;
    }
  }

  // Validate path safety
  if (!IsPathSafe(name)) {
    return std::nullopt;
  }

#if BUILDFLAG(IS_WIN)
  entry.path = base::FilePath(base::UTF8ToWide(name));
#else
  entry.path = base::FilePath(name);
#endif

  // Get size
  auto size_opt = ReadOctalNumber(header.subspan(kSizeOffset, kSizeLength));
  if (!size_opt) {
    return std::nullopt;
  }
  // Validate file size against maximum to prevent resource exhaustion
  if (*size_opt > kMaxFileSize) {
    return std::nullopt;
  }
  entry.size = *size_opt;

  // Get mtime (Unix timestamp)
  auto mtime_opt = ReadOctalNumber(header.subspan(kMtimeOffset, kMtimeLength));
  if (mtime_opt) {
    entry.mtime = base::Time::FromTimeT(static_cast<time_t>(*mtime_opt));
  }

  // Get type
  char typeflag = static_cast<char>(header[kTypeflagOffset]);
  switch (typeflag) {
    case kTypeRegular:
    case kTypeRegularAlt:
      entry.type = TarEntry::Type::kRegularFile;
      break;
    case kTypeDirectory:
      entry.type = TarEntry::Type::kDirectory;
      break;
    case kTypeSymlink:
    case kTypePaxExtended:
    case kTypePaxGlobal:
    case kTypeGnuLongName:
    case kTypeGnuLongLink:
    case kTypeHardLink:
    default:
      entry.type = TarEntry::Type::kOther;
      break;
  }

  return entry;
}

// static
std::optional<std::string> TarReader::ParsePaxPath(
    base::span<const uint8_t> content) {
  // PAX extended header format: "length key=value\n"
  // We're looking for "path" key
  std::string_view data(reinterpret_cast<const char*>(content.data()),
                        content.size());

  size_t pos = 0;
  while (pos < data.size()) {
    // Find the length field
    size_t space = data.find(' ', pos);
    if (space == std::string_view::npos || space <= pos) {
      break;
    }

    size_t record_len = 0;
    if (!base::StringToSizeT(data.substr(pos, space - pos), &record_len) ||
        record_len == 0) {
      break;
    }

    // Validate record_len doesn't exceed remaining data (prevent overflow)
    if (record_len > data.size() - pos) {
      break;
    }

    // Parse the key=value part
    size_t key_start = space + 1;
    size_t eq = data.find('=', key_start);
    if (eq == std::string_view::npos || eq >= pos + record_len) {
      pos += record_len;
      continue;
    }

    std::string_view key = data.substr(key_start, eq - key_start);
    // Value ends at newline (or record end)
    size_t value_start = eq + 1;
    // Safely compute value_end: pos + record_len - 1 for newline
    size_t record_end = pos + record_len;
    size_t value_end = (record_end > 0) ? record_end - 1 : 0;

    if (value_end > value_start) {
      std::string_view value =
          data.substr(value_start, value_end - value_start);
      if (key == "path") {
        // Validate the extracted path
        if (value.size() > kMaxPathLength) {
          return std::nullopt;
        }
        return std::string(value);
      }
    }

    pos += record_len;
  }

  return std::nullopt;
}

// ============================================================================
// XzDecompressor implementation
// ============================================================================

XzDecompressor::XzDecompressor() {
  EnsureCrcInitialized();

  ISzAlloc* alloc = static_cast<ISzAlloc*>(malloc(sizeof(ISzAlloc)));
  alloc->Alloc = [](ISzAllocPtr, size_t size) -> void* { return malloc(size); };
  alloc->Free = [](ISzAllocPtr, void* ptr) { free(ptr); };
  alloc_ = alloc;

  CXzUnpacker* state = static_cast<CXzUnpacker*>(malloc(sizeof(CXzUnpacker)));
  XzUnpacker_Construct(state, alloc);
  state_ = state;
}

XzDecompressor::~XzDecompressor() {
  if (state_) {
    XzUnpacker_Free(static_cast<CXzUnpacker*>(state_));
    free(state_);
  }
  if (alloc_) {
    free(alloc_);
  }
}

bool XzDecompressor::DecompressChunk(base::span<const uint8_t> input,
                                     std::vector<uint8_t>* output,
                                     bool input_finished) {
  if (!state_ || !output) {
    return false;
  }

  CXzUnpacker* state = static_cast<CXzUnpacker*>(state_);
  std::vector<uint8_t> temp_buffer(kTarBufferSize);

  ECoderStatus status = CODER_STATUS_NOT_FINISHED;
  size_t remaining_input = input.size();
  const uint8_t* input_ptr = input.data();

  while (status == CODER_STATUS_NOT_FINISHED && remaining_input > 0) {
    size_t decompressed_size = temp_buffer.size();
    size_t compressed_size = remaining_input;

    int result = XzUnpacker_Code(
        state, temp_buffer.data(), &decompressed_size, input_ptr,
        &compressed_size, input_finished && compressed_size == remaining_input,
        CODER_FINISH_ANY, &status);

    if (result != SZ_OK) {
      return false;
    }

    // Append decompressed data
    if (decompressed_size > 0) {
      output->insert(output->end(), temp_buffer.begin(),
                     temp_buffer.begin() + decompressed_size);
    }

    input_ptr += compressed_size;
    remaining_input -= compressed_size;
  }

  return true;
}

bool XzDecompressor::IsFinished() const {
  if (!state_) {
    return false;
  }
  return XzUnpacker_IsStreamWasFinished(static_cast<CXzUnpacker*>(state_)) != 0;
}

}  // namespace internal

// ============================================================================
// Archive extraction functions
// ============================================================================

ArchiveError ExtractTarXzSingleThread(const base::FilePath& archive_path,
                                      const base::FilePath& dest_dir,
                                      ExtractionProgressCallback progress) {
  // Open archive file
  base::File archive(archive_path,
                     base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!archive.IsValid()) {
    return ArchiveError::kFileNotFound;
  }

  // Create destination directory
  if (!base::CreateDirectory(dest_dir)) {
    return ArchiveError::kWriteError;
  }

  const uint64_t compressed_size = archive.GetLength();
  uint64_t compressed_bytes_read = 0;

  // Initialize decompressor
  internal::XzDecompressor decompressor;
  std::vector<uint8_t> xz_buffer(kXzBufferSize);
  std::vector<uint8_t> tar_buffer;

  size_t current_entry = 0;
  uint64_t bytes_extracted = 0;

  // State machine for tar parsing
  enum class State { kExpectingHeader, kReadingContent, kSkippingContent };
  State state = State::kExpectingHeader;

  std::optional<internal::TarEntry> current_tar_entry;
  base::File current_output_file;
  uint64_t content_remaining = 0;
  std::string pending_long_name;
  bool archive_finished = false;

  while (!archive_finished) {
    std::optional<size_t> bytes_read = archive.ReadAtCurrentPos(xz_buffer);
    if (!bytes_read || *bytes_read == 0) {
      break;
    }
    compressed_bytes_read += *bytes_read;

    if (!decompressor.DecompressChunk(
            base::span(xz_buffer).first(*bytes_read), &tar_buffer,
            archive.GetLength() == archive.Seek(base::File::FROM_CURRENT, 0))) {
      return ArchiveError::kInvalidFormat;
    }

    // Check for decompression bomb attack
    if (tar_buffer.size() > kMaxDecompressedSize) {
      return ArchiveError::kExtractionFailed;
    }

    // Process tar data
    size_t tar_pos = 0;
    while (tar_pos < tar_buffer.size() && !archive_finished) {
      if (state == State::kExpectingHeader) {
        if (tar_buffer.size() - tar_pos < internal::TarReader::kHeaderSize) {
          // Not enough data for header, keep remaining for next iteration
          tar_buffer.erase(tar_buffer.begin(), tar_buffer.begin() + tar_pos);
          tar_pos = 0;
          break;
        }

        auto header = base::span(tar_buffer)
                          .subspan(tar_pos, internal::TarReader::kHeaderSize);

        if (internal::TarReader::IsEndOfArchive(header)) {
          archive_finished = true;
          break;
        }

        // Check for special entry types (PAX, GNU long name)
        char typeflag =
            static_cast<char>(header[internal::TarReader::kTypeflagOffset]);

        if (typeflag == internal::TarReader::kTypeGnuLongName) {
          // GNU long filename - read content as the filename
          auto size_opt = internal::TarReader::ReadOctalNumber(
              header.subspan(internal::TarReader::kSizeOffset,
                             internal::TarReader::kSizeLength));
          if (!size_opt) {
            return ArchiveError::kInvalidHeader;
          }
          // Validate long name size to prevent memory exhaustion
          if (*size_opt > kMaxPathLength) {
            return ArchiveError::kInvalidHeader;
          }
          tar_pos += internal::TarReader::kHeaderSize;
          content_remaining = *size_opt;
          state = State::kSkippingContent;
          pending_long_name.clear();

          // Read the long name from content
          size_t available = tar_buffer.size() - tar_pos;
          size_t to_read = std::min(
              available, base::checked_cast<size_t>(content_remaining));
          pending_long_name.append(
              reinterpret_cast<const char*>(tar_buffer.data() + tar_pos),
              to_read);
          // Remove trailing null if present
          while (!pending_long_name.empty() &&
                 pending_long_name.back() == '\0') {
            pending_long_name.pop_back();
          }
          tar_pos += to_read;
          content_remaining -= to_read;

          // Skip padding
          if (content_remaining == 0) {
            size_t padding = (internal::TarReader::kHeaderSize -
                              (*size_opt % internal::TarReader::kHeaderSize)) %
                             internal::TarReader::kHeaderSize;
            tar_pos += std::min(padding, tar_buffer.size() - tar_pos);
            state = State::kExpectingHeader;
          }
          continue;
        }

        if (typeflag == internal::TarReader::kTypePaxExtended) {
          // PAX extended header - parse to get path
          auto size_opt = internal::TarReader::ReadOctalNumber(
              header.subspan(internal::TarReader::kSizeOffset,
                             internal::TarReader::kSizeLength));
          if (!size_opt) {
            return ArchiveError::kInvalidHeader;
          }
          // Validate PAX header size to prevent memory exhaustion
          // PAX headers are typically small (paths + metadata)
          if (*size_opt > kMaxPathLength * 2) {
            return ArchiveError::kInvalidHeader;
          }
          tar_pos += internal::TarReader::kHeaderSize;
          uint64_t pax_size = *size_opt;

          // Read PAX content
          size_t available = tar_buffer.size() - tar_pos;
          if (available < pax_size) {
            tar_buffer.erase(tar_buffer.begin(), tar_buffer.begin() + tar_pos);
            tar_pos = 0;
            break;
          }

          auto pax_path = internal::TarReader::ParsePaxPath(
              base::span(tar_buffer)
                  .subspan(tar_pos, static_cast<size_t>(pax_size)));
          if (pax_path) {
            pending_long_name = *pax_path;
          }

          tar_pos += pax_size;
          // Skip padding
          size_t padding = (internal::TarReader::kHeaderSize -
                            (pax_size % internal::TarReader::kHeaderSize)) %
                           internal::TarReader::kHeaderSize;
          tar_pos += std::min(padding, tar_buffer.size() - tar_pos);
          continue;
        }

        // Parse regular header
        auto entry_opt =
            internal::TarReader::ParseHeader(header, pending_long_name);
        pending_long_name.clear();

        if (!entry_opt) {
          return ArchiveError::kInvalidHeader;
        }

        current_tar_entry = *entry_opt;

        tar_pos += internal::TarReader::kHeaderSize;
        content_remaining = current_tar_entry->size;

        base::FilePath full_path = dest_dir.Append(current_tar_entry->path);

        if (current_tar_entry->type == internal::TarEntry::Type::kDirectory) {
          if (!base::CreateDirectory(full_path)) {
            return ArchiveError::kWriteError;
          }
          current_entry++;
          // Check entry count limit
          if (current_entry > kMaxEntryCount) {
            return ArchiveError::kExtractionFailed;
          }
          if (progress &&
              !progress.Run(compressed_bytes_read, compressed_size)) {
            return ArchiveError::kCancelled;
          }
          state = State::kExpectingHeader;
        } else if (current_tar_entry->type ==
                   internal::TarEntry::Type::kRegularFile) {
          // Create parent directories
          if (!base::CreateDirectory(full_path.DirName())) {
            return ArchiveError::kWriteError;
          }

          // Open output file
          current_output_file =
              base::File(full_path, base::File::FLAG_CREATE_ALWAYS |
                                        base::File::FLAG_WRITE);
          if (!current_output_file.IsValid()) {
            return ArchiveError::kWriteError;
          }

          if (content_remaining == 0) {
            // Empty file
            current_output_file.Close();
            if (!current_tar_entry->mtime.is_null()) {
              base::TouchFile(full_path, current_tar_entry->mtime,
                              current_tar_entry->mtime);
            }
            current_entry++;
            // Check entry count limit
            if (current_entry > kMaxEntryCount) {
              return ArchiveError::kExtractionFailed;
            }
            if (progress &&
                !progress.Run(compressed_bytes_read, compressed_size)) {
              return ArchiveError::kCancelled;
            }
            state = State::kExpectingHeader;
          } else {
            state = State::kReadingContent;
          }
        } else {
          // Skip unsupported types (symlinks, hard links, etc.)
          LOG(WARNING) << "Skipping unsupported archive entry: "
                       << current_tar_entry->path;
          state = State::kSkippingContent;
        }
      } else if (state == State::kReadingContent) {
        size_t available = tar_buffer.size() - tar_pos;
        size_t to_write =
            std::min(available, base::checked_cast<size_t>(content_remaining));

        if (to_write > 0) {
          if (!current_output_file.WriteAtCurrentPosAndCheck(
                  base::span(tar_buffer).subspan(tar_pos, to_write))) {
            current_output_file.Close();
            return ArchiveError::kWriteError;
          }
          tar_pos += to_write;
          content_remaining -= to_write;
          bytes_extracted += to_write;

          // Check total extraction size limit
          if (bytes_extracted > kMaxTotalExtractionSize) {
            current_output_file.Close();
            return ArchiveError::kDiskFull;
          }
        }

        if (content_remaining == 0) {
          // Finished writing file
          current_output_file.Close();

          // Set modification time
          base::FilePath full_path = dest_dir.Append(current_tar_entry->path);
          if (!current_tar_entry->mtime.is_null()) {
            base::TouchFile(full_path, current_tar_entry->mtime,
                            current_tar_entry->mtime);
          }

          current_entry++;
          // Check entry count limit
          if (current_entry > kMaxEntryCount) {
            return ArchiveError::kExtractionFailed;
          }
          if (progress &&
              !progress.Run(compressed_bytes_read, compressed_size)) {
            return ArchiveError::kCancelled;
          }

          // Skip padding to 512-byte boundary (use checked arithmetic)
          uint64_t entry_size = current_tar_entry->size;
          uint64_t padding = (internal::TarReader::kHeaderSize -
                              (entry_size % internal::TarReader::kHeaderSize)) %
                             internal::TarReader::kHeaderSize;
          size_t skip_amount = std::min(base::checked_cast<size_t>(padding),
                                        tar_buffer.size() - tar_pos);
          tar_pos += skip_amount;

          state = State::kExpectingHeader;
        }
      } else {
        // State::kSkippingContent
        size_t available = tar_buffer.size() - tar_pos;
        size_t to_skip =
            std::min(available, base::checked_cast<size_t>(content_remaining));
        tar_pos += to_skip;
        content_remaining -= to_skip;

        if (content_remaining == 0) {
          // Skip padding to 512-byte boundary
          if (current_tar_entry) {
            uint64_t padding =
                (internal::TarReader::kHeaderSize -
                 (current_tar_entry->size % internal::TarReader::kHeaderSize)) %
                internal::TarReader::kHeaderSize;
            tar_pos += std::min(base::checked_cast<size_t>(padding),
                                tar_buffer.size() - tar_pos);
          }
          state = State::kExpectingHeader;
        }
      }
    }

    // Clear processed data unless we're keeping a partial buffer
    if (tar_pos >= tar_buffer.size()) {
      tar_buffer.clear();
    }
  }

  if (current_output_file.IsValid()) {
    current_output_file.Close();
  }

  return ArchiveError::kSuccess;
}

ArchiveError ExtractTarXz(const base::FilePath& archive_path,
                          const base::FilePath& dest_dir,
                          const ExtractionConfig& config,
                          ExtractionProgressCallback progress) {
#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
  g_last_extraction_background_mode.store(config.background_mode ? 1 : 0,
                                          std::memory_order_relaxed);
#endif
  return ExtractTarXzParallel(archive_path, dest_dir, config,
                              std::move(progress));
}

namespace internal {

#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
void ResetLastExtractionBackgroundModeForTesting() {
  g_last_extraction_background_mode.store(-1, std::memory_order_relaxed);
}

std::optional<bool> GetLastExtractionBackgroundModeForTesting() {
  const int value =
      g_last_extraction_background_mode.load(std::memory_order_relaxed);
  return value < 0 ? std::nullopt : std::optional<bool>(value != 0);
}
#endif

}  // namespace internal

// ============================================================================
// Block decompression (for parallel extraction)
// ============================================================================

bool DecompressXzBlock(base::span<const uint8_t> block_data,
                       uint8_t check_type,
                       uint16_t stream_flags,
                       base::span<uint8_t> output) {
  if (block_data.empty() || output.empty()) {
    return false;
  }

  // Reject obviously oversized blocks to prevent OOM from untrusted input.
  if (static_cast<uint64_t>(output.size()) > kMaxTotalExtractionSize) {
    return false;
  }

  // In multi-threaded usage ExtractTarXzParallel initializes the CRC first on
  // the main thread to avoid data races. This inline call is just a convenience
  // for tests.
  EnsureCrcInitialized();

  // Use CXzUnpacker's random block decoding mode.
  // This properly handles block header parsing, LZMA2 decompression,
  // and check (CRC32/CRC64/SHA256) verification.
  ISzAlloc alloc;
  alloc.Alloc = [](ISzAllocPtr, size_t size) -> void* { return malloc(size); };
  alloc.Free = [](ISzAllocPtr, void* ptr) { free(ptr); };

  CXzUnpacker unpacker;
  XzUnpacker_Construct(&unpacker, &alloc);
  XzUnpacker_Init(&unpacker);

  // Set stream flags and prepare for random block decoding.
  unpacker.streamFlags = static_cast<CXzStreamFlags>(stream_flags);
  XzUnpacker_PrepareToRandomBlockDecoding(&unpacker);

  const uint8_t* src = block_data.data();
  SizeT src_len = base::checked_cast<SizeT>(block_data.size());
  SizeT src_pos = 0;

  uint8_t* dest = output.data();
  SizeT dest_len = base::checked_cast<SizeT>(output.size());
  SizeT dest_pos = 0;

  bool success = true;

  // Process block data. Use do-while because IsBlockFinished() returns
  // true in the initial state (before any data is fed).
  do {
    SizeT in_size = src_len - src_pos;
    SizeT out_size = dest_len - dest_pos;
    ECoderStatus status;

    SRes res = XzUnpacker_Code(&unpacker, dest + dest_pos, &out_size,
                               src + src_pos, &in_size,
                               1,  // srcFinished: we have all block data
                               CODER_FINISH_END, &status);

    src_pos += in_size;
    dest_pos += out_size;

    if (res != SZ_OK) {
      success = false;
      break;
    }

    if (in_size == 0 && out_size == 0) {
      // No progress - avoid infinite loop.
      success = false;
      break;
    }
  } while (!XzUnpacker_IsBlockFinished(&unpacker));

  XzUnpacker_Free(&unpacker);

  if (!success || dest_pos != output.size()) {
    return false;
  }

  return true;
}

// ============================================================================
// Tar extraction from memory buffer (for parallel extraction)
// ============================================================================

ArchiveError ExtractTarFromBuffer(base::span<const uint8_t> tar_data,
                                  const base::FilePath& dest_dir,
                                  ExtractionProgressCallback progress) {
  if (!base::CreateDirectory(dest_dir)) {
    return ArchiveError::kWriteError;
  }

  size_t pos = 0;
  size_t entry_count = 0;
  uint64_t bytes_extracted = 0;
  std::string pending_long_name;

  while (pos + internal::TarReader::kHeaderSize <= tar_data.size()) {
    auto header = tar_data.subspan(pos, internal::TarReader::kHeaderSize);

    if (internal::TarReader::IsEndOfArchive(header)) {
      break;
    }

    char typeflag =
        static_cast<char>(header[internal::TarReader::kTypeflagOffset]);

    // Handle GNU long name entries.
    if (typeflag == internal::TarReader::kTypeGnuLongName) {
      auto size_opt = internal::TarReader::ReadOctalNumber(header.subspan(
          internal::TarReader::kSizeOffset, internal::TarReader::kSizeLength));
      if (!size_opt || *size_opt > kMaxPathLength) {
        return ArchiveError::kInvalidHeader;
      }
      pos += internal::TarReader::kHeaderSize;
      if (pos + *size_opt > tar_data.size()) {
        return ArchiveError::kInvalidFormat;
      }
      pending_long_name.assign(
          reinterpret_cast<const char*>(tar_data.data() + pos), *size_opt);
      while (!pending_long_name.empty() && pending_long_name.back() == '\0') {
        pending_long_name.pop_back();
      }
      pos += ((*size_opt + 511) / 512) * 512;
      continue;
    }

    // Handle PAX extended headers.
    if (typeflag == internal::TarReader::kTypePaxExtended) {
      auto size_opt = internal::TarReader::ReadOctalNumber(header.subspan(
          internal::TarReader::kSizeOffset, internal::TarReader::kSizeLength));
      if (!size_opt || *size_opt > kMaxPathLength * 2) {
        return ArchiveError::kInvalidHeader;
      }
      pos += internal::TarReader::kHeaderSize;
      if (pos + *size_opt > tar_data.size()) {
        return ArchiveError::kInvalidFormat;
      }
      auto pax_path = internal::TarReader::ParsePaxPath(
          tar_data.subspan(pos, base::checked_cast<size_t>(*size_opt)));
      if (pax_path) {
        pending_long_name = *pax_path;
      }
      pos += ((*size_opt + 511) / 512) * 512;
      continue;
    }

    // Parse regular header.
    auto entry_opt =
        internal::TarReader::ParseHeader(header, pending_long_name);
    pending_long_name.clear();

    if (!entry_opt) {
      return ArchiveError::kInvalidHeader;
    }

    pos += internal::TarReader::kHeaderSize;

    base::FilePath full_path = dest_dir.Append(entry_opt->path);

    if (entry_opt->type == internal::TarEntry::Type::kDirectory) {
      if (!base::CreateDirectory(full_path)) {
        return ArchiveError::kWriteError;
      }
    } else if (entry_opt->type == internal::TarEntry::Type::kRegularFile) {
      if (!base::CreateDirectory(full_path.DirName())) {
        return ArchiveError::kWriteError;
      }

      if (pos + entry_opt->size > tar_data.size()) {
        return ArchiveError::kInvalidFormat;
      }

      base::File out_file(
          full_path, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
      if (!out_file.IsValid()) {
        return ArchiveError::kWriteError;
      }

      if (entry_opt->size > 0) {
        if (entry_opt->size > kMaxFileSize) {
          return ArchiveError::kInvalidHeader;
        }
        if (!out_file.WriteAtCurrentPosAndCheck(base::span(tar_data).subspan(
                pos, base::checked_cast<size_t>(entry_opt->size)))) {
          return ArchiveError::kWriteError;
        }
      }

      out_file.Close();

      if (!entry_opt->mtime.is_null()) {
        base::TouchFile(full_path, entry_opt->mtime, entry_opt->mtime);
      }

      bytes_extracted += entry_opt->size;

      if (bytes_extracted > kMaxTotalExtractionSize) {
        return ArchiveError::kDiskFull;
      }
    } else {
      LOG(WARNING) << "Skipping unsupported archive entry: " << entry_opt->path;
    }

    // Advance to next header (data is padded to 512-byte boundary).
    pos += base::checked_cast<size_t>(((entry_opt->size + 511) / 512) * 512);
    entry_count++;

    if (entry_count > kMaxEntryCount) {
      return ArchiveError::kExtractionFailed;
    }

    if (progress && !progress.Run(pos, tar_data.size())) {
      return ArchiveError::kCancelled;
    }
  }

  // Final progress report to signal completion.
  if (progress && !progress.Run(tar_data.size(), tar_data.size())) {
    return ArchiveError::kCancelled;
  }

  return ArchiveError::kSuccess;
}

const char* ArchiveErrorToString(ArchiveError error) {
  switch (error) {
    case ArchiveError::kSuccess:
      return "Success";
    case ArchiveError::kFileNotFound:
      return "Archive file not found";
    case ArchiveError::kFileReadError:
      return "Failed to read archive file";
    case ArchiveError::kInvalidFormat:
      return "Invalid archive format (xz decompression failed)";
    case ArchiveError::kInvalidHeader:
      return "Invalid tar header (checksum mismatch)";
    case ArchiveError::kExtractionFailed:
      return "Extraction failed";
    case ArchiveError::kDiskFull:
      return "Disk full";
    case ArchiveError::kPathTraversal:
      return "Archive contains path traversal attack";
    case ArchiveError::kAbsolutePath:
      return "Archive contains absolute path";
    case ArchiveError::kWriteError:
      return "Failed to write extracted file";
    case ArchiveError::kUnsupportedEntryType:
      return "Unsupported archive entry type";
    case ArchiveError::kCancelled:
      return "Extraction cancelled";
  }
  return "Unknown error";
}

}  // namespace cef_installer
