// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_CONTROLLER_H_
#define CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_CONTROLLER_H_

#include <windows.h>

#include <array>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "cef/libcef_dll/bootstrap/installer/installer_cdn_manifest.h"
#include "cef/libcef_dll/bootstrap/installer/installer_config.h"
#include "cef/libcef_dll/bootstrap/installer/installer_constants.h"
#include "cef/libcef_dll/bootstrap/installer/installer_database.h"
#include "cef/libcef_dll/bootstrap/installer/installer_file_ops.h"
#include "cef/libcef_dll/bootstrap/installer/installer_lock.h"
#include "cef/libcef_dll/bootstrap/installer/installer_logger.h"
#include "cef/libcef_dll/bootstrap/installer/installer_paths.h"
#include "cef/libcef_dll/bootstrap/installer/installer_policy.h"
#include "cef/libcef_dll/bootstrap/installer/installer_retention.h"
#include "cef/libcef_dll/bootstrap/installer/installer_revocation.h"
#include "cef/libcef_dll/bootstrap/installer/installer_version_metadata.h"

namespace cef_installer {

enum class ArchiveError;
enum class SignatureError;

enum class ExecutionContext {
  kExplicitCommand,
  kAutomaticStartup,
};

enum class UninstallExecution {
  kInProcess,
  kRelaunch,
  kReject,
};

// Process-local uninstall preparation shared by bootstrap preflight and the
// direct controller run. This is never serialized or exposed by RunInstaller.
struct PreparedUninstall {
  Command command = Command::kUninstall;
  UninstallExecution execution = UninstallExecution::kReject;
  std::string config_binding;
  std::string install_path;
  PolicyLoadResult policy_result;
  DirectoryResolutionContext resolution_context;
  std::optional<InstallDirectories> directories;
};

// Extended configuration settings
//
// SECURITY / TRUST MODEL:
// =======================
// ExtendedConfig allows overriding security-sensitive settings like CDN URL
// and certificate thumbprint. This is INTENTIONAL and assumes the config JSON
// comes from a TRUSTED source (the client DLL embedding CEF).
//
// The trust model is:
// - Client application is trusted (it's running the installer)
// - Config JSON passed to RunInstaller is trusted (from client DLL)
// - CDN responses are verified via certificate pinning
// - Downloaded binaries are verified via code signature
//
// The override capabilities exist for:
// - Testing with mock CDN servers (cdn_urls)
// - Supporting alternative signing certificates (certificate_thumbprint)
// - Custom deployment scenarios (install_path, bundled_cef_path)
//
// If config JSON could come from untrusted sources (e.g., user input, network),
// the caller MUST validate/sanitize it before passing to RunInstaller.
//
// HOW EXTENDED CONFIG IS POPULATED:
// =================================
// ExtendedConfig is populated through multiple sources with the following
// precedence:
//
// 1. **Programmatic (Option C)**: When client DLL calls RunInstaller() with a
//    JSON config string, extended settings can be included in the JSON:
//    {
//      "appid": "...", "vmin": "...", "vmax": "...", "abi_hash": "...",
//      "cdn_urls": ["https://custom.cdn.com/"],
//      "bundled_cef_path": "C:\\App\\CEF"
//    }
//    The Controller parses both Config and ExtendedConfig from this JSON.
//
// 2. **Command-line flags**: When running in standalone mode, flags like
//    /cef-forcecheck, /cef-background, /cef-headless, /cef-parent are parsed
//    into ExtendedConfig by ParseExtendedConfig() in bootstrap_win.cc.
//
// 3. **Defaults**: All fields have sensible defaults (see struct definition).
//    If not specified, defaults are used.
//
// The client DLL resource may additionally provide bundled_cef_path and
// install_path. install_path is resolved against that DLL and copied here only
// when an operation-specific value has not already been supplied. Bootstrap
// resources cannot provide install_path.
//
struct ExtendedConfig {
  // Ordered operation-specific CDN base URLs. Empty means the selected
  // application config or hardcoded default applies. Public JSON parsing
  // requires one through three HTTPS URLs; non-official tests may inject HTTP
  // values directly after parsing.
  std::vector<std::string> cdn_urls;

