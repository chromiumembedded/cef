// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_DOWNLOAD_H_
#define CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_DOWNLOAD_H_

#include <string>
#include <string_view>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/time/time.h"
#include "cef/libcef_dll/bootstrap/installer/installer_constants.h"

namespace cef_installer {

// Download progress callback.
// Parameters: bytes_downloaded, total_bytes (0 if unknown)
// Return false to cancel the download.
using DownloadProgressCallback =
    base::RepeatingCallback<bool(uint64_t, uint64_t)>;

// Error codes for download operations.
enum class DownloadError {
  kSuccess,
  kInvalidUrl,      // URL is malformed or not HTTPS
  kNetworkError,    // Connection failed, timeout, etc.
  kHttpError,       // Server returned error status (4xx, 5xx)
  kFileWriteError,  // Could not write to destination
  kHashMismatch,    // Downloaded file hash doesn't match expected
  kCancelled,       // Download cancelled via callback
  kFileTooLarge,    // Downloaded file exceeds max_download_size
  kProtocolError,   // Invalid Range/response protocol
};

// Default maximum download size (500 MB for archives)
constexpr uint64_t kDefaultMaxDownloadSize = 500 * 1024 * 1024;

// Default maximum size for in-memory downloads (10 MB for manifests, hashes)
constexpr uint64_t kDefaultMaxStringDownloadSize = 10 * 1024 * 1024;

// One clean full-request retry shared by all configured origins for one
// archive candidate. It is consumed by redirect-origin changes that need a
// new request, invalid Range responses, final-size mismatch, or hash mismatch.
struct CleanDownloadRetryBudget {
  bool consumed = false;

  bool TryConsume() {
    if (consumed) {
      return false;
    }
    consumed = true;
    return true;
  }
};

// Download options.
struct DownloadOptions {
  // Expected SHA256 hash of the file (hex string, 64 chars).
  // If non-empty, downloaded file is verified against this hash.
  std::string expected_sha256;

  // Expected SHA1 hash (hex string, 40 chars) - alternative to SHA256.
  std::string expected_sha1;

  // Progress callback (optional).
  DownloadProgressCallback progress_callback;

  // Maximum download size in bytes. Downloads exceeding this limit are rejected
  // to prevent disk exhaustion attacks. Set to 0 to disable the limit.
  // Default: 500 MB.
  uint64_t max_download_size = kDefaultMaxDownloadSize;

  // TEST ONLY: Allow HTTP URLs for testing with local test servers.
  // In production, only HTTPS is allowed. This flag should never be set
  // outside of unit tests.
  bool allow_http_for_testing = false;

  // TEST ONLY: Ignore certificate errors for loopback HTTPS test servers.
  // Production callers must leave this false; public configuration continues
  // to require normally trusted HTTPS endpoints.
  bool ignore_certificate_errors_for_testing = false;

  // Override for the WinHTTP receive-response timeout (milliseconds).
  // 0 means use the default (60 s). Shorter values let cancellation-aware
  // callers avoid blocking for the full default timeout when the server is
  // unresponsive.
  int receive_timeout_ms = 0;

  // Override for DNS resolution and connection timeouts (milliseconds).
  // 0 means use the defaults. Used with receive_timeout_ms to bound
  // launch-path revocation refresh.
  int connect_timeout_ms = 0;

  // Path to a local directory containing CDN-structured files.
  // When set, DownloadFile and DownloadWithCache read/copy files from this
  // directory instead of fetching from the network. The filename is extracted
  // from the URL path (last component). All usual validation (hash
  // verification, size limits) is still performed.
  base::FilePath local_download_path;

  // Enable origin-bound resumable transfer for a hash-addressed archive.
  // Small in-memory downloads and ordinary file downloads leave this false.
  bool enable_resume = false;

  // Optional candidate-wide retry budget. When null, DownloadFile owns a
  // per-call budget. The pointed-to value must outlive DownloadFile.
  CleanDownloadRetryBudget* clean_retry_budget = nullptr;

