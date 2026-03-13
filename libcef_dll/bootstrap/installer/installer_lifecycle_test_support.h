// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_LIFECYCLE_TEST_SUPPORT_H_
#define CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_LIFECYCLE_TEST_SUPPORT_H_

#include <windows.h>

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <string_view>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "cef/libcef_dll/bootstrap/installer/installer_lifecycle.h"

namespace cef_installer {
namespace test {

enum class InstallerLifecycleEventKind {
  kRelaunchStarted,
  kOperationResult,
};

enum class InstallerLifecycleParseStatus {
  kValid,
  kIgnored,
  kMalformed,
};

struct ParsedInstallerLifecycleEvent {
  InstallerLifecycleParseStatus status =
      InstallerLifecycleParseStatus::kMalformed;
  InstallerLifecycleEventKind kind =
      InstallerLifecycleEventKind::kRelaunchStarted;
  std::string operation_id;
  DWORD child_pid = 0;
  int exit_code = kExitCodeSuccess;
  std::optional<Result> result;
};

// Test/reference parser for a complete COPYDATA payload including exactly one
// trailing NUL. Unknown protocol versions and event names are ignored. Known
// events are strictly validated while unknown additive fields remain
// compatible.
ParsedInstallerLifecycleEvent ParseInstallerLifecyclePayload(
    std::string_view payload_with_nul);

enum class InstallerLifecycleCorrelationStatus {
  kHandoffRecorded,
  kTerminalBuffered,
  kTerminalCorrelated,
  kDuplicateIgnored,
  kIgnored,
};

// Test/reference receiver correlation logic. A terminal-first event remains
// unattributed until a matching handoff and only the first terminal wins.
// Tracked operations have bounded capacity and idle expiry.
class InstallerLifecycleCorrelator {
 public:
  using NowCallback = base::RepeatingCallback<base::TimeTicks()>;

  InstallerLifecycleCorrelator();
  InstallerLifecycleCorrelator(size_t capacity,
                               base::TimeDelta expiration,
                               NowCallback now_callback);

  InstallerLifecycleCorrelationStatus Accept(
      const ParsedInstallerLifecycleEvent& event);
  const ParsedInstallerLifecycleEvent* GetCorrelatedResult(
      std::string_view operation_id);
  std::optional<ParsedInstallerLifecycleEvent> TakeCorrelatedResult(
      std::string_view operation_id);
  bool Remove(std::string_view operation_id);
  size_t PurgeExpired();
  size_t tracked_operation_count();

 private:
  struct CorrelationEntry {
    std::optional<DWORD> child_pid;
    std::optional<ParsedInstallerLifecycleEvent> terminal;
    base::TimeTicks last_activity;
  };

  base::TimeTicks Now() const;
  size_t PurgeExpired(base::TimeTicks now);
  bool MakeRoomForNewEntry(bool terminal_first);

  const size_t capacity_;
  const base::TimeDelta expiration_;
  const NowCallback now_callback_;
  std::map<std::string, CorrelationEntry> entries_;
};

}  // namespace test
}  // namespace cef_installer

#endif  // CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_LIFECYCLE_TEST_SUPPORT_H_
