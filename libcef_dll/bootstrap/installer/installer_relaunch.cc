// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef_dll/bootstrap/installer/installer_relaunch.h"

#include <windows.h>

#include <limits>

#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "cef/libcef_dll/bootstrap/installer/installer_bootstrap_helpers.h"
#include "cef/libcef_dll/bootstrap/installer/installer_e2e_config.h"
#include "cef/libcef_dll/bootstrap/installer/installer_file_ops.h"
#include "cef/libcef_dll/bootstrap/installer/installer_paths.h"
namespace cef_installer {

namespace {

// Prefix for uninstall temp directories
constexpr wchar_t kUninstallTempPrefix[] = L"cef_uninstall_";

// Files to copy for uninstall (relative to exe directory)
constexpr wchar_t kChromeElfDll[] = L"chrome_elf.dll";
constexpr wchar_t kCrashReporterCfg[] = L"crash_reporter.cfg";
constexpr wchar_t kRelaunchStateFilename[] = L"cef_uninstall_state.json";

#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
internal::UninstallRelaunchCallbacksForTesting g_callbacks_for_testing;
#endif

struct LaunchedProcess {
  DWORD pid = 0;
  HANDLE process = nullptr;
  HANDLE thread = nullptr;
};

bool LaunchRelaunchedProcess(const base::FilePath& executable,
                             std::wstring_view command_line,
                             const base::FilePath& current_directory,
                             LaunchedProcess* launched) {
#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
  if (g_callbacks_for_testing.launch) {
    return g_callbacks_for_testing.launch(executable, command_line,
                                          current_directory, &launched->pid);
  }
#endif
  STARTUPINFOW startup_info = {sizeof(startup_info)};
  PROCESS_INFORMATION process_info = {};
  std::vector<wchar_t> cmd_buffer(command_line.begin(), command_line.end());
  cmd_buffer.push_back(L'\0');
  if (!::CreateProcessW(executable.value().c_str(), cmd_buffer.data(), nullptr,
                        nullptr, FALSE, 0, nullptr,
                        current_directory.value().c_str(), &startup_info,
                        &process_info)) {
    return false;
  }
  launched->pid = process_info.dwProcessId;
  launched->process = process_info.hProcess;
  launched->thread = process_info.hThread;
  return true;
}

InstallerLifecycleSendStatus SendRelaunchLifecycleMessage(
    HWND parent_window,
    std::string_view json) {
#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
  if (g_callbacks_for_testing.send_lifecycle) {
    return g_callbacks_for_testing.send_lifecycle(parent_window, json);
  }
#endif
  return SendInstallerLifecycleMessage(parent_window, json);
}

// Copy a single file if it exists. Returns false only on copy failure
// (missing source is OK for optional files).
bool CopyFileIfExists(const base::FilePath& source,
                      const base::FilePath& dest_dir,
                      const std::wstring& filename,
                      bool required) {
  base::FilePath source_file = source.DirName().Append(filename);
  if (!base::PathExists(source_file)) {
    if (required) {
      LOG(ERROR) << "Required file not found: " << source_file.value();
      return false;
    }
    // Optional file not found, that's OK
    return true;
  }

  base::FilePath dest_file = dest_dir.Append(filename);
  if (!base::CopyFile(source_file, dest_file)) {
    LOG(ERROR) << "Failed to copy " << source_file.value() << " to "
               << dest_file.value();
    return false;
  }

  return true;
}

bool CopyAuthenticatedClientModule(HMODULE client_dll_module,
                                   const base::FilePath& temp_dir) {
  if (!client_dll_module) {
    return true;
  }

  wchar_t module_path[MAX_PATH];
  const DWORD module_path_length =
      ::GetModuleFileNameW(client_dll_module, module_path, MAX_PATH);
  if (module_path_length == 0 || module_path_length >= MAX_PATH) {
    LOG(ERROR) << "Failed to resolve authenticated client DLL path";
    return false;
  }
  const base::FilePath source(module_path);
  const base::FilePath destination = temp_dir.Append(source.BaseName());
#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
  if (g_callbacks_for_testing.copy_client_module) {
    return g_callbacks_for_testing.copy_client_module(source, destination);
  }
#endif
  if (!base::CopyFile(source, destination)) {
    LOG(ERROR) << "Failed to copy authenticated client DLL " << source.value()
               << " to " << destination.value();
    return false;
  }
  return true;
}

}  // namespace

base::FilePath GetUninstallTempDirectory() {
  base::FilePath temp_dir = GetTempDirectoryPath();
  if (temp_dir.empty()) {
    return base::FilePath();
  }

  // Generate a random suffix for uniqueness
  uint64_t random_value = base::RandUint64();
  std::wstring dir_name =
      kUninstallTempPrefix + base::NumberToWString(random_value);

  return temp_dir.Append(dir_name);
}

namespace internal {

#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
void SetUninstallRelaunchCallbacksForTesting(
    const UninstallRelaunchCallbacksForTesting* callbacks) {
  g_callbacks_for_testing =
      callbacks ? *callbacks : UninstallRelaunchCallbacksForTesting();
}
#endif

bool WriteUninstallRelaunchState(
    const base::FilePath& temp_dir,
    std::string_view install_path,
    std::string_view nonce,
    const std::optional<UninstallLifecycleContext>& lifecycle_context) {
  if (temp_dir.empty() || !base::DirectoryExists(temp_dir) ||
      IsReparsePoint(temp_dir) || nonce.empty()) {
    return false;
  }
  if (!install_path.empty()) {
    const base::FilePath path =
        base::FilePath::FromUTF8Unsafe(std::string(install_path));
    if (!path.IsAbsolute() || path.ReferencesParent()) {
      return false;
    }
  }

  base::DictValue state;
  state.Set("install_path", std::string(install_path));
  state.Set("nonce", std::string(nonce));
  if (lifecycle_context) {
    if (!lifecycle_context->valid()) {
      return false;
    }
    state.Set("operation_id", lifecycle_context->operation_id);
    state.Set("parent_window", base::NumberToString(reinterpret_cast<uintptr_t>(
                                   lifecycle_context->parent_window)));
  }
  std::string json;
  if (!base::JSONWriter::Write(state, &json) ||
      json.size() > kMaxUninstallRelaunchStateSize) {
    return false;
  }
  return base::WriteFile(temp_dir.Append(kRelaunchStateFilename), json);
}

bool ReadUninstallRelaunchState(const base::FilePath& exe_path,
                                std::string_view expected_nonce,
                                HWND command_parent_window,
                                UninstallRelaunchState* output) {
  if (!output || expected_nonce.empty() || exe_path.empty() ||
      !IsRunningFromTempDirectory(exe_path)) {
    return false;
  }
  const base::FilePath temp_dir = exe_path.DirName();
  if (!base::StartsWith(temp_dir.BaseName().value(), kUninstallTempPrefix,
                        base::CompareCase::SENSITIVE) ||
      !base::DirectoryExists(temp_dir) || IsReparsePoint(temp_dir)) {
    return false;
  }

  const base::FilePath state_path = temp_dir.Append(kRelaunchStateFilename);
  if (IsReparsePoint(state_path)) {
    return false;
  }
  std::string json;
  if (!base::ReadFileToStringWithMaxSize(state_path, &json,
                                         kMaxUninstallRelaunchStateSize)) {
    return false;
  }
  std::optional<base::DictValue> state =
      base::JSONReader::ReadDict(json, base::JSON_PARSE_RFC);
  if (!state) {
    return false;
  }
  const std::string* path_value = state->FindString("install_path");
  const std::string* nonce = state->FindString("nonce");
  if (!path_value || !nonce || *nonce != expected_nonce) {
    return false;
  }
  if (!path_value->empty()) {
    const base::FilePath path = base::FilePath::FromUTF8Unsafe(*path_value);
    if (!path.IsAbsolute() || path.ReferencesParent()) {
      return false;
    }
  }
  const std::string* operation_id = state->FindString("operation_id");
  const std::string* parent_window = state->FindString("parent_window");
  if ((operation_id == nullptr) != (parent_window == nullptr)) {
    return false;
  }

  std::optional<UninstallLifecycleContext> lifecycle_context;
  if (operation_id) {
    uint64_t parent_value = 0;
    if (!IsValidInstallerLifecycleOperationId(*operation_id) ||
        parent_window->empty() ||
        !base::StringToUint64(*parent_window, &parent_value) ||
        parent_value == 0 ||
        parent_value > std::numeric_limits<uintptr_t>::max() ||
        base::NumberToString(parent_value) != *parent_window) {
      return false;
    }
    HWND bound_parent =
        reinterpret_cast<HWND>(static_cast<uintptr_t>(parent_value));
    if (bound_parent != command_parent_window) {
      return false;
    }
    lifecycle_context = UninstallLifecycleContext{*operation_id, bound_parent};
  }

  output->install_path = *path_value;
  output->lifecycle_context = std::move(lifecycle_context);
  return true;
}

bool ReadUninstallRelaunchState(const base::FilePath& exe_path,
                                std::string_view expected_nonce,
                                std::string* install_path) {
  if (!install_path) {
    return false;
  }
  UninstallRelaunchState state;
  if (!ReadUninstallRelaunchState(exe_path, expected_nonce, nullptr, &state) ||
      state.lifecycle_context) {
    return false;
  }
  *install_path = std::move(state.install_path);
  return true;
}

}  // namespace internal

UninstallRelaunchResult CopySelfToTempAndRelaunch(
    const base::FilePath& exe_path,
    HMODULE client_dll_module,
    std::wstring_view command_line_args,
    std::string_view install_path,
    HWND parent_window) {
  if (exe_path.empty()) {
    LOG(ERROR) << "Empty exe_path provided to CopySelfToTempAndRelaunch";
    return {};
  }

#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
  if (internal::GetInstallerE2EConfig().relaunch_failure) {
    LOG(ERROR) << "Forced uninstall relaunch failure for testing";
    return {};
  }
#endif

  VLOG(1) << "Preparing to relaunch from temp directory";
  VLOG(2) << "Source exe: " << exe_path.value();

  // Create unique temp directory
  base::FilePath temp_dir = GetUninstallTempDirectory();
  if (temp_dir.empty()) {
    LOG(ERROR) << "Failed to get temp directory for uninstall";
    return {};
  }

  VLOG(2) << "Creating temp directory: " << temp_dir.value();
  if (!base::CreateDirectory(temp_dir)) {
    LOG(ERROR) << "Failed to create temp directory: " << temp_dir.value();
    return {};
  }

  // Security: Verify the created directory is not a reparse point.
  // An attacker who could predict the random suffix might pre-create a
  // junction. This is defense-in-depth; the 64-bit random makes prediction
  // impractical.
  if (IsReparsePoint(temp_dir)) {
    LOG(ERROR) << "Temp directory is a reparse point (possible attack): "
               << temp_dir.value();
    return {};
  }

  ScopedDirectoryDeleter temp_dir_deleter(temp_dir);

  // Copy the executable and chrome_elf.dll (both required)
  std::wstring exe_name = exe_path.BaseName().value();
  base::FilePath dest_exe = temp_dir.Append(exe_name);

  if (!CopyFileIfExists(exe_path, temp_dir, exe_name, /*required=*/true) ||
      !CopyFileIfExists(exe_path, temp_dir, kChromeElfDll,
                        /*required=*/true)) {
    return {};
  }

  // Copy crash_reporter.cfg (optional)
  CopyFileIfExists(exe_path, temp_dir, kCrashReporterCfg, /*required=*/false);

  if (!CopyAuthenticatedClientModule(client_dll_module, temp_dir)) {
    return {};
  }

  std::optional<UninstallLifecycleContext> lifecycle_context;
  if (parent_window && ::IsWindow(parent_window)) {
    lifecycle_context = UninstallLifecycleContext{
        GenerateInstallerLifecycleOperationId(), parent_window};
  }
  const std::string relaunch_nonce = base::NumberToString(base::RandUint64());
  if (!internal::WriteUninstallRelaunchState(
          temp_dir, install_path, relaunch_nonce, lifecycle_context)) {
    LOG(ERROR) << "Failed to write uninstall relaunch state";
    return {};
  }

  // Build command line for the relaunched process
  std::wstring command_line = L"\"" + dest_exe.value() + L"\"";
  if (!command_line_args.empty()) {
    command_line += L' ';
    command_line += command_line_args;
  }
  command_line += L" --" + base::UTF8ToWide(kSwitchUninstallState) + L"=" +
                  base::UTF8ToWide(relaunch_nonce);

  VLOG(2) << "Launching relaunched process";

  LaunchedProcess launched;
  if (!LaunchRelaunchedProcess(dest_exe, command_line, temp_dir, &launched) ||
      launched.pid == 0) {
    LOG(ERROR) << "Failed to launch relaunched process. Error: "
               << GetLastError();
    return {};
  }

  // Child process launched successfully - don't delete the temp directory
  temp_dir_deleter.Release();

  VLOG(1) << "Relaunched process started (PID=" << launched.pid << ")";

  if (lifecycle_context) {
    std::optional<std::string> handoff = SerializeInstallerRelaunchStarted(
        lifecycle_context->operation_id, launched.pid);
    const InstallerLifecycleSendStatus send_status =
        handoff ? SendRelaunchLifecycleMessage(parent_window, *handoff)
                : InstallerLifecycleSendStatus::kSerializationError;
    if (send_status != InstallerLifecycleSendStatus::kDelivered) {
      LOG(WARNING) << "Uninstall lifecycle handoff delivery was not confirmed";
    }
  }

  // Close handles - we don't need to wait for the child process
  if (launched.thread) {
    ::CloseHandle(launched.thread);
  }
  if (launched.process) {
    ::CloseHandle(launched.process);
  }

  return {.status = UninstallRelaunchStatus::kStarted,
          .child_pid = launched.pid,
          .operation_id = lifecycle_context ? lifecycle_context->operation_id
                                            : std::string()};
}

}  // namespace cef_installer