  // Custom CEF installation path (default: use ResolveInstallDirectories())
  // If empty, uses the standard search order:
  //   registry -> %ProgramFiles% -> %LocalAppData%
  //
  // A custom path is authoritative and exclusive. A safe writable path is the
  // sole read/write store. A safe existing read-only path is the sole readable
  // store for query and automatic selection. Invalid, unsafe, inaccessible, or
  // missing read-only paths fail without default-search fallback.
  //
  // Security measures applied to custom paths:
  // - Reparse points (symlinks/junctions) are rejected
  std::string install_path;

  // Path to bundled CEF for full installers (default: none)
  // If set and no newer/compatible version is installed, use this bundled
  // version. A newer version may still be installed in background and used
  // after relaunch.
  //
  // Bundled CEF handling:
  // 1. The bundled CEF must be pre-extracted (not a .tar.xz archive)
  // 2. The bundled directory must contain cef_version.json with
  //    version/abi_hash
  // 3. catalog.cat must be a regular non-reparse file, but its signature and
  //    members are not verified at runtime
  // 4. If bundled version is compatible AND no better installed version
  //    exists, the bundled version will be loaded in-place
  // 5. If a newer compatible version is already installed, bundled CEF is
  //    ignored
  //
  // This enables offline/air-gapped installation while still preferring
  // newer shared versions when available.
  std::string bundled_cef_path;

  // Expected certificate thumbprint for CEF binaries.
  // Default: kCefCertificateThumbprint (hardcoded official CEF certificate)
  //
  // SECURITY NOTE: This is the primary security control. Overriding this
  // allows binaries signed by a different certificate to be installed.
  // Only override for testing or when using legitimately re-signed binaries.
  // An attacker who can control this value can bypass signature verification.
  std::string certificate_thumbprint = std::string(kCefCertificateThumbprint);

  // Force version check even if recently checked
  bool force_check = false;

  // Show progress UI (default: true)
  bool show_progress_ui = true;

  // Internal command-line operation context. Set only by /cef-background;
  // headless UI suppression alone must not lower extraction impact.
  bool background_mode = false;

  // Parent window handle for progress UI
  // If non-zero and show_progress_ui is true, the progress dialog is positioned
  // relative to this window (centered and topmost, but not directly parented
  // for security reasons). Also used for WM_COPYDATA progress notifications
  // when the caller wants to display custom UI.
  HWND parent_window = nullptr;

  // Path to a local directory containing CDN-structured files.
  // When set, the installer reads manifest JSON, .tar.xz archives, and
  // .sha256 files from this directory instead of downloading from CDN.
  // The directory must contain the same files that would be available on CDN
  // (e.g., {milestone}_{platform}.json, archive .tar.xz, .sha256).
  // All usual validation (hash verification, signature checking, version
  // filtering) is still performed.
  std::string local_download_path;

  // Minimum log level for file logging. Messages below this level are
  // discarded. Configurable via /cef-log-level=<level>.
  LogLevel log_level = LogLevel::kWarning;

  // Lock acquisition timeout in milliseconds. 0 = use default
  // (kDefaultLockTimeoutMs). Used by post-exit pruning to specify a short
  // timeout so it doesn't block process exit.
  uint32_t lock_timeout_ms = 0;

  // WinHTTP receive-response timeout for archive downloads (milliseconds).
  // 0 = use the default (60 s). Shorter values let tests avoid blocking for
  // the full default when the server is unresponsive.
  int download_timeout_ms = 0;

