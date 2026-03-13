// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_LOGGER_H_
#define CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_LOGGER_H_

#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "cef/libcef_dll/bootstrap/installer/installer_constants.h"
#include "cef/libcef_dll/bootstrap/installer/installer_download.h"

namespace cef_installer {

// Log severity levels.
enum class LogLevel {
  kInfo,
  kWarning,
  kError,
};

// Logger configuration.
struct LoggerConfig {
  // Enable file-based logging for detailed operation logs.
  bool enable_file_log = true;

  // Directory for the log file. Required if enable_file_log is true.
  // Log file will be created at: <log_directory>/cef_installer.log
  base::FilePath log_directory;

  // Maximum log file size before rotation.
  size_t max_file_size = kMaxLogFileSize;

  // Number of rotated log files to keep.
  int max_rotated_files = kMaxRotatedLogFiles;

  // Minimum log level. Messages below this level are discarded.
  LogLevel min_log_level = LogLevel::kWarning;
};

// Unified logging interface for the CEF installer.
//
// This class provides file-based logging for detailed debugging and operation
// tracking.
//
// Usage:
//   Logger::GetInstance().Initialize(config);
//   Logger::GetInstance().Info("Starting installation");
//   Logger::GetInstance().LogInstallationCompleted("137.1.0", path);
//
// File log format:
//   [2026-02-27 10:30:45.123] [INFO] Message text here
//   [2026-02-27 10:30:46.456] [WARNING] Warning message
//
// Thread-safe: All methods can be called from any thread.
class Logger {
 public:
  // Get the singleton instance.
  static Logger& GetInstance();

  // Initialize the logger with configuration.
  // Must be called before any logging methods.
  // Can be called multiple times to reconfigure.
  // Thread-safe.
  void Initialize(const LoggerConfig& config);

  // Shutdown the logger, flushing any pending writes.
  void Shutdown();

  // Check if the logger has been initialized.
  bool IsInitialized() const;

  // Get the current configuration.
  LoggerConfig GetConfig() const;

  // Get the log file path (empty if file logging is disabled).
  base::FilePath GetLogFilePath() const;

  // ============================================================================
  // Basic Logging Methods
  // ============================================================================

  // Log an informational message.
  void Info(const std::string& message);

  // Log a warning message.
  void Warning(const std::string& message);

  // Log an error message.
  void Error(const std::string& message);

  // Log a message at the specified level.
  void Log(LogLevel level, const std::string& message);

  // ============================================================================
  // Structured Operation Logging (File Log Only)
  // ============================================================================

  // Log the start of an installer operation.
  void LogOperationStart(Command command, const std::string& app_uuid);

  // Log the end of an installer operation.
  void LogOperationEnd(Command command,
                       bool success,
                       const std::string& error_message = "");

  // Log a download attempt.
  void LogDownloadAttempt(const std::string& url,
                          int attempt,
                          int max_attempts);

  // Log version resolution result.
  void LogVersionResolution(const std::string& app_uuid,
                            const std::string& resolved_version);

  // Log archive extraction progress.
  void LogExtractionProgress(const std::string& archive_name,
                             int files_extracted,
                             int total_files);

  // ============================================================================
  // Security Event Logging
  // ============================================================================

  // Log a successful installation.
  void LogInstallationCompleted(const std::string& version,
                                const base::FilePath& path);

  // Log a successful uninstallation.
  void LogUninstallationCompleted(const std::string& app_uuid);

  // Log a successful update.
  void LogUpdateCompleted(const std::string& old_version,
                          const std::string& new_version);

  // Log a download failure.
  void LogDownloadFailure(const std::string& url, DownloadError error);

  // Log a signature verification failure.
  void LogSignatureFailure(const base::FilePath& file,
                           const std::string& expected_thumbprint,
                           const std::string& actual_thumbprint);

  // Log a certificate pin mismatch.
  void LogCertificatePinMismatch(const base::FilePath& file,
                                 const std::string& expected_thumbprint,
                                 const std::string& actual_thumbprint);

  // Log catalog verification failure.
  void LogCatalogVerificationFailure(const base::FilePath& file,
                                     const base::FilePath& catalog_path,
                                     const std::string& error_description);

  // Log database tampering detection.
  void LogDatabaseTampering(const base::FilePath& database_path,
                            const std::string& details);

  // Log a revoked version being blocked.
  void LogRevokedVersionBlocked(const std::string& version,
                                const std::string& reason);

  // Log a failed directory resolution.
  // Called when ResolveInstallDirectories finds no writable directory,
  // meaning no install directory is available.
  void LogDirectoryResolutionFailure(Command command,
                                     const std::string& error_message);

  // ============================================================================
  // Security-Relevant File Operations
  // ============================================================================

  // Log reparse point (symlink/junction) rejection.
  void LogReparsePointRejected(const base::FilePath& path);

  // ============================================================================
  // Log File Management
  // ============================================================================

  // Rotate the log file if it exceeds the size limit.
  // Called automatically, but can be invoked manually.
  void RotateIfNeeded();

  // Force rotate the log file regardless of size.
  void ForceRotate();

  // Flush any buffered log data to disk.
  void Flush();

  // ============================================================================
  // Testing Support
  // ============================================================================

  // Reset the logger for testing.
  // WARNING: Only use in tests!
  void ResetForTesting();

 private:
  template <typename T>
  friend class base::NoDestructor;

  Logger();
  ~Logger();

  // Non-copyable.
  Logger(const Logger&) = delete;
  Logger& operator=(const Logger&) = delete;

  // Write to the file log (internal, must hold lock_).
  void WriteToFile(LogLevel level, const std::string& message);

  // Format a timestamp in ISO 8601 format.
  static std::string FormatTimestamp();

  // Rotate log files (internal, must hold lock_).
  void RotateLogFiles();

  // Get the current log file size.
  int64_t GetLogFileSize() const;

  mutable base::Lock lock_;
  LoggerConfig config_;
  base::FilePath log_file_path_;
  bool initialized_ = false;
};

// ============================================================================
// Utility Functions
// ============================================================================

// Convert Command enum to string for logging.
const char* CommandToString(Command command);

// Convert LogLevel to string.
const char* LogLevelToString(LogLevel level);

// Parse LogLevel from string (case-insensitive).
// Accepts: "info", "warning", "error"
// Returns nullopt if string doesn't match any valid level.
std::optional<LogLevel> LogLevelFromString(const std::string& str);

}  // namespace cef_installer

#endif  // CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_LOGGER_H_
