// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_CONSTANTS_H_
#define CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_CONSTANTS_H_

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <string_view>

namespace cef_installer {

// Resource name for embedded JSON configuration (RT_RCDATA)
// Using a string name instead of numeric ID to avoid conflicts with
// application-defined resource IDs.
constexpr const wchar_t* kConfigResourceName = L"CEF_INSTALLER_CONFIG";

// Resource name for compiled-in revocation list (RT_RCDATA)
// Embedded in the bootstrap binary at build time from revoked.json.
constexpr const wchar_t* kRevocationResourceName = L"CEF_REVOCATION_LIST";

// Default CDN base URL for CEF builds
constexpr std::string_view kDefaultCdnBaseUrl =
    "https://cef-builds.spotifycdn.com/";

// Hardcoded certificate thumbprint for official CEF signed binaries (SHA-1)
// This is the thumbprint of the code signing certificate used to sign
// all official CEF distributions. Must be updated when certificate is rotated.
// Format: 40 hex characters, uppercase, no separators
constexpr std::string_view kCefCertificateThumbprint =
    "3906F72B1D5FA45A6334A8590495FAEB93347E5B";

// Revocation list URL (relative to CDN base)
constexpr std::string_view kRevocationListPath = "revoked.json";

// Release channels
constexpr std::string_view kChannelStable = "stable";  // Default channel
constexpr std::string_view kChannelBeta = "beta";

// Installer database filename
constexpr std::wstring_view kDatabaseFilename = L"installer.json";

// Version metadata filename (per version directory)
constexpr std::wstring_view kVersionMetadataFilename = L"cef_version.json";

// Version index filename (per install directory root)
constexpr std::wstring_view kVersionIndexFilename = L"versions.json";

// CEF library filename
constexpr std::wstring_view kLibcefFilename = L"libcef.dll";

// Release subdirectory within CEF distribution archives.
// CEF distributions place binaries and resources in a Release/ subdirectory.
constexpr std::wstring_view kReleaseSubdirectory = L"Release";

// Catalog filename for signature verification
constexpr std::wstring_view kCatalogFilename = L"catalog.cat";

// Revocation cache filename (disk-cached CDN delta)
constexpr std::wstring_view kRevocationCacheFilename = L"revocation_cache.json";
constexpr std::wstring_view kRevocationBackoffFilename =
    L"revocation_refresh_backoff.json";
constexpr int kRevocationCacheValiditySeconds = 60 * 60;
constexpr int kRevocationFailureBackoffSeconds = 15 * 60;
constexpr int64_t kMaxRevocationBackoffFileSize = 4096;
constexpr int kLaunchRevocationConnectTimeoutMs = 3000;
constexpr int kLaunchRevocationReceiveTimeoutMs = 2000;
constexpr int kLaunchRevocationOverallTimeoutMs = 5000;

// Automatic-startup emergency recovery limits. The scanner runs only after
// index-backed and bundled selection misses, and only for roots whose index is
// missing or invalid. The elapsed budget is soft because a synchronous Windows
// filesystem operation cannot be interrupted at the deadline.
constexpr size_t kEmergencyRecoveryMaxRoots = 4;
constexpr size_t kEmergencyRecoveryMaxVersionEntries = 128;
constexpr int kEmergencyRecoveryTimeBudgetMs = 500;

// Installer log filename
constexpr std::wstring_view kLogFilename = L"cef_installer.log";

// Maximum log file size before rotation (1 MB)
constexpr uint64_t kMaxLogFileSize = 1 * 1024 * 1024;

// Number of rotated log files to keep
constexpr int kMaxRotatedLogFiles = 3;

// Mutex name prefix for installer lock
// Full name: Global\CEF_Installer_<hash>
constexpr std::string_view kMutexNamePrefix = "Global\\CEF_Installer_";

// Default lock acquisition timeout (30 seconds)
constexpr uint32_t kDefaultLockTimeoutMs = 30000;

// Default download retry settings
constexpr int kDefaultMaxRetries = 3;
constexpr uint32_t kDefaultRetryDelayMs = 1000;

// Manifest cache validity duration (1 hour)
constexpr int kManifestCacheValiditySeconds = 3600;

// Archive cache expiry duration (7 days). Cached archives are kept across
// retries and only deleted after successful installation. This expiry catches
// orphaned archives from failed installs that were never retried.
constexpr int kArchiveCacheValiditySeconds = 7 * 24 * 60 * 60;

// CEF subdirectory name within standard locations (e.g., %ProgramFiles%\CEF)
constexpr std::wstring_view kCefSubdirectory = L"CEF";

// Versions subdirectory within the install directory
constexpr std::wstring_view kVersionsSubdirectory = L"Versions";

// Staging subdirectory for downloads and extraction.
// Placed under the install directory to guarantee same-volume atomic renames
// into Versions/<version>/<platform>.
constexpr std::wstring_view kStagingSubdirectory = L".staging";

// Pending deletion directory
constexpr std::wstring_view kTrashSubdirectory = L".trash";

// ============================================================================
// Security: Field Length Limits
// ============================================================================
// Maximum lengths for string fields to prevent memory exhaustion from
// malformed/malicious data. These limits are enforced when parsing JSON
// from untrusted sources (manifests, database, metadata files).

constexpr size_t kMaxVersionLength = 64;    // Version strings (e.g., "137.3.5")
constexpr size_t kMaxUuidLength = 256;      // Application UUIDs
constexpr size_t kMaxAbiHashLength = 256;   // ABI compatibility hashes
constexpr size_t kMaxFilenameLength = 256;  // Archive filenames
constexpr size_t kMaxSha1Length = 40;       // SHA1 hex strings
constexpr size_t kMaxSha256Length = 64;     // SHA256 hex strings
constexpr size_t kMaxTimestampLength = 64;  // ISO 8601 timestamps
constexpr size_t kMaxVersionFullLength = 256;  // Full version strings
constexpr size_t kMaxReasonLength = 1024;      // Revocation reason strings

// Maximum size for CDN revocation list downloads (1 MB).
// Also used to bound the on-disk revocation cache file size (2x multiplier
// to accommodate CRC32 footer overhead and merges across sessions).
constexpr uint64_t kMaxRevocationDownloadSize = 1 * 1024 * 1024;
constexpr int64_t kMaxRevocationCacheFileSize =
    static_cast<int64_t>(kMaxRevocationDownloadSize) * 2;
constexpr size_t kMaxPlatformLength =
    32;  // Platform strings (e.g., "windows64")

// Maximum number of entries in a revocation list. The 2 MB file size cap
// (kMaxRevocationCacheFileSize) naturally limits entries to ~10K, but an
// explicit limit provides a clearer guarantee against CPU exhaustion during
// parsing and merge operations.
constexpr size_t kMaxRevocationEntryCount = 10000;

// ============================================================================
// Process Exit Codes
// ============================================================================
// Used when running in standalone installer mode (bootstrap.exe /cef-update,
// etc.) Exit code 0 indicates success; non-zero indicates specific error
// categories.
//
// These codes use a base offset of 100 to avoid conflicts with cef_resultcode_t
// values from cef_types.h (Chrome codes: 0-40+, sandbox codes: 7006+).
// The 100-199 range is reserved for installer-specific errors.

constexpr int kInstallerExitCodeBase = 100;

// The requested operation completed logically. Physical cleanup may still be
// deferred and reported as a warning.
constexpr int kExitCodeSuccess = 0;
// Required installer configuration was missing or invalid.
constexpr int kExitCodeConfigError = kInstallerExitCodeBase;  // 100
// A required download or other network operation failed.
constexpr int kExitCodeNetworkError = kInstallerExitCodeBase + 1;  // 101
// Catalog signature or catalog content verification failed.
constexpr int kExitCodeSignatureError = kInstallerExitCodeBase + 2;  // 102
// No non-revoked CEF version satisfied the requested constraints.
constexpr int kExitCodeNoMatchingVersion = kInstallerExitCodeBase + 3;  // 103
// The downloaded archive could not be extracted safely.
constexpr int kExitCodeExtractionError = kInstallerExitCodeBase + 4;  // 104
// Installer setup or publication failure, including indeterminate physical
// containment or a required uninstall relaunch that could not be prepared or
// started.
constexpr int kExitCodeInstallError = kInstallerExitCodeBase + 5;  // 105
// The registration database could not be read, updated, or published.
constexpr int kExitCodeDatabaseError = kInstallerExitCodeBase + 6;  // 106
// The writer lock could not be acquired before the configured timeout.
constexpr int kExitCodeLockTimeout = kInstallerExitCodeBase + 7;  // 107
// The operation was cancelled before its logical commit.
constexpr int kExitCodeCancelled = kInstallerExitCodeBase + 8;  // 108
// Uninstall work is pending in a successfully created child process. Returned
// only for explicit background mode or self-removal from the writable target.
constexpr int kExitCodeRelaunched = kInstallerExitCodeBase + 9;  // 109
// No active launch-health sentinel was found for launch-success confirmation.
constexpr int kExitCodeNoSentinel = kInstallerExitCodeBase + 10;  // 110
// The launch-health sentinel could not be read, validated, or confirmed.
constexpr int kExitCodeSentinelReadError = kInstallerExitCodeBase + 11;  // 111
// The launch-health sentinel belongs to a different process instance.
constexpr int kExitCodeSentinelOwnerMismatch =
    kInstallerExitCodeBase + 12;  // 112
// Valid enterprise policy prohibited a required mutation or download.
constexpr int kExitCodePolicyDenied = kInstallerExitCodeBase + 13;  // 113
// The checked version index could not be read, published, or revalidated.
constexpr int kExitCodeIndexError = kInstallerExitCodeBase + 14;  // 114
// Writer-locked staging, index, or orphan recovery failed.
constexpr int kExitCodeRecoveryError = kInstallerExitCodeBase + 15;  // 115
// Verified staging could not replace a proven-invalid destination.
constexpr int kExitCodeRepairError = kInstallerExitCodeBase + 16;  // 116
// An invalid destination could not be moved safely to quarantine or trash.
constexpr int kExitCodeQuarantineError = kInstallerExitCodeBase + 17;  // 117
// Final validation found that the retention removal scope had changed.
constexpr int kExitCodeRetentionSnapshotChanged =
    kInstallerExitCodeBase + 18;  // 118
// Enterprise policy was unreadable, malformed, or internally conflicting.
constexpr int kExitCodePolicyError = kInstallerExitCodeBase + 19;  // 119
// An unexpected or otherwise unclassified internal error occurred.
constexpr int kExitCodeUnknownError = kInstallerExitCodeBase + 99;  // 199

// ============================================================================
// WM_COPYDATA Progress Notifications
// ============================================================================
// Used for progress communication between the installer and a parent window.

// COPYDATASTRUCT.dwData identifier for CEF installer progress messages.
// 'CEFI' in little-endian: 0x49464543
// Type is uintptr_t to match ULONG_PTR without requiring <windows.h>.
constexpr uintptr_t kWmCopyDataInstallerProgress = 0x43454649;

// Separate 'CEFR' channel for uninstall lifecycle handoff and terminal result
// envelopes. Existing progress receivers may ignore this additive identity.
constexpr uintptr_t kWmCopyDataInstallerLifecycle = 0x43454652;
constexpr int kInstallerLifecycleProtocolVersion = 1;
constexpr size_t kInstallerLifecycleMaxPayloadSize = 32 * 1024;
constexpr size_t kInstallerLifecycleMaxErrorMessageSize = 4096;
constexpr size_t kInstallerLifecycleMaxWarningSize = 1024;
constexpr size_t kInstallerLifecycleMaxWarnings = 32;
constexpr uint32_t kInstallerLifecycleSendTimeoutMs = 500;

// Return this value from a WM_COPYDATA handler to request cancellation.
// Standard WM_COPYDATA return values (TRUE=handled, FALSE=not handled) are
// not treated as cancellation — only this specific sentinel value cancels.
// Type is intptr_t to match LRESULT without requiring <windows.h>.
constexpr intptr_t kWmCopyDataResultCancel = 2;

// ============================================================================
// Progress Steps
// ============================================================================
// Step indices for installer progress. Values are used as array indices.

enum Step {
  kStepInit = 0,
  kStepLock,
  kStepVersionCheck,
  kStepCdnResolve,
  kStepDownload,
  kStepExtract,
  kStepSignatureVerify,
  kStepInstall,
  kStepCommitting,
  kStepCleanup,
  kNumSteps,
};

// Display strings for each step (English fallback).
inline constexpr const char* kStepDisplayStrings[] = {
    "Initializing...",          // kStepInit
    "Initializing...",          // kStepLock
    "Checking versions...",     // kStepVersionCheck
    "Checking versions...",     // kStepCdnResolve
    "Downloading...",           // kStepDownload
    "Extracting...",            // kStepExtract
    "Verifying signatures...",  // kStepSignatureVerify
    "Installing...",            // kStepInstall
    "Committing...",            // kStepCommitting
    "Cleaning up...",           // kStepCleanup
};

static_assert(std::size(kStepDisplayStrings) == kNumSteps,
              "kStepDisplayStrings size mismatch");

// Returns the display string for a step (e.g., "Downloading...").
inline const char* StepDisplayString(Step step) {
  if (step < 0 || step >= kNumSteps) {
    return "Unknown...";
  }
  return kStepDisplayStrings[step];
}

// ============================================================================
// Installer Commands
// ============================================================================
// Commands that can be executed by the installer.

// Directory name for launch state files under the install directory.
// Files in this directory survive version pruning so crash history is
// preserved.
constexpr std::wstring_view kLaunchStateDirName = L".launch";
constexpr int64_t kMaxLaunchStateFileSize = 4096;
constexpr uint64_t kLivenessRefreshIntervalFileTimeTicks =
    30ULL * 24 * 60 * 60 * 10000000;
constexpr uint64_t kLaunchStateGcMaxAge = 90ULL * 24 * 60 * 60 * 10000000;

// Maximum consecutive launch failures before a version is disqualified
constexpr int kMaxConsecutiveFailures = 3;

enum class Command {
  kInstall,          // Install CEF for an application
  kUpdate,           // Update CEF to latest compatible version
  kUninstall,        // Unregister application and prune unused versions
  kQuery,            // Query installed versions (no modifications)
  kPrune,            // Prune unused versions only (no version resolution)
  kRetentionDryRun,  // Report explicit registration-retention effects
  kRetentionApply,   // Apply recomputed registration retention
  kLaunchSuccess,    // Confirm launch health — version is working
};

}  // namespace cef_installer

#endif  // CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_CONSTANTS_H_
