// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef_dll/bootstrap/installer/installer_logger.h"

#include <optional>
#include <sstream>

#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "cef/libcef_dll/bootstrap/installer/installer_constants.h"

namespace cef_installer {

namespace {

// Sanitize a string for safe logging by escaping control characters.
// Prevents log injection attacks where malicious input could forge log entries.
std::string SanitizeForLog(const std::string& input) {
  std::string result;
  result.reserve(input.size());
  for (char c : input) {
    if (c == '\n') {
      result += "\\n";
    } else if (c == '\r') {
      result += "\\r";
    } else if (c < 0x20 && c != '\t') {
      // Other control characters -> hex escape
      result += base::StringPrintf("\\x%02X", static_cast<unsigned char>(c));
    } else {
      result += c;
    }
  }
  return result;
}

}  // namespace

// static
Logger& Logger::GetInstance() {
  static base::NoDestructor<Logger> instance;
  return *instance;
}

Logger::Logger() = default;
Logger::~Logger() = default;

void Logger::Initialize(const LoggerConfig& config) {
  base::AutoLock lock(lock_);
  config_ = config;

  if (config_.enable_file_log && !config_.log_directory.empty()) {
    log_file_path_ = config_.log_directory.Append(kLogFilename);
    // Ensure the directory exists.
    base::CreateDirectory(config_.log_directory);
  } else {
    log_file_path_.clear();
  }

  initialized_ = true;
}

void Logger::Shutdown() {
  base::AutoLock lock(lock_);
  Flush();
  initialized_ = false;
}

bool Logger::IsInitialized() const {
  base::AutoLock lock(lock_);
  return initialized_;
}

LoggerConfig Logger::GetConfig() const {
  base::AutoLock lock(lock_);
  return config_;
}

base::FilePath Logger::GetLogFilePath() const {
  base::AutoLock lock(lock_);
  return log_file_path_;
}

// ============================================================================
// Basic Logging Methods
// ============================================================================

void Logger::Info(const std::string& message) {
  Log(LogLevel::kInfo, message);
}

void Logger::Warning(const std::string& message) {
  Log(LogLevel::kWarning, message);
}

void Logger::Error(const std::string& message) {
  Log(LogLevel::kError, message);
}

void Logger::Log(LogLevel level, const std::string& message) {
  base::AutoLock lock(lock_);
  if (!initialized_) {
    return;
  }

  if (level < config_.min_log_level) {
    return;
  }

  if (config_.enable_file_log && !log_file_path_.empty()) {
    WriteToFile(level, message);
  }
}

// ============================================================================
// Structured Operation Logging
// ============================================================================

void Logger::LogOperationStart(Command command, const std::string& app_uuid) {
  std::string msg = base::StringPrintf(
      "Operation started: %s, pid=%lu, app=%s", CommandToString(command),
      static_cast<unsigned long>(base::GetCurrentProcId()), app_uuid.c_str());
  Info(msg);
}

void Logger::LogOperationEnd(Command command,
                             bool success,
                             const std::string& error_message) {
  std::string msg;
  if (success) {
    msg = base::StringPrintf(
        "Operation completed: %s, pid=%lu, success=true",
        CommandToString(command),
        static_cast<unsigned long>(base::GetCurrentProcId()));
  } else {
    msg = base::StringPrintf(
        "Operation completed: %s, pid=%lu, success=false, error=%s",
        CommandToString(command),
        static_cast<unsigned long>(base::GetCurrentProcId()),
        error_message.c_str());
  }
  if (success) {
    Info(msg);
  } else {
    Error(msg);
  }
}

void Logger::LogDownloadAttempt(const std::string& url,
                                int attempt,
                                int max_attempts) {
  std::string msg = base::StringPrintf("Downloading %s (attempt %d/%d)",
                                       url.c_str(), attempt, max_attempts);
  Info(msg);
}

void Logger::LogVersionResolution(const std::string& app_uuid,
                                  const std::string& resolved_version) {
  std::string msg =
      base::StringPrintf("Resolved version %s for app %s",
                         resolved_version.c_str(), app_uuid.c_str());
  Info(msg);
}

void Logger::LogExtractionProgress(const std::string& archive_name,
                                   int files_extracted,
                                   int total_files) {
  std::string msg =
      base::StringPrintf("Extracting %s: %d/%d files", archive_name.c_str(),
                         files_extracted, total_files);
  Info(msg);
}

// ============================================================================
// Security Event Logging
// ============================================================================

void Logger::LogInstallationCompleted(const std::string& version,
                                      const base::FilePath& path) {
  std::string msg =
      base::StringPrintf("Installation completed: version=%s, path=%s",
                         version.c_str(), path.AsUTF8Unsafe().c_str());
  Info(msg);
}

void Logger::LogUninstallationCompleted(const std::string& app_uuid) {
  std::string msg =
      base::StringPrintf("Uninstallation completed: app=%s", app_uuid.c_str());
  Info(msg);
}

void Logger::LogUpdateCompleted(const std::string& old_version,
                                const std::string& new_version) {
  std::string msg = base::StringPrintf(
      "Update completed: %s -> %s", old_version.c_str(), new_version.c_str());
  Info(msg);
}

void Logger::LogDownloadFailure(const std::string& url, DownloadError error) {
  std::string msg =
      base::StringPrintf("Download failed: url=%s, error=%s", url.c_str(),
                         DownloadErrorToString(error));
  Warning(msg);
}

void Logger::LogSignatureFailure(const base::FilePath& file,
                                 const std::string& expected_thumbprint,
                                 const std::string& actual_thumbprint) {
  std::string msg = base::StringPrintf(
      "Signature verification failed: file=%s, expected=%s, actual=%s",
      file.AsUTF8Unsafe().c_str(),
      expected_thumbprint.empty() ? "(any)" : expected_thumbprint.c_str(),
      actual_thumbprint.empty() ? "(none)" : actual_thumbprint.c_str());
  Error(msg);
}

void Logger::LogCertificatePinMismatch(const base::FilePath& file,
                                       const std::string& expected_thumbprint,
                                       const std::string& actual_thumbprint) {
  std::string msg = base::StringPrintf(
      "Certificate pin mismatch: file=%s, expected=%s, actual=%s",
      file.AsUTF8Unsafe().c_str(), expected_thumbprint.c_str(),
      actual_thumbprint.c_str());
  Error(msg);
}

void Logger::LogCatalogVerificationFailure(
    const base::FilePath& file,
    const base::FilePath& catalog_path,
    const std::string& error_description) {
  std::string msg = base::StringPrintf(
      "Catalog verification failed: file=%s, catalog=%s, error=%s",
      file.AsUTF8Unsafe().c_str(), catalog_path.AsUTF8Unsafe().c_str(),
      error_description.c_str());
  Error(msg);
}

void Logger::LogDatabaseTampering(const base::FilePath& database_path,
                                  const std::string& details) {
  std::string msg =
      base::StringPrintf("Database tampering detected: path=%s, details=%s",
                         database_path.AsUTF8Unsafe().c_str(), details.c_str());
  Error(msg);
}

void Logger::LogDirectoryResolutionFailure(Command command,
                                           const std::string& error_message) {
  std::string msg = base::StringPrintf(
      "Install directory resolution failed: command=%s, error=%s",
      CommandToString(command), error_message.c_str());
  Error(msg);
}

void Logger::LogRevokedVersionBlocked(const std::string& version,
                                      const std::string& reason) {
  std::string msg =
      base::StringPrintf("Revoked version blocked: version=%s, reason=%s",
                         version.c_str(), reason.c_str());
  Warning(msg);
}

// ============================================================================
// Security-Relevant File Operations (File Log Only)
// ============================================================================

void Logger::LogReparsePointRejected(const base::FilePath& path) {
  std::string msg =
      base::StringPrintf("Reparse point (symlink/junction) rejected: path=%s",
                         path.AsUTF8Unsafe().c_str());
  Warning(msg);
}

// ============================================================================
// Log File Management
// ============================================================================

void Logger::RotateIfNeeded() {
  base::AutoLock lock(lock_);
  if (!config_.enable_file_log || log_file_path_.empty()) {
    return;
  }

  int64_t size = GetLogFileSize();
  if (size >= 0 && size >= static_cast<int64_t>(config_.max_file_size)) {
    RotateLogFiles();
  }
}

void Logger::ForceRotate() {
  base::AutoLock lock(lock_);
  if (!config_.enable_file_log || log_file_path_.empty()) {
    return;
  }
  RotateLogFiles();
}

void Logger::Flush() {
  // File writes are unbuffered (append mode), so nothing to flush.
}

void Logger::ResetForTesting() {
  base::AutoLock lock(lock_);
  initialized_ = false;
  config_ = LoggerConfig();
  log_file_path_.clear();
}

// ============================================================================
// Private Methods
// ============================================================================

void Logger::WriteToFile(LogLevel level, const std::string& message) {
  lock_.AssertAcquired();

  // Check for rotation before writing.
  int64_t size = GetLogFileSize();
  if (size >= 0 && size >= static_cast<int64_t>(config_.max_file_size)) {
    RotateLogFiles();
  }

  std::string timestamp = FormatTimestamp();
  const char* level_str = LogLevelToString(level);
  std::string safe_message = SanitizeForLog(message);

  std::string line = base::StringPrintf("[%s] [%s] %s\n", timestamp.c_str(),
                                        level_str, safe_message.c_str());

  // Append to the log file.
  base::File file(log_file_path_,
                  base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_APPEND);
  if (file.IsValid()) {
    file.WriteAtCurrentPosAndCheck(base::as_byte_span(line));
  } else {
    LOG(WARNING) << "CEF installer: failed to write to log file: "
                 << log_file_path_.value();
  }
}

// static
std::string Logger::FormatTimestamp() {
  base::Time now = base::Time::Now();
  base::Time::Exploded exploded;
  now.LocalExplode(&exploded);

  return base::StringPrintf("%04d-%02d-%02d %02d:%02d:%02d.%03d", exploded.year,
                            exploded.month, exploded.day_of_month,
                            exploded.hour, exploded.minute, exploded.second,
                            exploded.millisecond);
}

void Logger::RotateLogFiles() {
  // Delete the oldest rotated file if it exists.
  base::FilePath oldest = log_file_path_.InsertBeforeExtensionASCII(
      base::StringPrintf(".%d", config_.max_rotated_files));
  base::DeleteFile(oldest);

  // Rotate existing files: .2 -> .3, .1 -> .2, etc.
  for (int i = config_.max_rotated_files - 1; i >= 1; --i) {
    base::FilePath from =
        log_file_path_.InsertBeforeExtensionASCII(base::StringPrintf(".%d", i));
    base::FilePath to = log_file_path_.InsertBeforeExtensionASCII(
        base::StringPrintf(".%d", i + 1));
    base::Move(from, to);
  }

  // Rotate current log file to .1.
  base::FilePath rotated = log_file_path_.InsertBeforeExtensionASCII(".1");
  base::Move(log_file_path_, rotated);
}

int64_t Logger::GetLogFileSize() const {
  std::optional<int64_t> size = base::GetFileSize(log_file_path_);
  return size.value_or(-1);
}

// ============================================================================
// Utility Functions
// ============================================================================

const char* CommandToString(Command command) {
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

const char* LogLevelToString(LogLevel level) {
  switch (level) {
    case LogLevel::kInfo:
      return "INFO";
    case LogLevel::kWarning:
      return "WARNING";
    case LogLevel::kError:
      return "ERROR";
  }
  return "UNKNOWN";
}

std::optional<LogLevel> LogLevelFromString(const std::string& str) {
  std::string lower = base::ToLowerASCII(str);
  if (lower == "info") {
    return LogLevel::kInfo;
  }
  if (lower == "warning") {
    return LogLevel::kWarning;
  }
  if (lower == "error") {
    return LogLevel::kError;
  }
  return std::nullopt;
}

}  // namespace cef_installer
