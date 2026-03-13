// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_FILE_INTEGRITY_H_
#define CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_FILE_INTEGRITY_H_

#include <limits>
#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"

namespace cef_installer {

// Result codes for integrity-checked file reads.
enum class IntegrityResult {
  kSuccess,            // Content read and CRC32 verified
  kSuccessNoFooter,    // Content read, no integrity footer (legacy file)
  kFileNotFound,       // File does not exist
  kReadError,          // Could not read file
  kIntegrityMismatch,  // CRC32 mismatch
};

enum class IntegrityMismatchAction {
  kDoom,
  kPreserve,
};

enum class ConditionalDeleteResult {
  kDeleted,
  kChanged,
  kError,
};

// Atomically write content with a CRC32 integrity footer appended. The new
// contents are written to a unique same-directory temporary file and replaced
// into place only after the complete write succeeds. A failed write or replace
// leaves the prior destination intact when the platform supports it. No partial
// new destination is exposed to concurrent readers.
//
// On-disk format:
//   [content bytes]
//   [4 bytes: CRC32 of content, little-endian]
//   [4 bytes: reserved (zero)]
//   [8 bytes: magic number]
//
// Total footer overhead: 16 bytes.
// Returns true on success. Returns false for unsafe reparse paths, temporary
// creation/write failures, or replacement failures.
bool WriteFileWithIntegrity(const base::FilePath& path,
                            const std::string& content);

// Read a file, verifying its CRC32 integrity footer if present.
//
// - Footer present, CRC32 matches: returns kSuccess, |content| has the
//   original data with footer stripped.
// - Footer present, CRC32 mismatch: returns kIntegrityMismatch and either
//   deletes or preserves the corrupted file according to |mismatch_action|.
// - No footer (legacy file): returns kSuccessNoFooter, |content| has
//   the full file contents.
//
// The doom-on-mismatch behavior follows Chromium's disk cache pattern:
// a corrupted entry is worse than a missing one.
IntegrityResult ReadFileWithIntegrity(
    const base::FilePath& path,
    std::string* content,
    IntegrityMismatchAction mismatch_action = IntegrityMismatchAction::kDoom,
    size_t max_content_size = std::numeric_limits<size_t>::max(),
    base::FilePath* resolved_path = nullptr,
    std::string* raw_content = nullptr,
    bool* too_large = nullptr);

// Opens |path| without following a reparse point in the final component,
// rejects a reparse-point directory, and returns the handle-resolved path.
std::optional<base::FilePath> GetSafeDirectoryResolvedPath(
    const base::FilePath& path);

// Opens |path| itself without following a reparse point, verifies that the
// handle-resolved parent is |expected_resolved_parent|, verifies the payload
// and footer status on that handle, and marks that same file object for
// deletion only on an exact match. A concurrent atomic replacement names a
// different file object and is therefore preserved.
ConditionalDeleteResult DeleteFileWithIntegrityIfMatching(
    const base::FilePath& path,
    const std::string& expected_content,
    bool expected_integrity_protected,
    const base::FilePath& expected_resolved_parent);

// Opens |path| without following a reparse point, verifies its resolved parent
// and complete raw bytes, and marks that same file object for deletion only on
// an exact match. This is used for conclusively invalid publications whose
// integrity payload cannot be represented by the structured helper above.
ConditionalDeleteResult DeleteFileRawIfMatching(
    const base::FilePath& path,
    const std::string& expected_raw_content,
    const base::FilePath& expected_resolved_parent);

// Marks the same no-follow file object for deletion only if it still exceeds
// |max_content_size| plus the maximum integrity footer and its resolved parent
// matches. A bounded valid replacement is preserved.
ConditionalDeleteResult DeleteFileIfOversized(
    const base::FilePath& path,
    size_t max_content_size,
    const base::FilePath& expected_resolved_parent);

// Opens |path| without following the final component and marks that same file
// object for deletion only if it is still a reparse point whose resolved
// parent matches |expected_resolved_parent|. The reparse target is never
// opened, and a concurrent replacement is preserved.
ConditionalDeleteResult DeleteFileIfReparsePoint(
    const base::FilePath& path,
    const base::FilePath& expected_resolved_parent);

using ConditionalDeleteHookForTesting =
    base::RepeatingCallback<void(const base::FilePath&)>;
void SetConditionalDeleteHookForTesting(
    ConditionalDeleteHookForTesting callback);

// Convert result to string for logging.
const char* IntegrityResultToString(IntegrityResult result);

}  // namespace cef_installer

#endif  // CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_FILE_INTEGRITY_H_