  // Operation-specific registration-retention threshold. This is accepted
  // only by explicit retention commands and is never persisted as app config.
  int retention_max_age_days = kDefaultRetentionMaxAgeDays;
};

// Applies the test-only certificate thumbprint from |config| to |extended| and
// enables the corresponding signature-verification mode when needed. This is
// a no-op in official release builds.
void ApplyConfigThumbprintOverride(const Config& config,
                                   ExtendedConfig* extended);

struct EmergencyRecoveryScanLimits {
  size_t max_roots = kEmergencyRecoveryMaxRoots;
  size_t max_version_entries = kEmergencyRecoveryMaxVersionEntries;
  base::TimeDelta time_budget =
      base::Milliseconds(kEmergencyRecoveryTimeBudgetMs);
};

// Overrides automatic-startup emergency recovery bounds for deterministic
// tests. Passing nullopt restores production limits.
#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
void SetEmergencyRecoveryScanLimitsForTesting(
    std::optional<EmergencyRecoveryScanLimits> limits);
#endif

// Deterministic auxiliary cleanup failure for retention transaction tests.
void SetRetentionEvidenceDeleteFailureForTesting(bool fail);
void SetRetentionPostIndexFailureForTesting(bool fail);
void SetRetentionPendingRestoreFailureForTesting(bool fail);
void SetRetentionPostValidationEvidenceChangeForTesting(bool change);
void SetLaunchStateGcPreDeleteHookForTesting(base::RepeatingClosure callback);

// Logical completion state for an installer operation. Current producers use
// kCommitted or kFailed. kCleanupDeferred is reserved for command-specific
// cleanup semantics; that outcome is successful.
enum class Outcome {
  kCommitted,
  kCleanupDeferred,
  kFailed,
};

// Result of an installer operation.
struct Result {
  bool success = false;
  Outcome outcome = Outcome::kFailed;

  // Error information (if !success)
  int error_code = kExitCodeSuccess;  // One of kExitCode* constants
  // Unlocalized developer/support diagnostic. Never use for localized UI.
  std::string error_message;

  // Non-fatal work left for a later mutating pass. Present only with a
  // successful kCleanupDeferred outcome.
  std::vector<std::string> warnings;

  // Internal per-version cleanup effects. Not serialized independently;
  // retention copies these into each version report.
  std::set<VersionKey> deferred_version_cleanup;

  // Present for explicit retention operations.
  std::optional<RetentionPlan> retention_plan;
  int retention_max_age_days = 0;
  bool registrations_committed = false;
  bool versions_pruned = false;
  bool retry_required = false;

  // On successful install: path to libcef.dll
  base::FilePath libcef_path;

  // Version that was installed/is being used
  std::string installed_version;

  // Full version string from metadata (e.g.,
  // "150.0.1+gabc1234+chromium-150.0.7871.4")
  std::string version_full;

  // Whether the resolved libcef.dll came from a bundled CEF directory
  // (as opposed to a CDN-downloaded installed version). When true, the
  // libcef.dll may be signed with the client application's certificate
  // rather than the CEF certificate (kCefCertificateThumbprint).
  bool is_bundled = false;

  // Whether the selected version is a rollback (a different version was
  // selected because the newest version was disqualified by launch health).
  bool is_rollback = false;

  // Path to the launch state file for the bootstrap to write sentinel
  // and update after RunWinMain. Empty if launch state is not active.
  base::FilePath launch_state_path;

  // Consecutive failure count for the selected version. The bootstrap writes
  // this value into the sentinel before RunWinMain.
  int launch_consecutive_failures = 0;

  // Version and platform for the sentinel file. Avoids re-parsing the
  // launch_state_path filename in the bootstrap.
  std::string launch_version;
  std::string launch_platform;

  // Paths to .launch_* files from older versions. The bootstrap deletes
  // these on successful exit (version confirmed).
  std::vector<base::FilePath> launch_cleanup_paths;

  // Version-less retention liveness record written before RunWinMain for
  // every launch-health mode.
  base::FilePath liveness_path;

  // Internal use-time lease for an installed shared-store distribution.
  // Not serialized in the public Result JSON.
  std::shared_ptr<VersionLease> version_lease;

  // Convert to JSON string for RunInstaller return value
  std::string ToJson() const;

  // Parse from JSON string
  static std::optional<Result> FromJson(const std::string& json);

  // Factory methods for common results
  static Result Success(const base::FilePath& libcef_path,
                        const std::string& version,
                        const std::string& version_full = "",
                        bool is_bundled = false);
  static Result Error(int code, const std::string& message);
};

// Progress callback for UI updates.
// Parameters: step, bytes_done, bytes_total.
// Return false to cancel the operation.
using ProgressCallback =
    base::RepeatingCallback<bool(Step, uint64_t, uint64_t)>;

// Main installer controller class
class Controller {
 public:
  Controller();
  ~Controller();