  // Optional response-status output for callers that must distinguish archive
  // propagation lag (404) from a general origin failure. Set only after
  // response headers are available; never used for policy decisions.
  int* response_status_code = nullptr;
};

// Download a file from URL to destination path.
// - URL must be HTTPS (HTTP URLs are rejected for security)
// - Creates parent directories if needed
// - Downloads to a temp file first, then renames on success
// - Verifies hash if expected_sha256 or expected_sha1 is provided
DownloadError DownloadFile(const std::string& url,
                           const base::FilePath& dest_path,
                           const DownloadOptions& options = {});

// Safely removes deterministic partial variants for one hash-keyed archive.
// Used before configured-origin failover when sidecars change the hash path.
DownloadError DiscardDownloadPartials(const base::FilePath& dest_path);

// Download a file and return its contents as a string.
// Useful for small files like manifest JSON or hash files.
// Same security requirements as DownloadFile.
// Prefer DownloadWithCache() for repeated downloads of the same URL.
DownloadError DownloadToString(const std::string& url,
                               std::string* content,
                               const DownloadOptions& options = {});

// Download a small response using asynchronous WinHTTP with a cancellable
// absolute deadline. This is used by automatic-startup revocation refresh so
// DNS, connection, response headers, and response body share one wall-clock
// budget. |overall_timeout| must be positive. Progress callbacks are not
// supported because blocking caller code cannot participate in the network
// deadline; a configured callback returns kCancelled before network activity.
DownloadError DownloadToStringWithDeadline(const std::string& url,
                                           std::string* content,
                                           base::TimeDelta overall_timeout,
                                           const DownloadOptions& options = {});

// Compute SHA256 hash of a file (returns lowercase hex string, 64 chars).
// Returns empty string on error.
std::string ComputeFileSha256(const base::FilePath& path);

// Compute SHA1 hash of a file (returns lowercase hex string, 40 chars).
// Returns empty string on error.
std::string ComputeFileSha1(const base::FilePath& path);

// Verify a file's hash against expected value.
// Checks SHA256 first if provided, then SHA1.
// Returns true if at least one hash matches.
bool VerifyFileHash(const base::FilePath& path,
                    const std::string& expected_sha256,
                    const std::string& expected_sha1);

// ============================================================================
// Cache Management for CDN Manifest Files
// ============================================================================

// Get the cache directory path: <install_dir>/.cache/
base::FilePath GetCacheDirectory(const base::FilePath& install_dir);

// Get the cached file path for a URL.
// Converts URL to a safe filename: SHA256(url).cache
base::FilePath GetCacheFilePath(const base::FilePath& cache_dir,
                                const std::string& url);

// Check if a cached file is still valid (not expired).
// Returns true if file exists, is not a reparse point, and was modified
// within |max_age_seconds| (defaults to kManifestCacheValiditySeconds).
// Optionally returns the file's last-modified time for reuse (e.g. for
// If-Modified-Since headers).
// Note: uses a fixed expiry rather than CDN cache-control headers to avoid
// the complexity of HTTP header parsing. This is a reasonable approximation
// for the small files being cached.
bool IsCacheValid(const base::FilePath& cache_path,
                  base::Time* out_last_modified = nullptr,
                  int max_age_seconds = kManifestCacheValiditySeconds);

enum class StaleCacheFallback {
  kAllow,
  kSkip,
};

enum class CacheWriteBehavior {
  kAutomatic,
  kDefer,
};

enum class DownloadContentSource {
  kNetwork,
  kCache,
};

// Reads an integrity-valid cache entry without performing network or creating
// cache directories. |cache_key| is the source-neutral request identity used
// when the entry was written.
DownloadError ReadDownloadCache(const base::FilePath& cache_dir,
                                std::string_view cache_key,
                                std::string* content);

// Promotes artifact-specific validated content to a source-neutral cache
// entry. Callers must validate |content| before calling this function.
DownloadError WriteDownloadCache(const base::FilePath& cache_dir,
                                 std::string_view cache_key,
                                 std::string_view content);

// Discards a cache entry that failed artifact-specific validation.
bool DiscardDownloadCache(const base::FilePath& cache_dir,
                          std::string_view cache_key);

// Download a file with CDN-first caching.
//
// CDN-first flow (when cache exists and is within validity window):
// 1. Send HEAD request with If-Modified-Since to validate cache with CDN
// 2. If 304 Not Modified: serve from cache (CDN-validated)
// 3. If 200 OK: download fresh content and update cache
// 4. If network error: fall back to cache (offline fallback)
//
// When cache is missing, expired, or force_check is true:
// 1. Download fresh content from CDN
// 2. On network failure: try serving from cache as last resort
//
// This minimizes the window where tampered cache content could be served
// to only when the CDN is unreachable.
//
// Set |stale_cache_fallback| to kSkip while trying ordered origins, then call
// ReadDownloadCache() once after all origins fail. A non-empty |cache_key|
// gives those origins one source-neutral cache identity. kDefer preserves
// fresh-cache HEAD/304 behavior but leaves network content uncommitted until
// the caller validates and promotes it with WriteDownloadCache().
DownloadError DownloadWithCache(
    const std::string& url,
    const base::FilePath& cache_dir,
    std::string* content,
    const DownloadOptions& options = {},
    bool force_check = false,
    StaleCacheFallback stale_cache_fallback = StaleCacheFallback::kAllow,
    std::string_view cache_key = {},
    CacheWriteBehavior cache_write_behavior = CacheWriteBehavior::kAutomatic,
    DownloadContentSource* content_source = nullptr);

// Clean up expired cache files.
void PruneCacheDirectory(const base::FilePath& cache_dir);

// Convert error code to human-readable string for logging.
const char* DownloadErrorToString(DownloadError error);

// Check if a URL is valid for downloading (must be HTTPS).
bool IsValidDownloadUrl(const std::string& url);

// ============================================================================
// Internal helpers exposed for testing. Not part of the public API.
// ============================================================================
namespace internal {

// Parsed URL components for WinHTTP.
struct UrlComponents {
  std::wstring host;
  std::wstring path;
  uint16_t port = 0;
  bool is_https = false;
};

struct ContentRange {
  uint64_t start = 0;
  uint64_t end = 0;
  uint64_t total = 0;
};

// Strictly parses one complete byte Content-Range value and requires its start
// to equal |expected_start|. Wildcard totals, extra whitespace/ranges,
// overflow, contradictions, and totals above |max_size| are rejected.
bool ParseContentRange(std::string_view value,
                       uint64_t expected_start,
                       uint64_t max_size,
                       ContentRange* output);

// Normalizes a URL origin as lower-case scheme/host plus effective port.
// Paths, queries, fragments, and credentials are never returned.
bool NormalizeUrlOrigin(const std::string& url, std::string* origin);

// Returns the deterministic configured/final-origin-bound partial path used by
// resumable archive downloads. Empty indicates invalid input.
#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
base::FilePath GetDownloadPartialPathForTesting(
    const base::FilePath& destination,
    const std::string& configured_url,
    const std::string& final_url);

// Prunes against an injected wall-clock value so archive TTL boundaries are
// deterministic and do not depend on test execution timing.
void PruneCacheDirectoryAtTimeForTesting(const base::FilePath& cache_dir,
                                         base::Time now);
#endif

// Parse a URL into components for WinHTTP.
bool ParseUrl(const std::string& url, UrlComponents* out);

// Format a base::Time as an HTTP-date string (RFC 7231) for If-Modified-Since.
// Example: "Sat, 08 Mar 2026 12:00:00 GMT"
std::wstring FormatHttpDate(const base::Time& time);

// Result of a HEAD request for CDN cache validation.
enum class HeadResult {
  kNotModified,   // 304: cache is current
  kModified,      // 200: CDN has newer content
  kNetworkError,  // Could not reach CDN
};

// Perform a HEAD request with If-Modified-Since to validate cache freshness.
// When options.local_download_path is set, skips the network request and
// returns kModified so the caller always fetches fresh content from the local
// directory.
HeadResult HeadRequest(const std::string& url,
                       const base::Time& cache_time,
                       const DownloadOptions& options);

}  // namespace internal

}  // namespace cef_installer

#endif  // CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_DOWNLOAD_H_
