// Copyright (c) 2025 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <windows.h>

#include <algorithm>
#include <deque>
#include <iostream>
#include <optional>
#include <string_view>

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/hash/sha1.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/process/memory.h"
#include "base/process/process_info.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "cef/include/cef_sandbox_win.h"
#include "cef/include/cef_version.h"
#include "cef/include/cef_version_info.h"
#include "cef/include/internal/cef_types.h"
#include "cef/include/wrapper/cef_certificate_util_win.h"
#include "cef/include/wrapper/cef_util_win.h"
#include "cef/libcef/browser/crashpad_runner.h"
#include "cef/libcef/browser/preferred_stack_size_win.inc"
#include "cef/libcef_dll/bootstrap/bootstrap_util_win.h"
#include "cef/libcef_dll/bootstrap/installer/installer_bootstrap_helpers.h"
#include "cef/libcef_dll/bootstrap/installer/installer_config.h"
#include "cef/libcef_dll/bootstrap/installer/installer_controller.h"
#include "cef/libcef_dll/bootstrap/installer/installer_file_ops.h"
#include "cef/libcef_dll/bootstrap/installer/installer_launch_state.h"
#include "cef/libcef_dll/bootstrap/installer/installer_lifecycle.h"
#include "cef/libcef_dll/bootstrap/installer/installer_paths.h"
#include "cef/libcef_dll/bootstrap/installer/installer_progress_dialog.h"
#include "cef/libcef_dll/bootstrap/installer/installer_relaunch.h"
#include "cef/libcef_dll/bootstrap/installer/installer_signature.h"
#include "cef/libcef_dll/bootstrap/win/resource.h"
#include "chrome/app/delay_load_failure_hook_win.h"
#include "chrome/chrome_elf/chrome_elf_main.h"
#include "chrome/install_static/initialize_from_primary_module.h"
#include "content/public/app/sandbox_helper_win.h"
#include "sandbox/policy/mojom/sandbox.mojom.h"
#include "sandbox/policy/sandbox_type.h"
#include "sandbox/win/src/sandbox.h"
#include "third_party/boringssl/src/include/openssl/sha.h"
#include "third_party/crashpad/crashpad/client/annotation.h"