  Controller(const Controller&) = delete;
  Controller& operator=(const Controller&) = delete;

  // Execute an installer command.
  // This is the main entry point implementing the RunInstaller API.
  //
  // Query and automatic-startup resolution use lock-free read paths.
  // Mutating commands acquire the singleton writer mutex, reconcile state,
  // update the database, perform any download/install work, and prune.
  //
  // Parameters:
  // - command: The operation to perform
  // - app_config: Application configuration (appid, vmin, vmax, abi_hash).
  //               Must be pre-validated via ParseConfigFromJson() or
  //               equivalent; only a lightweight empty-field check is
  //               performed here.
  // - extended_config: Optional extended settings
  // - progress: Optional callback for progress updates
  //
  // Returns: Result with success/failure info and libcef.dll path on success
  Result Run(Command command,
             const Config& app_config,
             const ExtendedConfig& extended_config = {},
             ProgressCallback progress = {},
             ExecutionContext context = ExecutionContext::kExplicitCommand);

  // Internal bootstrap entry point for an in-process explicit uninstall. The
  // prepared policy and directory snapshot must authorize direct execution and
  // match the supplied command configuration. Mismatches fail with
  // configuration error before mutation.
  Result RunPreparedUninstall(const Config& app_config,
                              const ExtendedConfig& extended_config,
                              const PreparedUninstall& prepared,
                              ProgressCallback progress = {});

  // Convenience overload that parses config from JSON string
  // (for RunInstaller export function)
  Result Run(Command command,
             const std::string& config_json,
             ProgressCallback progress = {});

  // Read version indexes from multiple directories and merge results.
  // Deduplicates by (version, platform), keeping the entry from the first
  // (higher-priority) directory. When |allow_scan_fallback| is true, a
  // missing or corrupt index falls back to directory scanning.
  static std::vector<InstalledVersion> ReadMultipleVersionIndexes(
      const std::vector<base::FilePath>& directories,
      bool allow_scan_fallback = true,
      VersionIndexReadMode read_mode = VersionIndexReadMode::kDoomCorrupt);

 private:
  Result RunImpl(Command command,
                 const Config& app_config,
                 const ExtendedConfig& extended_config,
                 ProgressCallback progress,
                 ExecutionContext context,
                 const PreparedUninstall* prepared_uninstall);

  // Individual execution steps
  Result AcquireLock(const base::FilePath& install_dir,
                     uint32_t timeout_ms = 0);
  Result ResolveQuery(const Config& config,
                      const ExtendedConfig& extended,
                      const base::FilePath& install_dir,
                      const std::vector<base::FilePath>& read_dirs);
  std::optional<Result> ResolveStartupOffline(
      const Config& config,
      const ExtendedConfig& extended,
      const base::FilePath& install_dir,
      const std::vector<base::FilePath>& read_dirs,
      const std::vector<RevokedVersionRange>& revoked,
      bool allow_last_resort = false);
  void TryRegisterStartup(const Config& config,
                          const base::FilePath& install_dir);
  Result UpdateDatabase(Command command,
                        const Config& config,
                        const base::FilePath& install_dir);
  Result PublishRegistration(const Config& config,
                             const base::FilePath& install_dir);
  Result ReconcileInstallState(const base::FilePath& install_dir);

  // Quarantine safe canonical version directories that are absent from an
  // already validated authoritative index.
  Result QuarantineUnindexedVersions(
      const base::FilePath& install_dir,
      const std::vector<InstalledVersion>& indexed);
  Result ComputeAndInstall(const Config& config,
                           const ExtendedConfig& extended,
                           const base::FilePath& install_dir,
                           const std::vector<base::FilePath>& read_dirs,
                           Command command,
                           ProgressCallback progress);
  // Prune binaries and repair launch debris. Ordinary pruning age-gates valid
  // orphaned/below-vmin launch records without allowing them to protect
  // versions; explicit retention apply does not run that age GC.
  Result PruneUnusedVersions(
      const base::FilePath& install_dir,
      const Config& current_config,
      std::vector<InstalledVersion> installed = {},
      const std::vector<RevokedVersionRange>& revoked = {},
      bool retention_apply = false,
      const std::set<VersionKey>* retention_scope = nullptr,
      base::OnceCallback<bool()> logical_commit_complete = {});
  Result RunRetention(Command command,
                      DirectoryRole role,
                      const base::FilePath& install_dir,
                      const std::vector<base::FilePath>& read_dirs,
                      const ExtendedConfig& extended_config,
                      ProgressCallback progress);
  void ReleaseLock();

