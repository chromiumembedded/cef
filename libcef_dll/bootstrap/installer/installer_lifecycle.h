// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_LIFECYCLE_H_
#define CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_LIFECYCLE_H_

#include <windows.h>

#include <optional>
#include <string>
#include <string_view>

#include "cef/libcef_dll/bootstrap/installer/installer_controller.h"

namespace cef_installer {

struct UninstallLifecycleContext {
  std::string operation_id;
  HWND parent_window = nullptr;

  bool valid() const;
};

enum class InstallerLifecycleSendStatus {
  kDelivered,
  kInvalidWindow,
  kTimeout,
  kError,
  kSerializationError,
};

// Generates exactly 128 random bits encoded as 32 lowercase hexadecimal
// characters. The value is public correlation data, not authorization.
std::string GenerateInstallerLifecycleOperationId();
bool IsValidInstallerLifecycleOperationId(std::string_view operation_id);

// Returns true only for stable installer errors that a relaunched child may
// report as terminal. Success is validated separately; kExitCodeRelaunched is
// reserved for the original process.
bool IsValidInstallerLifecycleTerminalErrorCode(int exit_code);

// Returned JSON excludes the trailing NUL. The bounded send below appends
// exactly one NUL and includes it in COPYDATASTRUCT::cbData.
std::optional<std::string> SerializeInstallerRelaunchStarted(
    std::string_view operation_id,
    DWORD child_pid);
std::optional<std::string> SerializeInstallerOperationResult(
    std::string_view operation_id,
    const Result& result,
    int exit_code);

// Makes one bounded SendMessageTimeoutW attempt. Receiver return values are
// ignored and cannot cancel or otherwise affect installer work.
InstallerLifecycleSendStatus SendInstallerLifecycleMessage(
    HWND parent_window,
    std::string_view json);

// Sends a normalized terminal result when |context| is valid and always
// returns the unmodified ResultToExitCode() value.
int FinalizeUninstallLifecycle(
    const std::optional<UninstallLifecycleContext>& context,
    const Result& result);

// Controlled child-side failures that occur after trusted lifecycle context
// has been accepted but before a controller result exists.
enum class ControlledUninstallFailure {
  kInvalidRetentionOptions,
  kInvalidRetentionAge,
  kConfigLoad,
  kConfigNotFound,
  kExplicitModeDisabled,
  kPreflightRejected,
};

// Routes a controlled child-side failure through the same one-shot terminal
// finalizer as controller results. |error_code| remains the returned and
// serialized stable installer exit code.
int FinalizeControlledUninstallFailure(
    const std::optional<UninstallLifecycleContext>& context,
    ControlledUninstallFailure failure,
    int error_code = kExitCodeConfigError);

// Runs the UI-close action before attempting terminal delivery. This is the
// sole controller-result terminal boundary for relaunched uninstall.
int FinalizeUninstallLifecycleAfterProgressUi(
    const std::optional<UninstallLifecycleContext>& context,
    const Result& result,
    base::OnceClosure close_progress_ui);

}  // namespace cef_installer

#endif  // CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_LIFECYCLE_H_