namespace {

constexpr size_t kBufferSize = 64 * 1024;  // 64KB chunks

// Sets the current working directory for the process to the directory holding
// the executable if this is the browser process. This avoids leaking a handle
// to an arbitrary directory to child processes (e.g., the crashpad handler
// process).
void SetCwdForBrowserProcess() {
  if (!::IsBrowserProcess()) {
    return;
  }

  std::array<wchar_t, MAX_PATH + 1> buffer;
  buffer[0] = L'\0';
  DWORD length = ::GetModuleFileName(nullptr, &buffer[0], buffer.size());
  if (!length || length >= buffer.size()) {
    return;
  }

  base::SetCurrentDirectory(
      base::FilePath(base::FilePath::StringViewType(&buffer[0], length))
          .DirName());
}

#if DCHECK_IS_ON()
// Displays a message to the user with the error message. Used for fatal
// messages, where we close the app simultaneously. This is for developers only;
// we don't use this in circumstances (like release builds) where users could
// see it, since users don't understand these messages anyway.

// Load a string from the string table in bootstrap.rc.
std::wstring LoadString(int string_id) {
  const int kMaxSize = 100;
  TCHAR buff[kMaxSize] = {0};
  ::LoadString(::GetModuleHandle(nullptr), string_id, buff, kMaxSize);
  return buff;
}

// Replace $1-$2-$3..$9 in the format string with values from |subst|.
// Additionally, any number of consecutive '$' characters is replaced by that
// number less one. Eg $$->$, $$$->$$, etc. Supports up to 9 replacements.
std::wstring FormatErrorString(int string_id,
                               base::span<const std::u16string> subst) {
  return base::UTF16ToWide(base::ReplaceStringPlaceholders(
      base::WideToUTF16(LoadString(string_id)), subst, nullptr));
}

void ShowError(const std::wstring& error) {
  const auto subst = std::to_array<std::u16string>(
      {base::WideToUTF16(bootstrap_util::GetExePath().BaseName().value())});
  const auto& title = FormatErrorString(IDS_ERROR_TITLE, subst);
  const auto& extra_info = LoadString(IDS_ERROR_EXTRA_INFO);

#if defined(CEF_BUILD_BOOTSTRAP_CONSOLE)
  std::wcerr << title.c_str() << ": " << error << extra_info;
#else
  if (!::IsDebuggerPresent()) {
    // Displaying a dialog is unnecessary when debugging and can complicate
    // debugging.
    const std::wstring& msg = error + extra_info;
    ::MessageBox(nullptr, msg.c_str(), title.c_str(), MB_ICONERROR | MB_OK);
  }
#endif
}

#endif  // DCHECK_IS_ON()

std::wstring NormalizeError(const std::wstring& err) {
  std::wstring str = err;
  // Replace newlines.
  std::replace(str.begin(), str.end(), L'\n', L' ');
  return str;
}

// Calculate SHA1 hash of file content using 64KB chunks to avoid loading
// entire file into memory. Returns nullopt if file cannot be read.
std::optional<std::string> CalculateFileSHA1(const base::FilePath& file_path) {
  base::File file(file_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid()) {
    return std::nullopt;
  }

  size_t bytes_read = 0;
  std::vector<uint8_t> buf(kBufferSize);
  const size_t file_size = file.GetLength();
  SHA_CTX sha1_ctx;
  SHA1_Init(&sha1_ctx);
  while (bytes_read < file_size) {
    std::optional<size_t> bytes_currently_read =
        file.ReadAtCurrentPos(base::as_writable_byte_span(buf));

    if (!bytes_currently_read.has_value()) {
      return std::nullopt;
    }

    SHA1_Update(&sha1_ctx, buf.data(), bytes_currently_read.value());
    bytes_read += bytes_currently_read.value();
  }

  std::array<uint8_t, base::kSHA1Length> digest;
  SHA1_Final(digest.data(), &sha1_ctx);
  return base::HexEncode(digest);
}

// Verify DLL code signing requirements.
void CheckDllCodeSigning(
    const base::FilePath& dll_path,
    const cef_certificate_util::ThumbprintsInfo& exe_thumbprints) {
  cef_certificate_util::ThumbprintsInfo dll_thumbprints;
  cef_certificate_util::GetClientThumbprints(
      dll_path.value(), /*verify_binary=*/true, dll_thumbprints);

  // The DLL and EXE must either both be unsigned or both have all valid
  // signatures and the same primary thumbprint.
  if (!dll_thumbprints.IsSame(exe_thumbprints, /*allow_unsigned=*/true)) {
    // Some part of the certificate validation process failed.
#if DCHECK_IS_ON()
    const auto subst = std::to_array<std::u16string>(
        {base::WideToUTF16(dll_path.BaseName().value()),
         base::WideToUTF16(dll_thumbprints.errors)});
    ShowError(FormatErrorString(IDS_ERROR_INVALID_CERT, subst));
#endif
    const auto sha1 = CalculateFileSHA1(dll_path);
    if (!sha1.has_value()) {
      LOG(FATAL) << "Failed to read file: " << dll_path.value();
    } else if (dll_thumbprints.errors.empty()) {
      LOG(FATAL) << "Failed " << dll_path.value()
                 << " certificate requirements. SHA1: " << sha1.value();
    } else {
      LOG(FATAL) << "Failed " << dll_path.value() << " certificate checks: "
                 << NormalizeError(dll_thumbprints.errors)
                 << ". SHA1: " << sha1.value();
    }
  }
}

// Return true if we DON'T want to upload this flag to the crash server.
// Based on chrome/common/crash_keys.cc
bool IsBoringSwitch(const std::string& flag) {
  static const auto kIgnoreSwitches = std::to_array<std::string_view>({
      "string-annotations",
      "enable-logging",
      "flag-switches-begin",
      "flag-switches-end",
      "log-level",
      "type",
      "v",
      "vmodule",
      "gpu-preferences",
      "enable-features",
      "disable-features",
  });

  // Just about everything has this, don't bother.
  if (base::StartsWith(flag, "/prefetch:", base::CompareCase::SENSITIVE)) {
    return true;
  }

  if (!base::StartsWith(flag, "--", base::CompareCase::SENSITIVE)) {
    return false;
  }
  size_t end = flag.find("=");
  size_t len = (end == std::string::npos) ? flag.length() - 2 : end - 2;
  for (size_t i = 0; i < std::size(kIgnoreSwitches); ++i) {
    if (flag.compare(2, len, kIgnoreSwitches[i]) == 0) {
      return true;
    }
  }
  return false;
}

using SwitchesCrashKeys = std::deque<crashpad::StringAnnotation<64>>;
SwitchesCrashKeys& GetSwitchesCrashKeys() {
  static base::NoDestructor<SwitchesCrashKeys> switches_keys;
  return *switches_keys;
}

static crashpad::StringAnnotation<4> num_switches_key("bs-num-switches");

// Based on components/crash/core/common/crash_keys.cc
void SetCrashSwitchesFromCommandLine(const base::CommandLine& command_line) {
  const base::CommandLine::StringVector& argv = command_line.argv();

  // Set the number of switches in case of uninteresting switches in
  // command_line.
  num_switches_key.Set(base::NumberToString(argv.size() - 1));

  size_t key_i = 0;

  // Go through the argv, skipping the exec path. Stop if there are too many
  // switches to hold in crash keys.
  for (size_t i = 1; i < argv.size(); ++i) {
    std::string switch_str = base::WideToUTF8(argv[i]);

    // Skip uninteresting switches.
    if (IsBoringSwitch(switch_str)) {
      continue;
    }

    if (key_i >= GetSwitchesCrashKeys().size()) {
      static base::NoDestructor<std::deque<std::string>> crash_keys_names;
      crash_keys_names->emplace_back(
          base::StringPrintf("bs-switch-%" PRIuS, key_i + 1));
      GetSwitchesCrashKeys().emplace_back(crash_keys_names->back().c_str());
    }
    GetSwitchesCrashKeys()[key_i++].Set(switch_str);
  }

  // Clear any remaining switches.
  for (; key_i < GetSwitchesCrashKeys().size(); ++key_i) {
    GetSwitchesCrashKeys()[key_i].Clear();
  }
}

struct DllLoadResult {
  HMODULE module = nullptr;
  base::FilePath used_path;
};

// Creates a progress dialog and returns the progress callback that drives it.
// The caller must keep the returned unique_ptr alive for the duration of the
// operation. Returns nullptr dialog if show_progress_ui is false.
//
// The dialog is created but not shown until the progress callback receives a
// step indicating real work (CDN download or later). This avoids a brief
// dialog flash when a local/bundled CEF version resolves instantly.
std::pair<std::unique_ptr<cef_installer::ProgressDialog>,
          cef_installer::ProgressCallback>
CreateProgressUI(const cef_installer::ExtendedConfig& extended) {
  std::unique_ptr<cef_installer::ProgressDialog> dialog;
  cef_installer::ProgressCallback callback;

  if (extended.show_progress_ui) {
    dialog =
        std::make_unique<cef_installer::ProgressDialog>(extended.parent_window);
  }

  if (extended.show_progress_ui || extended.parent_window) {
    auto shown = std::make_shared<bool>(false);
    auto parent_cancel_pending = std::make_shared<bool>(false);
    callback = base::BindRepeating(
        [](cef_installer::ProgressDialog* dlg, HWND parent,
           const std::shared_ptr<bool>& shown,
           const std::shared_ptr<bool>& parent_cancel_pending,
           cef_installer::Step step, uint64_t bytes_done,
           uint64_t bytes_total) -> bool {
          const bool committing = step == cef_installer::kStepCommitting;
          if (dlg && !*shown && step >= cef_installer::kStepCdnResolve) {
            dlg->Show();
            *shown = true;
          }
          if (dlg) {
            dlg->SetCancellationDeferred(committing);
            dlg->SetStep(step);
            int percent = cef_installer::CalculateOverallProgress(
                step, bytes_done, bytes_total);
            dlg->SetProgress(percent);
            if (!committing && dlg->WasCancelled()) {
              return false;
            }
          }
          if (parent) {
            const bool continue_requested = cef_installer::SendProgressToParent(
                parent, step, bytes_done, bytes_total);
            if (committing && !continue_requested) {
              *parent_cancel_pending = true;
            } else if (!committing &&
                       (!continue_requested || *parent_cancel_pending)) {
              return false;
            }
          }
          return true;
        },
        dialog.get(), extended.parent_window, shown, parent_cancel_pending);
  }

  return {std::move(dialog), std::move(callback)};
}

// Closes the progress dialog, showing an error dialog first on failure.
void CloseProgressUI(cef_installer::ProgressDialog* dialog,
                     const cef_installer::Result& result) {
  if (!dialog) {
    return;
  }
  if (!result.success &&
      result.error_code != cef_installer::kExitCodeCancelled) {
    dialog->ShowErrorDialog(result.error_code);
  }
  dialog->Close();
}

bool IsSuccessOrNeutralExitCode(int exit_code) {
  switch (exit_code) {
    case CEF_RESULT_CODE_NORMAL_EXIT:
    case CEF_RESULT_CODE_PROFILE_IN_USE:
    case CEF_RESULT_CODE_NORMAL_EXIT_PROCESS_NOTIFIED:
    case CEF_RESULT_CODE_NORMAL_EXIT_PACK_EXTENSION_SUCCESS:
    case CEF_RESULT_CODE_NORMAL_EXIT_AUTO_DE_ELEVATED:
      return true;
    default:
      return false;
  }
}

void WriteRetentionOutput(const cef_installer::Result& result,
                          bool json_output) {
  std::string output =
      cef_installer::FormatRetentionOutput(result, json_output);
  HANDLE stdout_handle = ::GetStdHandle(STD_OUTPUT_HANDLE);
  if (stdout_handle && stdout_handle != INVALID_HANDLE_VALUE &&
      !cef_installer::WriteAllToHandle(stdout_handle, output)) {
    PLOG(ERROR) << "Failed to write retention output";
  }
}

// Run the installer for explicit commands (/cef-update, /cef-uninstall) or
// auto-install. Returns:
//   - Exit code >= 0 if the process should exit (installer ran and completed)
//   - -1 if the process should continue normally (no installer action taken)
//   - Populates |startup_state| with configured/success/failure and launch
//     health state owned through RunWinMain.
int MaybeRunInstaller(const base::CommandLine& command_line,
                      const base::FilePath& exe_path,
                      HMODULE client_dll_module,
                      cef_installer::InstallerStartupState* startup_state) {
  const bool client_dll_exists = (client_dll_module != nullptr);

  // Parse installer command from command line (/cef-update, /cef-uninstall)
  std::optional<cef_installer::Command> explicit_command =
      cef_installer::ParseInstallerCommand(command_line);
  if (cef_installer::HasConflictingInstallerCommands(command_line)) {
    LOG(ERROR) << "Conflicting explicit installer commands";
    return cef_installer::kExitCodeConfigError;
  }

  // Establish the minimal trusted relaunch context before loading config so
  // controlled child-side config failures can still report a terminal result.
  auto uninstall_context = cef_installer::ResolveUninstallInvocationContext(
      command_line, exe_path, explicit_command);
  if (!uninstall_context) {
    LOG(ERROR) << "Invalid or untrusted uninstall relaunch context";
    return cef_installer::kExitCodeConfigError;
  }
  const cef_installer::UninstallInvocationState uninstall_invocation =
      uninstall_context->invocation;
  const cef_installer::UninstallRelaunchState& trusted_uninstall_state =
      uninstall_context->relaunch_state;
  const std::optional<cef_installer::UninstallLifecycleContext>&
      lifecycle_context = trusted_uninstall_state.lifecycle_context;
  cef_installer::Config config;
  const cef_installer::ConfigLoadResult config_load =
      cef_installer::TryLoadInstallerConfig(
          client_dll_module, &config, nullptr,
          uninstall_invocation ==
              cef_installer::UninstallInvocationState::kRelaunched);
  *startup_state =
      cef_installer::MakeInstallerStartupState(config_load, config);
  const bool retention_command =
      explicit_command == cef_installer::Command::kRetentionDryRun ||
      explicit_command == cef_installer::Command::kRetentionApply;
  if (cef_installer::HasMisappliedRetentionOptions(command_line,
                                                   explicit_command)) {
    LOG(ERROR) << "cef-max-age-days is valid only with a retention command";
    return cef_installer::FinalizeControlledUninstallFailure(
        lifecycle_context,
        cef_installer::ControlledUninstallFailure::kInvalidRetentionOptions);
  }
  int retention_max_age_days = cef_installer::kDefaultRetentionMaxAgeDays;
  if (retention_command && !cef_installer::ParseRetentionMaxAgeDays(
                               command_line, &retention_max_age_days)) {
    LOG(ERROR) << "cef-max-age-days must be an integer from 90 through 3650";
    return cef_installer::FinalizeControlledUninstallFailure(
        lifecycle_context,
        cef_installer::ControlledUninstallFailure::kInvalidRetentionAge);
  }

  // All installer paths require a config. Return early if none is available.
  if (config_load.status == cef_installer::ConfigLoadStatus::kError) {
    LOG(ERROR) << startup_state->error_message;
    return cef_installer::FinalizeControlledUninstallFailure(
        lifecycle_context,
        cef_installer::ControlledUninstallFailure::kConfigLoad);
  }
  if (config_load.status == cef_installer::ConfigLoadStatus::kNotFound) {
    if (explicit_command.has_value()) {
      LOG(ERROR) << "Installer command requested but config not found";
      return cef_installer::FinalizeControlledUninstallFailure(
          lifecycle_context,
          cef_installer::ControlledUninstallFailure::kConfigNotFound);
    }
    return -1;
  }

  if (explicit_command.has_value()) {
    VLOG(1) << "Explicit installer command detected";

    // Block explicit commands when enable_explicit_modes is not set.
    // In non-official builds, allow explicit commands for development/testing.
#if defined(OFFICIAL_BUILD) && defined(NDEBUG)
    if (!config.enable_explicit_modes) {
      LOG(ERROR) << "Explicit installer commands require enable_explicit_modes";
      return cef_installer::FinalizeControlledUninstallFailure(
          lifecycle_context,
          cef_installer::ControlledUninstallFailure::kExplicitModeDisabled);
    }
#endif

    // Set lower process priority for background mode (standalone installer
    // only)
    if (command_line.HasSwitch(cef_installer::kSwitchBackground)) {
      SetPriorityClass(GetCurrentProcess(), BELOW_NORMAL_PRIORITY_CLASS);
    }
  }

  // Resolve relative paths in config against the client DLL's directory.
  // These fields are only parsed from the client DLL's embedded resource,
  // so they will be empty in standalone mode (no client DLL).
  if (client_dll_module &&
      (!config.unchecked_cef_path.empty() || !config.bundled_cef_path.empty() ||
       !config.install_path.empty())) {
    wchar_t module_path_buf[MAX_PATH];
    DWORD len =
        ::GetModuleFileNameW(client_dll_module, module_path_buf, MAX_PATH);
    base::FilePath client_dll_dir;
    if (len > 0 && len < MAX_PATH) {
      client_dll_dir = base::FilePath(module_path_buf).DirName();
    }

    if (!config.unchecked_cef_path.empty()) {
      if (client_dll_dir.empty()) {
        config.unchecked_cef_path.clear();
      } else {
        base::FilePath resolved = cef_installer::ResolvePathRelativeTo(
            config.unchecked_cef_path, client_dll_dir);
        if (resolved.empty()) {
          config.unchecked_cef_path.clear();
        } else {
          // Validate the resolved path for reparse points. Use the client
          // DLL's directory as trust anchor when the path is under it (the
          // common case, e.g., "."). For absolute paths outside the client
          // DLL's directory, use the resolved directory's parent so that at
          // minimum the directory and libcef.dll are checked.
          base::FilePath libcef_candidate =
              resolved.Append(cef_installer::kLibcefFilename);
          base::FilePath trusted_root =
              client_dll_dir.IsParent(libcef_candidate) ? client_dll_dir
                                                        : resolved.DirName();
          if (base::PathExists(libcef_candidate) &&
              !cef_installer::IsPathSafeForLoading(trusted_root,
                                                   libcef_candidate)) {
            LOG(ERROR) << "Reparse point detected in unchecked_cef_path: "
                       << libcef_candidate.value();
            config.unchecked_cef_path.clear();
          } else {
            config.unchecked_cef_path = resolved.AsUTF8Unsafe();
            VLOG(1) << "Resolved unchecked_cef_path to "
                    << config.unchecked_cef_path;
          }
        }
      }
    }

    if (!config.bundled_cef_path.empty()) {
      if (client_dll_dir.empty()) {
        config.bundled_cef_path.clear();
      } else {
        base::FilePath resolved = cef_installer::ResolvePathRelativeTo(
            config.bundled_cef_path, client_dll_dir);
        if (resolved.empty()) {
          config.bundled_cef_path.clear();
        } else {
          config.bundled_cef_path = resolved.AsUTF8Unsafe();
          VLOG(1) << "Resolved bundled_cef_path to " << config.bundled_cef_path;
        }
      }
    }

    if (!cef_installer::ResolveClientInstallPath(client_dll_module, &config)) {
      LOG(ERROR) << "Failed to resolve client-resource install_path";
    } else if (!config.install_path.empty()) {
      VLOG(1) << "Resolved install_path to " << config.install_path;
    }
  }

  // Parse extended config once, shared by all command paths below.
  cef_installer::ExtendedConfig extended;
  cef_installer::ParseExtendedConfigFromCommandLine(command_line, &extended);
  extended.retention_max_age_days = retention_max_age_days;

  // The selected application config supplies CDN URLs only when no
  // operation-specific list or local mirror was provided.
  if (extended.cdn_urls.empty() && extended.local_download_path.empty() &&
      !config.cdn_urls.empty()) {
    extended.cdn_urls = config.cdn_urls;
  }

  // Propagate bundled_cef_path from Config (client DLL resource) into
  // ExtendedConfig so the controller's version selection considers it.
  if (extended.bundled_cef_path.empty() && !config.bundled_cef_path.empty()) {
    extended.bundled_cef_path = config.bundled_cef_path;
  }
  if (extended.install_path.empty() && !config.install_path.empty()) {
    extended.install_path = config.install_path;
  }

  if (uninstall_invocation ==
      cef_installer::UninstallInvocationState::kRelaunched) {
    extended.install_path = trusted_uninstall_state.install_path;
  }

  cef_installer::ApplyConfigThumbprintOverride(config, &extended);

  if (explicit_command == cef_installer::Command::kRetentionDryRun ||
      explicit_command == cef_installer::Command::kRetentionApply) {
    // Retention is terminal maintenance. It never registers the invoking app
    // and never continues into resolved client-DLL execution.
    extended.show_progress_ui = false;
    cef_installer::Controller controller;
    cef_installer::Result result =
        controller.Run(*explicit_command, config, extended);
    WriteRetentionOutput(
        result, command_line.HasSwitch(cef_installer::kSwitchHeadless));
    return cef_installer::ResultToExitCode(result);
  }

  if (explicit_command == cef_installer::Command::kUninstall) {
    auto run_prepared_uninstall =
        [&](const cef_installer::PreparedUninstall& prepared) {
          auto [dialog, progress] = CreateProgressUI(extended);
          cef_installer::Controller controller;
          cef_installer::Result result = controller.RunPreparedUninstall(
              config, extended, prepared, progress);
          LOG(INFO) << "Uninstall completed: outcome="
                    << cef_installer::OutcomeToString(result.outcome)
                    << " success=" << result.success;
          for (const auto& warning : result.warnings) {
            LOG(WARNING) << "Uninstall warning: " << warning;
          }
          return cef_installer::FinalizeUninstallLifecycleAfterProgressUi(
              lifecycle_context, result,
              base::BindOnce(
                  [](cef_installer::ProgressDialog* dialog,
                     const cef_installer::Result* result) {
                    CloseProgressUI(dialog, *result);
                  },
                  base::Unretained(dialog.get()), base::Unretained(&result)));
        };

    // A trusted relaunched child always prepares a fresh controller snapshot
    // and runs terminally in-process. It can never launch another child.
    if (uninstall_invocation ==
        cef_installer::UninstallInvocationState::kRelaunched) {
      VLOG(1) << "Running relaunched uninstall from temp directory";
      auto preflight = cef_installer::PrepareUninstall(
          config, extended, exe_path, uninstall_invocation);
      if (!preflight.prepared ||
          preflight.decision.execution !=
              cef_installer::UninstallExecution::kInProcess) {
        LOG(ERROR) << "Relaunched uninstall preflight was rejected";
        return cef_installer::FinalizeControlledUninstallFailure(
            lifecycle_context,
            cef_installer::ControlledUninstallFailure::kPreflightRejected,
            preflight.decision.error_code);
      }
      return run_prepared_uninstall(*preflight.prepared);
    }

    // The original process resolves the mutation target once before creating
    // UI, copying files, acquiring a writer lock, or dispatching a controller.
    auto preflight = cef_installer::PrepareUninstall(config, extended, exe_path,
                                                     uninstall_invocation);
    if (!preflight.prepared || preflight.decision.execution ==
                                   cef_installer::UninstallExecution::kReject) {
      LOG(ERROR) << "Uninstall preflight failed";
      return preflight.decision.error_code;
    }
    if (preflight.decision.execution ==
        cef_installer::UninstallExecution::kRelaunch) {
      VLOG(1) << "Relaunching uninstall from a temporary directory";
      const std::wstring relaunch_args =
          command_line.GetArgumentsString() + L" /" +
          base::UTF8ToWide(cef_installer::kSwitchUninstallRelaunched);
      const cef_installer::UninstallRelaunchResult relaunch =
          cef_installer::CopySelfToTempAndRelaunch(
              exe_path,
              config_load.source ==
                      cef_installer::ConfigLoadSource::kClientResource
                  ? client_dll_module
                  : nullptr,
              relaunch_args, extended.install_path, extended.parent_window);
      if (!relaunch.started()) {
        LOG(ERROR) << "Required uninstall relaunch failed";
      }
      return cef_installer::UninstallRelaunchExitCode(relaunch.status);
    }
    return run_prepared_uninstall(*preflight.prepared);
  }

  // Handle explicit /cef-update
  if (explicit_command == cef_installer::Command::kUpdate) {
    VLOG(1) << "Running explicit update command";

    auto [dialog, progress] = CreateProgressUI(extended);
    cef_installer::Controller controller;
    cef_installer::Result result = controller.Run(
        cef_installer::Command::kUpdate, config, extended, progress);
    CloseProgressUI(dialog.get(), result);
    cef_installer::ApplyInstallerResultToStartupState(result, config, extended,
                                                      false, startup_state);

    if (result.error_code == cef_installer::kExitCodePolicyError) {
      LOG(ERROR) << result.error_message;
      return result.error_code;
    }

    if (result.success && !result.libcef_path.empty()) {
      VLOG(1) << "Update successful, libcef_path="
              << result.libcef_path.value();
    } else {
      VLOG(1) << "Update completed: success=" << result.success;
    }

    // When a client DLL is present, fall through to load it with the resolved
    // path (or empty path on failure — the client DLL handles fallback).
    // In standalone mode, exit with the installer result code.
    if (client_dll_exists) {
      return -1;
    }
    return cef_installer::ResultToExitCode(result);
  }

  // No explicit command — resolve the installed CEF version.
  // When a client DLL is present, always resolve so the path can be passed
  // via version_info. In standalone mode (no client DLL), this is auto-install
  // — gated by enable_explicit_modes in official builds to prevent a renamed
  // binary from silently downloading CEF.
  bool should_resolve = client_dll_exists;
  if (!should_resolve) {
#if defined(OFFICIAL_BUILD) && defined(NDEBUG)
    should_resolve = config.enable_explicit_modes;
#else
    should_resolve = true;
#endif
  }

  if (should_resolve) {
    VLOG(1) << "Resolving CEF version (client_dll_exists=" << client_dll_exists
            << ")";

    auto [dialog, progress] = CreateProgressUI(extended);
    cef_installer::Controller controller;
    cef_installer::Result result = controller.Run(
        cef_installer::Command::kInstall, config, extended, progress,
        cef_installer::ExecutionContext::kAutomaticStartup);
    CloseProgressUI(dialog.get(), result);
    cef_installer::ApplyInstallerResultToStartupState(result, config, extended,
                                                      true, startup_state);

    if (result.error_code == cef_installer::kExitCodePolicyError) {
      LOG(ERROR) << result.error_message;
      return result.error_code;
    }

    if (result.success && !result.libcef_path.empty()) {
      VLOG(1) << "CEF resolved, libcef_path=" << result.libcef_path.value();
    } else if (!client_dll_exists) {
      // Standalone install failed — exit with error.
      LOG(ERROR) << "Auto-install failed: " << result.error_message;
      return cef_installer::ResultToExitCode(result);
    } else {
      // Client DLL present but resolution failed — continue anyway.
      // The client DLL can fall back to its own CEF discovery.
      VLOG(1) << "CEF resolution failed, client DLL will handle fallback";
    }

    return -1;
  }

  VLOG(2) << "No installer action needed";
  return -1;
}

void SetInstallerCrashAnnotations(
    const cef_installer::InstallerStartupState& state) {
  static crashpad::StringAnnotation<16> error("bs-install-error");
  static crashpad::StringAnnotation<1024> message("bs-install-msg");
  static crashpad::StringAnnotation<64> vmin("bs-install-vmin");
  static crashpad::StringAnnotation<64> vmax("bs-install-vmax");
  static crashpad::StringAnnotation<4> policy_denied(
      "bs-install-policy-denied");
  static crashpad::StringAnnotation<4> cancelled("bs-install-cancelled");

  const auto values = cef_installer::BuildInstallerCrashAnnotationValues(state);
  if (!values.has_failure) {
    error.Clear();
    message.Clear();
    vmin.Clear();
    vmax.Clear();
    policy_denied.Clear();
    cancelled.Clear();
    return;
  }
  error.Set(values.error);
  message.Set(values.message);
  vmin.Set(values.vmin);
  vmax.Set(values.vmax);
  if (values.policy_denied.empty()) {
    policy_denied.Clear();
  } else {
    policy_denied.Set(values.policy_denied);
  }
  if (values.cancelled.empty()) {
    cancelled.Clear();
  } else {
    cancelled.Set(values.cancelled);
  }
}

constexpr DWORD kNormalLoad = 0;

template <DWORD LoadFlags>
DllLoadResult LoadClientDll(const std::wstring& dll_name,
                            const base::FilePath& exe_path) {
  DllLoadResult result;
  result.used_path = exe_path.DirName().Append(dll_name + L".dll");
  result.module = ::LoadLibraryEx(result.used_path.value().c_str(), nullptr,
                                  LoadFlags | LOAD_WITH_ALTERED_SEARCH_PATH);
  return result;
}

}  // namespace

