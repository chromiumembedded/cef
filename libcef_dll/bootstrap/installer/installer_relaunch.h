// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_RELAUNCH_H_
#define CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_RELAUNCH_H_

#include <windows.h>

#include <optional>
#include <string>
#include <string_view>

#include "base/files/file_path.h"
#include "cef/libcef_dll/bootstrap/installer/installer_lifecycle.h"

namespace cef_installer {

// Get a unique temp directory path for uninstall operations.
// Returns: %TEMP%\cef_uninstall_<random>\
// The directory is NOT created by this function; caller should create it.
base::FilePath GetUninstallTempDirectory();

// Copy the installer and required dependencies to a temp directory and
// relaunch. Bootstrap uses this conditionally when the running executable is
// inside the selected writable install directory or /cef-background requests
// asynchronous uninstall.
//
// Files copied:
// - The executable itself (bootstrap.exe or client-named exe)
// - chrome_elf.dll (required - bootstrap won't start without it)
// - crash_reporter.cfg (optional, crashpad configuration)
// - The authenticated client DLL (required only when its resource supplied the
//   selected application config)
// A separate trusted relaunch-state file carries the already-resolved absolute
// install_path.
//
// Parameters:
// - exe_path: Path to the current executable
// - client_dll_module: Authenticated selected client module, or null when the
//   bootstrap resource supplied the application config. Its full path and
//   actual basename are derived internally.
// - command_line_args: Command line arguments to pass to the relaunched process
// - install_path: Already-resolved exclusive namespace, or empty for defaults
//
enum class UninstallRelaunchStatus {
  kFailed,
  kStarted,
};

struct UninstallRelaunchResult {
  UninstallRelaunchStatus status = UninstallRelaunchStatus::kFailed;
  DWORD child_pid = 0;
  std::string operation_id;

  bool started() const { return status == UninstallRelaunchStatus::kStarted; }
};

// Returns typed launch status and supplemental PID/correlation data. On
// success, the current process should exit immediately. Lifecycle context is
// generated only for a currently valid |parent_window|.
UninstallRelaunchResult CopySelfToTempAndRelaunch(
    const base::FilePath& exe_path,
    HMODULE client_dll_module,
    std::wstring_view command_line_args,
    std::string_view install_path = {},
    HWND parent_window = nullptr);

struct UninstallRelaunchState {
  std::string install_path;
  std::optional<UninstallLifecycleContext> lifecycle_context;
};

namespace internal {

constexpr size_t kMaxUninstallRelaunchStateSize = 32 * 1024;

#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
struct UninstallRelaunchCallbacksForTesting {
  bool (*launch)(const base::FilePath& executable,
                 std::wstring_view command_line,
                 const base::FilePath& current_directory,
                 DWORD* child_pid) = nullptr;
  InstallerLifecycleSendStatus (
      *send_lifecycle)(HWND parent_window, std::string_view json) = nullptr;
  bool (*copy_client_module)(const base::FilePath& source,
                             const base::FilePath& destination) = nullptr;
};

// Installs deterministic launch/send seams. Passing nullptr clears them.
void SetUninstallRelaunchCallbacksForTesting(
    const UninstallRelaunchCallbacksForTesting* callbacks);
#endif

bool WriteUninstallRelaunchState(const base::FilePath& temp_dir,
                                 std::string_view install_path,
                                 std::string_view nonce,
                                 const std::optional<UninstallLifecycleContext>&
                                     lifecycle_context = std::nullopt);
bool ReadUninstallRelaunchState(const base::FilePath& exe_path,
                                std::string_view expected_nonce,
                                HWND command_parent_window,
                                UninstallRelaunchState* state);
bool ReadUninstallRelaunchState(const base::FilePath& exe_path,
                                std::string_view expected_nonce,
                                std::string* install_path);

}  // namespace internal

}  // namespace cef_installer

#endif  // CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_RELAUNCH_H_
