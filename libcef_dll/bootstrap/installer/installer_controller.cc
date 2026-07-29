// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef_dll/bootstrap/installer/installer_controller.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <utility>

#include "base/environment.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "cef/libcef_dll/bootstrap/installer/installer_archive.h"
#include "cef/libcef_dll/bootstrap/installer/installer_constants.h"
#include "cef/libcef_dll/bootstrap/installer/installer_download.h"
#include "cef/libcef_dll/bootstrap/installer/installer_file_integrity.h"
#include "cef/libcef_dll/bootstrap/installer/installer_file_ops.h"
#include "cef/libcef_dll/bootstrap/installer/installer_launch_state.h"
#include "cef/libcef_dll/bootstrap/installer/installer_paths.h"
#include "cef/libcef_dll/bootstrap/installer/installer_policy.h"
#include "cef/libcef_dll/bootstrap/installer/installer_progress_dialog.h"
#include "cef/libcef_dll/bootstrap/installer/installer_resources.h"
#include "cef/libcef_dll/bootstrap/installer/installer_result_json.h"
#include "cef/libcef_dll/bootstrap/installer/installer_revocation.h"
#include "cef/libcef_dll/bootstrap/installer/installer_signature.h"
#include "cef/libcef_dll/bootstrap/installer/installer_version_resolver.h"