#if defined(CEF_BUILD_BOOTSTRAP_CONSOLE)
int main(int argc, char* argv[]) {
#else   // !defined(CEF_BUILD_BOOTSTRAP_CONSOLE)
// Entry point function for all processes.
int APIENTRY wWinMain(HINSTANCE hInstance,
                      HINSTANCE hPrevInstance,
                      LPTSTR lpCmdLine,
                      int nCmdShow) {
  UNREFERENCED_PARAMETER(hPrevInstance);
  UNREFERENCED_PARAMETER(lpCmdLine);
#endif  // !defined(CEF_BUILD_BOOTSTRAP_CONSOLE)

#if defined(ARCH_CPU_32_BITS)
  // Run the main thread on 32-bit Windows using a fiber with the preferred 4MiB
  // stack size. This function must be called at the top of the executable entry
  // point function (`main()` or `wWinMain()`). It is used in combination with
  // the initial stack size of 0.5MiB configured via the `/STACK:0x80000` linker
  // flag on executable targets. This saves significant memory on threads (like
  // those in the Windows thread pool, and others) whose stack size can only be
  // controlled via the linker flag.
#if defined(CEF_BUILD_BOOTSTRAP_CONSOLE)
  int fiber_exit_code = CefRunMainWithPreferredStackSize(main, argc, argv);
#else
  int fiber_exit_code = CefRunWinMainWithPreferredStackSize(
      wWinMain, hInstance, lpCmdLine, nCmdShow);
#endif
  if (fiber_exit_code >= 0) {
    // The fiber has completed so return here.
    return fiber_exit_code;
  }
#endif

  SetCwdForBrowserProcess();
  install_static::InitializeFromPrimaryModule();
  SignalInitializeCrashReporting();
  if (IsBrowserProcess()) {
    DisableDelayLoadFailureHooksForMainExecutable();
  }

  // Done here to ensure that OOMs that happen early in process initialization
  // are correctly signaled to the OS.
  base::EnableTerminationOnOutOfMemory();
  logging::RegisterAbslAbortHook();

  // Parse command-line arguments.
  const base::CommandLine command_line =
      base::CommandLine::FromString(::GetCommandLineW());

  constexpr char kProcessType[] = "type";
  const bool is_subprocess = command_line.HasSwitch(kProcessType);
  const std::string& process_type =
      command_line.GetSwitchValueASCII(kProcessType);
  if (is_subprocess && process_type.empty()) {
    // Early exit on invalid process type.
    return CEF_RESULT_CODE_BAD_PROCESS_TYPE;
  }

  // Enable VLOG output based on --v flag. The bootstrap doesn't call
  // CommandLine::Init(), so the logging system can't read verbosity from the
  // global singleton. Apply it manually.
  if (command_line.HasSwitch("v")) {
    int verbose_level = 0;
    if (base::StringToInt(command_line.GetSwitchValueASCII("v"),
                          &verbose_level) &&
        verbose_level > 0) {
      logging::SetMinLogLevel(-verbose_level);
    }
  }

  // Run the crashpad handler now instead of waiting for libcef to load.
  constexpr char kCrashpadHandler[] = "crashpad-handler";
  if (process_type == kCrashpadHandler) {
    return crashpad_runner::RunAsCrashpadHandler(command_line);
  }

  // Include version info in crash reports.
  static crashpad::StringAnnotation<64> bs_version("bs-version");
  bs_version.Set(CEF_VERSION);

  // Include command-line switches in crash reports.
  SetCrashSwitchesFromCommandLine(command_line);

  // IsUnsandboxedSandboxType() can't be used here because its result can be
  // gated behind a feature flag, which are not yet initialized.
  // Match the logic in MainDllLoader::Launch.
  const bool is_sandboxed =
      sandbox::policy::SandboxTypeFromCommandLine(command_line) !=
      sandbox::mojom::Sandbox::kNoSandbox;

  std::wstring dll_name;
  base::FilePath exe_path;
  cef_certificate_util::ThumbprintsInfo exe_thumbprints;
  // Owns all installer strings and launch-health data through RunWinMain.
  cef_installer::InstallerStartupState installer_state;

  if (is_sandboxed) {
    // Running as a sandboxed sub-process. May already be locked down, so we
    // can't call WinAPI functions. The command-line will already have been
    // validated in ChromeContentBrowserClientCef::
    // AppendExtraCommandLineSwitches. Retrieve the module value without
    // additional validation.
    dll_name = bootstrap_util::GetModuleValue(command_line);
    if (dll_name.empty()) {
      // Default to the command-line program name without extension.
      dll_name = command_line.GetProgram().BaseName().RemoveExtension().value();
    }
  } else {
    // Running as the main process or unsandboxed sub-process.
    exe_path = bootstrap_util::GetExePath();

    // Retrieve the module name with validation.
    dll_name = bootstrap_util::GetValidatedModuleValue(command_line, exe_path);
    if (dll_name.empty()) {
      // Default to the executable module file name without extension. This is
      // safer than relying on the command-line program name.
      dll_name = bootstrap_util::GetDefaultModuleValue(exe_path);
    }

    if (bootstrap_util::IsDefaultExeName(dll_name)) {
#if DCHECK_IS_ON()
      ShowError(LoadString(IDS_ERROR_NO_MODULE_NAME));
#endif
      LOG(FATAL) << "Missing module name";
    }
  }

  // Subprocesses skip certificate validation, installer logic, and the
  // untrusted DLL pre-load. The browser process already verified EXE and
  // DLL signatures before launching child processes, and propagated the
  // validated module name via AppendExtraCommandLineSwitches. Repeating
  // three WinVerifyTrust calls per subprocess is redundant and expensive
  // (~30-300 ms).
  if (!is_subprocess) {
    cef_certificate_util::GetClientThumbprints(
        exe_path.value(), /*verify_binary=*/true, exe_thumbprints);

    // The executable must either be unsigned or have all valid signatures.
    if (!exe_thumbprints.IsUnsignedOrValid()) {
      // Some part of the certificate validation process failed.
#if DCHECK_IS_ON()
      const auto subst = std::to_array<std::u16string>(
          {base::WideToUTF16(exe_path.BaseName().value()),
           base::WideToUTF16(exe_thumbprints.errors)});
      ShowError(FormatErrorString(IDS_ERROR_INVALID_CERT, subst));
#endif
      if (exe_thumbprints.errors.empty()) {
        LOG(FATAL) << "Failed " << exe_path.value()
                   << " certificate requirements";
      } else {
        LOG(FATAL) << "Failed " << exe_path.value() << " certificate checks: "
                   << NormalizeError(exe_thumbprints.errors);
      }
    }

    // Check chrome_elf.dll which should be preloaded to support crash
    // reporting.
    if (HMODULE hModule = ::LoadLibrary(L"chrome_elf")) {
      const auto& dll_path = bootstrap_util::GetModulePath(hModule);

      // Must be in the same directory as the EXE.
      if (dll_path.DirName() != exe_path.DirName()) {
#if DCHECK_IS_ON()
        const auto subst = std::to_array<std::u16string>({u"chrome_elf"});
        ShowError(FormatErrorString(IDS_ERROR_INVALID_LOCATION, subst));
#endif
        LOG(FATAL) << "Invalid location: " << dll_path.value();
      }

      CheckDllCodeSigning(dll_path, exe_thumbprints);

      FreeLibrary(hModule);
    } else {
      LOG(FATAL) << "Failed to load chrome_elf.dll with error "
                 << ::GetLastError();
    }

    // Load the client DLL as untrusted (e.g. without executing DllMain or
    // loading additional modules) so that we can first check requirements.
    // LoadLibrary's "default search order" is tricky and we don't want to
    // guess about what DLL it will load. DONT_RESOLVE_DLL_REFERENCES is the
    // only option that doesn't execute DllMain while still allowing us
    // retrieve the path using GetModuleFileName. No execution of the DLL
    // should be attempted while loaded in this mode.
    const auto untrusted_result =
        LoadClientDll<DONT_RESOLVE_DLL_REFERENCES>(dll_name, exe_path);
    const bool client_dll_exists = (untrusted_result.module != nullptr);

    if (client_dll_exists) {
      const auto& dll_path =
          bootstrap_util::GetModulePath(untrusted_result.module);

      if (!bootstrap_util::IsModulePathAllowed(dll_path, exe_path)) {
#if DCHECK_IS_ON()
        const auto subst =
            std::to_array<std::u16string>({base::WideToUTF16(dll_name)});
        ShowError(FormatErrorString(IDS_ERROR_INVALID_LOCATION, subst));
#endif
        LOG(FATAL) << "Invalid location: " << dll_path.value();
      }

      CheckDllCodeSigning(dll_path, exe_thumbprints);
    }

    // Check if installer should run (explicit command or auto-install).
    // Pass the untrusted module handle so config can be loaded from client DLL.
    int installer_exit = MaybeRunInstaller(
        command_line, exe_path, untrusted_result.module, &installer_state);
    SetInstallerCrashAnnotations(installer_state);

    // Now we can free the untrusted module.
    if (untrusted_result.module) {
      FreeLibrary(untrusted_result.module);
    }

    if (installer_exit >= 0) {
      // Installer ran and process should exit.
      return installer_exit;
    }

    // In standalone mode (no client DLL), exit after installer completes.
    // There is no client DLL to hand off to.
    if (!client_dll_exists && !installer_state.libcef_path.empty()) {
      return CEF_RESULT_CODE_NORMAL_EXIT;
    }

    // If client DLL doesn't exist and installer didn't provide a path, fail.
    if (!client_dll_exists && installer_state.libcef_path.empty()) {
      const DWORD load_error = ::GetLastError();
#if DCHECK_IS_ON()
      const auto subst = std::to_array<std::u16string>(
          {base::WideToUTF16(dll_name),
           base::WideToUTF16(cef_util::GetLastErrorAsString())});
      ShowError(FormatErrorString(IDS_ERROR_LOAD_FAILED, subst));
#endif
      const auto sha1 = CalculateFileSHA1(untrusted_result.used_path);
      if (!sha1.has_value()) {
        LOG(FATAL) << "Failed to read file: "
                   << untrusted_result.used_path.value()
                   << " with error: " << load_error;
      } else {
        LOG(FATAL) << "Failed to load " << untrusted_result.used_path.value()
                   << " with error: " << load_error
                   << " SHA1: " << sha1.value();
      }
    }
  }

#if defined(CEF_BUILD_BOOTSTRAP_CONSOLE)
  constexpr char kProcName[] = "RunConsoleMain";
  using kProcType = decltype(&RunConsoleMain);
#else
  constexpr char kProcName[] = "RunWinMain";
  using kProcType = decltype(&RunWinMain);
#endif

  const auto result = LoadClientDll<kNormalLoad>(dll_name, exe_path);
  if (result.module) {
    if (auto* pFunc = (kProcType)::GetProcAddress(result.module, kProcName)) {
      // Initialize the sandbox services.
      // Match the logic in MainDllLoader::Launch.
      sandbox::SandboxInterfaceInfo sandbox_info = {nullptr};
      if (!is_subprocess || is_sandboxed) {
        // For child processes that are running as --no-sandbox, don't
        // initialize the sandbox info, otherwise they'll be treated as brokers
        // (as if they were the browser).
        content::InitializeSandboxInfo(
            &sandbox_info, IsExtensionPointDisableSet()
                               ? sandbox::MITIGATION_EXTENSION_POINT_DISABLE
                               : 0);
      }

      cef_version_info_t version_info = {};
      CEF_POPULATE_VERSION_INFO(&version_info);

      cef_installer::PopulateInstallerVersionInfo(installer_state,
                                                  &version_info);

      cef_installer::LaunchState sentinel;

      // Canonical retention liveness is independent of launch-health mode.
      // Publish it before the health sentinel and before client execution.
      if (!installer_state.liveness_path.empty() &&
          !cef_installer::RefreshLivenessPath(
              installer_state.liveness_path, installer_state.appid,
              cef_installer::GetCurrentPlatform(),
              cef_installer::GetCurrentWallTime())) {
        LOG(WARNING) << "Failed to refresh installer liveness record: "
                     << installer_state.liveness_path.value();
      }

      // Write launch health sentinel before RunWinMain (main process only).
      if (!installer_state.launch_state_path.empty()) {
        sentinel.appid = installer_state.appid;
        sentinel.pid = GetCurrentProcessId();
        sentinel.pid_start_time = cef_installer::GetCurrentPidStartTime();
        sentinel.consecutive_failures =
            installer_state.launch_consecutive_failures;
        sentinel.running = true;
        sentinel.confirmed = false;
        sentinel.version = installer_state.launch_version;
        sentinel.platform = installer_state.launch_platform;

        if (cef_installer::WriteLaunchStatePath(
                installer_state.launch_state_path, sentinel)) {
          // Publish the active sentinel path so an in-process
          // RunInstaller("launch_success") call (HandleLaunchSuccess) can
          // resolve and confirm this launch.
          cef_installer::SetActiveLaunchStatePath(
              installer_state.launch_state_path);
        } else {
          LOG(ERROR) << "Launch-health sentinel write failed; launching "
                        "untracked (appid="
                     << installer_state.appid
                     << ", path=" << installer_state.launch_state_path.value()
                     << ", mode="
                     << cef_installer::LaunchHealthModeToString(
                            installer_state.launch_health)
                     << ")";
          installer_state.launch_state_path.clear();
        }
      }

      // Call RunWinMain and capture the exit code.
      // Don't call FreeLibrary() to avoid an illegal access during shutdown.
      // The sandbox broker owns objects created inside libcef.dll
      // (SandboxWin::InitBrokerServices) and cleanup is triggered via an
      // _onexit handler (SingletonBase::OnExit) called after wWinMain exits.
#if defined(CEF_BUILD_BOOTSTRAP_CONSOLE)
      int exit_code = pFunc(argc, argv, &sandbox_info, &version_info);
#else
      int exit_code =
          pFunc(hInstance, lpCmdLine, nCmdShow, &sandbox_info, &version_info);
#endif

      // Client loading/execution has completed. Release the shared-store lease
      // before any post-exit pruning so deferred removal can now proceed.
      installer_state.version_lease.reset();

      // Post-RunWinMain: update launch state based on exit code.
      if (!installer_state.launch_state_path.empty()) {
        auto ls = cef_installer::ReadLaunchStatePath(
            installer_state.launch_state_path);
        bool is_ours = ls && ls->pid == sentinel.pid &&
                       ls->pid_start_time == sentinel.pid_start_time;

        if (is_ours) {
          // Compose with kLaunchSuccess: if the client already confirmed this
          // launch (running==false), the sentinel is already durable — skip
          // the write entirely (no-op compose path).
          sentinel = *ls;
          const bool normal_exit = exit_code == CEF_RESULT_CODE_NORMAL_EXIT;
          const bool neutral_exit =
              !normal_exit && IsSuccessOrNeutralExitCode(exit_code);
          bool launch_state_persisted = true;
          if (cef_installer::ApplyLaunchExit(installer_state.launch_health,
                                             normal_exit, neutral_exit,
                                             &sentinel)) {
            launch_state_persisted = cef_installer::WriteLaunchStatePath(
                installer_state.launch_state_path, sentinel);
            if (!launch_state_persisted) {
              LOG(ERROR) << "Failed to persist launch-health exit state (path="
                         << installer_state.launch_state_path.value()
                         << ", mode="
                         << cef_installer::LaunchHealthModeToString(
                                installer_state.launch_health)
                         << ")";
            }
          }

          // Pruning runs only on a clean exit (code 0). Neutral exits (notably
          // "profile in use") can imply a concurrent live instance with
          // libcef.dll loaded, or an imminent relaunch — both violate the "no
          // in-use file conflicts" premise that justifies post-exit pruning.
          // The next clean exit (or a standalone /cef-update) still prunes.
          if (exit_code == CEF_RESULT_CODE_NORMAL_EXIT) {
            // Keep older confirmed launch records, and therefore their
            // fallback versions, until this launch is durably confirmed. In
            // explicit mode an ordinary clean exit is neutral.
            if (launch_state_persisted && !sentinel.running &&
                sentinel.confirmed && sentinel.consecutive_failures == 0) {
              for (const auto& path : installer_state.launch_cleanup_paths) {
                base::DeleteFile(path);
              }
            }

            // Post-exit pruning: run with short timeout to avoid blocking exit.
            cef_installer::ExtendedConfig prune_ext = installer_state.extended;
            prune_ext.lock_timeout_ms = 1;
            prune_ext.show_progress_ui = false;
            cef_installer::Controller prune_controller;
            prune_controller.Run(cef_installer::Command::kPrune,
                                 installer_state.config, prune_ext);
          }
        }
      }

      // Off mode has no sentinel, but clean-exit pruning remains active.
      if (installer_state.launch_health ==
              cef_installer::LaunchHealthMode::kOff &&
          exit_code == CEF_RESULT_CODE_NORMAL_EXIT &&
          installer_state.configured) {
        cef_installer::ExtendedConfig prune_ext = installer_state.extended;
        prune_ext.lock_timeout_ms = 1;
        prune_ext.show_progress_ui = false;
        cef_installer::Controller prune_controller;
        prune_controller.Run(cef_installer::Command::kPrune,
                             installer_state.config, prune_ext);
      }

      return exit_code;
    } else {
      // Save the last error before calling any other function
      const DWORD proc_error = ::GetLastError();
#if DCHECK_IS_ON()
      if (!is_sandboxed) {
        const auto subst = std::to_array<std::u16string>(
            {base::WideToUTF16(dll_name),
             base::WideToUTF16(cef_util::GetLastErrorAsString()),
             base::ASCIIToUTF16(std::string(kProcName))});
        ShowError(FormatErrorString(IDS_ERROR_NO_PROC_EXPORT, subst));
      }
#endif
      const auto sha1 = CalculateFileSHA1(result.used_path);
      if (!sha1.has_value()) {
        LOG(FATAL) << "Failed to read file: " << result.used_path.value()
                   << " with error: " << proc_error;
      } else {
        LOG(FATAL) << "Failed to find " << kProcName << " in "
                   << result.used_path.value() << " with error: " << proc_error
                   << " SHA1: " << sha1.value();
      }
    }
  } else {
    // Save the last error before calling any other function
    const DWORD load_error = ::GetLastError();
#if DCHECK_IS_ON()
    if (!is_sandboxed) {
      const auto subst = std::to_array<std::u16string>(
          {base::WideToUTF16(dll_name),
           base::WideToUTF16(cef_util::GetLastErrorAsString())});
      ShowError(FormatErrorString(IDS_ERROR_LOAD_FAILED, subst));
    }
#endif
    const auto sha1 = CalculateFileSHA1(result.used_path);
    if (!sha1.has_value()) {
      LOG(FATAL) << "Failed to read file: " << result.used_path.value()
                 << " with error: " << load_error;
    } else {
      LOG(FATAL) << "Failed to load " << result.used_path.value()
                 << " with error: " << load_error << " SHA1: " << sha1.value();
    }
  }

  // LOG(FATAL) is [[noreturn]], so we never reach this point.
  NOTREACHED();
}

// Exported by bootstrap.exe and called by the client dll via cef_logging.cc.
// Keep the implementation synchronized with base/logging.cc.
extern "C" __declspec(dllexport) void SetLogFatalCrashKey(const char* file,
                                                          int line,
                                                          const char* message) {
  // In case of an out-of-memory condition, this code could be reentered when
  // constructing and storing the key. Using a static is not thread-safe, but if
  // multiple threads are in the process of a fatal crash at the same time, this
  // should work.
  static bool guarded = false;
  if (guarded) {
    return;
  }

  base::AutoReset<bool> guard(&guarded, true);

  // Only log last path component.
  if (file) {
    const char* slash = UNSAFE_TODO(strrchr(file, '\\'));
    if (!slash) {
      // Some builds may use forward slashes instead.
      slash = UNSAFE_TODO(strrchr(file, '/'));
    }
    if (slash) {
      file = UNSAFE_TODO(slash + 1);
    }
  }

  auto value = base::StringPrintf("%s:%d: %s", file, line, message);
  if (value.back() == '\n') {
    value.pop_back();
  }

  // Note that we intentionally use LOG_FATAL here (old name for LOGGING_FATAL)
  // as that's understood and used by the crash backend.
  // Using the Crashpad API directly here because base::debug::*CrashKeyString()
  // doesn't appear to work prior to Chromium initialization.
  static crashpad::StringAnnotation<1024> log_fatal("LOG_FATAL");
  log_fatal.Set(value);
}
