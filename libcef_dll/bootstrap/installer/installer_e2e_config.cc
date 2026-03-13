// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef_dll/bootstrap/installer/installer_e2e_config.h"

#include <windows.h>

#include <string_view>
#include <utility>

#include "base/environment.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/time/time.h"

namespace cef_installer::internal {

namespace {

constexpr base::TimeDelta kChildStateBarrierTimeout = base::Seconds(30);

InstallerE2EConfig LoadInstallerE2EConfigFromEnvironment() {
  InstallerE2EConfig config;
  auto environment = base::Environment::Create();

  const std::optional<std::string> directory_scenario =
      environment->GetVar("CEF_INSTALLER_TEST_DIRECTORY_SCENARIO");
  const std::optional<std::string> directory_root =
      environment->GetVar("CEF_INSTALLER_TEST_DIRECTORY_ROOT");

#if defined(OFFICIAL_BUILD) && defined(NDEBUG)
  if (directory_scenario || directory_root) {
    config.directory_override = InstallerE2EDirectoryOverride::kInvalid;
  }
  return config;
#else
  if (directory_scenario || directory_root) {
    if (!directory_scenario || !directory_root ||
        *directory_scenario != "admin_mutation_denied") {
      config.directory_override = InstallerE2EDirectoryOverride::kInvalid;
    } else {
      config.directory_root = base::FilePath::FromUTF8Unsafe(*directory_root)
                                  .StripTrailingSeparators();
      config.directory_override =
          config.directory_root.IsAbsolute() &&
                  !config.directory_root.ReferencesParent()
              ? InstallerE2EDirectoryOverride::kAdminMutationDenied
              : InstallerE2EDirectoryOverride::kInvalid;
    }
  }

  if (const auto value =
          environment->GetVar("CEF_INSTALLER_TEST_FILE_OPS_FAULT")) {
    if (*value == "quarantine_move") {
      config.file_ops_fault = InstallerE2EFileOpsFault::kQuarantineMove;
    } else if (*value == "repair_move") {
      config.file_ops_fault = InstallerE2EFileOpsFault::kRepairMove;
    } else if (*value == "trash_move") {
      config.file_ops_fault = InstallerE2EFileOpsFault::kTrashMove;
    } else if (*value == "trash_reclaim") {
      config.file_ops_fault = InstallerE2EFileOpsFault::kTrashReclaim;
    }
  }

  if (const auto value =
          environment->GetVar("CEF_INSTALLER_TEST_INDEX_FAULT")) {
    if (*value == "write") {
      config.version_index_fault = InstallerE2EVersionIndexFault::kWrite;
    } else if (*value == "replace") {
      config.version_index_fault = InstallerE2EVersionIndexFault::kReplace;
    } else if (*value == "reread") {
      config.version_index_fault = InstallerE2EVersionIndexFault::kReread;
    } else if (*value == "validation") {
      config.version_index_fault = InstallerE2EVersionIndexFault::kValidation;
    }
  }

  const auto enabled = [&environment](base::cstring_view name) {
    const auto value = environment->GetVar(name);
    return value && *value == "1";
  };
  config.database_save_failure =
      enabled("CEF_INSTALLER_TEST_DATABASE_SAVE_FAILURE");
  config.relaunch_failure = enabled("CEF_INSTALLER_TEST_RELAUNCH_FAILURE");
  config.child_config_failure =
      enabled("CEF_INSTALLER_TEST_CHILD_CONFIG_FAILURE");

  if (const auto value =
          environment->GetVar("CEF_INSTALLER_TEST_CHILD_STATE_BARRIER")) {
    base::FilePath path = base::FilePath::FromUTF8Unsafe(*value);
    if (path.IsAbsolute() && !path.ReferencesParent()) {
      config.child_state_barrier = std::move(path);
    }
  }
  return config;
#endif
}

#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
std::optional<InstallerE2EConfig>& ConfigForTesting() {
  static base::NoDestructor<std::optional<InstallerE2EConfig>> config;
  return *config;
}
#endif

}  // namespace

const InstallerE2EConfig& GetInstallerE2EConfig() {
#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
  if (ConfigForTesting()) {
    return *ConfigForTesting();
  }
#endif
  static const base::NoDestructor<InstallerE2EConfig> config(
      LoadInstallerE2EConfigFromEnvironment());
  return *config;
}

void WaitAtInstallerE2EChildStateBarrier() {
  const base::FilePath& path = GetInstallerE2EConfig().child_state_barrier;
  if (path.empty() || !base::WriteFile(path, "ready")) {
    return;
  }

  const base::TimeTicks deadline =
      base::TimeTicks::Now() + kChildStateBarrierTimeout;
  while (base::PathExists(path) && base::TimeTicks::Now() < deadline) {
    ::Sleep(25);
  }
  if (base::PathExists(path)) {
    LOG(ERROR) << "Timed out waiting at installer E2E child-state barrier";
  }
}

#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
InstallerE2EConfig LoadInstallerE2EConfigFromEnvironmentForTesting() {
  return LoadInstallerE2EConfigFromEnvironment();
}

void SetInstallerE2EConfigForTesting(
    const std::optional<InstallerE2EConfig>& config) {
  ConfigForTesting() = config;
}
#endif

}  // namespace cef_installer::internal
