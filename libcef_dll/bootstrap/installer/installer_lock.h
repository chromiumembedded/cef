// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_LOCK_H_
#define CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_LOCK_H_

#include <windows.h>

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "cef/libcef_dll/bootstrap/installer/installer_constants.h"

namespace cef_installer {

// RAII lock for installer database operations.
// Uses a named mutex to serialize access across processes.
class SingletonLock {
 public:
  // Attempt to acquire the lock. Blocks until lock is available or timeout.
  // Timeout of 0 means non-blocking (try once and return).
  // Timeout of INFINITE means wait forever.
  // Default timeout: kDefaultLockTimeoutMs (30 seconds)
  //
  // Returns nullptr if the lock could not be acquired within the timeout.
  static std::unique_ptr<SingletonLock> Acquire(
      const base::FilePath& install_dir,
      DWORD timeout_ms = kDefaultLockTimeoutMs);

  ~SingletonLock();

  SingletonLock(const SingletonLock&) = delete;
  SingletonLock& operator=(const SingletonLock&) = delete;

  // Check if lock was successfully acquired.
  bool IsHeld() const;

  // Returns true when acquisition recovered a mutex whose previous owning
  // process or thread terminated without releasing it. The caller owns the
  // mutex in this case and must run writer-locked recovery before mutation.
  bool was_abandoned() const;

 private:
  static std::unique_ptr<SingletonLock> AcquireNamed(std::wstring mutex_name,
                                                     DWORD timeout_ms);

  SingletonLock(HANDLE mutex_handle,
                std::wstring mutex_name,
                bool was_abandoned);

  HANDLE mutex_handle_ = nullptr;
  std::wstring mutex_name_;
  DWORD owner_thread_id_ = 0;
  bool was_abandoned_ = false;
};

// ============================================================================
// Internal helpers exposed for testing. Not part of the public API.
// ============================================================================
namespace internal {

// Get the mutex name for a given install directory.
std::wstring GetMutexName(const base::FilePath& install_dir);

// SHA-1 hash of |input|, returning the first 16 uppercase hex characters.
// Used for generating deterministic short identifiers (lock names, filenames).
std::string ComputeSha1HexPrefix(const std::string& input);

}  // namespace internal

}  // namespace cef_installer

#endif  // CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_LOCK_H_