  // CDN operations
  std::optional<CdnBuildEntry> QueryCdnForVersion(
      const Config& config,
      const ExtendedConfig& extended,
      const base::FilePath& install_dir,
      const std::set<Version>& skip_versions = {},
      std::vector<CdnBuildEntry>* validated_entries = nullptr);
  Result DownloadAndInstall(const CdnBuildEntry& entry,
                            const ExtendedConfig& extended,
                            const base::FilePath& install_dir,
                            ProgressCallback progress,
                            bool* try_next_candidate = nullptr);

  // Check if bundled CEF (from full installers) is suitable for use.
  // Reads valid cef_version.json from bundled_path and returns the candidate.
  // Platform, range, ABI, revocation, and disqualification are applied by the
  // shared side-effect-free selector.
  // catalog.cat must exist as a regular non-reparse file, but its signature and
  // members are not verified — hashing ~370 MB on every startup is too costly.
  // Bundled integrity is the app developer's responsibility (their package).
  // Returns nullopt when the layout or metadata is invalid.
  std::optional<InstalledVersion> CheckBundledCef(
      const Config& config,
      const ExtendedConfig& extended);

  // Construct a validated Result::Success for a libcef.dll path. Calls
  // GetLibcefPath and validates that every component between |trusted_root|
  // and the resulting path is free of reparse points. Returns an error
  // Result if validation fails.
  Result MakeValidatedLibcefResult(const base::FilePath& trusted_root,
                                   const base::FilePath& version_dir,
                                   const std::string& version_str,
                                   const std::string& version_full = "",
                                   bool is_bundled = false);

