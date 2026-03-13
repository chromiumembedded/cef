// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_BOOTSTRAP_HELPERS_H_
#define CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_BOOTSTRAP_HELPERS_H_

#include <windows.h>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "cef/include/cef_version_info.h"
#include "cef/libcef_dll/bootstrap/installer/installer_config.h"
#include "cef/libcef_dll/bootstrap/installer/installer_constants.h"
#include "cef/libcef_dll/bootstrap/installer/installer_controller.h"
#include "cef/libcef_dll/bootstrap/installer/installer_relaunch.h"

namespace cef_installer {

// Command-line switch names for installer modes (used with / prefix on Windows)
// These are parsed from the command line when running in standalone mode.
constexpr const char kSwitchUpdate[] = "cef-update";
constexpr const char kSwitchUninstall[] = "cef-uninstall";
constexpr const char kSwitchRetentionDryRun[] = "cef-retention-dry-run";
constexpr const char kSwitchRetentionApply[] = "cef-retention-apply";
constexpr const char kSwitchRetentionMaxAgeDays[] = "cef-max-age-days";
constexpr const char kSwitchUninstallRelaunched[] = "cef-uninstall-relaunched";
// Internal marker indicating that a self-relocated uninstall must load the
// trusted relaunch state written beside the temporary executable.
constexpr const char kSwitchUninstallState[] = "cef-uninstall-state";
constexpr const char kSwitchForceCheck[] = "cef-forcecheck";
constexpr const char kSwitchBackground[] = "cef-background";
constexpr const char kSwitchHeadless[] = "cef-headless";
constexpr const char kSwitchParentWindow[] = "cef-parent";
// /cef-download-path=<path> - Read manifests and archives from a local
// directory instead of downloading from CDN. Works as a modifier for any
// command (/cef-update, /cef-uninstall, auto-install). All post-download
// validation (hash, signature, size limits) still applies.
constexpr const char kSwitchDownloadPath[] = "cef-download-path";
// /cef-log-level=<level> - Set minimum log level for cef_installer.log.
// Accepted values: info, warning, error (case-insensitive). Default: warning.
constexpr const char kSwitchLogLevel[] = "cef-log-level";
#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
// /cef-install-path=<path> - Override the CEF installation directory. In
// standalone mode, bypasses the normal directory resolution (registry ->
// ProgramFiles -> LocalAppData). When a client DLL is present, install_path
// is passed programmatically via the RunInstaller JSON instead. Only
// available in non-official builds for testing.
constexpr const char kSwitchInstallPath[] = "cef-install-path";
#endif

// ============================================================================
// Command-line Parsing
// ============================================================================

// Parse installer command from command-line arguments.
// Returns the Command if /cef-update or /cef-uninstall is present, nullopt
// otherwise. On Windows, switches can use either -- or / prefix (e.g.,
// /cef-update or --cef-update)
std::optional<Command> ParseInstallerCommand(
    const base::CommandLine& command_line);

// Returns true when more than one explicit installer operation was supplied.
bool HasConflictingInstallerCommands(const base::CommandLine& command_line);

// Returns true if retention-only options are present without a retention
// operation.
bool HasMisappliedRetentionOptions(const base::CommandLine& command_line,
                                   std::optional<Command> command);

// Parse /cef-max-age-days for retention commands. Returns false for a missing
// value, non-integer, or value outside 90..3650.
bool ParseRetentionMaxAgeDays(const base::CommandLine& command_line,
                              int* max_age_days);

// Check if the internal --cef-uninstall-relaunched transport marker is
// present. The marker alone is never trusted; bootstrap also requires the
// nonce-bound state file written beside the temporary executable.
bool IsUninstallRelaunched(const base::CommandLine& command_line);

// Parse extended configuration flags from command-line.
// Modifies the provided ExtendedConfig based on command-line switches.
// Recognized switches:
//   /cef-forcecheck - set force_check = true
//   /cef-background - set show_progress_ui = false (also sets low priority)
//   /cef-headless   - set show_progress_ui = false
//   /cef-parent=<hwnd> - set parent_window to specified HWND value
//   /cef-download-path=<path> - set local_download_path
//   /cef-log-level=<level> - set log_level (info, warning, error)
void ParseExtendedConfigFromCommandLine(const base::CommandLine& command_line,
                                        ExtendedConfig* extended_config);

// Parses /cef-parent as an unsigned pointer-width decimal value. This does not
// require the HWND to remain valid; callers decide whether current IsWindow
// validity is required for their phase.
bool ParseParentWindowSwitch(const base::CommandLine& command_line,
                             HWND* parent_window);

// ============================================================================
// Utility Functions
// ============================================================================

// Parse Command from string (case-insensitive).
// Accepts: "install", "update", "uninstall", "query"
// Returns nullopt if string doesn't match any valid command.
std::optional<Command> CommandFromString(const std::string& str);

// ============================================================================
// Config Loading
// ============================================================================

// Try to load installer config using the resource priority chain:
//   1. Client DLL resource (if client_dll_module != nullptr)
//   2. Bootstrap resource (fallback when client DLL has no config, or
//      as the standalone source)
//
// enable_explicit_modes is always read from the bootstrap exe's resource,
// regardless of which source provides the application config. The client DLL
// cannot set this flag, and application-config provenance never describes it.
//
// |bootstrap_module| defaults to GetModuleHandle(nullptr) when nullptr.
// Testable via explicit module handles for the bootstrap_module parameter.
enum class ConfigLoadStatus {
  kLoaded,
  kNotFound,
  kError,
};

// Internal provenance for a successfully loaded application config. kNone is
// required for not-found and every error result.
enum class ConfigLoadSource {
  kNone,
  kClientResource,
  kBootstrapResource,
};

struct ConfigLoadResult {
  ConfigLoadStatus status = ConfigLoadStatus::kNotFound;
  ConfigError error = ConfigError::kSuccess;
  ConfigLoadSource source = ConfigLoadSource::kNone;
  std::string diagnostic;
};

ConfigLoadResult TryLoadInstallerConfig(
    HMODULE client_dll_module,
    Config* config,
    HMODULE bootstrap_module = nullptr,
    bool trusted_uninstall_relaunch = false);

// Resolves a non-empty client-resource install_path relative to the module
// directory. On failure the value remains non-empty so exclusive resolution
// cannot silently fall back to a default store.
bool ResolveClientInstallPath(HMODULE client_dll_module, Config* config);

// Owned installer state that remains alive through the client entry point.
struct InstallerStartupState {
  bool configured = false;
  base::FilePath libcef_path;
  bool is_bundled = false;
  std::string version_full;
  int error_code = kExitCodeSuccess;
  std::string error_message;
  std::string effective_vmin;
  std::string effective_vmax;

