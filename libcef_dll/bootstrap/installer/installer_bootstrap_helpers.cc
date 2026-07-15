// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef_dll/bootstrap/installer/installer_bootstrap_helpers.h"

#include <algorithm>
#include <limits>
#include <utility>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "cef/libcef_dll/bootstrap/installer/installer_config.h"
#include "cef/libcef_dll/bootstrap/installer/installer_e2e_config.h"
#include "cef/libcef_dll/bootstrap/installer/installer_launch_state.h"
#include "cef/libcef_dll/bootstrap/installer/installer_logger.h"
#include "cef/libcef_dll/bootstrap/installer/installer_paths.h"

namespace cef_installer {

std::optional<Command> ParseInstallerCommand(
    const base::CommandLine& command_line) {
  // Check for /cef-update or --cef-update
  if (command_line.HasSwitch(kSwitchUpdate)) {
    return Command::kUpdate;
  }

  // Check for /cef-uninstall or --cef-uninstall
  if (command_line.HasSwitch(kSwitchUninstall)) {
    return Command::kUninstall;
  }

  if (command_line.HasSwitch(kSwitchRetentionDryRun)) {
    return Command::kRetentionDryRun;
  }

  if (command_line.HasSwitch(kSwitchRetentionApply)) {
    return Command::kRetentionApply;
  }

  // No installer command present
  return std::nullopt;
}

bool HasConflictingInstallerCommands(const base::CommandLine& command_line) {
  int count = 0;
  for (const char* name : {kSwitchUpdate, kSwitchUninstall,
                           kSwitchRetentionDryRun, kSwitchRetentionApply}) {
    if (command_line.HasSwitch(name)) {
      ++count;
    }
  }
  return count > 1;
}

bool HasMisappliedRetentionOptions(const base::CommandLine& command_line,
                                   std::optional<Command> command) {
  const bool retention_command = command == Command::kRetentionDryRun ||
                                 command == Command::kRetentionApply;
  return command_line.HasSwitch(kSwitchRetentionMaxAgeDays) &&
         !retention_command;
}

bool ParseRetentionMaxAgeDays(const base::CommandLine& command_line,
                              int* max_age_days) {
  if (!max_age_days) {
    return false;
  }
  *max_age_days = kDefaultRetentionMaxAgeDays;
  if (!command_line.HasSwitch(kSwitchRetentionMaxAgeDays)) {
    return true;
  }
  int value = 0;
  if (!base::StringToInt(
          command_line.GetSwitchValueASCII(kSwitchRetentionMaxAgeDays),
          &value) ||
      !IsValidRetentionMaxAgeDays(value)) {
    return false;
  }
  *max_age_days = value;
  return true;
}

bool IsUninstallRelaunched(const base::CommandLine& command_line) {
  return command_line.HasSwitch(kSwitchUninstallRelaunched);
}

bool ParseParentWindowSwitch(const base::CommandLine& command_line,
                             HWND* parent_window) {
  if (!parent_window || !command_line.HasSwitch(kSwitchParentWindow)) {
    return false;
  }
  const std::string& hwnd_str =
      command_line.GetSwitchValueASCII(kSwitchParentWindow);
  uint64_t hwnd_value = 0;
  if (!base::StringToUint64(hwnd_str, &hwnd_value) || hwnd_value == 0 ||
      hwnd_value > std::numeric_limits<uintptr_t>::max()) {
    return false;
  }
  *parent_window = reinterpret_cast<HWND>(static_cast<uintptr_t>(hwnd_value));
  return true;
}

void ParseExtendedConfigFromCommandLine(const base::CommandLine& command_line,
                                        ExtendedConfig* extended_config) {
  if (!extended_config) {
    return;
  }

  // /cef-forcecheck - force version check even if recently checked
  if (command_line.HasSwitch(kSwitchForceCheck)) {
    extended_config->force_check = true;
    VLOG(2) << "Extended config: force_check=true";
  }

  // /cef-background selects low-impact extraction and also suppresses UI.
  // /cef-headless suppresses UI without changing extraction behavior.
  extended_config->background_mode = command_line.HasSwitch(kSwitchBackground);
  if (extended_config->background_mode ||
      command_line.HasSwitch(kSwitchHeadless)) {
    extended_config->show_progress_ui = false;
    VLOG(2) << "Extended config: show_progress_ui=false";
  }

  // /cef-parent=<hwnd> - parent window handle for progress UI
  if (command_line.HasSwitch(kSwitchParentWindow)) {
    if (ParseParentWindowSwitch(command_line,
                                &extended_config->parent_window)) {
      VLOG(2) << "Extended config: valid parent window supplied";
    } else {
      LOG(WARNING) << "Invalid parent window handle";
    }
  }

  // /cef-download-path=<path> - read from local directory instead of CDN
  if (command_line.HasSwitch(kSwitchDownloadPath)) {
    std::string path = command_line.GetSwitchValueASCII(kSwitchDownloadPath);
    if (!path.empty()) {
      extended_config->local_download_path = path;
      VLOG(2) << "Extended config: local_download_path=" << path;
    } else {
      LOG(WARNING) << "/cef-download-path requires a path value";
    }
  }

#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
  // /cef-install-path=<path> - override install directory
  if (command_line.HasSwitch(kSwitchInstallPath)) {
    std::string path = command_line.GetSwitchValueASCII(kSwitchInstallPath);
    if (!path.empty()) {
      extended_config->install_path = path;
      VLOG(2) << "Extended config: install_path=" << path;
    } else {
      LOG(WARNING) << "/cef-install-path requires a path value";
    }
  }
#endif

  // /cef-log-level=<level> - set minimum log level (info, warning, error)
  if (command_line.HasSwitch(kSwitchLogLevel)) {
    std::string level_str = command_line.GetSwitchValueASCII(kSwitchLogLevel);
    auto level = LogLevelFromString(level_str);
    if (level.has_value()) {
      extended_config->log_level = level.value();
      VLOG(2) << "Extended config: log_level=" << level_str;
    } else {
      LOG(WARNING) << "/cef-log-level invalid value: " << level_str
                   << " (expected: info, warning, error)";
    }
  }
}

std::optional<Command> CommandFromString(const std::string& str) {
  std::string lower = base::ToLowerASCII(str);

  if (lower == "install") {
    return Command::kInstall;
  }
  if (lower == "update") {
    return Command::kUpdate;
  }
  if (lower == "uninstall") {
    return Command::kUninstall;
  }
  if (lower == "query") {
    return Command::kQuery;
  }
  if (lower == "prune") {
    return Command::kPrune;
  }

  return std::nullopt;
}

ConfigLoadResult TryLoadInstallerConfig(HMODULE client_dll_module,
                                        Config* config,
                                        HMODULE bootstrap_module,
                                        bool trusted_uninstall_relaunch) {
  auto error_result = [](ConfigError error, const std::string& source,
                         const std::string& diagnostic) {
    return ConfigLoadResult{
        .status = ConfigLoadStatus::kError,
        .error = error,
        .diagnostic =
            source + ": " +
            (diagnostic.empty() ? ConfigErrorToString(error) : diagnostic)};
  };
  if (!config) {
    return error_result(ConfigError::kMissingRequiredField, "Installer config",
                        "Config output is null");
  }
  if (trusted_uninstall_relaunch &&
      internal::GetInstallerE2EConfig().child_config_failure) {
    return error_result(ConfigError::kJsonParseError, "Installer config",
                        "Forced child config-load failure for E2E testing");
  }

  if (!bootstrap_module) {
    bootstrap_module = ::GetModuleHandle(nullptr);
  }

  // When client DLL is present, use its config (or fall back to bootstrap).
  if (client_dll_module) {
    std::string client_diagnostic;
    ConfigError client_error =
        ReadConfigFromResource(client_dll_module, config,
                               {.allow_unchecked_cef_path = true,
                                .allow_bundled_cef_path = true,
                                .allow_install_path = true},
                               &client_diagnostic);
    if (client_error != ConfigError::kSuccess &&
        client_error != ConfigError::kResourceNotFound) {
      return error_result(client_error, "Client DLL resource",
                          client_diagnostic);
    }

    // Read enable_explicit_modes from the bootstrap exe's own resource.
    // This flag is only authoritative in the exe; the client DLL cannot set
    // it, and it does not change application-config provenance.
    Config bootstrap_config;
    std::string bootstrap_diagnostic;
    ConfigError bootstrap_error = ReadConfigFromResource(
        bootstrap_module, &bootstrap_config,
        {.allow_enable_explicit_modes = true}, &bootstrap_diagnostic);
    if (bootstrap_error != ConfigError::kSuccess &&
        bootstrap_error != ConfigError::kResourceNotFound) {
      return error_result(bootstrap_error, "Bootstrap resource",
                          bootstrap_diagnostic);
    }

    if (client_error == ConfigError::kSuccess) {
      VLOG(1) << "Installer config loaded from client DLL resource (appid="
              << config->appid << ", vmin=" << config->vmin << ")";
    } else {
      VLOG(2) << "Client DLL resource config not found";

      if (bootstrap_error == ConfigError::kSuccess) {
        *config = bootstrap_config;
        VLOG(1) << "Installer config loaded from bootstrap resource (appid="
                << config->appid << ", vmin=" << config->vmin << ")";
        return {.status = ConfigLoadStatus::kLoaded,
                .source = ConfigLoadSource::kBootstrapResource};
      }

      VLOG(1) << "No installer config found";
      return {.status = ConfigLoadStatus::kNotFound,
              .error = ConfigError::kResourceNotFound,
              .diagnostic = "No embedded installer config resource found"};
    }

    if (bootstrap_error == ConfigError::kSuccess) {
      config->enable_explicit_modes = bootstrap_config.enable_explicit_modes;
    }

    return {.status = ConfigLoadStatus::kLoaded,
            .source = ConfigLoadSource::kClientResource};
  }

  // Standalone mode (no client DLL).
  std::string bootstrap_diagnostic;
  ConfigError bootstrap_error = ReadConfigFromResource(
      bootstrap_module, config, {.allow_enable_explicit_modes = true},
      &bootstrap_diagnostic);
  if (bootstrap_error == ConfigError::kSuccess) {
    VLOG(1) << "Installer config loaded from bootstrap resource (appid="
            << config->appid << ", vmin=" << config->vmin << ")";
    return {.status = ConfigLoadStatus::kLoaded,
            .source = ConfigLoadSource::kBootstrapResource};
  }
  if (bootstrap_error != ConfigError::kResourceNotFound) {
    return error_result(bootstrap_error, "Bootstrap resource",
                        bootstrap_diagnostic);
  }

  VLOG(1) << "No installer config found";
  return {.status = ConfigLoadStatus::kNotFound,
          .error = ConfigError::kResourceNotFound,
          .diagnostic = "No embedded installer config resource found"};
}

bool ResolveClientInstallPath(HMODULE client_dll_module, Config* config) {
  if (!config) {
    return false;
  }
  if (config->install_path.empty()) {
    return true;
  }
  if (!client_dll_module) {
    return false;
  }

  wchar_t module_path[MAX_PATH];
  const DWORD len =
      ::GetModuleFileNameW(client_dll_module, module_path, MAX_PATH);
  if (len == 0 || len >= MAX_PATH) {
    return false;
  }
  const base::FilePath resolved = ResolvePathRelativeTo(
      config->install_path, base::FilePath(module_path).DirName());
  if (resolved.empty()) {
    return false;
  }
  config->install_path = resolved.AsUTF8Unsafe();
  return true;
}

InstallerStartupState MakeInstallerStartupState(
    const ConfigLoadResult& load_result,
    const Config& config) {
  InstallerStartupState state;
  state.configured = load_result.status == ConfigLoadStatus::kLoaded;
  if (state.configured) {
    state.effective_vmin = config.vmin;
    state.effective_vmax = config.vmax;
  } else if (load_result.status == ConfigLoadStatus::kError) {
    state.error_code = kExitCodeConfigError;
    state.error_message = load_result.diagnostic;
  }
  return state;
}

void ApplyInstallerResultToStartupState(const Result& result,
                                        const Config& config,
                                        const ExtendedConfig& extended,
                                        bool apply_launch_health,
                                        InstallerStartupState* state) {
  if (!state) {
    return;
  }
  state->configured = true;
  state->effective_vmin = config.vmin;
  state->effective_vmax = config.vmax;
  state->config = config;
  state->extended = extended;
  state->launch_health = config.launch_health;
  state->launch_state_path.clear();
  state->launch_consecutive_failures = 0;
  state->launch_version.clear();
  state->launch_platform.clear();
  state->launch_cleanup_paths.clear();
  state->liveness_path.clear();
  state->appid.clear();
  state->appid_hash.clear();
  state->version_lease.reset();
  if (result.success) {
    state->libcef_path = result.libcef_path;
    state->is_bundled = result.is_bundled;
    state->version_full = result.version_full;
    state->error_code = kExitCodeSuccess;
    state->error_message.clear();
    state->version_lease = result.version_lease;
    state->liveness_path = result.liveness_path;
    if (apply_launch_health && !result.libcef_path.empty()) {
      state->launch_state_path = result.launch_state_path;
      state->launch_consecutive_failures = result.launch_consecutive_failures;
      state->launch_version = result.launch_version;
      state->launch_platform = result.launch_platform;
      state->launch_cleanup_paths = result.launch_cleanup_paths;
      state->appid = config.appid;
      state->appid_hash = GetAppidHash(config.appid);
    }
  } else {
    state->libcef_path.clear();
    state->is_bundled = false;
    state->version_full.clear();
    state->error_code = result.error_code;
    state->error_message = result.error_message;
  }
}

void PopulateInstallerVersionInfo(const InstallerStartupState& state,
                                  cef_version_info_t* info) {
  if (!info) {
    return;
  }
#if CEF_API_ADDED(15101)
  if (info->size >= CEF_VERSION_INFO_SIZE_WITH_INSTALLER_ERROR) {
    info->libcef_path =
        state.libcef_path.empty() ? nullptr : state.libcef_path.value().c_str();
    info->libcef_is_bundled = state.is_bundled;
    info->libcef_version_full =
        state.version_full.empty() ? nullptr : state.version_full.c_str();
    info->installer_error_code = state.error_code;
    info->installer_error_message =
        state.error_message.empty() ? nullptr : state.error_message.c_str();
  }
#endif
}

InstallerCrashAnnotationValues BuildInstallerCrashAnnotationValues(
    const InstallerStartupState& state) {
  InstallerCrashAnnotationValues values;
  if (state.error_code == kExitCodeSuccess) {
    return values;
  }
  values.has_failure = true;
  values.error = base::NumberToString(state.error_code);
  values.message = state.error_message;
  values.vmin = state.effective_vmin;
  values.vmax =
      state.effective_vmax.empty() ? "unbounded" : state.effective_vmax;
  if (state.error_code == kExitCodePolicyDenied) {
    values.policy_denied = "1";
  }
  if (state.error_code == kExitCodeCancelled) {
    values.cancelled = "1";
  }
  return values;
}

namespace internal {
namespace {

bool WriteAllToHandleImpl(HANDLE handle,
                          std::string_view data,
                          DWORD max_chunk_size,
                          HandleWriteCallback write) {
  if (!handle || handle == INVALID_HANDLE_VALUE) {
    ::SetLastError(ERROR_INVALID_HANDLE);
    return false;
  }
  if (max_chunk_size == 0 || write.is_null()) {
    ::SetLastError(ERROR_INVALID_PARAMETER);
    return false;
  }
  while (!data.empty()) {
    const DWORD chunk_size = static_cast<DWORD>(
        std::min(data.size(), static_cast<size_t>(max_chunk_size)));
    DWORD written = 0;
    if (!write.Run(handle, data.data(), chunk_size, &written)) {
      return false;
    }
    if (written == 0 || written > chunk_size) {
      ::SetLastError(ERROR_WRITE_FAULT);
      return false;
    }
    data.remove_prefix(written);
  }
  return true;
}

}  // namespace

UninstallInvocationState ClassifyUninstallInvocation(
    std::optional<Command> command,
    bool has_relaunch_marker,
    bool has_relaunch_state,
    bool trusted_state_valid) {
  if (!has_relaunch_marker && !has_relaunch_state) {
    return command == Command::kUninstall
               ? UninstallInvocationState::kOriginal
               : UninstallInvocationState::kNotUninstall;
  }
  if (command != Command::kUninstall || !has_relaunch_marker ||
      !has_relaunch_state || !trusted_state_valid) {
    return UninstallInvocationState::kInvalid;
  }
  return UninstallInvocationState::kRelaunched;
}

}  // namespace internal

std::optional<UninstallInvocationContext> ResolveUninstallInvocationContext(
    const base::CommandLine& command_line,
    const base::FilePath& exe_path,
    std::optional<Command> command) {
  const bool has_relaunch_marker = IsUninstallRelaunched(command_line);
  const bool has_relaunch_state = command_line.HasSwitch(kSwitchUninstallState);
  bool trusted_state_valid = false;
  UninstallRelaunchState relaunch_state;

  if (command == Command::kUninstall && has_relaunch_marker &&
      has_relaunch_state) {
    HWND command_parent_window = nullptr;
    ParseParentWindowSwitch(command_line, &command_parent_window);
    trusted_state_valid = internal::ReadUninstallRelaunchState(
        exe_path, command_line.GetSwitchValueASCII(kSwitchUninstallState),
        command_parent_window, &relaunch_state);
    if (trusted_state_valid) {
      internal::WaitAtInstallerE2EChildStateBarrier();
    }
  }

  const UninstallInvocationState invocation =
      internal::ClassifyUninstallInvocation(command, has_relaunch_marker,
                                            has_relaunch_state,
                                            trusted_state_valid);
  if (invocation == UninstallInvocationState::kInvalid) {
    return std::nullopt;
  }
  return UninstallInvocationContext{invocation, std::move(relaunch_state)};
}

namespace internal {

UninstallExecutionDecision DecideUninstallExecution(
    UninstallInvocationState invocation,
    bool has_writable_target,
    bool background_mode,
    PathContainment containment) {
  if (invocation == UninstallInvocationState::kInvalid ||
      invocation == UninstallInvocationState::kNotUninstall) {
    return {.execution = UninstallExecution::kReject,
            .error_code = kExitCodeConfigError};
  }
  if (invocation == UninstallInvocationState::kRelaunched ||
      !has_writable_target) {
    return {.execution = UninstallExecution::kInProcess,
            .error_code = kExitCodeSuccess};
  }
  if (background_mode || containment == PathContainment::kContained) {
    return {.execution = UninstallExecution::kRelaunch,
            .error_code = kExitCodeSuccess};
  }
  if (containment == PathContainment::kOutside) {
    return {.execution = UninstallExecution::kInProcess,
            .error_code = kExitCodeSuccess};
  }
  return {.execution = UninstallExecution::kReject,
          .error_code = kExitCodeInstallError};
}

}  // namespace internal

int UninstallRelaunchExitCode(UninstallRelaunchStatus status) {
  return status == UninstallRelaunchStatus::kStarted ? kExitCodeRelaunched
                                                     : kExitCodeInstallError;
}

UninstallPreflightResult PrepareUninstall(const Config& config,
                                          const ExtendedConfig& extended,
                                          const base::FilePath& executable_path,
                                          UninstallInvocationState invocation) {
  UninstallPreflightResult result;
  if (invocation != UninstallInvocationState::kOriginal &&
      invocation != UninstallInvocationState::kRelaunched) {
    result.decision = internal::DecideUninstallExecution(
        invocation, false, extended.background_mode,
        internal::PathContainment::kIndeterminate);
    return result;
  }

  if (invocation == UninstallInvocationState::kOriginal) {
    const internal::InstallerE2EConfig& e2e = internal::GetInstallerE2EConfig();
    if (e2e.directory_override ==
        internal::InstallerE2EDirectoryOverride::kInvalid) {
      result.decision = {UninstallExecution::kReject, kExitCodeConfigError};
      return result;
    }
    if (e2e.directory_override ==
        internal::InstallerE2EDirectoryOverride::kAdminMutationDenied) {
      const base::FilePath admin = e2e.directory_root.Append(L"Admin");
      const base::FilePath per_user = e2e.directory_root.Append(L"PerUser");
      if (!base::DirectoryExists(admin) || !base::DirectoryExists(per_user)) {
        result.decision = {UninstallExecution::kReject, kExitCodeConfigError};
        return result;
      }
      internal::OverrideInstallDirectoryCandidatesForTesting({
          {admin, DirectoryRole::kHklmDefault, true, true},
          {per_user, DirectoryRole::kPerUserDefault, true, true},
      });
      internal::OverrideProcessElevationForTesting(true);
      internal::OverrideAdminMutationAllowedForTesting(false);
    }
  }

  PreparedUninstall prepared;
  prepared.config_binding = ConfigToJson(config);
  prepared.install_path = extended.install_path;
  prepared.resolution_context.mutation_capable = true;
  prepared.resolution_context.is_elevated = IsCurrentProcessElevated();
  prepared.resolution_context.allow_admin_mutation =
      IsAdminMutationAllowed(false, config.enable_explicit_modes);
  prepared.policy_result = LoadEnterprisePolicy();
  if (prepared.policy_result.valid()) {
    prepared.resolution_context.allow_shared_user_store =
        prepared.policy_result.policy.allow_shared_user_store;
    prepared.directories = ResolveInstallDirectories(
        extended.install_path, prepared.resolution_context);
  }

  const bool has_writable_target =
      prepared.directories &&
      prepared.directories->write_error == PathError::kSuccess &&
      !prepared.directories->writable_dir.empty();
  internal::PathContainment containment =
      internal::PathContainment::kIndeterminate;
  if (invocation == UninstallInvocationState::kOriginal &&
      has_writable_target && !extended.background_mode) {
    containment = internal::GetPhysicalPathContainment(
        prepared.directories->writable_dir, executable_path);
  }
  result.decision = internal::DecideUninstallExecution(
      invocation, has_writable_target, extended.background_mode, containment);
  prepared.execution = result.decision.execution;
  result.prepared = std::move(prepared);
  return result;
}

std::string FormatRetentionOutput(const Result& result, bool json_output) {
  if (json_output) {
    return result.ToJson() + "\r\n";
  }

  std::string output =
      "retention outcome=" + std::string(OutcomeToString(result.outcome)) +
      " max_age_days=" + base::NumberToString(result.retention_max_age_days) +
      " registrations_committed=" +
      (result.registrations_committed ? "true" : "false") +
      " versions_pruned=" + (result.versions_pruned ? "true" : "false") +
      " retry_required=" + (result.retry_required ? "true" : "false") + "\r\n";
  if (result.retention_plan) {
    if (!result.retention_plan->blocker.empty()) {
      output += "store blocker=" + result.retention_plan->blocker + "\r\n";
    }
    for (const auto& registration : result.retention_plan->registrations) {
      output +=
          "registration appid=" + registration.entry.uuid +
          " platform=" + registration.entry.platform +
          " vmin=" + registration.entry.vmin +
          " vmax=" + registration.entry.vmax +
          " abi_hash=" + registration.entry.abi_hash + " evidence_kind=" +
          RetentionEvidenceKindToString(registration.evidence.kind) +
          " evidence_time=" +
          base::NumberToString(registration.evidence.timestamp) + " age_days=" +
          (registration.age_days ? base::NumberToString(*registration.age_days)
                                 : "unknown") +
          " decision=" +
          RetentionRegistrationDecisionToString(registration.decision) +
          " reason=" + RetentionReasonToString(registration.reason);
      if (!registration.evidence.diagnostic.empty()) {
        output += " diagnostic=" + registration.evidence.diagnostic;
      }
      output += "\r\n";
    }
    for (const auto& version : result.retention_plan->versions) {
      output +=
          "version value=" + version.version.ToString() +
          " platform=" + version.platform +
          " required_before=" + (version.required_before ? "true" : "false") +
          " required_after=" + (version.required_after ? "true" : "false") +
          " expected_removal=" + (version.expected_removal ? "true" : "false") +
          " cleanup_deferred=" + (version.cleanup_deferred ? "true" : "false") +
          " decision=" + RetentionVersionDecisionToString(version.decision) +
          " reason=" + RetentionReasonToString(version.reason) + "\r\n";
    }
  }
  if (!result.error_message.empty()) {
    output += "error=" + result.error_message + "\r\n";
  }
  std::vector<std::string> warnings = result.warnings;
  std::ranges::sort(warnings);
  for (const auto& warning : warnings) {
    output += "warning=" + warning + "\r\n";
  }
  return output;
}

bool WriteAllToHandle(HANDLE handle, std::string_view data) {
  return internal::WriteAllToHandleImpl(
      handle, data, std::numeric_limits<DWORD>::max(),
      base::BindRepeating([](HANDLE destination, const char* bytes, DWORD size,
                             DWORD* written) {
        return !!::WriteFile(destination, bytes, size, written, nullptr);
      }));
}

namespace internal {

#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
bool WriteAllToHandleForTesting(HANDLE handle,
                                std::string_view data,
                                DWORD max_chunk_size,
                                HandleWriteCallback write) {
  return WriteAllToHandleImpl(handle, data, max_chunk_size, std::move(write));
}
#endif

}  // namespace internal

}  // namespace cef_installer
