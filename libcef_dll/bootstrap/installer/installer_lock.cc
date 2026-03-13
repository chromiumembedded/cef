// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef_dll/bootstrap/installer/installer_lock.h"

#include <algorithm>
#include <set>
#include <utility>

#include "base/check.h"
#include "base/hash/sha1.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"

namespace cef_installer {

namespace {

// A Windows mutex is recursive for its owning thread. Track names owned by
// this thread so SingletonLock retains the old semaphore's non-recursive
// contract instead of silently accepting a programming error.
thread_local base::NoDestructor<std::set<std::wstring>> g_owned_mutex_names;

}  // namespace

namespace internal {

std::string ComputeSha1HexPrefix(const std::string& input) {
  std::array<uint8_t, base::kSHA1Length> hash =
      base::SHA1Hash(base::as_byte_span(input));
  return base::HexEncode(base::span(hash).first(8u));
}

std::wstring GetMutexName(const base::FilePath& install_dir) {
  // Normalize path: lowercase, forward slashes
  std::string normalized = base::ToLowerASCII(install_dir.AsUTF8Unsafe());
  std::replace(normalized.begin(), normalized.end(), '\\', '/');

  return base::UTF8ToWide(std::string(kMutexNamePrefix) +
                          ComputeSha1HexPrefix(normalized));
}

}  // namespace internal

// static
std::unique_ptr<SingletonLock> SingletonLock::Acquire(
    const base::FilePath& install_dir,
    DWORD timeout_ms) {
  return AcquireNamed(internal::GetMutexName(install_dir), timeout_ms);
}

// static
std::unique_ptr<SingletonLock> SingletonLock::AcquireNamed(
    std::wstring mutex_name,
    DWORD timeout_ms) {
  if (g_owned_mutex_names->contains(mutex_name)) {
    return nullptr;
  }

  // Reuse the historical kernel-object name. If an older process has created
  // a semaphore with this name, CreateMutexW fails with ERROR_INVALID_HANDLE,
  // which fails closed instead of allowing old and new writers concurrently.
  HANDLE mutex = CreateMutexW(nullptr, FALSE, mutex_name.c_str());
  if (!mutex) {
    return nullptr;
  }

  DWORD wait_result = WaitForSingleObject(mutex, timeout_ms);
  if (wait_result == WAIT_OBJECT_0 || wait_result == WAIT_ABANDONED) {
    g_owned_mutex_names->insert(mutex_name);
    return std::unique_ptr<SingletonLock>(new SingletonLock(
        mutex, std::move(mutex_name), wait_result == WAIT_ABANDONED));
  }

  CloseHandle(mutex);
  return nullptr;
}

SingletonLock::SingletonLock(HANDLE mutex_handle,
                             std::wstring mutex_name,
                             bool was_abandoned)
    : mutex_handle_(mutex_handle),
      mutex_name_(std::move(mutex_name)),
      owner_thread_id_(GetCurrentThreadId()),
      was_abandoned_(was_abandoned) {}

SingletonLock::~SingletonLock() {
  if (mutex_handle_) {
    DCHECK_EQ(owner_thread_id_, GetCurrentThreadId());
    ReleaseMutex(mutex_handle_);
    CloseHandle(mutex_handle_);
    if (owner_thread_id_ == GetCurrentThreadId()) {
      g_owned_mutex_names->erase(mutex_name_);
    }
    mutex_handle_ = nullptr;
  }
}

bool SingletonLock::IsHeld() const {
  return mutex_handle_ != nullptr;
}

bool SingletonLock::was_abandoned() const {
  return was_abandoned_;
}

}  // namespace cef_installer