  // Launch-health handoff.
  LaunchHealthMode launch_health = LaunchHealthMode::kOff;
  base::FilePath launch_state_path;
  int launch_consecutive_failures = 0;
  std::string launch_version;
  std::string launch_platform;
  std::vector<base::FilePath> launch_cleanup_paths;
  base::FilePath liveness_path;
  std::string appid;
  std::wstring appid_hash;
  Config config;
  ExtendedConfig extended;
  std::shared_ptr<VersionLease> version_lease;
};

InstallerStartupState MakeInstallerStartupState(
    const ConfigLoadResult& load_result,
    const Config& config);
void ApplyInstallerResultToStartupState(const Result& result,
                                        const Config& config,
                                        const ExtendedConfig& extended,
                                        bool apply_launch_health,
                                        InstallerStartupState* state);
void PopulateInstallerVersionInfo(const InstallerStartupState& state,
                                  cef_version_info_t* info);

struct InstallerCrashAnnotationValues {
  bool has_failure = false;
  std::string error;
  std::string message;
  std::string vmin;
  std::string vmax;
  std::string policy_denied;
  std::string cancelled;
};

InstallerCrashAnnotationValues BuildInstallerCrashAnnotationValues(
    const InstallerStartupState& state);

// Trusted relaunch classification. kInvalid covers a marker/state mismatch,
// invalid trusted state, or relaunch transport state on a non-uninstall
// command. kNotUninstall is the ordinary no-transport case.
enum class UninstallInvocationState {
  kNotUninstall,
  kOriginal,
  kRelaunched,
  kInvalid,
};

struct UninstallInvocationContext {
  UninstallInvocationState invocation = UninstallInvocationState::kInvalid;
  UninstallRelaunchState relaunch_state;
};

struct UninstallExecutionDecision {
  UninstallExecution execution = UninstallExecution::kReject;
  int error_code = kExitCodeConfigError;
};

// Validates the relaunch marker/state command shape and loads the minimal
// nonce-bound state needed before installer config is available. Returns null
// for malformed transport or untrusted state.
std::optional<UninstallInvocationContext> ResolveUninstallInvocationContext(
    const base::CommandLine& command_line,
    const base::FilePath& exe_path,
    std::optional<Command> command);

namespace internal {

// Classifies the relaunch marker/state pair after any trusted state read.
UninstallInvocationState ClassifyUninstallInvocation(
    std::optional<Command> command,
    bool has_relaunch_marker,
    bool has_relaunch_state,
    bool trusted_state_valid);

// Pure execution matrix used by bootstrap preflight and table-driven tests.
UninstallExecutionDecision DecideUninstallExecution(
    UninstallInvocationState invocation,
    bool has_writable_target,
    bool background_mode,
    PathContainment containment);

}  // namespace internal

// A successful child launch is pending (109); any relaunch preparation or
// process-creation failure is an installer setup failure (105).
int UninstallRelaunchExitCode(UninstallRelaunchStatus status);

struct UninstallPreflightResult {
  UninstallExecutionDecision decision;
  std::optional<PreparedUninstall> prepared;
};

// Loads immutable policy, resolves the explicit mutation-permitted uninstall
// target once, and chooses direct execution or asynchronous relaunch.
// For an original, non-background uninstall with a writable target,
// indeterminate physical containment returns 105 before mutation or relaunch.
// A valid relaunched child also prepares a fresh snapshot but is always
// in-process.
UninstallPreflightResult PrepareUninstall(const Config& config,
                                          const ExtendedConfig& extended,
                                          const base::FilePath& executable_path,
                                          UninstallInvocationState invocation);

// Format explicit retention output for the console. Headless mode returns one
// JSON object followed by CRLF; normal mode returns deterministic text.
std::string FormatRetentionOutput(const Result& result, bool json_output);

// Write all bytes to a Win32 handle, retrying partial writes.
bool WriteAllToHandle(HANDLE handle, std::string_view data);

namespace internal {

using HandleWriteCallback =
    base::RepeatingCallback<bool(HANDLE, const char*, DWORD, DWORD*)>;

// Injectable write implementation and chunk bound for deterministic tests.
#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
bool WriteAllToHandleForTesting(HANDLE handle,
                                std::string_view data,
                                DWORD max_chunk_size,
                                HandleWriteCallback write);
#endif

}  // namespace internal

}  // namespace cef_installer

#endif  // CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_BOOTSTRAP_HELPERS_H_
