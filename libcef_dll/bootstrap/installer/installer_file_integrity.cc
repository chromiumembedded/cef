// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef_dll/bootstrap/installer/installer_file_integrity.h"

#include <string.h>

#include <limits>
#include <type_traits>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/no_destructor.h"
#include "cef/libcef_dll/bootstrap/installer/installer_constants.h"
#include "cef/libcef_dll/bootstrap/installer/installer_crc.h"
#include "cef/libcef_dll/bootstrap/installer/installer_file_ops.h"
#include "third_party/lzma_sdk/src/C/7zCrc.h"

namespace cef_installer {

namespace {

constexpr uint64_t kIntegrityFooterMagic = UINT64_C(0xCEF09E1F4A3B2C1D);

// Footer appended to files written with WriteFileWithIntegrity.
// Matches the on-disk layout exactly (no implicit padding).
struct IntegrityFooter {
  uint32_t data_crc32;  // CRC32 of content preceding the footer
  uint32_t reserved;    // Must be zero (future use)
  uint64_t magic;       // kIntegrityFooterMagic
};
static_assert(sizeof(IntegrityFooter) == 16);
static_assert(std::has_unique_object_representations_v<IntegrityFooter>);

constexpr size_t kFooterSize = sizeof(IntegrityFooter);

bool IsSameFileObject(base::File* left, base::File* right) {
  BY_HANDLE_FILE_INFORMATION left_info = {};
  BY_HANDLE_FILE_INFORMATION right_info = {};
  if (!::GetFileInformationByHandle(left->GetPlatformFile(), &left_info) ||
      !::GetFileInformationByHandle(right->GetPlatformFile(), &right_info)) {
    return true;
  }
  return left_info.dwVolumeSerialNumber == right_info.dwVolumeSerialNumber &&
         left_info.nFileIndexHigh == right_info.nFileIndexHigh &&
         left_info.nFileIndexLow == right_info.nFileIndexLow;
}

base::File OpenForConditionalDeleteNoFollow(const base::FilePath& path) {
  return base::File(::CreateFileW(
      path.value().c_str(), GENERIC_READ | DELETE,
      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
      OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
}

base::File OpenForReadNoFollow(const base::FilePath& path) {
  return base::File(::CreateFileW(
      path.value().c_str(), GENERIC_READ,
      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
      OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
}

bool IsOpenedReparsePoint(base::File* file) {
  FILE_ATTRIBUTE_TAG_INFO info = {};
  return !::GetFileInformationByHandleEx(file->GetPlatformFile(),
                                         FileAttributeTagInfo, &info,
                                         sizeof(info)) ||
         (info.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
}

std::optional<base::FilePath> GetOpenedResolvedPath(base::File* file) {
  constexpr DWORD kMaxFinalPathLength = 32768;
  DWORD capacity = MAX_PATH;
  std::wstring buffer(capacity, L'\0');
  DWORD length = ::GetFinalPathNameByHandleW(
      file->GetPlatformFile(), buffer.data(), capacity,
      FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
  if (length == 0 || length >= kMaxFinalPathLength) {
    return std::nullopt;
  }
  if (length >= capacity) {
    capacity = length + 1;
    buffer.resize(capacity);
    length = ::GetFinalPathNameByHandleW(
        file->GetPlatformFile(), buffer.data(), capacity,
        FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
    if (length == 0 || length >= capacity) {
      return std::nullopt;
    }
  }
  buffer.resize(length);
  return base::FilePath(std::move(buffer));
}

#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
base::NoDestructor<ConditionalDeleteHookForTesting> g_conditional_delete_hook;
#endif

ConditionalDeleteResult FinishConditionalDelete(base::File* file,
                                                const base::FilePath& path) {
#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
  if (*g_conditional_delete_hook) {
    g_conditional_delete_hook->Run(path);
  }
#endif
  if (file->DeleteOnClose(true)) {
    return ConditionalDeleteResult::kDeleted;
  }

  // ReplaceFile may already have detached the observed file object from
  // |path|. In that case the failed disposition cannot affect the replacement,
  // and cleanup of the observed object is already complete for our purposes.
  base::File current(path, base::File::FLAG_OPEN | base::File::FLAG_READ |
                               base::File::FLAG_WIN_SHARE_DELETE);
  if (!current.IsValid()) {
    return base::PathExists(path) ? ConditionalDeleteResult::kChanged
                                  : ConditionalDeleteResult::kDeleted;
  }
  if (!IsSameFileObject(file, &current)) {
    return ConditionalDeleteResult::kDeleted;
  }
  return ConditionalDeleteResult::kError;
}

}  // namespace

bool WriteFileWithIntegrity(const base::FilePath& path,
                            const std::string& content) {
  const base::FilePath parent = path.DirName();
  if (!base::DirectoryExists(parent) || IsReparsePoint(parent) ||
      IsReparsePoint(path)) {
    return false;
  }

  EnsureCrcInitialized();
  IntegrityFooter footer = {};
  footer.data_crc32 = CrcCalc(content.data(), content.size());
  footer.magic = kIntegrityFooterMagic;

  std::string output;
  output.reserve(content.size() + kFooterSize);
  output.append(content);
  output.append(reinterpret_cast<const char*>(&footer), kFooterSize);

  base::FilePath temp_path;
  if (!base::CreateTemporaryFileInDir(parent, &temp_path) ||
      IsReparsePoint(temp_path)) {
    return false;
  }

  bool write_succeeded = false;
  {
    base::File temp(temp_path, base::File::FLAG_OPEN | base::File::FLAG_WRITE |
                                   base::File::FLAG_WIN_SHARE_DELETE);
    if (temp.IsValid()) {
      std::optional<size_t> written =
          temp.WriteAtCurrentPos(base::as_byte_span(output));
      write_succeeded = written && *written == output.size() && temp.Flush();
    }
  }
  const bool replaced =
      write_succeeded && base::ReplaceFile(temp_path, path, nullptr);
  if (!replaced) {
    base::DeleteFile(temp_path);
  }
  return replaced;
}

IntegrityResult ReadFileWithIntegrity(const base::FilePath& path,
                                      std::string* content,
                                      IntegrityMismatchAction mismatch_action,
                                      size_t max_content_size,
                                      base::FilePath* resolved_path,
                                      std::string* raw_content,
                                      bool* too_large) {
  if (!content) {
    return IntegrityResult::kReadError;
  }
  if (too_large) {
    *too_large = false;
  }

  if (!base::PathExists(path)) {
    return IntegrityResult::kFileNotFound;
  }

  std::string raw;
  base::File file = OpenForReadNoFollow(path);
  if (!file.IsValid() || IsOpenedReparsePoint(&file)) {
    return IntegrityResult::kReadError;
  }
  if (resolved_path) {
    std::optional<base::FilePath> opened_path = GetOpenedResolvedPath(&file);
    if (!opened_path) {
      return IntegrityResult::kReadError;
    }
    *resolved_path = std::move(*opened_path);
  }
  int64_t length = file.GetLength();
  const size_t max_file_size =
      max_content_size <= std::numeric_limits<size_t>::max() - kFooterSize
          ? max_content_size + kFooterSize
          : std::numeric_limits<size_t>::max();
  if (length < 0 || static_cast<uint64_t>(length) > max_file_size) {
    if (too_large && length >= 0 &&
        static_cast<uint64_t>(length) > max_file_size) {
      *too_large = true;
    }
    return IntegrityResult::kReadError;
  }
  raw.resize(static_cast<size_t>(length));
  std::optional<size_t> bytes_read =
      file.ReadAtCurrentPos(base::as_writable_byte_span(raw));
  if (!bytes_read || *bytes_read != raw.size()) {
    return IntegrityResult::kReadError;
  }
  if (raw_content) {
    *raw_content = raw;
  }

  // File too small to contain a footer.
  if (raw.size() < kFooterSize) {
    if (raw.size() > max_content_size) {
      return IntegrityResult::kReadError;
    }
    *content = std::move(raw);
    return IntegrityResult::kSuccessNoFooter;
  }

  // Check for magic number at end of file.
  IntegrityFooter footer;
  memcpy(&footer, raw.data() + raw.size() - kFooterSize, kFooterSize);

  if (footer.magic != kIntegrityFooterMagic) {
    if (raw.size() > max_content_size) {
      return IntegrityResult::kReadError;
    }
    // No integrity footer — treat as legacy file.
    *content = std::move(raw);
    return IntegrityResult::kSuccessNoFooter;
  }

  // Footer present — verify CRC32 of content.
  EnsureCrcInitialized();
  size_t content_size = raw.size() - kFooterSize;
  uint32_t actual_crc = CrcCalc(raw.data(), content_size);

  if (actual_crc != footer.data_crc32) {
    if (mismatch_action == IntegrityMismatchAction::kDoom) {
      // Corrupted. Delete the file (doom pattern from Chromium's disk cache).
      base::DeleteFile(path);
    }
    return IntegrityResult::kIntegrityMismatch;
  }

  // Integrity verified. Return content without footer.
  raw.resize(content_size);
  *content = std::move(raw);
  return IntegrityResult::kSuccess;
}

std::optional<base::FilePath> GetSafeDirectoryResolvedPath(
    const base::FilePath& path) {
  base::File directory(::CreateFileW(
      path.value().c_str(), FILE_READ_ATTRIBUTES,
      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
      OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT,
      nullptr));
  if (!directory.IsValid() || IsOpenedReparsePoint(&directory)) {
    return std::nullopt;
  }
  FILE_ATTRIBUTE_TAG_INFO info = {};
  if (!::GetFileInformationByHandleEx(directory.GetPlatformFile(),
                                      FileAttributeTagInfo, &info,
                                      sizeof(info)) ||
      (info.FileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
    return std::nullopt;
  }
  return GetOpenedResolvedPath(&directory);
}

ConditionalDeleteResult DeleteFileWithIntegrityIfMatching(
    const base::FilePath& path,
    const std::string& expected_content,
    bool expected_integrity_protected,
    const base::FilePath& expected_resolved_parent) {
  base::File file = OpenForConditionalDeleteNoFollow(path);
  if (!file.IsValid()) {
    return !base::PathExists(path) || IsReparsePoint(path)
               ? ConditionalDeleteResult::kChanged
               : ConditionalDeleteResult::kError;
  }
  if (IsOpenedReparsePoint(&file)) {
    return ConditionalDeleteResult::kChanged;
  }
  std::optional<base::FilePath> resolved_path = GetOpenedResolvedPath(&file);
  if (!resolved_path || expected_resolved_parent.empty() ||
      !base::FilePath::CompareEqualIgnoreCase(
          resolved_path->DirName().value(), expected_resolved_parent.value())) {
    return ConditionalDeleteResult::kChanged;
  }

  int64_t length = file.GetLength();
  if (expected_content.size() > static_cast<size_t>(kMaxLaunchStateFileSize)) {
    return ConditionalDeleteResult::kChanged;
  }
  const size_t expected_length =
      expected_content.size() +
      (expected_integrity_protected ? kFooterSize : 0u);
  if (length < 0 || static_cast<uint64_t>(length) != expected_length) {
    return ConditionalDeleteResult::kChanged;
  }
  if (static_cast<uint64_t>(length) > std::numeric_limits<size_t>::max()) {
    return ConditionalDeleteResult::kError;
  }
  std::string raw(static_cast<size_t>(length), '\0');
  std::optional<size_t> bytes_read =
      file.ReadAtCurrentPos(base::as_writable_byte_span(raw));
  if (!bytes_read || *bytes_read != raw.size()) {
    return ConditionalDeleteResult::kError;
  }

  IntegrityResult integrity = IntegrityResult::kSuccessNoFooter;
  std::string content;
  if (raw.size() < kFooterSize) {
    content = std::move(raw);
  } else {
    IntegrityFooter footer;
    memcpy(&footer, raw.data() + raw.size() - kFooterSize, kFooterSize);
    if (footer.magic != kIntegrityFooterMagic) {
      content = std::move(raw);
    } else {
      EnsureCrcInitialized();
      size_t content_size = raw.size() - kFooterSize;
      if (CrcCalc(raw.data(), content_size) != footer.data_crc32) {
        integrity = IntegrityResult::kIntegrityMismatch;
      } else {
        raw.resize(content_size);
        content = std::move(raw);
        integrity = IntegrityResult::kSuccess;
      }
    }
  }

  if (content != expected_content ||
      ((integrity == IntegrityResult::kSuccess) !=
       expected_integrity_protected) ||
      (integrity != IntegrityResult::kSuccess &&
       integrity != IntegrityResult::kSuccessNoFooter)) {
    return ConditionalDeleteResult::kChanged;
  }
  return FinishConditionalDelete(&file, path);
}

ConditionalDeleteResult DeleteFileRawIfMatching(
    const base::FilePath& path,
    const std::string& expected_raw_content,
    const base::FilePath& expected_resolved_parent) {
  base::File file = OpenForConditionalDeleteNoFollow(path);
  if (!file.IsValid()) {
    return !base::PathExists(path) || IsReparsePoint(path)
               ? ConditionalDeleteResult::kChanged
               : ConditionalDeleteResult::kError;
  }
  if (IsOpenedReparsePoint(&file)) {
    return ConditionalDeleteResult::kChanged;
  }
  std::optional<base::FilePath> resolved_path = GetOpenedResolvedPath(&file);
  if (!resolved_path || expected_resolved_parent.empty() ||
      !base::FilePath::CompareEqualIgnoreCase(
          resolved_path->DirName().value(), expected_resolved_parent.value())) {
    return ConditionalDeleteResult::kChanged;
  }
  if (expected_raw_content.size() >
      static_cast<size_t>(kMaxLaunchStateFileSize) + kFooterSize) {
    return ConditionalDeleteResult::kChanged;
  }
  const int64_t length = file.GetLength();
  if (length < 0 ||
      static_cast<uint64_t>(length) != expected_raw_content.size()) {
    return ConditionalDeleteResult::kChanged;
  }
  std::string raw(static_cast<size_t>(length), '\0');
  std::optional<size_t> bytes_read =
      file.ReadAtCurrentPos(base::as_writable_byte_span(raw));
  if (!bytes_read || *bytes_read != raw.size()) {
    return ConditionalDeleteResult::kError;
  }
  if (raw != expected_raw_content) {
    return ConditionalDeleteResult::kChanged;
  }
  return FinishConditionalDelete(&file, path);
}

ConditionalDeleteResult DeleteFileIfOversized(
    const base::FilePath& path,
    size_t max_content_size,
    const base::FilePath& expected_resolved_parent) {
  base::File file = OpenForConditionalDeleteNoFollow(path);
  if (!file.IsValid()) {
    return !base::PathExists(path) || IsReparsePoint(path)
               ? ConditionalDeleteResult::kChanged
               : ConditionalDeleteResult::kError;
  }
  if (IsOpenedReparsePoint(&file)) {
    return ConditionalDeleteResult::kChanged;
  }
  std::optional<base::FilePath> resolved_path = GetOpenedResolvedPath(&file);
  if (!resolved_path || expected_resolved_parent.empty() ||
      !base::FilePath::CompareEqualIgnoreCase(
          resolved_path->DirName().value(), expected_resolved_parent.value())) {
    return ConditionalDeleteResult::kChanged;
  }
  const size_t max_file_size =
      max_content_size <= std::numeric_limits<size_t>::max() - kFooterSize
          ? max_content_size + kFooterSize
          : std::numeric_limits<size_t>::max();
  const int64_t length = file.GetLength();
  if (length < 0) {
    return ConditionalDeleteResult::kError;
  }
  if (static_cast<uint64_t>(length) <= max_file_size) {
    return ConditionalDeleteResult::kChanged;
  }
  return FinishConditionalDelete(&file, path);
}

ConditionalDeleteResult DeleteFileIfReparsePoint(
    const base::FilePath& path,
    const base::FilePath& expected_resolved_parent) {
  base::File file = OpenForConditionalDeleteNoFollow(path);
  if (!file.IsValid()) {
    return !base::PathExists(path) || !IsReparsePoint(path)
               ? ConditionalDeleteResult::kChanged
               : ConditionalDeleteResult::kError;
  }
  FILE_ATTRIBUTE_TAG_INFO info = {};
  if (!::GetFileInformationByHandleEx(
          file.GetPlatformFile(), FileAttributeTagInfo, &info, sizeof(info))) {
    return ConditionalDeleteResult::kError;
  }
  if ((info.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0) {
    return ConditionalDeleteResult::kChanged;
  }
  std::optional<base::FilePath> resolved_path = GetOpenedResolvedPath(&file);
  if (!resolved_path || expected_resolved_parent.empty() ||
      !base::FilePath::CompareEqualIgnoreCase(
          resolved_path->DirName().value(), expected_resolved_parent.value())) {
    return ConditionalDeleteResult::kChanged;
  }
  return FinishConditionalDelete(&file, path);
}

void SetConditionalDeleteHookForTesting(
    ConditionalDeleteHookForTesting callback) {
#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
  *g_conditional_delete_hook = std::move(callback);
#endif
}

const char* IntegrityResultToString(IntegrityResult result) {
  switch (result) {
    case IntegrityResult::kSuccess:
      return "Success (verified)";
    case IntegrityResult::kSuccessNoFooter:
      return "Success (no integrity footer)";
    case IntegrityResult::kFileNotFound:
      return "File not found";
    case IntegrityResult::kReadError:
      return "Read error";
    case IntegrityResult::kIntegrityMismatch:
      return "Integrity mismatch";
  }
  return "Unknown";
}

}  // namespace cef_installer
