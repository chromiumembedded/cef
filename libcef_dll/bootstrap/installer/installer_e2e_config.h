// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_E2E_CONFIG_H_
#define CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_E2E_CONFIG_H_

#include <optional>

#include "base/files/file_path.h"

namespace cef_installer::internal {

enum class InstallerE2EDirectoryOverride {
  kNone,
  kAdminMutationDenied,
  kInvalid,
};

enum class InstallerE2EFileOpsFault {
  kNone,
  kQuarantineMove,
  kRepairMove,
  kTrashMove,
  kTrashReclaim,
};

enum class InstallerE2EVersionIndexFault {
  kNone,
  kWrite,
  kReplace,
  kReread,
  kValidation,
};

// Typed, process-wide configuration for subprocess/E2E-only fault injection.
// Environment variables are parsed only here; product components consume
// these values at the boundary that owns the simulated failure.
struct InstallerE2EConfig {
  InstallerE2EDirectoryOverride directory_override =
      InstallerE2EDirectoryOverride::kNone;
  base::FilePath directory_root;
  InstallerE2EFileOpsFault file_ops_fault = InstallerE2EFileOpsFault::kNone;
  InstallerE2EVersionIndexFault version_index_fault =
      InstallerE2EVersionIndexFault::kNone;
  bool database_save_failure = false;
  bool relaunch_failure = false;
  bool child_config_failure = false;
  base::FilePath child_state_barrier;
};

const InstallerE2EConfig& GetInstallerE2EConfig();

// Signals and waits at the trusted-child-state checkpoint when configured.
// The wait is bounded so abandoned test state cannot stall the process
// indefinitely.
void WaitAtInstallerE2EChildStateBarrier();

#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
InstallerE2EConfig LoadInstallerE2EConfigFromEnvironmentForTesting();
void SetInstallerE2EConfigForTesting(
    const std::optional<InstallerE2EConfig>& config);
#endif

}  // namespace cef_installer::internal

#endif  // CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_E2E_CONFIG_H_