  // Internal state
  std::unique_ptr<SingletonLock> lock_;
  std::unique_ptr<Database> database_;
  bool cancelled_ = false;
  bool version_published_ = false;
  base::FilePath pending_archive_cleanup_path_;
  std::vector<RevokedVersionRange> effective_revocations_;
  EnterprisePolicy enterprise_policy_;
  EffectiveDownloadSource effective_download_source_;
};

// Get the process exit code for a result
int ResultToExitCode(const Result& result);

// Convert an outcome to its normalized JSON/logging representation.
const char* OutcomeToString(Outcome outcome);

// Calculate overall progress percentage for the installer.
// Maps step/byte progress to a 0-100% range where each step gets a portion:
//   Steps 0-1 (Init/Lock): 0-5%
//   Steps 2-3 (VersionCheck/CdnResolve): 5-15%
//   Step 4 (Download): 15-55%
//   Step 5 (Extract): 55-80%
//   Step 6 (SignatureVerify): 80-90%
//   Step 7 (Install): 90-95%
//   Step 8 (Committing): 95% (retention apply only)
//   Step 9 (Cleanup): 95-100%
// If bytes_total > 0, interpolates within the step based on byte progress.
int CalculateOverallProgress(Step step,
                             uint64_t bytes_done,
                             uint64_t bytes_total);

// Send progress notification to parent window via WM_COPYDATA.
// Rate-limited to avoid flooding the receiver during rapid progress updates.
// Uses SendMessageTimeout with SMTO_ABORTIFHUNG to prevent hanging.
// Returns true to continue, false if the parent requested cancellation
// (by returning kWmCopyDataResultCancel from its WM_COPYDATA handler).
// Timeouts and errors are not treated as cancellation.
bool SendProgressToParent(HWND parent,
                          Step step,
                          uint64_t bytes_done,
                          uint64_t bytes_total);

// ============================================================================
// Internal helpers exposed for testing. Not part of the public API.
// ============================================================================
namespace internal {

// Typed candidate-fallback classification shared by production control flow
// and the retry-matrix unit tests.
#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
bool IsCandidateRetryableArchiveErrorForTesting(ArchiveError error);
bool IsCandidateRetryableSignatureErrorForTesting(SignatureError error);
bool IsCandidateRetryableMetadataErrorForTesting(MetadataError error);
#endif

// Parse ExtendedConfig from JSON string.
// Used when parsing the combined config JSON in RunInstaller.
bool ParseExtendedConfigFromJson(const std::string& json,
                                 ExtendedConfig* config,
                                 std::string* diagnostic = nullptr);

// Parse both Config and ExtendedConfig from a combined JSON string.
bool ParseCombinedConfig(const std::string& json,
                         Config* config,
                         ExtendedConfig* extended,
                         std::string* diagnostic = nullptr);

// Parse Command from string (case-insensitive)
std::optional<Command> ParseCommand(const std::string& str);

// Handle the kLaunchSuccess command: confirm the active CEF version is
// healthy by writing running=false, consecutive_failures=0 to the sentinel
// file.
//
// This is a lightweight path — it does NOT go through Controller::Run, the
// installer lock, the database, or progress UI. It reads the sentinel,
// verifies PID ownership, and writes the confirmation directly.
//
// Returns a Result with:
// - success=true (kExitCodeSuccess) on confirmation, already-confirmed
//   (idempotent), and writes back preserving all other fields.
// - kExitCodeNoSentinel if |launch_state_path| is empty.
// - kExitCodeSentinelReadError if the sentinel is missing or corrupt.
// - kExitCodeSentinelOwnerMismatch if the sentinel is owned by another
//   process (PID/start-time mismatch).
//
// The no-arg overload resolves the path from the process-global active launch
// state path (set by the bootstrap before RunWinMain). The path-taking
// overload lets tests exercise the logic without mutating process-global
// state.
Result HandleLaunchSuccess();
Result HandleLaunchSuccess(const base::FilePath& launch_state_path);

// Convert between exit code ints and JSON string representations.
const char* ExitCodeToString(int code);
int StringToExitCode(const std::string& str);
std::optional<Outcome> StringToOutcome(const std::string& str);

// Returns the canonical name for a step (e.g., "downloading").
// Used in WM_COPYDATA JSON progress notifications.
const char* StepCanonicalName(Step step);

// Build JSON string for WM_COPYDATA progress notification.
std::string BuildProgressJson(Step step,
                              uint64_t bytes_done,
                              uint64_t bytes_total);

// Reset rate limiting state for SendProgressToParent (for testing).
void ResetProgressNotificationState();

// Enable testing mode for RunInstaller.
// When enabled, allows HTTP URLs and suppresses error dialogs.
// Call with true in test SetUp, false in TearDown.
void SetTestingMode(bool enabled);

}  // namespace internal

// ============================================================================
// DLL Export Function
// ============================================================================

// Exported function for client DLL to call (Option C: Programmatic
// Configuration). This is the DLL export that wraps Controller.
//
// Parameters:
// - command: "install", "update", "uninstall", or "query"
// - config_json: JSON configuration string (or empty for default)
//
// Returns: JSON result with boolean success and an outcome of "committed",
//          "cleanup_deferred", or "failed". Failed results contain numeric
//          error_code, symbolic error_name, and error_message. Successful
//          results omit error fields and may include libcef_path,
//          installed_version, version_full, and is_bundled.
//          The returned pointer is valid until the next call to RunInstaller
//          on the same thread or until that thread exits. Copy the JSON before
//          either event if it must be retained.
//
// Thread safety: Uses thread-local storage for the returned string, so
// concurrent calls from different threads are safe. The installer itself
// serializes operations via a global lock.
//
// Example usage from client DLL:
//   const char* result = RunInstaller("install",
//       R"({"appid":"...", "vmin":"137.1"})");
//   auto parsed = Result::FromJson(result);
//   if (parsed && parsed->success &&
//       parsed->outcome != Outcome::kFailed)
//     LoadLibrary(parsed->libcef_path);
extern "C" __declspec(dllexport) const char* RunInstaller(
    const char* command,
    const char* config_json);

}  // namespace cef_installer

#endif  // CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_CONTROLLER_H_