namespace cef_installer {

namespace {

// Minimum interval between WM_COPYDATA progress updates (milliseconds).
// Prevents flooding the parent window during rapid progress callbacks.
constexpr DWORD kProgressUpdateIntervalMs = 100;

// Rate limiting state for SendProgressToParent
DWORD g_last_send_time = 0;
int g_last_step = -1;

bool IsCandidateSpecificArchiveError(ArchiveError error) {
  switch (error) {
    case ArchiveError::kInvalidFormat:
    case ArchiveError::kInvalidHeader:
    case ArchiveError::kExtractionFailed:
    case ArchiveError::kPathTraversal:
    case ArchiveError::kAbsolutePath:
    case ArchiveError::kUnsupportedEntryType:
      return true;
    case ArchiveError::kSuccess:
    case ArchiveError::kFileNotFound:
    case ArchiveError::kFileReadError:
    case ArchiveError::kDiskFull:
    case ArchiveError::kWriteError:
    case ArchiveError::kCancelled:
      return false;
  }
  return false;
}

bool IsCandidateSpecificSignatureError(SignatureError error) {
  switch (error) {
    case SignatureError::kFileNotFound:
    case SignatureError::kNotSigned:
    case SignatureError::kSignatureInvalid:
    case SignatureError::kCertificateExpired:
    case SignatureError::kCertificateRevoked:
    case SignatureError::kThumbprintMismatch:
    case SignatureError::kCatalogNotFound:
    case SignatureError::kCatalogInvalid:
    case SignatureError::kFileNotInCatalog:
    case SignatureError::kHashMismatch:
      return true;
    case SignatureError::kSuccess:
    case SignatureError::kCancelled:
      return false;
  }
  return false;
}

bool IsCandidateSpecificMetadataError(MetadataError error) {
  switch (error) {
    case MetadataError::kFileNotFound:
    case MetadataError::kJsonParseError:
    case MetadataError::kMissingRequiredField:
      return true;
    case MetadataError::kSuccess:
    case MetadataError::kFileReadError:
    case MetadataError::kFileWriteError:
    case MetadataError::kIndexValidationError:
    case MetadataError::kIntegrityMismatch:
      return false;
  }
  return false;
}

const char* PolicyCommandName(Command command) {
  switch (command) {
    case Command::kInstall:
      return "install";
    case Command::kUpdate:
      return "update";
    case Command::kUninstall:
      return "uninstall";
    case Command::kQuery:
      return "query";
    case Command::kPrune:
      return "prune";
    case Command::kRetentionDryRun:
      return "retention_dry_run";
    case Command::kRetentionApply:
      return "retention_apply";
    case Command::kLaunchSuccess:
      return "launch_success";
  }
  return "unknown";
}

std::string BuildPolicyDenialMessage(
    Command command,
    std::string_view reason,
    const Config* config = nullptr,
    const std::vector<InstalledVersion>* installed = nullptr) {
  std::string message = "Enterprise policy blocked ";
  message += PolicyCommandName(command);
  message += ": ";
  message += reason;
  if (config) {
    message += "; effective vmin=" + config->vmin;
  }
  if (installed) {
    message += "; installed versions considered=";
    if (installed->empty()) {
      message += "none";
    } else {
      for (size_t i = 0; i < installed->size(); ++i) {
        if (i) {
          message += ",";
        }
        message += (*installed)[i].metadata.version.ToString();
      }
    }
  }
  message += ". Contact your administrator.";
  for (char& character : message) {
    if (static_cast<unsigned char>(character) < 0x20) {
      character = ' ';
    }
  }
  constexpr size_t kMaxPolicyDiagnosticLength = 1024;
  if (message.size() > kMaxPolicyDiagnosticLength) {
    message.resize(kMaxPolicyDiagnosticLength - 3);
    message += "...";
  }
  return message;
}

#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
bool g_fail_retention_evidence_delete = false;
bool g_fail_retention_post_index = false;
bool g_fail_retention_pending_restore = false;
bool g_retention_post_validation_evidence_change = false;
base::NoDestructor<base::RepeatingClosure> g_launch_state_gc_pre_delete_hook;
#endif

constexpr wchar_t kRetentionPendingFilename[] = L"retention_pending.json";

base::FilePath GetRetentionPendingPath(const base::FilePath& install_dir) {
  return install_dir.Append(kRetentionPendingFilename);
}

bool ReadRetentionPendingVersions(const base::FilePath& install_dir,
                                  std::set<VersionKey>* versions) {
  versions->clear();
  const base::FilePath path = GetRetentionPendingPath(install_dir);
  if (!base::PathExists(path)) {
    return true;
  }
  std::string json;
  if (ReadFileWithIntegrity(path, &json, IntegrityMismatchAction::kPreserve) !=
      IntegrityResult::kSuccess) {
    return false;
  }
  std::optional<base::DictValue> parsed =
      base::JSONReader::ReadDict(json, base::JSON_PARSE_RFC);
  const base::ListValue* entries =
      parsed ? parsed->FindList("versions") : nullptr;
  if (!entries) {
    return false;
  }
  for (const auto& value : *entries) {
    const base::DictValue* entry = value.GetIfDict();
    const std::string* version = entry ? entry->FindString("version") : nullptr;
    const std::string* platform =
        entry ? entry->FindString("platform") : nullptr;
    Version parsed_version = version ? Version::Parse(*version) : Version{};
    if (!parsed_version.IsValid() || !platform || platform->empty()) {
      return false;
    }
    versions->insert({parsed_version, *platform});
  }
  return true;
}

bool WriteRetentionPendingVersions(const base::FilePath& install_dir,
                                   const std::set<VersionKey>& versions) {
  base::DictValue root;
  base::ListValue entries;
  for (const auto& key : versions) {
    base::DictValue entry;
    entry.Set("version", key.version.ToString());
    entry.Set("platform", key.platform);
    entries.Append(std::move(entry));
  }
  root.Set("versions", std::move(entries));
  std::string json;
  const base::FilePath path = GetRetentionPendingPath(install_dir);
  return base::JSONWriter::Write(root, &json) && VerifySafeFilePath(path) &&
         WriteFileWithIntegrity(path, json);
}

bool RestoreRetentionPendingVersions(const base::FilePath& install_dir,
                                     const std::set<VersionKey>& versions) {
#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
  if (g_fail_retention_pending_restore) {
    return false;
  }
#endif
  if (!versions.empty()) {
    return WriteRetentionPendingVersions(install_dir, versions);
  }
  const base::FilePath path = GetRetentionPendingPath(install_dir);
  return !base::PathExists(path) || base::DeleteFile(path);
}

void CompleteRetentionLogicalCommit(
    const std::set<RetentionRegistrationKey>& removed,
    const RetentionEvidenceSnapshot& evidence_snapshot,
    Result* result) {
  for (const auto& file : evidence_snapshot.files) {
    if (!removed.contains(file.key)) {
      continue;
    }
    ConditionalDeleteResult deleted;
#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
    deleted = g_fail_retention_evidence_delete
                  ? ConditionalDeleteResult::kError
                  : DeleteFileWithIntegrityIfMatching(
                        file.path, file.content, file.integrity_protected,
                        file.resolved_parent_path);
#else
    deleted = DeleteFileWithIntegrityIfMatching(file.path, file.content,
                                                file.integrity_protected,
                                                file.resolved_parent_path);
#endif
    if (deleted == ConditionalDeleteResult::kError) {
      result->outcome = Outcome::kCleanupDeferred;
      result->warnings.push_back("Deferred retention evidence cleanup: " +
                                 file.path.BaseName().AsUTF8Unsafe());
    }
  }
}

// Canonical names for each step, indexed by Step.
constexpr const char* kStepCanonicalNames[] = {
    "initializing",  // kStepInit
    "initializing",  // kStepLock
    "checking",      // kStepVersionCheck
    "checking",      // kStepCdnResolve
    "downloading",   // kStepDownload
    "extracting",    // kStepExtract
    "verifying",     // kStepSignatureVerify
    "installing",    // kStepInstall
    "committing",    // kStepCommitting
    "cleaning",      // kStepCleanup
};

// Determine the trusted root for a version path based on whether it came
// from a bundled directory or an installed (read_dirs/install_dir) location.
base::FilePath FindTrustedRootForVersion(
    const base::FilePath& version_path,
    bool is_bundled,
    const base::FilePath& bundled_base_path,
    const std::vector<base::FilePath>& read_dirs,
    const base::FilePath& install_dir) {
  if (is_bundled) {
    return bundled_base_path.DirName();
  }
  for (const auto& dir : read_dirs) {
    if (dir.IsParent(version_path)) {
      return dir;
    }
  }
  return install_dir;
}

}  // namespace

void ApplyConfigThumbprintOverride(const Config& config,
                                   ExtendedConfig* extended) {
#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
  if (!extended || config.certificate_thumbprint.empty()) {
    return;
  }

  extended->certificate_thumbprint = config.certificate_thumbprint;
  if (config.certificate_thumbprint != std::string(kCefCertificateThumbprint)) {
    internal::SetSignatureTestingMode(true);
  }
#else
  (void)config;
  (void)extended;
#endif
}

void SetRetentionEvidenceDeleteFailureForTesting(bool fail) {
#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
  g_fail_retention_evidence_delete = fail;
#endif
}

void SetRetentionPostIndexFailureForTesting(bool fail) {
#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
  g_fail_retention_post_index = fail;
#endif
}

void SetRetentionPendingRestoreFailureForTesting(bool fail) {
#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
  g_fail_retention_pending_restore = fail;
#endif
}

void SetRetentionPostValidationEvidenceChangeForTesting(bool change) {
#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
  g_retention_post_validation_evidence_change = change;
#endif
}

void SetLaunchStateGcPreDeleteHookForTesting(base::RepeatingClosure callback) {
#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
  *g_launch_state_gc_pre_delete_hook = std::move(callback);
#endif
}

namespace internal {

const char* StepCanonicalName(Step step) {
  if (step < 0 || step >= kNumSteps) {
    return "unknown";
  }
  return kStepCanonicalNames[step];
}

std::string BuildProgressJson(Step step,
                              uint64_t bytes_done,
                              uint64_t bytes_total) {
  base::DictValue dict;
  dict.Set("step_name", StepCanonicalName(step));
  dict.Set("step", static_cast<int>(step));
  dict.Set("total_steps", kNumSteps - 1);
  // Use double for large values since JSON doesn't have 64-bit int
  dict.Set("bytes_done", static_cast<double>(bytes_done));
  dict.Set("bytes_total", static_cast<double>(bytes_total));
  dict.Set("overall_percent",
           CalculateOverallProgress(step, bytes_done, bytes_total));
  dict.Set("message", StepDisplayString(step));

  std::string json;
  base::JSONWriter::Write(dict, &json);
  return json;
}

void ResetProgressNotificationState() {
  g_last_send_time = 0;
  g_last_step = -1;
}

}  // namespace internal

bool SendProgressToParent(HWND parent,
                          Step step,
                          uint64_t bytes_done,
                          uint64_t bytes_total) {
  if (!parent || !IsWindow(parent)) {
    return true;
  }

  // Rate limiting: track last send time
  DWORD now = GetTickCount();

  // Always send on step changes or if enough time has passed
  bool is_step_change = (static_cast<int>(step) != g_last_step);

  if (!is_step_change && (now - g_last_send_time) < kProgressUpdateIntervalMs) {
    return true;  // Skipped due to rate limiting - not a cancellation
  }

  g_last_send_time = now;
  g_last_step = static_cast<int>(step);

  std::string json = internal::BuildProgressJson(step, bytes_done, bytes_total);

  COPYDATASTRUCT cds = {};
  cds.dwData = kWmCopyDataInstallerProgress;
  cds.cbData = static_cast<DWORD>(json.size() + 1);  // Include null terminator
  cds.lpData = const_cast<char*>(json.c_str());

  DWORD_PTR result = 0;
  // 500ms timeout to prevent blocking if receiver is hung
  LRESULT send_result = SendMessageTimeoutW(parent, WM_COPYDATA, 0,
                                            reinterpret_cast<LPARAM>(&cds),
                                            SMTO_ABORTIFHUNG, 500, &result);

  if (send_result == 0) {
    DWORD error = GetLastError();
    if (error == ERROR_TIMEOUT) {
      Logger::GetInstance().Warning(
          "Progress notification timed out - parent window may be hung");
    }
    // Timeout or error is not a cancellation - continue the operation.
    return true;
  }

  // Parent returns kWmCopyDataResultCancel to request cancellation.
  return static_cast<LRESULT>(result) != kWmCopyDataResultCancel;
}

int CalculateOverallProgress(Step step,
                             uint64_t bytes_done,
                             uint64_t bytes_total) {
  // Step boundaries (step -> start%, end%)
  constexpr int kStepStart[] = {0, 3, 5, 10, 15, 55, 80, 90, 95, 95};
  constexpr int kStepEnd[] = {3, 5, 10, 15, 55, 80, 90, 95, 95, 100};
  static_assert(std::size(kStepStart) == kNumSteps, "step array mismatch");
  static_assert(std::size(kStepEnd) == kNumSteps, "step array mismatch");

  int s = (step < 0)            ? 0
          : (step >= kNumSteps) ? kNumSteps - 1
                                : static_cast<int>(step);
  int step_start = kStepStart[s];
  int step_end = kStepEnd[s];

  if (bytes_total > 0) {
    // Interpolate within the step based on byte progress
    double byte_fraction =
        static_cast<double>(bytes_done) / static_cast<double>(bytes_total);
    return step_start +
           static_cast<int>((step_end - step_start) * byte_fraction);
  }

  // At the start of this step
  return step_start;
}

namespace {

// Report progress to callback if provided.
bool ReportProgress(const ProgressCallback& callback,
                    Step step,
                    uint64_t bytes_done = 0,
                    uint64_t bytes_total = 0) {
  if (callback) {
    return callback.Run(step, bytes_done, bytes_total);
  }
  return true;  // Continue if no callback
}

// Testing mode state - allows HTTP and suppresses error dialogs.
// No-op in official release builds.
// Declared here (before FetchRevocationList) so IsTestingMode() is available.
#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
bool g_testing_mode = false;
#endif

#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
std::optional<EmergencyRecoveryScanLimits>
    g_emergency_recovery_limits_for_testing;
#endif

EmergencyRecoveryScanLimits GetEmergencyRecoveryScanLimits() {
#if defined(OFFICIAL_BUILD) && defined(NDEBUG)
  return EmergencyRecoveryScanLimits();
#else
  return g_emergency_recovery_limits_for_testing.value_or(
      EmergencyRecoveryScanLimits());
#endif
}

bool IsTestingMode() {
#if defined(OFFICIAL_BUILD) && defined(NDEBUG)
  return false;
#else
  return g_testing_mode;
#endif
}

bool ShouldIgnoreCertificateErrorsForTesting() {
#if defined(OFFICIAL_BUILD) && defined(NDEBUG)
  return false;
#else
  if (IsTestingMode()) {
    return true;
  }
  return base::Environment::Create()->HasVar(
      "CEF_INSTALLER_IGNORE_CERTIFICATE_ERRORS_FOR_TESTING");
#endif
}

// Fetch revocation list from CDN and merge with compiled baseline.
// The compiled baseline is always included — CDN data can only add entries.
// The CDN delta (entries not in the compiled list) is written to disk cache
// so offline code paths can benefit from it.
std::vector<RevokedVersionRange> FetchRevocationList(
    const ExtendedConfig& extended,
    const EffectiveDownloadSource& source,
    const base::FilePath& install_dir,
    const std::vector<RevokedVersionRange>& effective,
    bool launch_budget = false,
    bool* fetched = nullptr) {
  if (fetched) {
    *fetched = false;
  }
  if (source.downloads_disabled()) {
    return effective;
  }
  const std::vector<std::string>& bases = source.urls;

  const base::FilePath cache_dir = GetCacheDirectory(install_dir);
  DownloadOptions opts;
  opts.max_download_size = kMaxRevocationDownloadSize;
  opts.allow_http_for_testing = IsTestingMode();
  opts.ignore_certificate_errors_for_testing =
      ShouldIgnoreCertificateErrorsForTesting();
  if (launch_budget) {
    opts.connect_timeout_ms = kLaunchRevocationConnectTimeoutMs;
    opts.receive_timeout_ms = kLaunchRevocationReceiveTimeoutMs;
  }
  opts.local_download_path = source.mirror_path;

  std::vector<RevokedVersionRange> cdn_fetched;
  const base::TimeTicks deadline =
      launch_budget ? base::TimeTicks::Now() +
                          base::Milliseconds(kLaunchRevocationOverallTimeoutMs)
                    : base::TimeTicks();
  DownloadError last_error = DownloadError::kNetworkError;
  std::optional<RevocationError> last_parse_error;
  bool external_success = false;
  bool cache_success = false;
  bool cache_rejected = false;
  for (const auto& base_url : bases) {
    std::string content;
    const std::string url = BuildRevocationListUrl(base_url);
    DownloadContentSource content_source = DownloadContentSource::kNetwork;
    if (launch_budget) {
      const base::TimeDelta remaining = deadline - base::TimeTicks::Now();
      if (!remaining.is_positive()) {
        break;
      }
      last_error = DownloadToStringWithDeadline(url, &content, remaining, opts);
    } else {
      last_error = DownloadWithCache(
          url, cache_dir, &content, opts,
          extended.force_check || cache_rejected, StaleCacheFallback::kSkip,
          kRevocationListPath, CacheWriteBehavior::kDefer, &content_source);
    }
    if (last_error == DownloadError::kSuccess) {
      const RevocationError parse_error =
          ParseRevocationList(content, &cdn_fetched);
      if (parse_error == RevocationError::kSuccess) {
        external_success = true;
        if (!launch_budget && source.mirror_path.empty() &&
            content_source == DownloadContentSource::kNetwork) {
          WriteDownloadCache(cache_dir, kRevocationListPath, content);
        }
        break;
      }
      last_parse_error = parse_error;
      if (content_source == DownloadContentSource::kCache) {
        cache_rejected = true;
        DiscardDownloadCache(cache_dir, kRevocationListPath);
      }
    }
    cdn_fetched.clear();
  }

  // Only after every origin has failed may one source-neutral stale cache
  // entry satisfy the request. Launch refresh uses the separate effective
  // revocation cache and never consults the download cache.
  if (!external_success && !launch_budget && !extended.force_check &&
      !cache_rejected) {
    std::string content;
    if (ReadDownloadCache(cache_dir, kRevocationListPath, &content) ==
        DownloadError::kSuccess) {
      const RevocationError parse_error =
          ParseRevocationList(content, &cdn_fetched);
      cache_success = parse_error == RevocationError::kSuccess;
      if (!cache_success) {
        last_parse_error = parse_error;
        cdn_fetched.clear();
      }
    }
  }
  if (!external_success && !cache_success) {
    if (last_parse_error) {
      Logger::GetInstance().Warning(
          "Failed to parse revocation list: " +
          std::string(RevocationErrorToString(*last_parse_error)));
    } else {
      Logger::GetInstance().Warning(
          "Failed to fetch revocation list: " +
          std::string(DownloadErrorToString(last_error)));
    }
    return effective;
  }
  if (fetched && external_success) {
    *fetched = true;
  }

  // Persist only data retrieved from an authoritative external source. A
  // stale download-cache fallback is useful for this operation but does not
  // claim a durable refresh.
  if (external_success &&
      (source.mirror_path.empty() || source.persist_revocations())) {
    RevocationError write_err = WriteRevocationCache(install_dir, cdn_fetched);
    if (write_err != RevocationError::kSuccess) {
      Logger::GetInstance().Warning(
          "Failed to write revocation cache: " +
          std::string(RevocationErrorToString(write_err)));
    }
  }

  return MergeRevocationLists(effective, cdn_fetched);
}

std::vector<RevokedVersionRange> LoadQueryRevocations(
    const ExtendedConfig& extended,
    const std::vector<base::FilePath>& read_dirs) {
  std::vector<RevokedVersionRange> effective =
      LoadEffectiveRevocationList(read_dirs);
  if (extended.local_download_path.empty()) {
    return effective;
  }
  base::FilePath path =
      base::FilePath::FromUTF8Unsafe(extended.local_download_path)
          .AppendASCII("revoked.json");
  std::string content;
  std::vector<RevokedVersionRange> local;
  if (base::ReadFileToString(path, &content) &&
      ParseRevocationList(content, &local) == RevocationError::kSuccess) {
    return MergeRevocationLists(effective, local);
  }
  return effective;
}

}  // namespace

#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
void SetEmergencyRecoveryScanLimitsForTesting(
    std::optional<EmergencyRecoveryScanLimits> limits) {
  g_emergency_recovery_limits_for_testing = std::move(limits);
}
#endif

namespace internal {

#if defined(OFFICIAL_BUILD) && defined(NDEBUG)
void SetTestingMode(bool enabled) {}
#else
void SetTestingMode(bool enabled) {
  g_testing_mode = enabled;
}
#endif

// ============================================================================
// Result Implementation
// ============================================================================

const char* ExitCodeToString(int code) {
  switch (code) {
    case kExitCodeSuccess:
      return "SUCCESS";
    case kExitCodeConfigError:
      return "CONFIG_ERROR";
    case kExitCodeNetworkError:
      return "NETWORK_ERROR";
    case kExitCodeSignatureError:
      return "SIGNATURE_ERROR";
    case kExitCodeNoMatchingVersion:
      return "NO_MATCHING_VERSION";
    case kExitCodeExtractionError:
      return "EXTRACTION_ERROR";
    case kExitCodeInstallError:
      return "INSTALL_ERROR";
    case kExitCodeDatabaseError:
      return "DATABASE_ERROR";
    case kExitCodeLockTimeout:
      return "LOCK_TIMEOUT";
    case kExitCodeCancelled:
      return "CANCELLED";
    case kExitCodeRelaunched:
      return "RELAUNCHED";
    case kExitCodeNoSentinel:
      return "NO_SENTINEL";
    case kExitCodeSentinelReadError:
      return "SENTINEL_READ_ERROR";
    case kExitCodeSentinelOwnerMismatch:
      return "SENTINEL_OWNER_MISMATCH";
    case kExitCodePolicyDenied:
      return "POLICY_DENIED";
    case kExitCodeIndexError:
      return "INDEX_ERROR";
    case kExitCodeRecoveryError:
      return "RECOVERY_ERROR";
    case kExitCodeRepairError:
      return "REPAIR_ERROR";
    case kExitCodeQuarantineError:
      return "QUARANTINE_ERROR";
    case kExitCodeRetentionSnapshotChanged:
      return "RETENTION_SNAPSHOT_CHANGED";
    case kExitCodePolicyError:
      return "POLICY_ERROR";
    default:
      return "UNKNOWN_ERROR";
  }
}

int StringToExitCode(const std::string& str) {
  if (str == "CONFIG_ERROR") {
    return kExitCodeConfigError;
  }
  if (str == "NETWORK_ERROR") {
    return kExitCodeNetworkError;
  }
  if (str == "SIGNATURE_ERROR") {
    return kExitCodeSignatureError;
  }
  if (str == "NO_MATCHING_VERSION") {
    return kExitCodeNoMatchingVersion;
  }
  if (str == "EXTRACTION_ERROR") {
    return kExitCodeExtractionError;
  }
  if (str == "INSTALL_ERROR") {
    return kExitCodeInstallError;
  }
  if (str == "DATABASE_ERROR") {
    return kExitCodeDatabaseError;
  }
  if (str == "LOCK_TIMEOUT") {
    return kExitCodeLockTimeout;
  }
  if (str == "CANCELLED") {
    return kExitCodeCancelled;
  }
  if (str == "RELAUNCHED") {
    return kExitCodeRelaunched;
  }
  if (str == "NO_SENTINEL") {
    return kExitCodeNoSentinel;
  }
  if (str == "SENTINEL_READ_ERROR") {
    return kExitCodeSentinelReadError;
  }
  if (str == "SENTINEL_OWNER_MISMATCH") {
    return kExitCodeSentinelOwnerMismatch;
  }
  if (str == "POLICY_DENIED") {
    return kExitCodePolicyDenied;
  }
  if (str == "INDEX_ERROR") {
    return kExitCodeIndexError;
  }
  if (str == "RECOVERY_ERROR") {
    return kExitCodeRecoveryError;
  }
  if (str == "REPAIR_ERROR") {
    return kExitCodeRepairError;
  }
  if (str == "QUARANTINE_ERROR") {
    return kExitCodeQuarantineError;
  }
  if (str == "RETENTION_SNAPSHOT_CHANGED") {
    return kExitCodeRetentionSnapshotChanged;
  }
  if (str == "POLICY_ERROR") {
    return kExitCodePolicyError;
  }
  if (str == "SUCCESS") {
    return kExitCodeSuccess;
  }
  return kExitCodeUnknownError;
}

}  // namespace internal

const char* OutcomeToString(Outcome outcome) {
  switch (outcome) {
    case Outcome::kCommitted:
      return "committed";
    case Outcome::kCleanupDeferred:
      return "cleanup_deferred";
    case Outcome::kFailed:
      return "failed";
  }
}

namespace internal {

std::optional<Outcome> StringToOutcome(const std::string& str) {
  if (str == "committed") {
    return Outcome::kCommitted;
  }
  if (str == "cleanup_deferred") {
    return Outcome::kCleanupDeferred;
  }
  if (str == "failed") {
    return Outcome::kFailed;
  }
  return std::nullopt;
}

}  // namespace internal

std::string Result::ToJson() const {
  std::string json;
  base::JSONWriter::Write(internal::BuildResultJsonValue(*this), &json);
  return json;
}

// static
std::optional<Result> Result::FromJson(const std::string& json) {
  std::optional<base::DictValue> parsed =
      base::JSONReader::ReadDict(json, base::JSON_PARSE_RFC);
  if (!parsed) {
    return std::nullopt;
  }

  Result result;

  std::optional<bool> success = parsed->FindBool("success");
  if (!success.has_value()) {
    return std::nullopt;
  }
  result.success = *success;

  const std::string* outcome_name = parsed->FindString("outcome");
  if (!outcome_name) {
    return std::nullopt;
  }
  std::optional<Outcome> outcome = internal::StringToOutcome(*outcome_name);
  if (!outcome || (result.success && *outcome == Outcome::kFailed) ||
      (!result.success && *outcome != Outcome::kFailed)) {
    return std::nullopt;
  }
  result.outcome = *outcome;

  if (!result.success) {
    std::optional<int> error_code = parsed->FindInt("error_code");
    const std::string* error_name = parsed->FindString("error_name");
    const std::string* error_message = parsed->FindString("error_message");
    if (!error_code || !error_name || !error_message ||
        *error_name != internal::ExitCodeToString(*error_code)) {
      return std::nullopt;
    }
    result.error_code = *error_code;
    result.error_message = *error_message;
  } else if (parsed->contains("error_code") || parsed->contains("error_name") ||
             parsed->contains("error_message")) {
    return std::nullopt;
  }

  if (const std::string* libcef_path = parsed->FindString("libcef_path")) {
    result.libcef_path = base::FilePath::FromUTF8Unsafe(*libcef_path);
  }

  if (const std::string* version = parsed->FindString("installed_version")) {
    result.installed_version = *version;
  }

  if (const std::string* vfull = parsed->FindString("version_full")) {
    if (vfull->size() <= kMaxVersionFullLength) {
      result.version_full = *vfull;
    }
  }

  result.is_bundled = parsed->FindBool("is_bundled").value_or(false);

  if (const base::ListValue* registrations =
          parsed->FindList("registrations")) {
    std::optional<int> max_age_days = parsed->FindInt("max_age_days");
    std::optional<bool> registrations_committed =
        parsed->FindBool("registrations_committed");
    std::optional<bool> versions_pruned = parsed->FindBool("versions_pruned");
    std::optional<bool> retry_required = parsed->FindBool("retry_required");
    std::optional<bool> eligible = parsed->FindBool("eligible");
    std::optional<bool> store_blocked = parsed->FindBool("store_blocked");
    const base::ListValue* versions = parsed->FindList("versions");
    if (!max_age_days || !IsValidRetentionMaxAgeDays(*max_age_days) ||
        !registrations_committed || !versions_pruned || !retry_required ||
        !eligible || !store_blocked || !versions) {
      return std::nullopt;
    }
    RetentionPlan plan;
    plan.eligible = *eligible;
    plan.store_blocked = *store_blocked;
    plan.blocker = parsed->FindString("store_blocker")
                       ? *parsed->FindString("store_blocker")
                       : "";
    for (const auto& value : *registrations) {
      const base::DictValue* item = value.GetIfDict();
      if (!item) {
        return std::nullopt;
      }
      const std::string* appid = item->FindString("appid");
      const std::string* platform = item->FindString("platform");
      const std::string* vmin = item->FindString("vmin");
      const std::string* vmax = item->FindString("vmax");
      const std::string* abi_hash = item->FindString("abi_hash");
      const std::string* evidence_kind = item->FindString("evidence_kind");
      const std::string* decision = item->FindString("decision");
      const std::string* reason = item->FindString("reason");
      if (!appid || !platform || !vmin || !vmax || !abi_hash ||
          !evidence_kind || !decision || !reason) {
        return std::nullopt;
      }
      RetentionRegistrationReport report;
      report.entry = {*appid, *platform, *vmin, *vmax, *abi_hash};
      if (*evidence_kind == "health_sentinel") {
        report.evidence.kind = RetentionEvidenceKind::kHealthSentinel;
      } else if (*evidence_kind == "liveness") {
        report.evidence.kind = RetentionEvidenceKind::kLiveness;
      } else if (*evidence_kind != "none") {
        return std::nullopt;
      }
      if (const std::string* timestamp = item->FindString("evidence_time");
          timestamp &&
          !base::StringToUint64(*timestamp, &report.evidence.timestamp)) {
        return std::nullopt;
      }
      if (const std::string* age = item->FindString("age_days")) {
        uint64_t parsed_age = 0;
        if (!base::StringToUint64(*age, &parsed_age)) {
          return std::nullopt;
        }
        report.age_days = parsed_age;
      }
      if (*decision == "reclaim") {
        report.decision = RetentionRegistrationDecision::kReclaim;
        plan.candidates.push_back({*appid, *platform});
      } else if (*decision != "protected") {
        return std::nullopt;
      }
      std::optional<RetentionReason> parsed_reason =
          RetentionReasonFromString(*reason);
      if (!parsed_reason) {
        return std::nullopt;
      }
      report.reason = *parsed_reason;
      if (const std::string* diagnostic = item->FindString("diagnostic")) {
        report.evidence.diagnostic = *diagnostic;
      }
      plan.registrations.push_back(std::move(report));
    }
    for (const auto& value : *versions) {
      const base::DictValue* item = value.GetIfDict();
      if (!item) {
        return std::nullopt;
      }
      const std::string* version = item->FindString("version");
      const std::string* platform = item->FindString("platform");
      const std::string* decision = item->FindString("decision");
      const std::string* reason = item->FindString("reason");
      std::optional<bool> required_before = item->FindBool("required_before");
      std::optional<bool> required_after = item->FindBool("required_after");
      std::optional<bool> expected_removal = item->FindBool("expected_removal");
      std::optional<bool> cleanup_deferred = item->FindBool("cleanup_deferred");
      if (!version || !platform || !decision || !reason || !required_before ||
          !required_after || !expected_removal || !cleanup_deferred) {
        return std::nullopt;
      }
      RetentionVersionReport report;
      report.version = Version::Parse(*version);
      if (!report.version.IsValid()) {
        return std::nullopt;
      }
      report.platform = *platform;
      report.required_before = *required_before;
      report.required_after = *required_after;
      report.expected_removal = *expected_removal;
      report.cleanup_deferred = *cleanup_deferred;
      std::optional<RetentionReason> parsed_reason =
          RetentionReasonFromString(*reason);
      if (!parsed_reason) {
        return std::nullopt;
      }
      report.reason = *parsed_reason;
      if (*decision == "retain_required") {
        report.decision = RetentionVersionDecision::kRetainRequired;
      } else if (*decision == "newly_reclaimable") {
        report.decision = RetentionVersionDecision::kNewlyReclaimable;
      } else if (*decision == "already_unreferenced") {
        report.decision = RetentionVersionDecision::kAlreadyUnreferenced;
      } else if (*decision == "revoked") {
        report.decision = RetentionVersionDecision::kRevoked;
      } else if (*decision == "confirmed_protected") {
        report.decision = RetentionVersionDecision::kConfirmedProtected;
      } else {
        return std::nullopt;
      }
      plan.versions.push_back(std::move(report));
    }
    result.retention_plan = std::move(plan);
    result.retention_max_age_days = *max_age_days;
    result.registrations_committed = *registrations_committed;
    result.versions_pruned = *versions_pruned;
    result.retry_required = *retry_required;
  }

  if (const base::ListValue* warning_list = parsed->FindList("warnings")) {
    for (const auto& value : *warning_list) {
      const std::string* warning = value.GetIfString();
      if (!warning) {
        return std::nullopt;
      }
      result.warnings.push_back(*warning);
    }
  }
  if ((!result.warnings.empty()) !=
      (result.outcome == Outcome::kCleanupDeferred)) {
    return std::nullopt;
  }

  return result;
}

// static
Result Result::Success(const base::FilePath& libcef_path,
                       const std::string& version,
                       const std::string& version_full,
                       bool is_bundled) {
  Result result;
  result.success = true;
  result.outcome = Outcome::kCommitted;
  result.libcef_path = libcef_path;
  result.is_bundled = is_bundled;
  result.installed_version = version;
  result.version_full = version_full;
  return result;
}

// static
Result Result::Error(int code, const std::string& message) {
  Result result;
  result.success = false;
  result.outcome = Outcome::kFailed;
  result.error_code = code;
  result.error_message = message;
  return result;
}

namespace internal {

#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
bool IsCandidateRetryableArchiveErrorForTesting(ArchiveError error) {
  return IsCandidateSpecificArchiveError(error);
}

bool IsCandidateRetryableSignatureErrorForTesting(SignatureError error) {
  return IsCandidateSpecificSignatureError(error);
}

bool IsCandidateRetryableMetadataErrorForTesting(MetadataError error) {
  return IsCandidateSpecificMetadataError(error);
}
#endif

// ============================================================================
// ExtendedConfig Parsing
// ============================================================================

bool ParseExtendedConfigFromJson(const std::string& json,
                                 ExtendedConfig* config,
                                 std::string* diagnostic) {
  if (diagnostic) {
    diagnostic->clear();
  }
  auto fail = [diagnostic](const char* message) {
    if (diagnostic) {
      *diagnostic = message;
    }
    return false;
  };
  if (!config) {
    return fail("Extended config output is null");
  }

  std::optional<base::DictValue> parsed =
      base::JSONReader::ReadDict(json, base::JSON_PARSE_RFC);
  if (!parsed) {
    return fail("Malformed extended config JSON");
  }

  // All fields are optional - use defaults if not present.
  const base::Value* cdn_urls = parsed->Find(config_fields::kCdnUrlsField);
  if (!cdn_urls) {
    config->cdn_urls.clear();
  } else if (!cdn_urls->is_list()) {
    config->cdn_urls.clear();
    return fail("cdn_urls must be an array");
  } else {
    std::vector<std::string> values;
    values.reserve(cdn_urls->GetList().size());
    for (const auto& value : cdn_urls->GetList()) {
      if (!value.is_string()) {
        config->cdn_urls.clear();
        return fail("cdn_urls entries must be strings");
      }
      values.push_back(value.GetString());
    }
    std::string url_diagnostic;
    if (!ValidateAndNormalizeCdnUrls(values, &config->cdn_urls,
                                     &url_diagnostic)) {
      config->cdn_urls.clear();
      if (diagnostic) {
        *diagnostic = "Invalid cdn_urls: " + url_diagnostic;
      }
      return false;
    }
  }

  if (const std::string* install_path =
          parsed->FindString(config_fields::kInstallPathField)) {
    config->install_path = *install_path;
  }

  if (const std::string* bundled =
          parsed->FindString(config_fields::kBundledCefPathField)) {
    config->bundled_cef_path = *bundled;
  }

  if (const std::string* thumbprint =
          parsed->FindString(config_fields::kCertificateThumbprintField);
      thumbprint && !thumbprint->empty()) {
    config->certificate_thumbprint = *thumbprint;
  }

  if (std::optional<bool> val =
          parsed->FindBool(config_fields::kForceCheckField)) {
    config->force_check = *val;
  }

  if (std::optional<bool> val =
          parsed->FindBool(config_fields::kShowProgressUiField)) {
    config->show_progress_ui = *val;
  }

  // Prefer a base-10 string so pointer-width values never pass through a
  // potentially rounded JSON double. Exactly represented numeric values remain
  // accepted for compatibility.
  if (const base::Value* parent =
          parsed->Find(config_fields::kParentWindowField)) {
    uint64_t value = 0;
    if (parent->is_string()) {
      if (!base::StringToUint64(parent->GetString(), &value)) {
        return fail("parent_window must be a base-10 unsigned integer string");
      }
    } else if (parent->is_int()) {
      const int int_value = parent->GetInt();
      if (int_value < 0) {
        return fail("parent_window must not be negative");
      }
      value = static_cast<uint64_t>(int_value);
    } else if (parent->is_double()) {
      const double number = parent->GetDouble();
      double max_value =
          static_cast<double>(std::numeric_limits<uintptr_t>::max());
      if constexpr (sizeof(uintptr_t) == sizeof(uint64_t)) {
        // UINT64_MAX rounds up when converted to double.
        max_value = std::nextafter(max_value, 0.0);
      }
      if (!std::isfinite(number) || number < 0 ||
          std::trunc(number) != number || number > max_value) {
        return fail(
            "parent_window number must be finite, non-negative, integral, "
            "and exactly representable as uintptr_t");
      }
      value = static_cast<uint64_t>(number);
      if (static_cast<double>(value) != number) {
        return fail("parent_window number was rounded");
      }
    } else {
      return fail("parent_window must be a base-10 string or number");
    }
    if (value > std::numeric_limits<uintptr_t>::max()) {
      return fail("parent_window exceeds uintptr_t");
    }
    config->parent_window =
        reinterpret_cast<HWND>(static_cast<uintptr_t>(value));
  }

  if (const std::string* local_path =
          parsed->FindString(config_fields::kLocalDownloadPathField)) {
    config->local_download_path = *local_path;
  }

  if (const std::string* level_str =
          parsed->FindString(config_fields::kLogLevelField)) {
    auto level = LogLevelFromString(*level_str);
    if (level.has_value()) {
      config->log_level = level.value();
    }
  }

  if (std::optional<int> val =
          parsed->FindInt(config_fields::kDownloadTimeoutMsField)) {
    config->download_timeout_ms = *val;
  }

  if (const base::Value* value =
          parsed->Find(config_fields::kMaxAgeDaysField)) {
    if (!value->is_int() || !IsValidRetentionMaxAgeDays(value->GetInt())) {
      return fail("max_age_days must be an integer from 90 through 3650");
    }
    config->retention_max_age_days = value->GetInt();
  }

  return true;
}

bool ParseCombinedConfig(const std::string& json,
                         Config* config,
                         ExtendedConfig* extended,
                         std::string* diagnostic) {
  if (!config || !extended) {
    return false;
  }

  // unchecked_cef_path is not allowed here — it is a bootstrap-only concept
  // (resolved against the client DLL directory in MaybeRunInstaller). The
  // RunInstaller export is for install/update/query operations that should go
  // through the normal installer flow. bundled_cef_path is allowed so
  // RunInstaller callers can specify a bundled version.
  ConfigError config_err = ParseConfigFromJson(
      json, config, {.allow_bundled_cef_path = true}, diagnostic);
  if (config_err != ConfigError::kSuccess) {
    if (diagnostic && diagnostic->empty()) {
      *diagnostic = ConfigErrorToString(config_err);
    }
    return false;
  }

  // Parse extended fields from the same JSON
  if (!ParseExtendedConfigFromJson(json, extended, diagnostic)) {
    return false;
  }

  // Both parsers read bundled_cef_path from the same JSON. The controller
  // reads it from ExtendedConfig, so clear the Config copy to avoid
  // ambiguity.
  config->bundled_cef_path.clear();
  // cdn_urls in combined JSON is operation-specific. Keep only the validated
  // ExtendedConfig copy so application-source precedence remains explicit.
  config->cdn_urls.clear();
  return true;
}

std::optional<Command> ParseCommand(const std::string& str) {
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
  if (lower == "retention_dry_run") {
    return Command::kRetentionDryRun;
  }
  if (lower == "retention_apply") {
    return Command::kRetentionApply;
  }
  if (lower == "launch_success") {
    return Command::kLaunchSuccess;
  }
  return std::nullopt;
}

Result HandleLaunchSuccess() {
  // Resolve the sentinel path published by the bootstrap before RunWinMain.
  return HandleLaunchSuccess(GetActiveLaunchStatePath());
}

Result HandleLaunchSuccess(const base::FilePath& path) {
  if (path.empty()) {
    // Not running under the bootstrap, or a bundled/unchecked path that does
    // not participate in launch health. Not an error condition for clients.
    return Result::Error(kExitCodeNoSentinel,
                         "No active launch state — not running under "
                         "the bootstrap launch health path");
  }

  std::optional<LaunchState> ls = ReadLaunchStatePath(path);
  if (!ls) {
    return Result::Error(kExitCodeSentinelReadError,
                         "Launch state sentinel missing or corrupt");
  }

  // Already confirmed (e.g. a prior launch_success call). Idempotent — skip
  // the write.
  if (!ls->running) {
    Result result;
    result.success = true;
    result.outcome = Outcome::kCommitted;
    return result;
  }

  // Verify this process owns the sentinel before mutating it.
  if (ls->pid != ::GetCurrentProcessId() ||
      ls->pid_start_time != GetCurrentPidStartTime()) {
    return Result::Error(kExitCodeSentinelOwnerMismatch,
                         "Launch state sentinel owned by another process");
  }

  // Confirm the version: clear running and reset the failure count, preserving
  // all other fields (appid, pid, pid_start_time, version, platform).
  ls->running = false;
  ls->consecutive_failures = 0;
  ls->confirmed = true;
  if (!WriteLaunchStatePath(path, *ls)) {
    return Result::Error(kExitCodeSentinelReadError,
                         "Failed to write launch state confirmation");
  }

  // Cleanup of older-version .launch_* files is deferred to the post-exit
  // path in bootstrap_win.cc; this keeps the call lightweight.
  Result result;
  result.success = true;
  result.outcome = Outcome::kCommitted;
  return result;
}

}  // namespace internal

int ResultToExitCode(const Result& result) {
  return result.success ? kExitCodeSuccess : result.error_code;
}

// ============================================================================
// Controller Implementation
// ============================================================================

namespace {

struct VersionRootRead {
  base::FilePath root;
  MetadataError index_error = MetadataError::kFileNotFound;
  std::vector<InstalledVersion> indexed;
  std::vector<InstalledVersion> recovered;
};

std::vector<VersionRootRead> ReadVersionRoots(
    const std::vector<base::FilePath>& directories) {
  std::vector<VersionRootRead> roots;
  roots.reserve(directories.size());
  for (const auto& directory : directories) {
    VersionRootRead root;
    root.root = directory;
    root.index_error = ReadVersionIndex(directory, &root.indexed,
                                        VersionIndexReadMode::kPreserveCorrupt);
    roots.push_back(std::move(root));
  }
  return roots;
}

std::vector<InstalledVersion> MergeVersionRoots(
    const std::vector<VersionRootRead>& roots,
    bool include_recovered) {
  std::vector<InstalledVersion> merged;
  for (const auto& root : roots) {
    const std::vector<InstalledVersion>* versions = nullptr;
    if (root.index_error == MetadataError::kSuccess) {
      versions = &root.indexed;
    } else if (include_recovered) {
      versions = &root.recovered;
    } else {
      continue;
    }
    for (const auto& version : *versions) {
      const bool is_recovered = root.index_error != MetadataError::kSuccess;
      const bool is_canonical =
          version.path == GetVersionPath(root.root, version.metadata.version,
                                         version.metadata.platform);
      const bool is_complete =
          is_recovered ||
          ValidateDistribution(root.root, version.path, version.metadata) ==
              DistributionValidation::kComplete;
      if (!is_canonical || !is_complete) {
        if (is_recovered) {
          Logger::GetInstance().Warning(
              "Rejected an invalid emergency startup recovery candidate: " +
              version.path.AsUTF8Unsafe());
        }
        continue;
      }
      merged.push_back(version);
    }
  }
  return merged;
}

bool IsEmergencyRecoveryEligibleIndexError(MetadataError error) {
  switch (error) {
    case MetadataError::kFileNotFound:
    case MetadataError::kJsonParseError:
    case MetadataError::kMissingRequiredField:
    case MetadataError::kIndexValidationError:
    case MetadataError::kIntegrityMismatch:
      return true;
    case MetadataError::kSuccess:
    case MetadataError::kFileReadError:
    case MetadataError::kFileWriteError:
      return false;
  }
  return false;
}

}  // namespace

// static
std::vector<InstalledVersion> Controller::ReadMultipleVersionIndexes(
    const std::vector<base::FilePath>& directories,
    bool allow_scan_fallback,
    VersionIndexReadMode read_mode) {
  std::vector<InstalledVersion> merged;
  std::set<std::pair<std::string, std::string>> seen;

  for (const auto& dir : directories) {
    // Prefer the index file; fall back to directory scan if the index is
    // missing or corrupt (e.g., first install, or written by an older
    // installer that didn't create the index).
    std::vector<InstalledVersion> versions;
    if (ReadVersionIndex(dir, &versions, read_mode) !=
            MetadataError::kSuccess &&
        allow_scan_fallback) {
      versions = ScanInstalledVersionsWithMetadata(dir);
    }
    for (auto& iv : versions) {
      if (ValidateDistribution(dir, iv.path, iv.metadata) !=
          DistributionValidation::kComplete) {
        continue;
      }
      auto key =
          std::make_pair(iv.metadata.version.ToString(), iv.metadata.platform);
      if (seen.insert(std::move(key)).second) {
        merged.push_back(std::move(iv));
      }
    }
  }

  return merged;
}

Controller::Controller() {
  // Ensure the logger is available from the start so that all code paths
  // through the controller have logging. Uses the temp directory
  // as a fallback; Run() re-initializes with the real install directory once
  // it is known.
  if (!Logger::GetInstance().IsInitialized()) {
    LoggerConfig log_config;
    base::GetTempDir(&log_config.log_directory);
    Logger::GetInstance().Initialize(log_config);
  }
}

Controller::~Controller() {
  ReleaseLock();
}

Result Controller::MakeValidatedLibcefResult(const base::FilePath& trusted_root,
                                             const base::FilePath& version_dir,
                                             const std::string& version_str,
                                             const std::string& version_full,
                                             bool is_bundled) {
  base::FilePath libcef_path = GetLibcefPath(version_dir);
  if (!IsPathSafeForLoading(trusted_root, libcef_path)) {
    Logger::GetInstance().Error("Reparse point detected in libcef path: " +
                                libcef_path.AsUTF8Unsafe());
    return Result::Error(kExitCodeInstallError,
                         "Reparse point detected in libcef path");
  }
  return Result::Success(libcef_path, version_str, version_full, is_bundled);
}

Result Controller::Run(Command command,
                       const Config& app_config,
                       const ExtendedConfig& extended_config,
                       ProgressCallback progress,
                       ExecutionContext context) {
  return RunImpl(command, app_config, extended_config, std::move(progress),
                 context, nullptr);
}

Result Controller::RunPreparedUninstall(const Config& app_config,
                                        const ExtendedConfig& extended_config,
                                        const PreparedUninstall& prepared,
                                        ProgressCallback progress) {
  return RunImpl(Command::kUninstall, app_config, extended_config,
                 std::move(progress), ExecutionContext::kExplicitCommand,
                 &prepared);
}

Result Controller::RunImpl(Command command,
                           const Config& app_config,
                           const ExtendedConfig& extended_config,
                           ProgressCallback progress,
                           ExecutionContext context,
                           const PreparedUninstall* prepared_uninstall) {
  if (prepared_uninstall &&
      (command != Command::kUninstall ||
       prepared_uninstall->command != Command::kUninstall ||
       prepared_uninstall->execution != UninstallExecution::kInProcess ||
       context != ExecutionContext::kExplicitCommand ||
       prepared_uninstall->config_binding != ConfigToJson(app_config) ||
       prepared_uninstall->install_path != extended_config.install_path)) {
    return Result::Error(kExitCodeConfigError,
                         "Prepared uninstall configuration mismatch");
  }

  cancelled_ = false;
  version_published_ = false;
  pending_archive_cleanup_path_.clear();
  effective_revocations_.clear();

  // Short-circuit: if unchecked_cef_path is set and this is not an uninstall,
  // check for libcef.dll at the specified path and return immediately if found.
  // No version, ABI, platform, signature, or revocation checks are performed.
  const bool retention_command = command == Command::kRetentionDryRun ||
                                 command == Command::kRetentionApply;
  const bool retention_dry_run = command == Command::kRetentionDryRun;
  if (retention_command) {
    // Directory ownership is not known yet. Keep retention resolution and
    // provisioning rejection free of file-log writes; eligible apply
    // re-enables store logging after the ownership and writability gates.
    LoggerConfig log_config;
    log_config.enable_file_log = false;
    Logger::GetInstance().Initialize(log_config);
  } else {
    Logger::GetInstance().LogOperationStart(command, app_config.appid);
  }
  if (!app_config.unchecked_cef_path.empty() &&
      command != Command::kUninstall && !retention_command) {
    base::FilePath unchecked_dir =
        base::FilePath::FromUTF8Unsafe(app_config.unchecked_cef_path);
    base::FilePath libcef_path = unchecked_dir.Append(kLibcefFilename);
    if (base::PathExists(libcef_path)) {
      Logger::GetInstance().Info("Using unchecked CEF path: " +
                                 libcef_path.AsUTF8Unsafe());
      return Result::Success(libcef_path, "", "", /*is_bundled=*/true);
    }
    Logger::GetInstance().Warning(
        "unchecked_cef_path set but libcef.dll not found at " +
        libcef_path.AsUTF8Unsafe() + "; falling through to installer");
  }

  // Policy is an immutable operation snapshot. Load it after the successful
  // unchecked-path short circuit, but before entering shared-store resolution
  // or any store/network side effect.
  const PolicyLoadResult policy_result = prepared_uninstall
                                             ? prepared_uninstall->policy_result
                                             : LoadEnterprisePolicy();
  if (!policy_result.valid()) {
    return Result::Error(kExitCodePolicyError, policy_result.diagnostic);
  }
  enterprise_policy_ = policy_result.policy;
  effective_download_source_ = ResolveEffectiveDownloadSource(
      enterprise_policy_, extended_config.cdn_urls, app_config.cdn_urls,
      base::FilePath::FromUTF8Unsafe(extended_config.local_download_path));

  // Step 1: Find install directories.
  // Resolve writable directory and readable directories in a single pass.
  // Readable dirs are collected in priority order up to and including the
  // first writable directory — never from lower-priority locations.
  const bool downloads_disabled_operation =
      effective_download_source_.downloads_disabled() &&
      (command == Command::kInstall || command == Command::kUpdate);
  DirectoryResolutionContext resolution_context;
  InstallDirectories dirs;
  if (prepared_uninstall) {
    if (!prepared_uninstall->directories) {
      return Result::Error(kExitCodeConfigError,
                           "Prepared uninstall directories are missing");
    }
    resolution_context = prepared_uninstall->resolution_context;
    dirs = *prepared_uninstall->directories;
  } else {
    resolution_context.mutation_capable = command != Command::kQuery &&
                                          !retention_command &&
                                          !downloads_disabled_operation;
    resolution_context.is_elevated = IsCurrentProcessElevated();
    resolution_context.allow_admin_mutation =
        IsAdminMutationAllowed(context == ExecutionContext::kAutomaticStartup,
                               app_config.enable_explicit_modes);
    resolution_context.allow_shared_user_store =
        enterprise_policy_.allow_shared_user_store;
    dirs = ResolveInstallDirectories(extended_config.install_path,
                                     resolution_context);
  }
  base::FilePath install_dir = dirs.writable_dir;
  std::optional<DirectoryRole> install_role = dirs.writable_role;
  PathError path_err = dirs.write_error;
  bool has_writable_dir = (path_err == PathError::kSuccess);
  if (retention_command && !dirs.readable_dirs.empty()) {
    install_dir = dirs.readable_dirs.front();
    install_role = dirs.readable_roles.front();
    path_err = PathError::kSuccess;
    has_writable_dir = true;
  }
  DCHECK_EQ(has_writable_dir, install_role.has_value());
  base::FilePath query_state_dir = install_dir;
  if (command == Command::kQuery && query_state_dir.empty()) {
    if (!extended_config.install_path.empty() && !dirs.readable_dirs.empty()) {
      query_state_dir = dirs.readable_dirs.front();
    } else {
      for (size_t i = 0; i < dirs.readable_dirs.size(); ++i) {
        if (dirs.readable_roles[i] == DirectoryRole::kPerUserDefault) {
          query_state_dir = dirs.readable_dirs[i];
          break;
        }
      }
      if (query_state_dir.empty() && resolution_context.is_elevated &&
          !dirs.readable_dirs.empty()) {
        query_state_dir = dirs.readable_dirs.front();
      }
    }
  }
  std::vector<base::FilePath> read_dirs = std::move(dirs.readable_dirs);
  std::shared_ptr<VersionLease> retained_downloads_disabled_lease;

  // A custom namespace is authoritative. Invalid, unsafe, inaccessible, or
  // missing read-only overrides fail before installed/bundled selection and
  // never fall through to a default store. Download-disabled install/update
  // operations remain read-only and return the policy result below.
  if (!extended_config.install_path.empty() && !has_writable_dir &&
      read_dirs.empty() && !downloads_disabled_operation) {
    auto result = Result::Error(kExitCodeConfigError,
                                "Could not use exclusive install directory: " +
                                    std::string(PathErrorToString(path_err)));
    Logger::GetInstance().LogDirectoryResolutionFailure(command,
                                                        result.error_message);
    return result;
  }

  if (downloads_disabled_operation) {
    const std::vector<InstalledVersion> installed =
        ReadMultipleVersionIndexes(read_dirs);
    if (command == Command::kInstall) {
      std::optional<Result> offline = ResolveStartupOffline(
          app_config, extended_config, install_dir, read_dirs,
          LoadEffectiveRevocationList(read_dirs),
          /*allow_last_resort=*/true);
      if (offline) {
        // Automatic and unsuccessful selections remain read-only. An explicit
        // install with a usable local candidate may now re-resolve with
        // mutation enabled so the ordinary registration path can complete
        // without authorizing any download.
        if (context == ExecutionContext::kAutomaticStartup ||
            !offline->success) {
          return std::move(*offline);
        }
        DirectoryResolutionContext writable_context = resolution_context;
        writable_context.mutation_capable = true;
        writable_context.required_readable_dirs = read_dirs;
        InstallDirectories writable = ResolveInstallDirectories(
            extended_config.install_path, writable_context);
        if (writable.mutation_blocked_by_required_readable_dirs) {
          return Result::Error(
              kExitCodePolicyDenied,
              BuildPolicyDenialMessage(command, "downloads are disabled",
                                       &app_config, &installed));
        }
        if (writable.write_error != PathError::kSuccess) {
          return std::move(*offline);
        }
        const std::vector<InstalledVersion> writable_installed =
            ReadMultipleVersionIndexes(writable.readable_dirs);
        std::optional<Result> writable_offline = ResolveStartupOffline(
            app_config, extended_config, writable.writable_dir,
            writable.readable_dirs,
            LoadEffectiveRevocationList(writable.readable_dirs),
            /*allow_last_resort=*/true);
        if (!writable_offline || !writable_offline->success) {
          return Result::Error(
              kExitCodePolicyDenied,
              BuildPolicyDenialMessage(command, "downloads are disabled",
                                       &app_config, &writable_installed));
        }
        // Keep the candidate protected until final selection completes. The
        // writer lock serializes store mutation, but a concurrent pruner may
        // have selected the version before this operation acquires that lock.
        retained_downloads_disabled_lease = writable_offline->version_lease;
        install_dir = writable.writable_dir;
        install_role = writable.writable_role;
        path_err = writable.write_error;
        has_writable_dir = true;
        read_dirs = std::move(writable.readable_dirs);
      } else {
        return Result::Error(
            kExitCodePolicyDenied,
            BuildPolicyDenialMessage(command, "downloads are disabled",
                                     &app_config, &installed));
      }
    } else {
      return Result::Error(
          kExitCodePolicyDenied,
          BuildPolicyDenialMessage(command, "downloads are disabled",
                                   &app_config, &installed));
    }
  }

  const bool shared_store_blocked =
      !enterprise_policy_.allow_shared_user_store &&
      extended_config.install_path.empty() && !has_writable_dir;
  if (shared_store_blocked && command != Command::kQuery &&
      command != Command::kRetentionDryRun) {
    if (command == Command::kInstall) {
      std::optional<Result> offline =
          ResolveStartupOffline(app_config, extended_config, {}, read_dirs,
                                LoadEffectiveRevocationList(read_dirs),
                                /*allow_last_resort=*/true);
      if (offline) {
        return std::move(*offline);
      }
    }
    std::vector<InstalledVersion> installed;
    if (command == Command::kInstall || command == Command::kUpdate) {
      installed = ReadMultipleVersionIndexes(read_dirs);
    }
    return Result::Error(
        kExitCodePolicyDenied,
        BuildPolicyDenialMessage(
            command, "the shared user store is disabled",
            (command == Command::kInstall || command == Command::kUpdate)
                ? &app_config
                : nullptr,
            (command == Command::kInstall || command == Command::kUpdate)
                ? &installed
                : nullptr));
  }

  // Retention resolves the authoritative source read-only first. Reject
  // provisioning stores before any write probe, logger write, or lock-file
  // side effect. Apply checks writability only after this ownership gate.
  if (retention_command && has_writable_dir &&
      !IsUserRetentionEligible(*install_role)) {
    Result result = Result::Error(
        kExitCodeConfigError,
        "Registration retention is blocked: provisioning_store_ineligible");
    Database database;
    database.Load(GetDatabasePath(install_dir),
                  IntegrityMismatchAction::kPreserve);
    RetentionPlan plan = BuildRetentionPlan(
        *install_role, database, {}, {}, {}, {},
        {.max_age_days = extended_config.retention_max_age_days,
         .now = GetCurrentWallTime()});
    result.retention_plan = std::move(plan);
    result.retention_max_age_days = extended_config.retention_max_age_days;
    return result;
  }
  if (command == Command::kRetentionApply && has_writable_dir) {
    DirectoryResolutionContext writable_context = resolution_context;
    writable_context.mutation_capable = true;
    InstallDirectories writable = ResolveInstallDirectories(
        extended_config.install_path, writable_context);
    if (writable.write_error != PathError::kSuccess ||
        writable.writable_dir != install_dir ||
        writable.writable_role != install_role) {
      return Result::Error(kExitCodeConfigError,
                           "Retention store is not writable");
    }
  }

  if (!has_writable_dir && command != Command::kQuery) {
    if (context == ExecutionContext::kAutomaticStartup &&
        command == Command::kInstall) {
      std::optional<Result> offline =
          ResolveStartupOffline(app_config, extended_config, {}, read_dirs,
                                LoadEffectiveRevocationList(read_dirs),
                                /*allow_last_resort=*/true);
      if (offline) {
        Logger::GetInstance().LogOperationEnd(command, offline->success,
                                              offline->error_message);
        return std::move(*offline);
      }
    }

    // Fallback: for install/update without a writable directory, check if a
    // compatible version already exists in a readable directory before failing.
    // Uses compiled + disk-cached revocation data to skip revoked versions.
    if (context != ExecutionContext::kAutomaticStartup &&
        extended_config.install_path.empty() &&
        (command == Command::kInstall || command == Command::kUpdate) &&
        (!read_dirs.empty() || !extended_config.bundled_cef_path.empty())) {
      std::vector<InstalledVersion> installed =
          ReadMultipleVersionIndexes(read_dirs);
      std::vector<RevokedVersionRange> revoked =
          LoadEffectiveRevocationList(read_dirs);
      std::optional<InstalledVersion> bundled;
      if (!extended_config.bundled_cef_path.empty()) {
        bundled = CheckBundledCef(app_config, extended_config);
      }
      OfflineSelectionResult selection = SelectOfflineCandidate(
          app_config, GetCurrentPlatform(), installed, bundled, revoked);
      std::optional<InstalledVersion> best =
          selection.preferred ? selection.preferred : selection.last_resort;
      if (best) {
        bool is_bundled =
            (selection.preferred
                 ? selection.preferred_source
                 : selection.last_resort_source) == CandidateSource::kBundled;
        Logger::GetInstance().Warning(
            "No writable install directory available. Using existing "
            "compatible version " +
            best->metadata.version.ToString() + " from read-only location.");
        return MakeValidatedLibcefResult(
            FindTrustedRootForVersion(
                best->path, is_bundled,
                bundled ? bundled->path : base::FilePath(), read_dirs, {}),
            best->path, best->metadata.version.ToString(),
            best->metadata.version_full, is_bundled);
      }
    }

    auto result = Result::Error(kExitCodeConfigError,
                                "Could not find or create install directory: " +
                                    std::string(PathErrorToString(path_err)));
    Logger::GetInstance().LogDirectoryResolutionFailure(command,
                                                        result.error_message);
    return result;
  }

  // Re-initialize logger with the real install directory (if available).
  if (has_writable_dir && !retention_dry_run) {
    LoggerConfig log_config;
    log_config.log_directory = install_dir;
    log_config.min_log_level = extended_config.log_level;
    Logger::GetInstance().Initialize(log_config);
  } else if (retention_dry_run) {
    LoggerConfig log_config;
    log_config.enable_file_log = false;
    Logger::GetInstance().Initialize(log_config);
  }
  if (retention_command) {
    Logger::GetInstance().LogOperationStart(command, app_config.appid);
  }

  // Report initializing progress
  if (!ReportProgress(progress, kStepInit)) {
    return Result::Error(kExitCodeCancelled, "Operation cancelled by user");
  }

  if (command != Command::kPrune && !retention_command &&
      (app_config.appid.empty() || app_config.vmin.empty())) {
    auto result =
        Result::Error(kExitCodeConfigError, "Missing required config fields");
    Logger::GetInstance().LogOperationEnd(command, false, result.error_message);
    return result;
  }

  // Query is a side-effect-free offline read regardless of whether a writable
  // store exists. It must not wait for the global writer lock or load the
  // registration database.
  if (command == Command::kQuery) {
    if (!ReportProgress(progress, kStepVersionCheck)) {
      return Result::Error(kExitCodeCancelled, "Operation cancelled by user");
    }
    Result result =
        ResolveQuery(app_config, extended_config, query_state_dir, read_dirs);
    Logger::GetInstance().LogOperationEnd(command, result.success,
                                          result.error_message);
    return result;
  }

  auto resolve_startup_with_revocation_refresh = [&]() {
    std::vector<RevokedVersionRange> effective =
        LoadEffectiveRevocationList(read_dirs);
    std::optional<Result> offline = ResolveStartupOffline(
        app_config, extended_config, install_dir, read_dirs, effective);
    if (offline && offline->success && has_writable_dir &&
        !effective_download_source_.downloads_disabled() &&
        !IsRevocationCacheFresh(install_dir, base::Time::Now())) {
      const std::string source =
          GetDownloadSourceIdentity(effective_download_source_);
      if (extended_config.force_check ||
          !IsRevocationRefreshBackedOff(install_dir, source,
                                        base::Time::Now())) {
        // Release the preliminary lease while refreshing, then select and
        // lease again using the newly fetched revocation gate.
        offline.reset();
        bool fetched = false;
        effective =
            FetchRevocationList(extended_config, effective_download_source_,
                                install_dir, effective, true, &fetched);
        if (!fetched) {
          RecordRevocationRefreshFailure(install_dir, source,
                                         base::Time::Now());
        }
        offline = ResolveStartupOffline(app_config, extended_config,
                                        install_dir, read_dirs, effective);
      }
    }
    return offline;
  };

  if (context == ExecutionContext::kAutomaticStartup &&
      command == Command::kInstall) {
    std::optional<Result> offline = resolve_startup_with_revocation_refresh();
    if (offline) {
      if (offline->success && has_writable_dir) {
        TryRegisterStartup(app_config, install_dir);
      }
      Logger::GetInstance().LogOperationEnd(command, offline->success,
                                            offline->error_message);
      return std::move(*offline);
    }
  }

  // Step 2: Acquire lock (skip when query has no writable dir — read-only)
  if (has_writable_dir) {
    constexpr uint32_t kStartupLockTimeoutMs = 5000;
    uint32_t timeout_ms = context == ExecutionContext::kAutomaticStartup &&
                                  extended_config.lock_timeout_ms == 0
                              ? kStartupLockTimeoutMs
                              : extended_config.lock_timeout_ms;
    Result lock_result = AcquireLock(install_dir, timeout_ms);
    if (!lock_result.success) {
      if (context == ExecutionContext::kAutomaticStartup) {
        lock_result.error_message =
            "Another application is installing CEF. Please try again when "
            "that install completes.";
      }
      Logger::GetInstance().LogOperationEnd(command, false,
                                            lock_result.error_message);
      return lock_result;
    }
  }

  // Release the lock when leaving this scope (no-op if lock was never
  // acquired).
  base::ScopedClosureRunner lock_guard(
      base::BindOnce(&Controller::ReleaseLock, base::Unretained(this)));

  Result deferred_cleanup = Result::Success({}, "");

  if (!ReportProgress(progress, kStepLock)) {
    return Result::Error(kExitCodeCancelled, "Operation cancelled by user");
  }

  if (retention_command) {
    Result result = RunRetention(command, *install_role, install_dir, read_dirs,
                                 extended_config, progress);
    Logger::GetInstance().LogOperationEnd(command, result.success,
                                          result.error_message);
    return result;
  }

  // Step 3: Retry pending deletions from previous runs and clean stale
  // staging directories left by crashes.
  if (has_writable_dir &&
      (command == Command::kInstall || command == Command::kUpdate ||
       command == Command::kUninstall)) {
    deferred_cleanup = ReconcileInstallState(install_dir);
    if (!deferred_cleanup.success) {
      return deferred_cleanup;
    }
    PruneCacheDirectory(GetCacheDirectory(install_dir));
  }

  // Step 4: Validate required config fields (kPrune skips — no version
  // resolution)
  if (command != Command::kPrune && !retention_command &&
      (app_config.appid.empty() || app_config.vmin.empty())) {
    auto result =
        Result::Error(kExitCodeConfigError, "Missing required config fields");
    Logger::GetInstance().LogOperationEnd(command, false, result.error_message);
    return result;
  }

  // Step 5: Update database (skip when query has no writable dir)
  if (has_writable_dir) {
    Result db_result = UpdateDatabase(command, app_config, install_dir);
    if (!db_result.success && command != Command::kQuery) {
      Logger::GetInstance().LogOperationEnd(command, false,
                                            db_result.error_message);
      return db_result;
    }
  }

  // State may have changed while automatic startup waited for the writer.
  // Re-run the offline selector after reconciliation and database load, before
  // any revocation/manifest/archive network work.
  if (context == ExecutionContext::kAutomaticStartup &&
      command == Command::kInstall) {
    std::optional<Result> offline = resolve_startup_with_revocation_refresh();
    if (offline) {
      Result result = std::move(*offline);
      if (result.success) {
        Result registration = PublishRegistration(app_config, install_dir);
        if (!registration.success) {
          result = std::move(registration);
        }
      }
      Logger::GetInstance().LogOperationEnd(command, result.success,
                                            result.error_message);
      return result;
    }
  }

  if (!ReportProgress(progress, kStepVersionCheck)) {
    return Result::Error(kExitCodeCancelled, "Operation cancelled by user");
  }

  Result result;

  switch (command) {
    case Command::kInstall:
    case Command::kUpdate: {
      result = ComputeAndInstall(app_config, extended_config, install_dir,
                                 read_dirs, command, progress);
      if (result.success) {
        Result registration = PublishRegistration(app_config, install_dir);
        if (!registration.success) {
          result = std::move(registration);
          break;
        }
        if (!pending_archive_cleanup_path_.empty()) {
          const bool archive_removed =
              VerifySafeFilePath(pending_archive_cleanup_path_) &&
              (!base::PathExists(pending_archive_cleanup_path_) ||
               base::DeleteFile(pending_archive_cleanup_path_));
          const bool partials_removed =
              DiscardDownloadPartials(pending_archive_cleanup_path_) ==
              DownloadError::kSuccess;
          if (!archive_removed || !partials_removed) {
            Logger::GetInstance().Warning(
                "Committed install archive cleanup deferred");
            result.outcome = Outcome::kCleanupDeferred;
            result.warnings.push_back(
                "Committed install archive cleanup deferred");
          } else {
            pending_archive_cleanup_path_.clear();
          }
        }
        bool has_revoked_installed = false;
        if (!effective_revocations_.empty()) {
          for (const auto& iv :
               ScanInstalledVersionsWithMetadata(install_dir)) {
            if (IsVersionRevoked(iv.metadata.version, effective_revocations_)) {
              has_revoked_installed = true;
              break;
            }
          }
        }
        if (version_published_ || has_revoked_installed) {
          Result prune = PruneUnusedVersions(install_dir, app_config, {},
                                             effective_revocations_);
          if (!prune.success) {
            result = std::move(prune);
          } else if (prune.outcome == Outcome::kCleanupDeferred) {
            result.outcome = Outcome::kCleanupDeferred;
            result.warnings.insert(result.warnings.end(),
                                   prune.warnings.begin(),
                                   prune.warnings.end());
          }
        }
      }
      break;
    }

    case Command::kUninstall:
      // Unregister already done in UpdateDatabase
      // Now prune unused versions
      result = PruneUnusedVersions(install_dir, app_config, {},
                                   LoadEffectiveRevocationList(read_dirs));
      if (result.success) {
        Logger::GetInstance().LogUninstallationCompleted(app_config.appid);
      }
      break;

    case Command::kQuery:
      // Queries return before writer-lock acquisition and mutating dispatch.
      result = Result::Error(kExitCodeUnknownError,
                             "query reached mutating command dispatch");
      break;

    case Command::kPrune:
      if (!database_ || !database_->CanPrune()) {
        result = Result::Success({}, "");
        break;
      }
      {
        Result orphan_cleanup = Result::Success({}, "");
        std::vector<InstalledVersion> indexed;
        if (ReadVersionIndex(install_dir, &indexed) ==
            MetadataError::kSuccess) {
          orphan_cleanup = QuarantineUnindexedVersions(install_dir, indexed);
        }
        result = PruneUnusedVersions(install_dir, app_config, {},
                                     LoadEffectiveRevocationList(read_dirs));
        result.warnings.insert(result.warnings.end(),
                               orphan_cleanup.warnings.begin(),
                               orphan_cleanup.warnings.end());
        if (result.success &&
            orphan_cleanup.outcome == Outcome::kCleanupDeferred) {
          result.outcome = Outcome::kCleanupDeferred;
        }
      }
      break;

    case Command::kRetentionDryRun:
    case Command::kRetentionApply:
      result = Result::Error(kExitCodeUnknownError,
                             "retention reached ordinary command dispatch");
      break;

    case Command::kLaunchSuccess:
      // Handled by internal::HandleLaunchSuccess in the RunInstaller export
      // before reaching the Controller. Never dispatched through Run.
      result =
          Result::Error(kExitCodeUnknownError,
                        "launch_success is not handled by Controller::Run");
      break;
  }

  if (result.success && deferred_cleanup.outcome == Outcome::kCleanupDeferred) {
    result.outcome = Outcome::kCleanupDeferred;
    result.warnings.insert(result.warnings.end(),
                           deferred_cleanup.warnings.begin(),
                           deferred_cleanup.warnings.end());
  }

  if (result.success && context == ExecutionContext::kAutomaticStartup &&
      command == Command::kInstall && !result.is_bundled &&
      !result.libcef_path.empty() && !result.version_lease) {
    std::unique_ptr<VersionLease> lease;
    base::FilePath version_dir = result.libcef_path.DirName().DirName();
    VersionLeaseError lease_error =
        AcquireVersionLease(install_dir, version_dir, &lease);
    if (lease_error != VersionLeaseError::kSuccess) {
      result =
          Result::Error(kExitCodeInstallError,
                        lease_error == VersionLeaseError::kLostRace
                            ? "Installed version was removed before launch"
                            : "Failed to protect installed version for launch");
    } else {
      result.version_lease = std::shared_ptr<VersionLease>(std::move(lease));
    }
  }

  if (result.success && context == ExecutionContext::kAutomaticStartup &&
      command == Command::kInstall && !result.is_bundled &&
      !install_dir.empty()) {
    result.liveness_path = GetInstallDirLivenessPath(
        install_dir, GetAppidHash(app_config.appid), GetCurrentPlatform());
  }

  if (result.success &&
      (command == Command::kUninstall || command == Command::kPrune)) {
    ReportProgress(progress, kStepCleanup);
  }

  Logger::GetInstance().LogOperationEnd(command, result.success,
                                        result.error_message);
  return result;
}

Result Controller::Run(Command command,
                       const std::string& config_json,
                       ProgressCallback progress) {
  Config config;
  ExtendedConfig extended;

  if (config_json.empty()) {
    return Result::Error(kExitCodeConfigError, "No configuration provided");
  }

  if (!internal::ParseCombinedConfig(config_json, &config, &extended)) {
    return Result::Error(kExitCodeConfigError,
                         "Failed to parse configuration JSON");
  }

  return Run(command, config, extended, progress);
}

Result Controller::AcquireLock(const base::FilePath& install_dir,
                               uint32_t timeout_ms) {
  lock_ = SingletonLock::Acquire(
      install_dir, timeout_ms > 0 ? timeout_ms : kDefaultLockTimeoutMs);
  if (!lock_ || !lock_->IsHeld()) {
    return Result::Error(kExitCodeLockTimeout,
                         "Could not acquire installer lock within timeout");
  }
  if (lock_->was_abandoned()) {
    Logger::GetInstance().Warning(
        "Acquired abandoned installer mutex; writer recovery is required");
  }
  return Result::Success({}, "");
}

Result Controller::ResolveQuery(const Config& config,
                                const ExtendedConfig& extended,
                                const base::FilePath& install_dir,
                                const std::vector<base::FilePath>& read_dirs) {
  std::vector<InstalledVersion> installed =
      ReadMultipleVersionIndexes(read_dirs, /*allow_scan_fallback=*/false,
                                 VersionIndexReadMode::kPreserveCorrupt);
  std::vector<RevokedVersionRange> revoked =
      LoadQueryRevocations(extended, read_dirs);

  std::optional<InstalledVersion> bundled;
  if (!extended.bundled_cef_path.empty()) {
    bundled = CheckBundledCef(config, extended);
  }

  std::set<VersionKey> disqualified_versions;
  AppLaunchHealthEvaluation health = EvaluateAppLaunchHealth(
      install_dir, config.appid, config.launch_health, kMaxConsecutiveFailures);
  for (const Version& version : health.disqualified_versions) {
    disqualified_versions.insert({version, GetCurrentPlatform()});
  }

  OfflineSelectionResult selection =
      SelectOfflineCandidate(config, GetCurrentPlatform(), installed, bundled,
                             revoked, disqualified_versions);
  std::optional<InstalledVersion> best =
      selection.preferred ? selection.preferred : selection.last_resort;
  bool best_is_bundled = (selection.preferred ? selection.preferred_source
                                              : selection.last_resort_source) ==
                         CandidateSource::kBundled;
  if (!best) {
    return Result::Error(kExitCodeNoMatchingVersion,
                         BuildNoMatchingInstalledVersionMessage(
                             config, GetCurrentPlatform(), selection.rejected));
  }

  return MakeValidatedLibcefResult(
      FindTrustedRootForVersion(best->path, best_is_bundled,
                                bundled ? bundled->path : base::FilePath(),
                                read_dirs, install_dir),
      best->path, best->metadata.version.ToString(),
      best->metadata.version_full, best_is_bundled);
}

std::optional<Result> Controller::ResolveStartupOffline(
    const Config& config,
    const ExtendedConfig& extended,
    const base::FilePath& install_dir,
    const std::vector<base::FilePath>& read_dirs,
    const std::vector<RevokedVersionRange>& revoked,
    bool allow_last_resort) {
  const EmergencyRecoveryScanLimits recovery_limits =
      GetEmergencyRecoveryScanLimits();
  base::TimeTicks recovery_deadline;
  size_t recovery_roots_scanned = 0;
  size_t recovery_entries_visited = 0;

  for (int attempt = 0; attempt < 2; ++attempt) {
    std::vector<VersionRootRead> roots = ReadVersionRoots(read_dirs);
    std::vector<InstalledVersion> installed =
        MergeVersionRoots(roots, /*include_recovered=*/false);
    std::optional<InstalledVersion> bundled;
    if (!extended.bundled_cef_path.empty()) {
      bundled = CheckBundledCef(config, extended);
    }

    AppLaunchHealthEvaluation health =
        EvaluateAppLaunchHealth(install_dir, config.appid, config.launch_health,
                                kMaxConsecutiveFailures);
    std::set<VersionKey> disqualified;
    for (const Version& version : health.disqualified_versions) {
      disqualified.insert({version, GetCurrentPlatform()});
    }

    OfflineSelectionResult selection =
        SelectOfflineCandidate(config, GetCurrentPlatform(), installed, bundled,
                               revoked, disqualified);
    std::optional<InstalledVersion> candidate = selection.preferred;
    if (!candidate && allow_last_resort) {
      candidate = selection.last_resort;
    }
    if (!candidate) {
      if (recovery_deadline.is_null()) {
        recovery_deadline =
            base::TimeTicks::Now() + recovery_limits.time_budget;
      }
      bool attempted_recovery = false;
      bool recovery_requires_warning = false;
      bool bounded_out = false;
      for (auto& root : roots) {
        if (root.index_error == MetadataError::kSuccess) {
          continue;
        }
        if (!IsEmergencyRecoveryEligibleIndexError(root.index_error)) {
          Logger::GetInstance().Warning(
              "Emergency startup recovery failed closed after an "
              "inconclusive index read error in " +
              root.root.AsUTF8Unsafe() + ": " +
              MetadataErrorToString(root.index_error));
          continue;
        }
        if (recovery_roots_scanned >= recovery_limits.max_roots ||
            recovery_entries_visited >= recovery_limits.max_version_entries ||
            base::TimeTicks::Now() >= recovery_deadline) {
          bounded_out = true;
          break;
        }

        attempted_recovery = true;
        ++recovery_roots_scanned;
        if (root.index_error != MetadataError::kFileNotFound) {
          recovery_requires_warning = true;
          Logger::GetInstance().Warning(
              "Attempting bounded emergency startup recovery in " +
              root.root.AsUTF8Unsafe() + " after index read failed: " +
              MetadataErrorToString(root.index_error));
        }
        BoundedInstalledVersionScanResult scan =
            ScanInstalledVersionsWithMetadataBounded(
                root.root,
                recovery_limits.max_version_entries - recovery_entries_visited,
                recovery_deadline);
        recovery_entries_visited += scan.entries_visited;
        recovery_requires_warning |= scan.entries_visited > 0;
        root.recovered = std::move(scan.versions);
        if (scan.entry_limit_reached || scan.time_limit_reached) {
          recovery_requires_warning = true;
          bounded_out = true;
          Logger::GetInstance().Warning(
              "Emergency startup recovery reached its scan bound in " +
              root.root.AsUTF8Unsafe());
          break;
        }
      }

      if (attempted_recovery) {
        installed = MergeVersionRoots(roots, /*include_recovered=*/true);
        selection =
            SelectOfflineCandidate(config, GetCurrentPlatform(), installed,
                                   bundled, revoked, disqualified);
        candidate = selection.preferred;
        if (!candidate && allow_last_resort) {
          candidate = selection.last_resort;
        }
        if (candidate) {
          Logger::GetInstance().Warning(
              "Using an installed version found by bounded emergency startup "
              "recovery; a later writer should repair the index");
        } else if (recovery_requires_warning) {
          Logger::GetInstance().Warning(
              "Bounded emergency startup recovery found no usable version");
        }
      }
      if (bounded_out) {
        Logger::GetInstance().Warning(
            "Emergency startup recovery stopped at its root, entry, or soft "
            "elapsed-time bound");
      }
    }
    if (!candidate) {
      return std::nullopt;
    }

    const InstalledVersion& selected = *candidate;
    bool using_preferred = selection.preferred.has_value();
    CandidateSource source = using_preferred ? selection.preferred_source
                                             : selection.last_resort_source;
    bool is_bundled = source == CandidateSource::kBundled;
    base::FilePath trusted_root = FindTrustedRootForVersion(
        selected.path, is_bundled, bundled ? bundled->path : base::FilePath(),
        read_dirs, install_dir);
    Result result = MakeValidatedLibcefResult(
        trusted_root, selected.path, selected.metadata.version.ToString(),
        selected.metadata.version_full, is_bundled);
    if (!result.success) {
      return result;
    }

    if (!is_bundled) {
      std::unique_ptr<VersionLease> lease;
      VersionLeaseError lease_error =
          AcquireVersionLease(trusted_root, selected.path, &lease);
      if (lease_error == VersionLeaseError::kLostRace) {
        continue;
      }
      if (lease_error != VersionLeaseError::kSuccess) {
        return Result::Error(kExitCodeInstallError,
                             "Failed to protect installed version for launch");
      }
      result.version_lease = std::shared_ptr<VersionLease>(std::move(lease));
    }

    if (!is_bundled && !install_dir.empty() &&
        config.launch_health != LaunchHealthMode::kOff) {
      std::wstring appid_hash = GetAppidHash(config.appid);
      result.launch_state_path = GetInstallDirLaunchStatePath(
          install_dir, appid_hash, selected.metadata.version,
          selected.metadata.platform);
      result.launch_version = selected.metadata.version.ToString();
      result.launch_platform = selected.metadata.platform;
      auto assessment = health.assessments.find(selected.metadata.version);
      if (assessment != health.assessments.end()) {
        result.launch_consecutive_failures =
            assessment->second.consecutive_failures;
      }
      result.is_rollback = selection.preferred_is_rollback;
      for (const auto& [version, state] : health.states) {
        if (version < selected.metadata.version && !state.running &&
            state.confirmed && state.consecutive_failures == 0) {
          result.launch_cleanup_paths.push_back(GetInstallDirLaunchStatePath(
              install_dir, appid_hash, version, GetCurrentPlatform()));
        }
      }
    }
    if (!is_bundled && !install_dir.empty()) {
      result.liveness_path = GetInstallDirLivenessPath(
          install_dir, GetAppidHash(config.appid), selected.metadata.platform);
    }
    return result;
  }

  return Result::Error(kExitCodeNoMatchingVersion,
                       "Installed version changed during launch selection");
}

void Controller::TryRegisterStartup(const Config& config,
                                    const base::FilePath& install_dir) {
  auto registration_lock = SingletonLock::Acquire(install_dir, 0);
  if (!registration_lock) {
    Logger::GetInstance().Info(
        "Skipping startup registration while installer lock is busy");
    return;
  }
  if (registration_lock->was_abandoned()) {
    Logger::GetInstance().Warning(
        "Skipping startup registration after abandoned installer lock");
    return;
  }

  Database database;
  DatabaseError load_error = database.Load(GetDatabasePath(install_dir));
  if (load_error != DatabaseError::kSuccess &&
      load_error != DatabaseError::kFileNotFound) {
    Logger::GetInstance().Warning(
        "Skipping startup registration because database load failed");
    return;
  }

  AppEntry entry;
  entry.uuid = config.appid;
  entry.platform = GetCurrentPlatform();
  entry.vmin = config.vmin;
  entry.vmax = config.vmax;
  entry.abi_hash = config.abi_hash;
  if (database.RegisterApp(entry) &&
      database.Save(GetDatabasePath(install_dir)) != DatabaseError::kSuccess) {
    Logger::GetInstance().Warning("Best-effort startup registration failed");
  }
}

Result Controller::UpdateDatabase(Command command,
                                  const Config& config,
                                  const base::FilePath& install_dir) {
  base::FilePath db_path = GetDatabasePath(install_dir);

  database_ = std::make_unique<Database>();
  DatabaseError load_err = database_->Load(db_path);
  if (load_err == DatabaseError::kIntegrityMismatch) {
    // Corrupted file was deleted. Start fresh but suspend pruning for a
    // grace period so other apps have time to re-register.
    Logger::GetInstance().Warning(
        "Database integrity check failed — file was corrupted. "
        "Pruning suspended for 7 days.");
    database_->SuspendPruning();
  } else if (load_err != DatabaseError::kSuccess &&
             load_err != DatabaseError::kFileNotFound) {
    return Result::Error(kExitCodeDatabaseError,
                         "Failed to load database: " +
                             std::string(DatabaseErrorToString(load_err)));
  }

  switch (command) {
    case Command::kInstall:
    case Command::kUpdate:
      // Addition registration is published only after destination and checked
      // expanded-index publication succeeds.
      break;

    case Command::kUninstall:
      // Remove app entry
      database_->UnregisterApp(config.appid, GetCurrentPlatform());
      break;

    case Command::kQuery:
    case Command::kPrune:
    case Command::kRetentionDryRun:
    case Command::kRetentionApply:
    case Command::kLaunchSuccess:
      // No database modification. (kLaunchSuccess never reaches the Controller
      // — it is handled in the RunInstaller export.)
      break;
  }

  // Save database
  if (command == Command::kUninstall) {
    DatabaseError save_err = database_->Save(db_path);
    if (save_err != DatabaseError::kSuccess) {
      return Result::Error(kExitCodeDatabaseError,
                           "Failed to save database: " +
                               std::string(DatabaseErrorToString(save_err)));
    }
  }

  return Result::Success({}, "");
}

Result Controller::PublishRegistration(const Config& config,
                                       const base::FilePath& install_dir) {
  if (!database_) {
    return Result::Error(kExitCodeDatabaseError, "Database is not loaded");
  }
  AppEntry entry;
  entry.uuid = config.appid;
  entry.platform = GetCurrentPlatform();
  entry.vmin = config.vmin;
  entry.vmax = config.vmax;
  entry.abi_hash = config.abi_hash;
  if (!database_->RegisterApp(entry)) {
    return Result::Success({}, "");
  }
  DatabaseError error = database_->Save(GetDatabasePath(install_dir));
  if (error != DatabaseError::kSuccess) {
    return Result::Error(kExitCodeDatabaseError,
                         "Failed to save database: " +
                             std::string(DatabaseErrorToString(error)));
  }
  return Result::Success({}, "");
}

Result Controller::ReconcileInstallState(const base::FilePath& install_dir) {
  Result result = Result::Success({}, "");

  int cleaned = RetryPendingDeletions(install_dir);
  if (cleaned > 0) {
    Logger::GetInstance().Info("Cleaned up " + std::to_string(cleaned) +
                               " pending deletions");
  }
  base::FilePath trash_root = install_dir.Append(kTrashSubdirectory);
  if (base::DirectoryExists(trash_root) && !IsReparsePoint(trash_root)) {
    base::FileEnumerator trash_entries(
        trash_root, false,
        base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES);
    if (!trash_entries.Next().empty()) {
      result.outcome = Outcome::kCleanupDeferred;
      result.warnings.push_back("Pending trash reclamation remains");
    }
  }

  base::FilePath staging_root = install_dir.Append(kStagingSubdirectory);
  if (!VerifySafeDirectoryPath(staging_root, /*delete_if_exists=*/true) ||
      base::PathExists(staging_root)) {
    return Result::Error(kExitCodeRecoveryError,
                         "Failed to reconcile staging directory");
  }

  std::vector<InstalledVersion> indexed;
  MetadataError index_error = ReadVersionIndex(install_dir, &indexed);
  if (index_error != MetadataError::kSuccess) {
    std::vector<InstalledVersion> scanned =
        ScanInstalledVersionsWithMetadata(install_dir);
    std::erase_if(scanned, [&](const InstalledVersion& iv) {
      return ValidateDistribution(install_dir, iv.path, iv.metadata) !=
             DistributionValidation::kComplete;
    });
    MetadataError rebuild_error = WriteVersionIndex(install_dir, scanned);
    if (rebuild_error != MetadataError::kSuccess) {
      return Result::Error(
          kExitCodeRecoveryError,
          "Failed to rebuild version index: " +
              std::string(MetadataErrorToString(rebuild_error)));
    }
    return result;
  }

  Result orphan_cleanup = QuarantineUnindexedVersions(install_dir, indexed);
  if (orphan_cleanup.outcome == Outcome::kCleanupDeferred) {
    result.outcome = Outcome::kCleanupDeferred;
    result.warnings.insert(result.warnings.end(),
                           orphan_cleanup.warnings.begin(),
                           orphan_cleanup.warnings.end());
  }
  return result;
}

Result Controller::QuarantineUnindexedVersions(
    const base::FilePath& install_dir,
    const std::vector<InstalledVersion>& indexed) {
  Result result = Result::Success({}, "");
  std::set<base::FilePath> indexed_paths;
  for (const auto& iv : indexed) {
    indexed_paths.insert(iv.path);
  }
  base::FilePath versions_root = install_dir.Append(kVersionsSubdirectory);
  if (!base::DirectoryExists(versions_root) || IsReparsePoint(versions_root)) {
    return result;
  }

  base::FileEnumerator version_dirs(versions_root, false,
                                    base::FileEnumerator::DIRECTORIES);
  for (base::FilePath version_dir = version_dirs.Next(); !version_dir.empty();
       version_dir = version_dirs.Next()) {
    Version version = Version::Parse(version_dir.BaseName().AsUTF8Unsafe());
    if (!version.IsValid()) {
      result.outcome = Outcome::kCleanupDeferred;
      result.warnings.push_back(
          "Unparseable unindexed version cleanup deferred");
      continue;
    }
    if (IsReparsePoint(version_dir)) {
      result.outcome = Outcome::kCleanupDeferred;
      result.warnings.push_back(
          "Reparse-point unindexed version cleanup deferred");
      continue;
    }
    base::FileEnumerator platform_dirs(version_dir, false,
                                       base::FileEnumerator::DIRECTORIES);
    for (base::FilePath platform_dir = platform_dirs.Next();
         !platform_dir.empty(); platform_dir = platform_dirs.Next()) {
      if (indexed_paths.contains(platform_dir)) {
        continue;
      }
      std::string platform = platform_dir.BaseName().AsUTF8Unsafe();
      base::FilePath canonical_path =
          GetVersionPath(install_dir, version, platform);
      if (platform_dir != canonical_path) {
        result.outcome = Outcome::kCleanupDeferred;
        result.warnings.push_back(
            "Non-canonical unindexed version cleanup deferred");
        continue;
      }
      FileOpsError error = UninstallVersion(install_dir, version, platform);
      if (error != FileOpsError::kSuccess) {
        // A valid reduced index has already made this directory
        // non-authoritative. Failure to move or reclaim it is physical cleanup,
        // not a recovery commit failure, and must not block other mutations.
        result.outcome = Outcome::kCleanupDeferred;
        result.warnings.push_back("Unindexed version cleanup deferred: " +
                                  std::string(FileOpsErrorToString(error)));
      }
    }
  }
  return result;
}

Result Controller::ComputeAndInstall(
    const Config& config,
    const ExtendedConfig& extended,
    const base::FilePath& install_dir,
    const std::vector<base::FilePath>& read_dirs,
    Command command,
    ProgressCallback progress) {
  // Read version indexes from all readable directories so that versions
  // in read-only locations (e.g., %ProgramFiles%) are found even when
  // running non-elevated.
  std::vector<InstalledVersion> installed =
      ReadMultipleVersionIndexes(read_dirs);

  // Fetch revocation list
  std::vector<RevokedVersionRange> effective =
      LoadEffectiveRevocationList(read_dirs);
  std::vector<RevokedVersionRange> revoked = FetchRevocationList(
      extended, effective_download_source_, install_dir, effective);
  effective_revocations_ = revoked;

  // Log any revoked versions we have installed
  for (const auto& ver : installed) {
    if (IsVersionRevoked(ver.metadata.version, revoked)) {
      Logger::GetInstance().LogRevokedVersionBlocked(
          ver.metadata.version.ToString(), "Version is in revocation list");
    }
  }

  // Launch state: scan and filter disqualified versions.
  // Cache scan results to avoid redundant file I/O later.
  std::wstring appid_hash;
  std::set<VersionKey> disqualified_versions;
  std::vector<base::FilePath> cleanup_paths;
  if (config.launch_health != LaunchHealthMode::kOff &&
      !VerifySafeDirectoryPath(GetLaunchStateDir(install_dir))) {
    Logger::GetInstance().Warning(
        "Failed to repair unsafe launch-health directory");
  }
  AppLaunchHealthEvaluation health = EvaluateAppLaunchHealth(
      install_dir, config.appid, config.launch_health, kMaxConsecutiveFailures);
  const std::map<Version, LaunchState>& launch_state_cache = health.states;

  if (config.launch_health != LaunchHealthMode::kOff) {
    appid_hash = GetAppidHash(config.appid);
    for (const Version& version : health.disqualified_versions) {
      disqualified_versions.insert({version, GetCurrentPlatform()});
      Logger::GetInstance().Info(
          "Version " + version.ToString() +
          " disqualified (projected failures=" +
          std::to_string(health.assessments.at(version).consecutive_failures) +
          ")");
    }
  }

  // Build filtered installed list (without disqualified versions).
  // When nothing is disqualified, reference installed directly to avoid a copy.

  // Consider both installed and bundled versions as candidates.
  // Bundled versions are used in-place (not copied to the shared install
  // location) since they are less trusted than CDN-downloaded versions.
  std::optional<InstalledVersion> bundled;
  if (!extended.bundled_cef_path.empty()) {
    bundled = CheckBundledCef(config, extended);
  }

  // Check if bundled version is revoked. Revocation demotes the bundled
  // version: it loses the comparison with installed versions but remains
  // available as a last resort if CDN download also fails (a revoked version
  // is better than a completely broken app).
  OfflineSelectionResult selection =
      SelectOfflineCandidate(config, GetCurrentPlatform(), installed, bundled,
                             revoked, disqualified_versions);

  // Pick the best local candidate. Installed (CDN-downloaded) wins ties
  // since it is more trusted. A revoked bundled version is excluded from
  // this comparison — it can only be used as a last resort below.

  // Helper to populate launch state result fields for a selected version.
  // Uses launch_state_cache to avoid re-reading files from disk.
  auto PopulateLaunchResult = [&](Result& result,
                                  const InstalledVersion* selected,
                                  bool is_bundled_ver) {
    if (config.launch_health == LaunchHealthMode::kOff || appid_hash.empty() ||
        is_bundled_ver || !selected) {
      return;
    }

    result.launch_state_path = GetInstallDirLaunchStatePath(
        install_dir, appid_hash, selected->metadata.version,
        selected->metadata.platform);
    result.launch_version = selected->metadata.version.ToString();
    result.launch_platform = selected->metadata.platform;

    // Determine consecutive_failures from cached scan results.
    auto it = launch_state_cache.find(selected->metadata.version);
    if (it != launch_state_cache.end()) {
      auto assessment = health.assessments.find(selected->metadata.version);
      if (assessment != health.assessments.end()) {
        result.launch_consecutive_failures =
            assessment->second.consecutive_failures;
      }
    }

    // Detect rollback: selected version differs from what would have won
    // without disqualification.
    result.is_rollback = selection.preferred_is_rollback;

    // Build cleanup paths from scan results. Only delete confirmed-state
    // files for older versions — preserve crash-history files to prevent
    // the CDN doom loop.
    for (const auto& [version, ls] : launch_state_cache) {
      if (version < selected->metadata.version) {
        bool is_confirmed =
            !ls.running && ls.confirmed && ls.consecutive_failures == 0;
        if (is_confirmed) {
          cleanup_paths.push_back(GetInstallDirLaunchStatePath(
              install_dir, appid_hash, version, GetCurrentPlatform()));
        }
      }
    }
    result.launch_cleanup_paths = std::move(cleanup_paths);
  };

  std::optional<Result> local_result;
  if (selection.preferred) {
    const InstalledVersion& best = *selection.preferred;
    bool best_is_bundled =
        selection.preferred_source == CandidateSource::kBundled;
    Logger::GetInstance().LogVersionResolution(
        config.appid, best.metadata.version.ToString());
    Result result = MakeValidatedLibcefResult(
        FindTrustedRootForVersion(best.path, best_is_bundled,
                                  bundled ? bundled->path : base::FilePath(),
                                  read_dirs, install_dir),
        best.path, best.metadata.version.ToString(), best.metadata.version_full,
        best_is_bundled);
    PopulateLaunchResult(result, &best, best_is_bundled);
    if (!result.success || command != Command::kUpdate) {
      return result;
    }
    local_result = std::move(result);
  }

  // No suitable local version — try fallback chain: CDN → revoked bundled →
  // disqualified installed. Returns nullopt if no fallback is available.
  auto TryFallbackToLastResort =
      [&](const std::string& reason) -> std::optional<Result> {
    if (selection.last_resort) {
      const InstalledVersion& fallback = *selection.last_resort;
      bool is_bundled =
          selection.last_resort_source == CandidateSource::kBundled;
      if (selection.last_resort_is_revoked_bundled) {
        Logger::GetInstance().Warning(
            "Falling back to revoked bundled CEF version " +
            fallback.metadata.version.ToString() + " (" + reason + ")");
      } else {
        Logger::GetInstance().Warning("Falling back to disqualified version " +
                                      fallback.metadata.version.ToString() +
                                      " (" + reason + ")");
      }
      Result result = MakeValidatedLibcefResult(
          FindTrustedRootForVersion(fallback.path, is_bundled,
                                    bundled ? bundled->path : base::FilePath(),
                                    read_dirs, install_dir),
          fallback.path, fallback.metadata.version.ToString(),
          fallback.metadata.version_full, is_bundled);
      PopulateLaunchResult(result, &fallback, is_bundled);
      return result;
    }
    return std::nullopt;
  };

  if (!ReportProgress(progress, kStepCdnResolve)) {
    return Result::Error(kExitCodeCancelled, "Operation cancelled by user");
  }

  // Build skip set: installed versions (unconditional — avoids redundant
  // re-downloads) plus disqualified versions from .launch/ scan (includes
  // pruned-but-crashed versions whose crash history survived pruning).
  std::set<Version> skip_versions;
  CdnBuildExclusionReasons exclusion_reasons;
  auto ExcludeCdnVersion = [&](const Version& version,
                               CdnBuildExclusionReason reason) {
    skip_versions.insert(version);
    exclusion_reasons[version].insert(reason);
  };
  for (const auto& ver : installed) {
    ExcludeCdnVersion(ver.metadata.version,
                      CdnBuildExclusionReason::kAlreadyInstalled);
  }
  for (const auto& key : disqualified_versions) {
    ExcludeCdnVersion(key.version,
                      CdnBuildExclusionReason::kLaunchDisqualified);
  }

  std::vector<CdnBuildEntry> manifest_entries;
  QueryCdnForVersion(config, extended, install_dir, skip_versions,
                     &manifest_entries);
  auto SelectNextCdnEntry = [&]() -> std::optional<CdnBuildEntry> {
    while (true) {
      std::optional<CdnBuildEntry> entry =
          FindBestBuildEntry(manifest_entries, config.vmin, config.vmax,
                             config.abi_hash, skip_versions);
      if (!entry || !entry->version.IsValid() ||
          !IsVersionRevoked(entry->version, revoked)) {
        return entry;
      }
      Logger::GetInstance().LogRevokedVersionBlocked(
          entry->version.ToString(), "CDN version is in revocation list");
      ExcludeCdnVersion(entry->version, CdnBuildExclusionReason::kRevoked);
    }
  };
  std::optional<CdnBuildEntry> cdn_entry = SelectNextCdnEntry();
  if (!cdn_entry) {
    if (local_result) {
      return *local_result;
    }
    const std::string no_match_message = BuildNoMatchingCdnVersionMessage(
        manifest_entries, GetCurrentPlatform(), config.vmin, config.vmax,
        config.abi_hash, exclusion_reasons);
    auto fallback = TryFallbackToLastResort(no_match_message);
    return fallback.value_or(
        Result::Error(kExitCodeNoMatchingVersion, no_match_message));
  }

  if (selection.preferred &&
      cdn_entry->version <= selection.preferred->metadata.version) {
    return *local_result;
  }

  std::optional<Result> last_download_result;
  for (int candidate_attempt = 0; candidate_attempt < 2 && cdn_entry;
       ++candidate_attempt) {
    if (selection.preferred &&
        cdn_entry->version <= selection.preferred->metadata.version) {
      return *local_result;
    }
    bool try_next_candidate = false;
    Result download_result = DownloadAndInstall(
        *cdn_entry, extended, install_dir, progress, &try_next_candidate);
    if (download_result.success) {
      // Populate launch state fields for the newly installed version.
      // DownloadAndInstall returns a fresh Result without these.
      if (!appid_hash.empty() &&
          config.launch_health != LaunchHealthMode::kOff) {
        download_result.launch_state_path = GetInstallDirLaunchStatePath(
            install_dir, appid_hash, cdn_entry->version, GetCurrentPlatform());
        download_result.launch_version = cdn_entry->version.ToString();
        download_result.launch_platform = GetCurrentPlatform();
      }
      return download_result;
    }
    last_download_result = std::move(download_result);
    if (!try_next_candidate || candidate_attempt == 1) {
      break;
    }
    Logger::GetInstance().Warning(
        "Excluding failed CDN candidate " + cdn_entry->version.ToString() +
        " and trying one next-best compatible version");
    ExcludeCdnVersion(cdn_entry->version,
                      CdnBuildExclusionReason::kPriorDownloadFailure);
    cdn_entry = SelectNextCdnEntry();
  }

  if (local_result) {
    return *local_result;
  }
  auto fallback = TryFallbackToLastResort("CDN download failed");
  if (fallback) {
    return *fallback;
  }
  return last_download_result.value_or(Result::Error(
      kExitCodeNoMatchingVersion,
      BuildNoMatchingCdnVersionMessage(manifest_entries, GetCurrentPlatform(),
                                       config.vmin, config.vmax,
                                       config.abi_hash, exclusion_reasons)));
}

Result Controller::RunRetention(Command command,
                                DirectoryRole role,
                                const base::FilePath& install_dir,
                                const std::vector<base::FilePath>& read_dirs,
                                const ExtendedConfig& extended_config,
                                ProgressCallback progress) {
  DCHECK(command == Command::kRetentionDryRun ||
         command == Command::kRetentionApply);
  database_ = std::make_unique<Database>();
  DatabaseError load_error = database_->Load(
      GetDatabasePath(install_dir), IntegrityMismatchAction::kPreserve);
  if (load_error != DatabaseError::kSuccess &&
      load_error != DatabaseError::kSchemaVersionTooNew) {
    Result result = Result::Error(
        kExitCodeDatabaseError,
        "Retention could not load authoritative registration state: " +
            std::string(DatabaseErrorToString(load_error)));
    RetentionPlan plan;
    plan.store_blocked = true;
    plan.blocker = "database_load_failed";
    result.retention_plan = std::move(plan);
    result.retention_max_age_days = extended_config.retention_max_age_days;
    return result;
  }
  if (!ReportProgress(progress, kStepVersionCheck)) {
    return Result::Error(kExitCodeCancelled, "Operation cancelled by user");
  }

  std::vector<InstalledVersion> installed;
  if (ReadVersionIndex(install_dir, &installed,
                       VersionIndexReadMode::kPreserveCorrupt) !=
      MetadataError::kSuccess) {
    installed = ScanInstalledVersionsWithMetadata(install_dir);
  }
  if (!ReportProgress(progress, kStepVersionCheck)) {
    return Result::Error(kExitCodeCancelled, "Operation cancelled by user");
  }
  std::vector<RevokedVersionRange> revoked = LoadEffectiveRevocationList(
      read_dirs, IntegrityMismatchAction::kPreserve);
  if (!ReportProgress(progress, kStepVersionCheck)) {
    return Result::Error(kExitCodeCancelled, "Operation cancelled by user");
  }
  RetentionOptions options{
      .max_age_days = extended_config.retention_max_age_days,
      .now = GetCurrentWallTime(),
  };
  auto build_plan = [&](const RetentionEvidenceMap& evidence) {
    RetentionPlan preliminary = BuildRetentionPlan(
        role, *database_, evidence, installed, revoked, {}, options);
    std::set<RetentionRegistrationKey> preliminary_removed(
        preliminary.candidates.begin(), preliminary.candidates.end());
    std::vector<AppEntry> remaining;
    for (const auto& app : database_->GetAllApps()) {
      if (!preliminary_removed.contains({app.uuid, app.platform})) {
        remaining.push_back(app);
      }
    }
    std::set<VersionKey> confirmed =
        CollectConfirmedVersionProtection(install_dir, remaining);
    return BuildRetentionPlan(role, *database_, evidence, installed, revoked,
                              confirmed, options);
  };
  std::set<VersionKey> pending_before;
  if (command == Command::kRetentionApply) {
    if (!ReadRetentionPendingVersions(install_dir, &pending_before)) {
      return Result::Error(kExitCodeDatabaseError,
                           "Retention retry state is invalid and was "
                           "preserved");
    }
    if (!pending_before.empty()) {
      std::vector<InstalledVersion> scanned =
          ScanInstalledVersionsWithMetadata(install_dir);
      for (const auto& pending : pending_before) {
        const bool already_loaded =
            std::ranges::any_of(installed, [&](const InstalledVersion& item) {
              return item.metadata.version == pending.version &&
                     item.metadata.platform == pending.platform;
            });
        if (already_loaded) {
          continue;
        }
        auto found =
            std::ranges::find_if(scanned, [&](const InstalledVersion& item) {
              return item.metadata.version == pending.version &&
                     item.metadata.platform == pending.platform;
            });
        if (found != scanned.end()) {
          installed.push_back(*found);
        }
      }
    }
  }
  options.now = GetCurrentWallTime();
  RetentionEvidenceSnapshot preliminary_snapshot =
      CollectRetentionEvidenceSnapshot(install_dir, database_->GetAllApps());
  RetentionPlan plan = build_plan(preliminary_snapshot.evidence);

  auto make_result = [&](Result result) {
    result.retention_plan = plan;
    result.retention_max_age_days = options.max_age_days;
    return result;
  };
  if (!plan.eligible || plan.store_blocked) {
    return make_result(
        Result::Error(kExitCodeConfigError,
                      "Registration retention is blocked: " + plan.blocker));
  }
  if (command == Command::kRetentionDryRun) {
    return make_result(Result::Success({}, ""));
  }

  // Give the caller a checkpoint before the authoritative final collection.
  if (!ReportProgress(progress, kStepVersionCheck)) {
    return make_result(
        Result::Error(kExitCodeCancelled, "Operation cancelled by user"));
  }

  options.now = GetCurrentWallTime();
  RetentionEvidenceSnapshot final_snapshot =
      CollectRetentionEvidenceSnapshot(install_dir, database_->GetAllApps());
  RetentionPlan final_plan = build_plan(final_snapshot.evidence);
  if (!final_plan.eligible || final_plan.store_blocked) {
    plan = std::move(final_plan);
    return make_result(
        Result::Error(kExitCodeConfigError,
                      "Registration retention is blocked: " + plan.blocker));
  }

  auto version_removal_scope = [](const RetentionPlan& value) {
    std::set<VersionKey> result;
    for (const auto& version : value.versions) {
      if (version.expected_removal) {
        result.insert({version.version, version.platform});
      }
    }
    return result;
  };
  const bool plan_changed =
      plan.candidates != final_plan.candidates ||
      version_removal_scope(plan) != version_removal_scope(final_plan);
  plan = std::move(final_plan);
  if (plan_changed) {
    Result changed = Result::Error(
        kExitCodeRetentionSnapshotChanged,
        "Launch evidence changed during retention final validation");
    changed.retry_required = true;
    changed.warnings.push_back(
        "Review the recomputed retention report before retrying apply");
    return make_result(std::move(changed));
  }

  // Final cancellation checkpoint. Evidence written while the callback runs
  // is ordered after the registration's completed final observation and is
  // preserved by
  // compare-before-delete cleanup. Once accepted, the UI enters committing
  // state until the logically coupled publication sequence completes.
  if (!ReportProgress(progress, kStepVersionCheck)) {
    return make_result(
        Result::Error(kExitCodeCancelled, "Operation cancelled by user"));
  }
  // Announce the non-cancellable logical publication phase. Cancellation
  // responses to this state are intentionally ignored.
  ReportProgress(progress, kStepCommitting);
#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
  if (g_retention_post_validation_evidence_change && !plan.candidates.empty()) {
    const auto& [appid, platform] = plan.candidates.front();
    WriteLivenessPath(
        GetInstallDirLivenessPath(install_dir, GetAppidHash(appid), platform),
        {appid, platform, GetCurrentWallTime()});
  }
#endif
  for (auto& version : plan.versions) {
    if (!version.required_after &&
        pending_before.contains({version.version, version.platform}) &&
        version.decision != RetentionVersionDecision::kConfirmedProtected) {
      version.expected_removal = true;
    }
  }

  std::set<RetentionRegistrationKey> removed(plan.candidates.begin(),
                                             plan.candidates.end());
  std::set<VersionKey> version_scope = pending_before;
  for (const auto& version : plan.versions) {
    if (version.expected_removal) {
      version_scope.insert({version.version, version.platform});
    }
  }
  if (version_scope != pending_before &&
      !WriteRetentionPendingVersions(install_dir, version_scope)) {
    return make_result(Result::Error(
        kExitCodeDatabaseError,
        "Failed to publish retention retry state before database commit"));
  }
  if (!removed.empty()) {
    for (const auto& [appid, platform] : removed) {
      database_->UnregisterApp(appid, platform);
    }
    DatabaseError save_error = database_->Save(GetDatabasePath(install_dir));
    if (save_error != DatabaseError::kSuccess) {
      Result failed =
          Result::Error(kExitCodeDatabaseError,
                        "Failed to publish retained registration database: " +
                            std::string(DatabaseErrorToString(save_error)));
      if (!RestoreRetentionPendingVersions(install_dir, pending_before)) {
        failed.retry_required = true;
        failed.warnings.push_back(
            "Failed to roll back retention retry state after database "
            "publication failure");
      }
      return make_result(std::move(failed));
    }
  }

  const bool has_version_work = !version_scope.empty();
  Result result = Result::Success({}, "");
  result.registrations_committed = true;
  if (has_version_work) {
    base::OnceCallback<bool()> logical_commit_complete = base::BindOnce(
        [](const std::set<RetentionRegistrationKey>& registrations,
           const RetentionEvidenceSnapshot* snapshot, Result* retention_result,
           ProgressCallback progress) {
          CompleteRetentionLogicalCommit(registrations, *snapshot,
                                         retention_result);
          return ReportProgress(progress, kStepCleanup);
        },
        removed, base::Unretained(&final_snapshot), base::Unretained(&result),
        progress);
    Result prune =
        PruneUnusedVersions(install_dir, Config{}, installed, revoked,
                            /*retention_apply=*/true, &version_scope,
                            std::move(logical_commit_complete));
    if (!prune.success) {
      prune.warnings.insert(prune.warnings.end(), result.warnings.begin(),
                            result.warnings.end());
      Result failed = make_result(std::move(prune));
      failed.registrations_committed = true;
      failed.versions_pruned = false;
      failed.retry_required = true;
      return failed;
    }
    result.versions_pruned = true;
    if (prune.outcome == Outcome::kCleanupDeferred) {
      result.outcome = Outcome::kCleanupDeferred;
      result.retry_required = prune.retry_required;
      result.warnings.insert(result.warnings.end(), prune.warnings.begin(),
                             prune.warnings.end());
      for (auto& version : plan.versions) {
        version.cleanup_deferred = prune.deferred_version_cleanup.contains(
            {version.version, version.platform});
      }
    }
  } else {
    result.versions_pruned = true;
    CompleteRetentionLogicalCommit(removed, final_snapshot, &result);
    ReportProgress(progress, kStepCleanup);
  }

  if (!result.retry_required &&
      !RestoreRetentionPendingVersions(install_dir, {})) {
    result.outcome = Outcome::kCleanupDeferred;
    result.warnings.push_back("Deferred retention retry-state cleanup");
  }

  return make_result(std::move(result));
}

Result Controller::PruneUnusedVersions(
    const base::FilePath& install_dir,
    const Config& current_config,
    std::vector<InstalledVersion> installed,
    const std::vector<RevokedVersionRange>& revoked,
    bool retention_apply,
    const std::set<VersionKey>* retention_scope,
    base::OnceCallback<bool()> logical_commit_complete) {
  if (!database_ || !database_->CanPrune()) {
    // Don't prune if database has newer schema (forward compatibility)
    return Result::Success({}, "");
  }

  // Get installed versions (all platforms)
  if (installed.empty()) {
    if (ReadVersionIndex(install_dir, &installed) != MetadataError::kSuccess) {
      installed = ScanInstalledVersionsWithMetadata(install_dir);
    }
  }

  // Compute which versions are required (per platform)
  std::set<VersionKey> required =
      GetRequiredVersionSet(*database_, installed, revoked);

  // Single pass over .launch/ files: classify canonical records before
  // collecting confirmed-version protection, and snapshot records eligible
  // for post-prune age cleanup.
  //
  // This is intentionally NOT gated to the current platform. Version pruning
  // below is cross-platform (a 64-bit installer can prune orphaned 32-bit
  // versions and vice versa), so launch-state handling must be cross-platform
  // too — otherwise a prune on one platform could delete a version that another
  // platform's launch health is keeping as a rollback target, or let an
  // orphaned other-platform sentinel wrongly protect a dead version. The shared
  // database provides registration context for every platform, so each .launch/
  // file is evaluated against its own (appid, platform) and that platform's
  // vmin.
  std::set<ConfirmedVersionKey> confirmed;
  std::vector<LaunchRecordSnapshot> repair_launch_files;
  std::vector<LaunchRecordSnapshot> expired_launch_files;
  {
    // Registered (appid, platform) pairs across all platforms. An appid
    // registered for one platform does not make another platform's launch
    // file live, so the platform is part of the key.
    std::set<std::pair<std::string, std::string>> registered_apps;
    for (const auto& app : database_->GetAllApps()) {
      registered_apps.insert({app.uuid, app.platform});
    }

    // Per-platform global vmin, computed lazily and cached (a handful of
    // platforms at most).
    std::map<std::string, Version> global_vmin_by_platform;
    auto global_vmin_for = [&](const std::string& platform) -> Version {
      auto it = global_vmin_by_platform.find(platform);
      if (it == global_vmin_by_platform.end()) {
        it = global_vmin_by_platform
                 .emplace(platform, database_->GetGlobalVmin(platform))
                 .first;
      }
      return it->second;
    };

    base::FilePath launch_dir = GetLaunchStateDir(install_dir);
    if (!retention_apply && IsReparsePoint(launch_dir) &&
        !VerifySafeDirectoryPath(launch_dir)) {
      Logger::GetInstance().Warning(
          "Failed to repair unsafe launch-health directory during prune");
    }
    if (base::DirectoryExists(launch_dir) && !IsReparsePoint(launch_dir)) {
      std::optional<base::FilePath> resolved_launch_dir =
          GetSafeDirectoryResolvedPath(launch_dir);
      if (!resolved_launch_dir) {
        Logger::GetInstance().Warning(
            "Failed to resolve launch-health directory during prune");
      } else {
        base::FileEnumerator enumerator(launch_dir, /*recursive=*/false,
                                        base::FileEnumerator::FILES);
        for (base::FilePath path = enumerator.Next(); !path.empty();
             path = enumerator.Next()) {
          std::optional<LaunchRecordSnapshot> snapshot =
              ReadLaunchRecordSnapshot(path, *resolved_launch_dir);
          if (!snapshot) {
            // An indeterminate observation is preserved for a later pass.
            continue;
          }

          // Writer-locked prune is the repair path for conclusively invalid,
          // corrupt, footerless, and abandoned temporary launch files. Keep
          // the exact observation so a concurrent valid replacement survives.
          if (snapshot->kind == LaunchRecordKind::kInvalid) {
            if (!retention_apply) {
              repair_launch_files.push_back(std::move(*snapshot));
            }
            continue;
          }

          if (snapshot->kind == LaunchRecordKind::kLiveness) {
            const LivenessRecord& liveness = snapshot->liveness;
            base::FilePath canonical = GetInstallDirLivenessPath(
                install_dir, GetAppidHash(liveness.appid), liveness.platform);
            if (path != canonical) {
              if (!retention_apply) {
                repair_launch_files.push_back(std::move(*snapshot));
              }
              continue;
            }
            const bool orphaned =
                !registered_apps.contains({liveness.appid, liveness.platform});
            if (!retention_apply && orphaned &&
                ClassifyLaunchStateGcAge(liveness.last_launch) ==
                    LaunchStateGcAge::kExpired) {
              expired_launch_files.push_back(std::move(*snapshot));
            }
            // Liveness records never protect a version.
            continue;
          }

          const LaunchState& ls = snapshot->health;
          Version ver = Version::Parse(ls.version);
          base::FilePath canonical_path;
          if (ver.IsValid()) {
            canonical_path = GetInstallDirLaunchStatePath(
                install_dir, GetAppidHash(ls.appid), ver, ls.platform);
          }
          if (!ver.IsValid() || path != canonical_path) {
            if (!retention_apply) {
              repair_launch_files.push_back(std::move(*snapshot));
            }
            continue;
          }

          // The mutating app's opt-out state keeps its existing no-grace
          // cleanup behavior. Other apps' modes are not known here.
          if (current_config.launch_health == LaunchHealthMode::kOff &&
              ls.appid == current_config.appid &&
              ls.platform == GetCurrentPlatform()) {
            if (!retention_apply) {
              repair_launch_files.push_back(std::move(*snapshot));
            }
            continue;
          }

          const bool orphaned =
              !registered_apps.contains({ls.appid, ls.platform});
          Version global_vmin = global_vmin_for(ls.platform);
          const bool below_vmin = global_vmin.IsValid() && ver < global_vmin;
          if (orphaned || below_vmin) {
            // Eligible history never protects binaries, even when its clock
            // is unavailable or its timestamp is future-dated.
            if (!retention_apply && ClassifyLaunchStateGcAge(ls.last_update) ==
                                        LaunchStateGcAge::kExpired) {
              expired_launch_files.push_back(std::move(*snapshot));
            }
            continue;
          }

          // Active registered confirmed state protects independently of age.
          if (!ls.running && ls.confirmed && ls.consecutive_failures == 0) {
            confirmed.insert({ver, ls.platform});
          }
        }
      }
    }
  }

  // Translate confirmed launch state into the policy-free resolver seam.
  std::set<VersionKey> confirmed_protected;
  for (const auto& iv : installed) {
    ConfirmedVersionKey cvk;
    cvk.version = iv.metadata.version;
    cvk.platform = iv.metadata.platform;
    if (confirmed.count(cvk) > 0) {
      VersionKey key;
      key.version = iv.metadata.version;
      key.platform = iv.metadata.platform;
      Logger::GetInstance().Info(
          "Version " + iv.metadata.version.ToString() +
          " protected from pruning (confirmed launch state in .launch/)");
      confirmed_protected.insert(std::move(key));
    }
  }

  // Find versions to prune (includes platform info)
  std::vector<InstalledVersion> to_prune =
      GetPrunableVersions(installed, required, revoked, confirmed_protected);
  if (retention_scope) {
    std::erase_if(to_prune, [&](const InstalledVersion& version) {
      return !retention_scope->contains(
          {version.metadata.version, version.metadata.platform});
    });
  }

  if (!to_prune.empty()) {
    Logger::GetInstance().Info("Pruning " + std::to_string(to_prune.size()) +
                               " unused versions");
  }

  // Build the set of versions to keep (installed minus prunable).
  std::set<VersionKey> prunable_keys;
  for (const auto& iv : to_prune) {
    VersionKey key;
    key.version = iv.metadata.version;
    key.platform = iv.metadata.platform;
    prunable_keys.insert(std::move(key));
  }
  std::vector<InstalledVersion> kept;
  for (const auto& iv : installed) {
    VersionKey key;
    key.version = iv.metadata.version;
    key.platform = iv.metadata.platform;
    if (prunable_keys.count(key) == 0) {
      kept.push_back(iv);
    }
  }

  if (!to_prune.empty()) {
    // Update the index BEFORE deleting from disk so readers never reference
    // a version that's about to be removed.
    MetadataError index_error = WriteVersionIndex(install_dir, kept);
    if (index_error != MetadataError::kSuccess) {
      return Result::Error(kExitCodeIndexError,
                           "Failed to publish reduced version index: " +
                               std::string(MetadataErrorToString(index_error)));
    }

    if (logical_commit_complete && !std::move(logical_commit_complete).Run()) {
      Result cleanup = Result::Success({}, "");
      cleanup.outcome = Outcome::kCleanupDeferred;
      cleanup.retry_required = true;
      cleanup.warnings.push_back(
          "Physical version cleanup deferred after cancellation");
      for (const InstalledVersion& iv : to_prune) {
        cleanup.deferred_version_cleanup.insert(
            {iv.metadata.version, iv.metadata.platform});
      }
      return cleanup;
    }
#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
    if (g_fail_retention_post_index) {
      return Result::Error(kExitCodeIndexError,
                           "Simulated interruption after index publication");
    }
#endif

    // Delete prunable versions from disk.
    Result cleanup = Result::Success({}, "");
    for (const InstalledVersion& iv : to_prune) {
      FileOpsError err = UninstallVersion(install_dir, iv.metadata.version,
                                          iv.metadata.platform);
      if (err == FileOpsError::kInUse) {
        Logger::GetInstance().Warning(
            "Version " + iv.metadata.version.ToString() + "/" +
            iv.metadata.platform + " is in use, will retry later");
      } else if (err != FileOpsError::kSuccess) {
        Logger::GetInstance().Warning(
            "Failed to uninstall version " + iv.metadata.version.ToString() +
            "/" + iv.metadata.platform + ": " + FileOpsErrorToString(err));
      }
      if (err != FileOpsError::kSuccess) {
        cleanup.deferred_version_cleanup.insert(
            {iv.metadata.version, iv.metadata.platform});
        cleanup.outcome = Outcome::kCleanupDeferred;
        cleanup.warnings.push_back(
            "Deferred removal of " + iv.metadata.version.ToString() + "/" +
            iv.metadata.platform + ": " + FileOpsErrorToString(err));
      }
    }
    if (cleanup.outcome == Outcome::kCleanupDeferred) {
      return cleanup;
    }
  } else if (logical_commit_complete) {
    std::move(logical_commit_complete).Run();
  }

  // Repair invalid debris and delete expired canonical records only after
  // version pruning reaches its normal cleanup boundary. Exact-publication
  // deletion preserves any concurrent atomic replacement.
  if (!retention_apply) {
    for (const auto& snapshot : repair_launch_files) {
      ConditionalDeleteResult delete_result =
          snapshot.reparse_point
              ? DeleteFileIfReparsePoint(snapshot.path,
                                         snapshot.resolved_parent_path)
          : snapshot.oversized
              ? DeleteFileIfOversized(snapshot.path, kMaxLaunchStateFileSize,
                                      snapshot.resolved_parent_path)
              : DeleteFileRawIfMatching(snapshot.path, snapshot.raw_content,
                                        snapshot.resolved_parent_path);
      if (delete_result == ConditionalDeleteResult::kError) {
        Logger::GetInstance().Warning(
            "Failed to delete invalid launch-health file: " +
            snapshot.path.AsUTF8Unsafe());
      }
    }
    for (const auto& snapshot : expired_launch_files) {
#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
      if (*g_launch_state_gc_pre_delete_hook) {
        g_launch_state_gc_pre_delete_hook->Run();
      }
#endif
      const uint64_t timestamp = snapshot.kind == LaunchRecordKind::kHealth
                                     ? snapshot.health.last_update
                                     : snapshot.liveness.last_launch;
      // Registration eligibility is stable under the writer lock. Repeat the
      // clock-dependent decision here; the conditional delete below repeats
      // path, parent, schema/integrity payload, and publication identity.
      if (ClassifyLaunchStateGcAge(timestamp) != LaunchStateGcAge::kExpired) {
        continue;
      }
      ConditionalDeleteResult delete_result = DeleteFileWithIntegrityIfMatching(
          snapshot.path, snapshot.content,
          /*expected_integrity_protected=*/true, snapshot.resolved_parent_path);
      if (delete_result == ConditionalDeleteResult::kError) {
        Logger::GetInstance().Warning(
            "Failed to delete expired launch-health file: " +
            snapshot.path.AsUTF8Unsafe());
      }
    }
  }

  return Result::Success({}, "");
}

void Controller::ReleaseLock() {
  lock_.reset();
}

std::optional<CdnBuildEntry> Controller::QueryCdnForVersion(
    const Config& config,
    const ExtendedConfig& extended,
    const base::FilePath& install_dir,
    const std::set<Version>& skip_versions,
    std::vector<CdnBuildEntry>* validated_entries) {
  if (validated_entries) {
    validated_entries->clear();
  }
  // Parse minimum version to get milestone
  Version vmin = Version::Parse(config.vmin);
  if (!vmin.IsValid()) {
    Logger::GetInstance().Error("Invalid minimum version: " + config.vmin);
    return std::nullopt;
  }
  if (effective_download_source_.downloads_disabled()) {
    return std::nullopt;
  }

  base::FilePath cache_dir = GetCacheDirectory(install_dir);

  std::string platform = GetCurrentPlatform();
  DownloadOptions opts;
  opts.max_download_size = 10 * 1024 * 1024;  // 10 MB max for manifests
  opts.allow_http_for_testing = IsTestingMode();
  opts.ignore_certificate_errors_for_testing =
      ShouldIgnoreCertificateErrorsForTesting();
  opts.local_download_path = effective_download_source_.mirror_path;

  const std::vector<std::string>& bases = effective_download_source_.urls;
  const std::string cache_key =
      !config.abi_hash.empty()
          ? BuildAbiHashUrl("", config.abi_hash, platform, config.channel)
          : BuildPlatformUrl("", vmin.GetMilestone(), platform, config.channel);
  bool cache_rejected = false;
  for (const auto& base_url : bases) {
    const std::string manifest_url =
        !config.abi_hash.empty()
            ? BuildAbiHashUrl(base_url, config.abi_hash, platform,
                              config.channel)
            : BuildPlatformUrl(base_url, vmin.GetMilestone(), platform,
                               config.channel);
    std::string content;
    DownloadContentSource content_source = DownloadContentSource::kNetwork;
    DownloadError err = DownloadWithCache(
        manifest_url, cache_dir, &content, opts,
        extended.force_check || cache_rejected, StaleCacheFallback::kSkip,
        cache_key, CacheWriteBehavior::kDefer, &content_source);
    if (err != DownloadError::kSuccess) {
      Logger::GetInstance().LogDownloadFailure(manifest_url, err);
      continue;
    }
    std::vector<CdnBuildEntry> entries;
    ManifestError manifest_err = ParsePlatformManifest(content, &entries);
    if (manifest_err != ManifestError::kSuccess) {
      Logger::GetInstance().Error(
          "Failed to parse manifest: " +
          std::string(ManifestErrorToString(manifest_err)));
      if (content_source == DownloadContentSource::kCache) {
        cache_rejected = true;
        DiscardDownloadCache(cache_dir, cache_key);
      }
      continue;
    }
    if (opts.local_download_path.empty() &&
        content_source == DownloadContentSource::kNetwork) {
      WriteDownloadCache(cache_dir, cache_key, content);
    }
    if (validated_entries) {
      *validated_entries = entries;
    }
    return FindBestBuildEntry(entries, config.vmin, config.vmax,
                              config.abi_hash, skip_versions);
  }

  if (!extended.force_check && !cache_rejected) {
    std::string content;
    if (ReadDownloadCache(cache_dir, cache_key, &content) ==
        DownloadError::kSuccess) {
      std::vector<CdnBuildEntry> entries;
      if (ParsePlatformManifest(content, &entries) == ManifestError::kSuccess) {
        if (validated_entries) {
          *validated_entries = entries;
        }
        return FindBestBuildEntry(entries, config.vmin, config.vmax,
                                  config.abi_hash, skip_versions);
      }
    }
  }
  return std::nullopt;
}

Result Controller::DownloadAndInstall(const CdnBuildEntry& entry,
                                      const ExtendedConfig& extended,
                                      const base::FilePath& install_dir,
                                      ProgressCallback progress,
                                      bool* try_next_candidate) {
  if (try_next_candidate) {
    *try_next_candidate = false;
  }
  // Report downloading step
  if (!ReportProgress(progress, kStepDownload)) {
    return Result::Error(kExitCodeCancelled, "Operation cancelled by user");
  }
  if (effective_download_source_.downloads_disabled()) {
    return Result::Error(
        kExitCodePolicyDenied,
        BuildPolicyDenialMessage(Command::kInstall, "downloads are disabled"));
  }

  // Set up cache directory (used for both hash sidecar and archive caching).
  base::FilePath cache_dir = GetCacheDirectory(install_dir);
  if (!VerifySafeDirectoryPath(cache_dir)) {
    return Result::Error(kExitCodeInstallError,
                         "Failed to remove cache directory reparse point");
  }
  if (!base::CreateDirectory(cache_dir)) {
    return Result::Error(kExitCodeInstallError,
                         "Failed to create cache directory");
  }

  const std::vector<std::string>& bases = effective_download_source_.urls;
  base::FilePath archive_path;
  DownloadError last_error = DownloadError::kNetworkError;
  CleanDownloadRetryBudget clean_retry_budget;
  bool saw_archive_attempt = false;
  bool all_archives_missing = true;
  bool saw_transport_hash_mismatch = false;
  base::FilePath previous_archive_path;
  for (size_t origin = 0; origin < bases.size(); ++origin) {
    // Never contact a new origin while a partial from the prior origin is
    // still present. In particular, this must happen before requesting the
    // new origin's hash sidecar.
    if (origin > 0 && !previous_archive_path.empty() &&
        DiscardDownloadPartials(previous_archive_path) !=
            DownloadError::kSuccess) {
      return Result::Error(kExitCodeInstallError,
                           "Failed to discard prior-origin partial archive");
    }
    const std::string archive_url = BuildArchiveUrl(bases[origin], entry.file);
    const std::string hash_url = BuildHashFileUrl(bases[origin], entry.file);
    DownloadOptions opts;
    opts.allow_http_for_testing = IsTestingMode();
    opts.ignore_certificate_errors_for_testing =
        ShouldIgnoreCertificateErrorsForTesting();
    opts.receive_timeout_ms = extended.download_timeout_ms;
    opts.local_download_path = effective_download_source_.mirror_path;
    opts.enable_resume = true;
    opts.clean_retry_budget = &clean_retry_budget;
    int response_status_code = 0;
    opts.response_status_code = &response_status_code;
    DownloadOptions hash_opts;
    hash_opts.allow_http_for_testing = IsTestingMode();
    hash_opts.ignore_certificate_errors_for_testing =
        ShouldIgnoreCertificateErrorsForTesting();
    hash_opts.local_download_path = effective_download_source_.mirror_path;

    // The sidecar and archive are one coherent origin attempt. If that
    // origin lacks a sidecar, its manifest SHA1 is the fallback.
    std::string sha256_content;
    if (DownloadToString(hash_url, &sha256_content, hash_opts) ==
            DownloadError::kSuccess &&
        sha256_content.size() >= 64) {
      opts.expected_sha256 = sha256_content.substr(0, 64);
    } else {
      opts.expected_sha1 = entry.sha1;
    }
    const std::string archive_hash =
        base::ToLowerASCII(opts.expected_sha256.empty() ? opts.expected_sha1
                                                        : opts.expected_sha256);
    if (archive_hash.empty()) {
      continue;
    }
    archive_path = cache_dir.AppendASCII(archive_hash + ".tar.xz");
    previous_archive_path = archive_path;
    if (!VerifySafeFilePath(archive_path)) {
      return Result::Error(kExitCodeInstallError,
                           "Failed to remove archive reparse point");
    }
    const bool already_cached =
        base::PathExists(archive_path) &&
        VerifyFileHash(archive_path, opts.expected_sha256, opts.expected_sha1);
    if (already_cached) {
      Logger::GetInstance().Info("Using cached archive: " +
                                 archive_path.AsUTF8Unsafe());
      last_error = DownloadError::kSuccess;
      break;
    }
    if (base::PathExists(archive_path) && !base::DeleteFile(archive_path)) {
      return Result::Error(kExitCodeInstallError,
                           "Failed to remove invalid cached archive");
    }
    if (progress) {
      opts.progress_callback = base::BindRepeating(
          [](ProgressCallback cb, uint64_t done, uint64_t total) {
            return cb.Run(kStepDownload, done, total);
          },
          progress);
    }
    Logger::GetInstance().LogDownloadAttempt(archive_url,
                                             static_cast<int>(origin + 1),
                                             static_cast<int>(bases.size()));
    const bool local_archive_missing =
        !opts.local_download_path.empty() &&
        !base::PathExists(opts.local_download_path.AppendASCII(entry.file));
    last_error = local_archive_missing
                     ? DownloadError::kNetworkError
                     : DownloadFile(archive_url, archive_path, opts);
    saw_archive_attempt = true;
    const bool archive_missing =
        local_archive_missing || response_status_code == 404;
    all_archives_missing &= archive_missing;
    saw_transport_hash_mismatch |= last_error == DownloadError::kHashMismatch;
    if (last_error == DownloadError::kSuccess) {
      break;
    }
    Logger::GetInstance().LogDownloadFailure(archive_url, last_error);
    base::DeleteFile(archive_path);
    if (last_error == DownloadError::kCancelled) {
      return Result::Error(kExitCodeCancelled, "Download cancelled");
    }
    if (last_error == DownloadError::kFileWriteError) {
      return Result::Error(kExitCodeInstallError,
                           "Failed to write downloaded archive");
    }
  }
  if (last_error != DownloadError::kSuccess) {
    if (saw_archive_attempt && all_archives_missing && !archive_path.empty() &&
        DiscardDownloadPartials(archive_path) != DownloadError::kSuccess) {
      return Result::Error(kExitCodeInstallError,
                           "Failed to discard unavailable archive partial");
    }
    if (try_next_candidate && saw_archive_attempt &&
        (all_archives_missing || saw_transport_hash_mismatch)) {
      *try_next_candidate = true;
    }
    return Result::Error(
        kExitCodeNetworkError,
        "Download failed: " + std::string(DownloadErrorToString(last_error)));
  }

  // Report verification step
  if (!ReportProgress(progress, kStepExtract)) {
    return Result::Error(kExitCodeCancelled, "Operation cancelled by user");
  }

  // Create staging directory under install_dir to guarantee same-volume
  // atomic renames into Versions/<version>/<platform>. This avoids the
  // cross-volume fallback (copy+delete) that %TEMP% would require when the
  // install directory is on a different volume.
  base::FilePath staging_root = install_dir.Append(kStagingSubdirectory);
  if (!VerifySafeDirectoryPath(staging_root)) {
    return Result::Error(kExitCodeInstallError,
                         "Failed to remove staging directory reparse point");
  }
  if (!base::CreateDirectory(staging_root)) {
    return Result::Error(kExitCodeInstallError,
                         "Failed to create staging directory");
  }
  base::FilePath temp_dir;
  if (!base::CreateTemporaryDirInDir(staging_root, L"cef_install_",
                                     &temp_dir)) {
    return Result::Error(kExitCodeInstallError,
                         "Failed to create temp directory");
  }
  ScopedDirectoryDeleter temp_dir_deleter(temp_dir);

  // Extract archive
  if (!ReportProgress(progress, kStepExtract)) {
    return Result::Error(kExitCodeCancelled, "Operation cancelled by user");
  }

  base::FilePath extract_dir = temp_dir.Append(L"extracted");
  ExtractionProgressCallback extract_progress;
  if (progress) {
    extract_progress = base::BindRepeating(
        [](ProgressCallback cb, uint64_t bytes_done, uint64_t bytes_total) {
          return cb.Run(kStepExtract, bytes_done, bytes_total);
        },
        progress);
  }

  Logger::GetInstance().Info("Extracting archive to " +
                             extract_dir.AsUTF8Unsafe());

  ExtractionConfig extraction_config;
  extraction_config.background_mode = extended.background_mode;
  ArchiveError extract_err = ExtractTarXz(archive_path, extract_dir,
                                          extraction_config, extract_progress);
  if (extract_err != ArchiveError::kSuccess) {
    if (extract_err == ArchiveError::kCancelled) {
      return Result::Error(kExitCodeCancelled, "Operation cancelled by user");
    }
    if (try_next_candidate && IsCandidateSpecificArchiveError(extract_err)) {
      *try_next_candidate = true;
    }
    return Result::Error(
        kExitCodeExtractionError,
        "Extraction failed: " + std::string(ArchiveErrorToString(extract_err)));
  }

  // Report signature verification step
  if (!ReportProgress(progress, kStepSignatureVerify)) {
    return Result::Error(kExitCodeCancelled, "Operation cancelled by user");
  }

  // Verify signatures using catalog (required)
  CatalogProgressCallback catalog_progress;
  if (progress) {
    catalog_progress = base::BindRepeating(
        [](ProgressCallback cb, uint64_t files_done, uint64_t files_total) {
          return cb.Run(kStepSignatureVerify, files_done, files_total);
        },
        progress);
  }

  Logger::GetInstance().Info("Verifying catalog signature in " +
                             extract_dir.AsUTF8Unsafe());
  SignatureError sig_err = VerifyWithCatalog(
      extract_dir, extended.certificate_thumbprint, catalog_progress);
  if (sig_err == SignatureError::kCancelled) {
    return Result::Error(kExitCodeCancelled, "Operation cancelled by user");
  }
  if (sig_err != SignatureError::kSuccess) {
    Logger::GetInstance().LogSignatureFailure(
        extract_dir.Append(kCatalogFilename), extended.certificate_thumbprint,
        "");
    if (try_next_candidate && IsCandidateSpecificSignatureError(sig_err)) {
      *try_next_candidate = true;
    }
    return Result::Error(kExitCodeSignatureError,
                         "Signature verification failed: " +
                             std::string(SignatureErrorToString(sig_err)));
  }

  // Report installing step
  if (!ReportProgress(progress, kStepInstall)) {
    return Result::Error(kExitCodeCancelled, "Operation cancelled by user");
  }

  // Verify version metadata from the archive. The build includes
  // cef_version.json in the signed catalog, so after catalog verification
  // we can trust its contents. Verify that the version matches what the
  // CDN manifest claimed to detect CDN-side misconfigurations.
  VersionMetadata metadata;
  MetadataError meta_err = ReadVersionMetadata(extract_dir, &metadata);
  if (meta_err != MetadataError::kSuccess) {
    if (try_next_candidate && IsCandidateSpecificMetadataError(meta_err)) {
      *try_next_candidate = true;
    }
    return Result::Error(kExitCodeInstallError,
                         "Archive missing cef_version.json: " +
                             std::string(MetadataErrorToString(meta_err)));
  }
  if (metadata.version != entry.version) {
    if (try_next_candidate) {
      *try_next_candidate = true;
    }
    return Result::Error(kExitCodeInstallError,
                         "Archive version mismatch: expected " +
                             entry.version.ToString() + ", got " +
                             metadata.version.ToString());
  }

  // Install to final location
  bool replacement_cleanup_deferred = false;
  FileOpsError install_err = InstallVersion(
      extract_dir, install_dir, entry.version, &replacement_cleanup_deferred);
  if (install_err != FileOpsError::kSuccess) {
    int error_code = install_err == FileOpsError::kQuarantineFailed
                         ? kExitCodeQuarantineError
                     : install_err == FileOpsError::kRepairFailed
                         ? kExitCodeRepairError
                         : kExitCodeInstallError;
    return Result::Error(error_code,
                         "Installation failed: " +
                             std::string(FileOpsErrorToString(install_err)));
  }

  // Report cleaning step
  ReportProgress(progress, kStepCleanup);

  // Update version index after install so readers see the new version.
  // Then prune unused versions (which updates the index again before
  // deleting anything from disk).
  std::vector<InstalledVersion> installed;
  MetadataError current_index_error = ReadVersionIndex(install_dir, &installed);
  if (current_index_error != MetadataError::kSuccess) {
    return Result::Error(
        kExitCodeIndexError,
        "Failed to read current version index: " +
            std::string(MetadataErrorToString(current_index_error)));
  }
  std::erase_if(installed, [&](const InstalledVersion& iv) {
    return iv.metadata.version == metadata.version &&
           iv.metadata.platform == metadata.platform;
  });
  installed.push_back({metadata, GetVersionPath(install_dir, entry.version)});
  MetadataError index_error = WriteVersionIndex(install_dir, installed);
  if (index_error != MetadataError::kSuccess) {
    return Result::Error(kExitCodeIndexError,
                         "Failed to publish version index: " +
                             std::string(MetadataErrorToString(index_error)));
  }
  version_published_ = true;
  pending_archive_cleanup_path_ = archive_path;

  // Return success
  base::FilePath version_path = GetVersionPath(install_dir, entry.version);
  std::string version_str = entry.version.ToString();
  base::FilePath final_path = GetLibcefPath(version_path);
  Logger::GetInstance().LogInstallationCompleted(version_str, final_path);

  Result result = MakeValidatedLibcefResult(install_dir, version_path,
                                            version_str, metadata.version_full);
  if (result.success && replacement_cleanup_deferred) {
    result.outcome = Outcome::kCleanupDeferred;
    result.warnings.push_back(
        "Existing destination quarantine cleanup deferred");
  }
  return result;
}

std::optional<InstalledVersion> Controller::CheckBundledCef(
    const Config& config,
    const ExtendedConfig& extended) {
  base::FilePath bundled_path =
      base::FilePath::FromUTF8Unsafe(extended.bundled_cef_path);

  if (!base::DirectoryExists(bundled_path)) {
    return std::nullopt;
  }

  // A bundled distribution must retain the standard catalog layout even
  // though startup does not verify the catalog signature or hash every member.
  // Reject directories and reparse points so catalog.cat is an actual file in
  // the bundled root, not an alias to content elsewhere.
  const base::FilePath catalog_path = bundled_path.Append(kCatalogFilename);
  base::File::Info catalog_info;
  if (!base::GetFileInfo(catalog_path, &catalog_info) ||
      catalog_info.is_directory || IsReparsePoint(catalog_path)) {
    Logger::GetInstance().Warning(
        "Bundled CEF catalog.cat is missing or is not a regular "
        "non-reparse file");
    return std::nullopt;
  }

  // Read metadata from bundled directory
  VersionMetadata metadata;
  MetadataError err = ReadVersionMetadata(bundled_path, &metadata);
  if (err != MetadataError::kSuccess) {
    Logger::GetInstance().Warning("Failed to read bundled CEF metadata: " +
                                  std::string(MetadataErrorToString(err)));
    return std::nullopt;
  }

  // Validate platform matches current system

  // Build a candidate and use FindBestVersion for version range and ABI hash
  // filtering (same logic used for installed versions). Revocation is not
  // applied — bundled versions ship with the app, not via CDN, so the CDN
  // revocation list does not apply. If a bundled version is bad, the app
  // ships an update to replace it.
  InstalledVersion candidate;
  candidate.metadata = metadata;
  candidate.path = bundled_path;

  // No catalog signature or member verification here. Hashing all files (~236
  // files, ~370 MB in a typical distribution) would add seconds to every app
  // startup. The presence check above enforces layout only. Bundled integrity
  // is the app developer's responsibility through their signed package.

  return candidate;
}

// ============================================================================
// DLL Export Function
// ============================================================================

extern "C" __declspec(dllexport) const char* RunInstaller(
    const char* command,
    const char* config_json) {
  // Thread-local storage for the returned JSON string. Each calling thread
  // gets its own buffer, so concurrent calls from different threads are safe.
  // The returned pointer is valid until the next RunInstaller call on the
  // same thread or until that thread exits.
  thread_local base::NoDestructor<std::string> result_storage;

  // Parse command
  std::optional<Command> cmd = internal::ParseCommand(command ? command : "");
  if (!cmd) {
    Result error = Result::Error(
        kExitCodeConfigError,
        "Invalid command: " + std::string(command ? command : "(null)"));
    *result_storage = error.ToJson();
    return result_storage->c_str();
  }

  // kLaunchSuccess dispatch — must run before config parsing. This command
  // needs nothing from the config (the sentinel path is a process-global set
  // by the bootstrap), and the config-parse block below fails early on a
  // null/empty config, which the cefclient reference passes for this command.
  // Lightweight path: read sentinel, verify PID, write confirmation. No
  // Controller, no lock, no database, no progress UI.
  if (*cmd == Command::kLaunchSuccess) {
    Result result = internal::HandleLaunchSuccess();
    *result_storage = result.ToJson();
    return result_storage->c_str();
  }

  // Parse config
  std::string config_str = config_json ? config_json : "";
  Config config;
  ExtendedConfig extended;
  bool config_parsed = false;
  std::string config_diagnostic;
  const bool retention_command =
      *cmd == Command::kRetentionDryRun || *cmd == Command::kRetentionApply;
  if (retention_command) {
    if (config_str.empty()) {
      config_str = "{}";
    }
    std::optional<base::DictValue> parsed =
        base::JSONReader::ReadDict(config_str, base::JSON_PARSE_RFC);
    if (!parsed) {
      config_diagnostic = "Malformed retention configuration JSON";
    } else {
      static constexpr std::array<std::string_view, 3> kAllowedFields = {
          "install_path", "max_age_days", "log_level"};
      config_parsed = true;
      for (const auto [key, value] : *parsed) {
        if (std::ranges::find(kAllowedFields, key) == kAllowedFields.end()) {
          config_parsed = false;
          config_diagnostic =
              "Unsupported retention option: " + std::string(key);
          break;
        }
        if (key == "install_path" && !value.is_string()) {
          config_parsed = false;
          config_diagnostic = "install_path must be a string";
          break;
        }
        if (key == "log_level" &&
            (!value.is_string() ||
             !LogLevelFromString(value.GetString()).has_value())) {
          config_parsed = false;
          config_diagnostic = "log_level must be info, warning, or error";
          break;
        }
      }
      if (config_parsed) {
        config_parsed = internal::ParseExtendedConfigFromJson(
            config_str, &extended, &config_diagnostic);
      }
      // API retention is headless and reports exclusively through JSON.
      extended.show_progress_ui = false;
    }
  } else if (!config_str.empty()) {
    config_parsed = internal::ParseCombinedConfig(
        config_str, &config, &extended, &config_diagnostic);
  }

  // Fail early if config is missing or invalid — there's no point creating
  // a Controller (which resolves install directories, acquires locks, etc.)
  // only to fail at config validation.
  if (!config_parsed) {
    Result error = Result::Error(
        kExitCodeConfigError,
        config_str.empty() ? "No configuration provided" : config_diagnostic);
    *result_storage = error.ToJson();
    return result_storage->c_str();
  }

#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
  if (config_parsed) {
    ApplyConfigThumbprintOverride(config, &extended);
  }
#endif

  // Determine if we should show progress UI
  // Only show UI if:
  // 1. Config was successfully parsed (so we know caller's intent), AND
  // 2. show_progress_ui is true (default)
  // This prevents error dialogs when config is invalid/empty.
  bool should_show_ui = config_parsed && extended.show_progress_ui;

  // Create progress dialog if needed
  std::unique_ptr<ProgressDialog> dialog;
  if (should_show_ui) {
    dialog = std::make_unique<ProgressDialog>(extended.parent_window);
    dialog->Show();
  }

  // Create progress callback that updates dialog and sends WM_COPYDATA
  ProgressCallback progress_callback;
  if (should_show_ui || extended.parent_window) {
    auto parent_cancel_pending = std::make_shared<bool>(false);
    progress_callback = base::BindRepeating(
        [](ProgressDialog* dlg, HWND parent,
           const std::shared_ptr<bool>& parent_cancel_pending, Step step,
           uint64_t bytes_done, uint64_t bytes_total) -> bool {
          const bool committing = step == kStepCommitting;
          // Update dialog if present
          if (dlg) {
            dlg->SetCancellationDeferred(committing);
            dlg->SetStep(step);
            int percent =
                CalculateOverallProgress(step, bytes_done, bytes_total);
            dlg->SetProgress(percent);

            if (!committing && dlg->WasCancelled()) {
              return false;
            }
          }

          // Send WM_COPYDATA to parent window if specified.
          // Parent returns kWmCopyDataResultCancel to request cancellation.
          if (parent) {
            const bool continue_requested =
                SendProgressToParent(parent, step, bytes_done, bytes_total);
            if (committing && !continue_requested) {
              *parent_cancel_pending = true;
            } else if (!committing &&
                       (!continue_requested || *parent_cancel_pending)) {
              return false;
            }
          }

          return true;
        },
        dialog.get(), extended.parent_window, parent_cancel_pending);
  }

  // Run controller. ThreadPool setup (if needed) is handled internally
  // by the parallel extraction code.
  Controller controller;
  Result result = controller.Run(*cmd, config, extended, progress_callback);

  // Show error dialog on failure (unless in testing mode or user cancelled).
  // Don't show dialog for CANCELLED - user already knows they cancelled.
  if (!result.success && should_show_ui && !IsTestingMode() &&
      result.error_code != kExitCodeCancelled) {
    Logger::GetInstance().Info(std::string("Showing error dialog: ") +
                               internal::ExitCodeToString(result.error_code));
    dialog->ShowErrorDialog(result.error_code);
  }

  // Close progress dialog after the error dialog is dismissed.
  if (dialog) {
    dialog->Close();
  }

  *result_storage = result.ToJson();
  return result_storage->c_str();
}

}  // namespace cef_installer
