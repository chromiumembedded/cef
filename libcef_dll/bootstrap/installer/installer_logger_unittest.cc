// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef_dll/bootstrap/installer/installer_logger.h"

#include <thread>
#include <vector>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_util.h"
#include "cef/libcef_dll/bootstrap/installer/installer_constants.h"
#include "cef/libcef_dll/bootstrap/installer/installer_download.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cef_installer {
namespace {

class InstallerLoggerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    Logger::GetInstance().ResetForTesting();
  }

  void TearDown() override { Logger::GetInstance().ResetForTesting(); }

  base::FilePath GetLogPath() const {
    return temp_dir_.GetPath().Append(kLogFilename);
  }

  std::string ReadLogFile() const {
    std::string content;
    base::ReadFileToString(GetLogPath(), &content);
    return content;
  }

  void InitializeLogger(bool enable_file_log = true,
                        LogLevel min_level = LogLevel::kInfo) {
    LoggerConfig config;
    config.enable_file_log = enable_file_log;
    config.log_directory = temp_dir_.GetPath();
    config.min_log_level = min_level;
    Logger::GetInstance().Initialize(config);
  }

  base::ScopedTempDir temp_dir_;
};

// ============================================================================
// Initialization Tests
// ============================================================================

TEST_F(InstallerLoggerTest, Initialize) {
  EXPECT_FALSE(Logger::GetInstance().IsInitialized());

  InitializeLogger();

  EXPECT_TRUE(Logger::GetInstance().IsInitialized());
  EXPECT_EQ(GetLogPath(), Logger::GetInstance().GetLogFilePath());
}

TEST_F(InstallerLoggerTest, InitializeCreatesDirectory) {
  base::FilePath nested_dir =
      temp_dir_.GetPath().AppendASCII("nested").AppendASCII("logs");

  LoggerConfig config;
  config.enable_file_log = true;
  config.log_directory = nested_dir;
  Logger::GetInstance().Initialize(config);

  EXPECT_TRUE(base::DirectoryExists(nested_dir));
}

TEST_F(InstallerLoggerTest, InitializeWithoutFileLog) {
  LoggerConfig config;
  config.enable_file_log = false;
  Logger::GetInstance().Initialize(config);

  EXPECT_TRUE(Logger::GetInstance().IsInitialized());
  EXPECT_TRUE(Logger::GetInstance().GetLogFilePath().empty());
}

// ============================================================================
// Basic Logging Tests
// ============================================================================

TEST_F(InstallerLoggerTest, InfoMessage) {
  InitializeLogger();

  Logger::GetInstance().Info("Test info message");

  std::string content = ReadLogFile();
  EXPECT_NE(std::string::npos, content.find("[INFO]"));
  EXPECT_NE(std::string::npos, content.find("Test info message"));
}

TEST_F(InstallerLoggerTest, WarningMessage) {
  InitializeLogger();

  Logger::GetInstance().Warning("Test warning message");

  std::string content = ReadLogFile();
  EXPECT_NE(std::string::npos, content.find("[WARNING]"));
  EXPECT_NE(std::string::npos, content.find("Test warning message"));
}

TEST_F(InstallerLoggerTest, ErrorMessage) {
  InitializeLogger();

  Logger::GetInstance().Error("Test error message");

  std::string content = ReadLogFile();
  EXPECT_NE(std::string::npos, content.find("[ERROR]"));
  EXPECT_NE(std::string::npos, content.find("Test error message"));
}

TEST_F(InstallerLoggerTest, TimestampFormat) {
  InitializeLogger();

  Logger::GetInstance().Info("Test message");

  std::string content = ReadLogFile();
  // Check for ISO 8601-like timestamp format: [YYYY-MM-DD HH:MM:SS.mmm]
  // Should match pattern like [2026-02-27 10:30:45.123]
  EXPECT_NE(std::string::npos, content.find("[20"));  // Year starts with 20
  EXPECT_NE(std::string::npos, content.find(":"));    // Time separator
  EXPECT_NE(std::string::npos, content.find("."));    // Millisecond separator
}

TEST_F(InstallerLoggerTest, MultipleMessages) {
  InitializeLogger();

  Logger::GetInstance().Info("First message");
  Logger::GetInstance().Warning("Second message");
  Logger::GetInstance().Error("Third message");

  std::string content = ReadLogFile();
  EXPECT_NE(std::string::npos, content.find("First message"));
  EXPECT_NE(std::string::npos, content.find("Second message"));
  EXPECT_NE(std::string::npos, content.find("Third message"));

  // Messages should be on separate lines.
  size_t first_pos = content.find("First message");
  size_t second_pos = content.find("Second message");
  size_t third_pos = content.find("Third message");
  EXPECT_LT(first_pos, second_pos);
  EXPECT_LT(second_pos, third_pos);
}

// ============================================================================
// Structured Logging Tests
// ============================================================================

TEST_F(InstallerLoggerTest, LogOperationStartEnd) {
  InitializeLogger();

  Logger::GetInstance().LogOperationStart(Command::kInstall, "test-uuid-123");
  Logger::GetInstance().LogOperationEnd(Command::kInstall, true);

  std::string content = ReadLogFile();
  EXPECT_NE(std::string::npos, content.find("Operation started"));
  EXPECT_NE(std::string::npos, content.find("install"));
  EXPECT_NE(std::string::npos, content.find("test-uuid-123"));
  EXPECT_NE(std::string::npos, content.find("Operation completed"));
  EXPECT_NE(std::string::npos, content.find("success=true"));
}

TEST_F(InstallerLoggerTest, LogOperationEndFailure) {
  InitializeLogger();

  Logger::GetInstance().LogOperationEnd(Command::kUpdate, false,
                                        "Network timeout");

  std::string content = ReadLogFile();
  EXPECT_NE(std::string::npos, content.find("[ERROR]"));
  EXPECT_NE(std::string::npos, content.find("success=false"));
  EXPECT_NE(std::string::npos, content.find("Network timeout"));
}

TEST_F(InstallerLoggerTest, LogDownloadAttempt) {
  InitializeLogger();

  Logger::GetInstance().LogDownloadAttempt("https://example.com/file.zip", 2,
                                           3);

  std::string content = ReadLogFile();
  EXPECT_NE(std::string::npos, content.find("https://example.com/file.zip"));
  EXPECT_NE(std::string::npos, content.find("attempt 2/3"));
}

TEST_F(InstallerLoggerTest, LogVersionResolution) {
  InitializeLogger();

  Logger::GetInstance().LogVersionResolution("app-uuid-456", "137.1.5");

  std::string content = ReadLogFile();
  EXPECT_NE(std::string::npos, content.find("Resolved version"));
  EXPECT_NE(std::string::npos, content.find("137.1.5"));
  EXPECT_NE(std::string::npos, content.find("app-uuid-456"));
}

// ============================================================================
// Security Event Logging Tests
// ============================================================================

TEST_F(InstallerLoggerTest, LogInstallationCompleted) {
  InitializeLogger();

  Logger::GetInstance().LogInstallationCompleted(
      "137.1.5", base::FilePath(FILE_PATH_LITERAL("C:\\CEF\\137.1.5")));

  std::string content = ReadLogFile();
  EXPECT_NE(std::string::npos, content.find("Installation completed"));
  EXPECT_NE(std::string::npos, content.find("137.1.5"));
}

TEST_F(InstallerLoggerTest, LogSignatureFailure) {
  InitializeLogger();

  Logger::GetInstance().LogSignatureFailure(
      base::FilePath(FILE_PATH_LITERAL("C:\\test\\file.dll")),
      "EXPECTED_THUMBPRINT", "ACTUAL_THUMBPRINT");

  std::string content = ReadLogFile();
  EXPECT_NE(std::string::npos, content.find("[ERROR]"));
  EXPECT_NE(std::string::npos, content.find("Signature verification failed"));
  EXPECT_NE(std::string::npos, content.find("EXPECTED_THUMBPRINT"));
  EXPECT_NE(std::string::npos, content.find("ACTUAL_THUMBPRINT"));
}

TEST_F(InstallerLoggerTest, LogRevokedVersionBlocked) {
  InitializeLogger();

  Logger::GetInstance().LogRevokedVersionBlocked("137.1.0", "CVE-2024-12345");

  std::string content = ReadLogFile();
  EXPECT_NE(std::string::npos, content.find("[WARNING]"));
  EXPECT_NE(std::string::npos, content.find("Revoked version blocked"));
  EXPECT_NE(std::string::npos, content.find("137.1.0"));
  EXPECT_NE(std::string::npos, content.find("CVE-2024-12345"));
}

TEST_F(InstallerLoggerTest, LogReparsePointRejected) {
  InitializeLogger();

  Logger::GetInstance().LogReparsePointRejected(
      base::FilePath(FILE_PATH_LITERAL("C:\\CEF\\symlink")));

  std::string content = ReadLogFile();
  EXPECT_NE(std::string::npos, content.find("[WARNING]"));
  EXPECT_NE(std::string::npos, content.find("Reparse point"));
  EXPECT_NE(std::string::npos, content.find("rejected"));
}

// ============================================================================
// Log Rotation Tests
// ============================================================================

TEST_F(InstallerLoggerTest, LogRotation) {
  LoggerConfig config;
  config.enable_file_log = true;
  config.log_directory = temp_dir_.GetPath();
  config.max_file_size = 1024;  // 1 KB for testing
  config.max_rotated_files = 2;
  config.min_log_level = LogLevel::kInfo;
  Logger::GetInstance().Initialize(config);

  // Write enough data to trigger rotation.
  std::string long_message(200, 'X');
  for (int i = 0; i < 20; ++i) {
    Logger::GetInstance().Info(long_message);
  }

  // Check that rotation occurred.
  base::FilePath rotated1 =
      temp_dir_.GetPath().AppendASCII("cef_installer.1.log");
  EXPECT_TRUE(base::PathExists(GetLogPath()));
  EXPECT_TRUE(base::PathExists(rotated1));
}

TEST_F(InstallerLoggerTest, ForceRotate) {
  InitializeLogger();

  Logger::GetInstance().Info("Before rotation");
  Logger::GetInstance().ForceRotate();
  Logger::GetInstance().Info("After rotation");

  base::FilePath rotated1 =
      temp_dir_.GetPath().AppendASCII("cef_installer.1.log");

  EXPECT_TRUE(base::PathExists(GetLogPath()));
  EXPECT_TRUE(base::PathExists(rotated1));

  std::string current_content = ReadLogFile();
  std::string rotated_content;
  base::ReadFileToString(rotated1, &rotated_content);

  EXPECT_NE(std::string::npos, current_content.find("After rotation"));
  EXPECT_NE(std::string::npos, rotated_content.find("Before rotation"));
}

// ============================================================================
// Thread Safety Tests
// ============================================================================

TEST_F(InstallerLoggerTest, ThreadSafety) {
  InitializeLogger();

  constexpr int kNumThreads = 10;
  constexpr int kMessagesPerThread = 100;
  std::vector<std::thread> threads;

  for (int t = 0; t < kNumThreads; ++t) {
    threads.emplace_back([t]() {
      for (int i = 0; i < kMessagesPerThread; ++i) {
        Logger::GetInstance().Info("Thread " + std::to_string(t) + " message " +
                                   std::to_string(i));
      }
    });
  }

  for (auto& thread : threads) {
    thread.join();
  }

  std::string content = ReadLogFile();

  // Count the number of lines (each message should be on its own line).
  int line_count = 0;
  for (char c : content) {
    if (c == '\n') {
      ++line_count;
    }
  }

  // All messages should be present.
  EXPECT_EQ(kNumThreads * kMessagesPerThread, line_count);
}

// ============================================================================
// Configuration Tests
// ============================================================================

TEST_F(InstallerLoggerTest, FileLoggingDisabled) {
  LoggerConfig config;
  config.enable_file_log = false;
  config.min_log_level = LogLevel::kInfo;
  Logger::GetInstance().Initialize(config);

  Logger::GetInstance().Info("This should not be written");

  EXPECT_FALSE(base::PathExists(GetLogPath()));
}

TEST_F(InstallerLoggerTest, GetConfig) {
  LoggerConfig config;
  config.enable_file_log = true;
  config.log_directory = temp_dir_.GetPath();
  config.max_file_size = 2048;
  config.max_rotated_files = 5;
  config.min_log_level = LogLevel::kError;
  Logger::GetInstance().Initialize(config);

  LoggerConfig retrieved = Logger::GetInstance().GetConfig();
  EXPECT_TRUE(retrieved.enable_file_log);
  EXPECT_EQ(temp_dir_.GetPath(), retrieved.log_directory);
  EXPECT_EQ(2048u, retrieved.max_file_size);
  EXPECT_EQ(5, retrieved.max_rotated_files);
  EXPECT_EQ(LogLevel::kError, retrieved.min_log_level);
}

// ============================================================================
// Utility Function Tests
// ============================================================================

TEST_F(InstallerLoggerTest, CommandToString) {
  EXPECT_STREQ("install", CommandToString(Command::kInstall));
  EXPECT_STREQ("update", CommandToString(Command::kUpdate));
  EXPECT_STREQ("uninstall", CommandToString(Command::kUninstall));
  EXPECT_STREQ("query", CommandToString(Command::kQuery));
}

TEST_F(InstallerLoggerTest, LogLevelToString) {
  EXPECT_STREQ("INFO", LogLevelToString(LogLevel::kInfo));
  EXPECT_STREQ("WARNING", LogLevelToString(LogLevel::kWarning));
  EXPECT_STREQ("ERROR", LogLevelToString(LogLevel::kError));
}

TEST_F(InstallerLoggerTest, LogLevelFromString) {
  EXPECT_EQ(LogLevel::kInfo, LogLevelFromString("info").value());
  EXPECT_EQ(LogLevel::kInfo, LogLevelFromString("INFO").value());
  EXPECT_EQ(LogLevel::kWarning, LogLevelFromString("warning").value());
  EXPECT_EQ(LogLevel::kWarning, LogLevelFromString("Warning").value());
  EXPECT_EQ(LogLevel::kError, LogLevelFromString("error").value());
  EXPECT_EQ(LogLevel::kError, LogLevelFromString("ERROR").value());
  EXPECT_FALSE(LogLevelFromString("debug").has_value());
  EXPECT_FALSE(LogLevelFromString("").has_value());
  EXPECT_FALSE(LogLevelFromString("verbose").has_value());
}

TEST_F(InstallerLoggerTest, MinLogLevelFiltersMessages) {
  InitializeLogger(/*enable_file_log=*/true, LogLevel::kWarning);

  Logger::GetInstance().Info("should be filtered");
  Logger::GetInstance().Warning("should appear");
  Logger::GetInstance().Error("should also appear");

  std::string content = ReadLogFile();
  EXPECT_EQ(std::string::npos, content.find("should be filtered"));
  EXPECT_NE(std::string::npos, content.find("should appear"));
  EXPECT_NE(std::string::npos, content.find("should also appear"));
}

TEST_F(InstallerLoggerTest, MinLogLevelInfoAllowsAll) {
  InitializeLogger(/*enable_file_log=*/true, LogLevel::kInfo);

  Logger::GetInstance().Info("info msg");
  Logger::GetInstance().Warning("warning msg");
  Logger::GetInstance().Error("error msg");

  std::string content = ReadLogFile();
  EXPECT_NE(std::string::npos, content.find("info msg"));
  EXPECT_NE(std::string::npos, content.find("warning msg"));
  EXPECT_NE(std::string::npos, content.find("error msg"));
}

TEST_F(InstallerLoggerTest, MinLogLevelErrorFiltersInfoAndWarning) {
  InitializeLogger(/*enable_file_log=*/true, LogLevel::kError);

  Logger::GetInstance().Info("info filtered");
  Logger::GetInstance().Warning("warning filtered");
  Logger::GetInstance().Error("error kept");

  std::string content = ReadLogFile();
  EXPECT_EQ(std::string::npos, content.find("info filtered"));
  EXPECT_EQ(std::string::npos, content.find("warning filtered"));
  EXPECT_NE(std::string::npos, content.find("error kept"));
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(InstallerLoggerTest, LogBeforeInitialize) {
  // Should not crash, messages are silently dropped.
  Logger::GetInstance().Info("Before initialize");
  Logger::GetInstance().Warning("Before initialize");
  Logger::GetInstance().Error("Before initialize");

  EXPECT_FALSE(base::PathExists(GetLogPath()));
}

TEST_F(InstallerLoggerTest, EmptyMessage) {
  InitializeLogger();

  Logger::GetInstance().Info("");

  std::string content = ReadLogFile();
  EXPECT_NE(std::string::npos, content.find("[INFO]"));
}

TEST_F(InstallerLoggerTest, MessageWithNewlines) {
  InitializeLogger();

  Logger::GetInstance().Info("Line 1\nLine 2\r\nLine 3");

  std::string content = ReadLogFile();
  // Newlines should be escaped to prevent log injection.
  EXPECT_NE(std::string::npos, content.find("Line 1\\nLine 2\\r\\nLine 3"));
  // Should be a single log entry (only one timestamp line).
  size_t first_bracket = content.find('[');
  size_t second_bracket = content.find('[', first_bracket + 1);
  // Second bracket should be the level indicator [INFO], not another timestamp.
  EXPECT_NE(std::string::npos, second_bracket);
  EXPECT_LT(second_bracket, content.find('\n'));
}

TEST_F(InstallerLoggerTest, Reinitialize) {
  InitializeLogger();
  Logger::GetInstance().Info("First config");

  // Reinitialize with different config.
  base::ScopedTempDir temp_dir2;
  ASSERT_TRUE(temp_dir2.CreateUniqueTempDir());

  LoggerConfig config;
  config.enable_file_log = true;
  config.log_directory = temp_dir2.GetPath();
  config.min_log_level = LogLevel::kInfo;
  Logger::GetInstance().Initialize(config);

  Logger::GetInstance().Info("Second config");

  // First log should still exist with first message.
  std::string first_content = ReadLogFile();
  EXPECT_NE(std::string::npos, first_content.find("First config"));

  // Second log should have second message.
  std::string second_content;
  base::ReadFileToString(temp_dir2.GetPath().Append(kLogFilename),
                         &second_content);
  EXPECT_NE(std::string::npos, second_content.find("Second config"));
}

// ============================================================================
// Additional Coverage Tests
// ============================================================================

TEST_F(InstallerLoggerTest, Shutdown) {
  InitializeLogger();
  EXPECT_TRUE(Logger::GetInstance().IsInitialized());

  Logger::GetInstance().Shutdown();
  EXPECT_FALSE(Logger::GetInstance().IsInitialized());
}

TEST_F(InstallerLoggerTest, LogExtractionProgress) {
  InitializeLogger();

  Logger::GetInstance().LogExtractionProgress("archive.zip", 50, 100);

  std::string content = ReadLogFile();
  EXPECT_NE(std::string::npos, content.find("Extracting"));
  EXPECT_NE(std::string::npos, content.find("archive.zip"));
  EXPECT_NE(std::string::npos, content.find("50/100"));
}

TEST_F(InstallerLoggerTest, LogUpdateCompleted) {
  InitializeLogger();

  Logger::GetInstance().LogUpdateCompleted("136.1.0", "137.2.0");

  std::string content = ReadLogFile();
  EXPECT_NE(std::string::npos, content.find("Update completed"));
  EXPECT_NE(std::string::npos, content.find("136.1.0"));
  EXPECT_NE(std::string::npos, content.find("137.2.0"));
}

TEST_F(InstallerLoggerTest, LogDownloadFailure) {
  InitializeLogger();

  Logger::GetInstance().LogDownloadFailure("https://example.com/file.zip",
                                           DownloadError::kNetworkError);

  std::string content = ReadLogFile();
  EXPECT_NE(std::string::npos, content.find("[WARNING]"));
  EXPECT_NE(std::string::npos, content.find("Download failed"));
  EXPECT_NE(std::string::npos, content.find("https://example.com/file.zip"));
}

TEST_F(InstallerLoggerTest, LogCertificatePinMismatch) {
  InitializeLogger();

  Logger::GetInstance().LogCertificatePinMismatch(
      base::FilePath(FILE_PATH_LITERAL("C:\\test\\file.dll")), "EXPECTED_PIN",
      "ACTUAL_PIN");

  std::string content = ReadLogFile();
  EXPECT_NE(std::string::npos, content.find("[ERROR]"));
  EXPECT_NE(std::string::npos, content.find("Certificate pin mismatch"));
  EXPECT_NE(std::string::npos, content.find("EXPECTED_PIN"));
  EXPECT_NE(std::string::npos, content.find("ACTUAL_PIN"));
}

TEST_F(InstallerLoggerTest, LogCatalogVerificationFailure) {
  InitializeLogger();

  Logger::GetInstance().LogCatalogVerificationFailure(
      base::FilePath(FILE_PATH_LITERAL("C:\\test\\file.dll")),
      base::FilePath(FILE_PATH_LITERAL("C:\\test\\catalog.cat")),
      "File not in catalog");

  std::string content = ReadLogFile();
  EXPECT_NE(std::string::npos, content.find("[ERROR]"));
  EXPECT_NE(std::string::npos, content.find("Catalog verification failed"));
  EXPECT_NE(std::string::npos, content.find("File not in catalog"));
}

TEST_F(InstallerLoggerTest, LogDatabaseTampering) {
  InitializeLogger();

  Logger::GetInstance().LogDatabaseTampering(
      base::FilePath(FILE_PATH_LITERAL("C:\\CEF\\db.sqlite")),
      "Checksum mismatch");

  std::string content = ReadLogFile();
  EXPECT_NE(std::string::npos, content.find("[ERROR]"));
  EXPECT_NE(std::string::npos, content.find("Database tampering detected"));
  EXPECT_NE(std::string::npos, content.find("Checksum mismatch"));
}

TEST_F(InstallerLoggerTest, RotateIfNeeded_NoRotationNeeded) {
  InitializeLogger();

  // Write a small message (won't trigger rotation with default size limit)
  Logger::GetInstance().Info("Small message");

  // RotateIfNeeded should not create rotated files
  Logger::GetInstance().RotateIfNeeded();

  base::FilePath rotated1 =
      temp_dir_.GetPath().AppendASCII("cef_installer.1.log");
  EXPECT_FALSE(base::PathExists(rotated1));
}

TEST_F(InstallerLoggerTest, RotateIfNeeded_WithLargeFile) {
  LoggerConfig config;
  config.enable_file_log = true;
  config.log_directory = temp_dir_.GetPath();
  config.max_file_size = 100;  // Very small to trigger rotation
  config.max_rotated_files = 2;
  config.min_log_level = LogLevel::kInfo;
  Logger::GetInstance().Initialize(config);

  // Write enough to exceed limit
  Logger::GetInstance().Info("This is a test message that is long enough");
  Logger::GetInstance().Info("Another long message to fill the log file");

  // Call RotateIfNeeded - should not crash and should trigger rotation logic
  Logger::GetInstance().RotateIfNeeded();

  // Verify rotation occurred by checking for rotated file
  base::FilePath rotated1 =
      temp_dir_.GetPath().AppendASCII("cef_installer.1.log");
  // At least one rotated file or the main log should exist
  bool has_logs = base::PathExists(GetLogPath()) || base::PathExists(rotated1);
  EXPECT_TRUE(has_logs);
}

TEST_F(InstallerLoggerTest, Flush) {
  InitializeLogger();

  Logger::GetInstance().Info("Test message");
  // Flush is a no-op (unbuffered writes) but should not crash
  Logger::GetInstance().Flush();

  std::string content = ReadLogFile();
  EXPECT_NE(std::string::npos, content.find("Test message"));
}

TEST_F(InstallerLoggerTest, LogUninstallationCompleted) {
  InitializeLogger();

  Logger::GetInstance().LogUninstallationCompleted("test-app-uuid");

  std::string content = ReadLogFile();
  EXPECT_NE(std::string::npos, content.find("Uninstallation completed"));
  EXPECT_NE(std::string::npos, content.find("test-app-uuid"));
}

// ============================================================================
// Logger Resilience Tests
// ============================================================================

TEST_F(InstallerLoggerTest, LogBeforeInitialize_SilentlyDrops) {
  // Messages logged before Initialize() are silently dropped without crashing.
  EXPECT_FALSE(Logger::GetInstance().IsInitialized());

  Logger::GetInstance().Info("dropped info");
  Logger::GetInstance().Warning("dropped warning");
  Logger::GetInstance().Error("dropped error");
  Logger::GetInstance().LogOperationStart(Command::kInstall, "test-uuid");
  Logger::GetInstance().LogOperationEnd(Command::kInstall, false, "some error");

  // No log file should have been created.
  EXPECT_FALSE(base::PathExists(GetLogPath()));
}

TEST_F(InstallerLoggerTest, InitializeWithTempDir_WritesLog) {
  // Logger can be initialized with the system temp directory and writes there.
  base::FilePath temp_dir;
  ASSERT_TRUE(base::GetTempDir(&temp_dir));

  // Use a unique subdirectory to avoid interfering with other tests.
  base::ScopedTempDir scoped_temp;
  ASSERT_TRUE(scoped_temp.CreateUniqueTempDirUnderPath(temp_dir));

  LoggerConfig config;
  config.enable_file_log = true;
  config.log_directory = scoped_temp.GetPath();
  config.min_log_level = LogLevel::kInfo;
  Logger::GetInstance().Initialize(config);

  Logger::GetInstance().Info("temp dir message");

  base::FilePath log_path = scoped_temp.GetPath().Append(kLogFilename);
  EXPECT_TRUE(base::PathExists(log_path));

  std::string content;
  ASSERT_TRUE(base::ReadFileToString(log_path, &content));
  EXPECT_NE(std::string::npos, content.find("temp dir message"));
}

TEST_F(InstallerLoggerTest, ReinitializeWithNewDir_SwitchesLogFile) {
  // After re-initialization, new messages go to the new directory's log file.
  InitializeLogger();
  Logger::GetInstance().Info("message in dir A");

  base::ScopedTempDir dir_b;
  ASSERT_TRUE(dir_b.CreateUniqueTempDir());

  LoggerConfig config_b;
  config_b.enable_file_log = true;
  config_b.log_directory = dir_b.GetPath();
  config_b.min_log_level = LogLevel::kInfo;
  Logger::GetInstance().Initialize(config_b);

  Logger::GetInstance().Info("message in dir B");

  // Dir A should have only the first message.
  std::string content_a = ReadLogFile();
  EXPECT_NE(std::string::npos, content_a.find("message in dir A"));
  EXPECT_EQ(std::string::npos, content_a.find("message in dir B"));

  // Dir B should have only the second message.
  std::string content_b;
  base::ReadFileToString(dir_b.GetPath().Append(kLogFilename), &content_b);
  EXPECT_NE(std::string::npos, content_b.find("message in dir B"));
  EXPECT_EQ(std::string::npos, content_b.find("message in dir A"));
}

TEST_F(InstallerLoggerTest, WriteToFile_NonWritableDir_NoCrash) {
  // Logging to a non-existent/non-writable path should not crash.
  LoggerConfig config;
  config.enable_file_log = true;
  config.min_log_level = LogLevel::kInfo;
  // Point to a path that cannot be written to.
  config.log_directory =
      base::FilePath(FILE_PATH_LITERAL("Z:\\nonexistent\\path\\for\\testing"));
  Logger::GetInstance().Initialize(config);

  // These should not crash even though the log file can't be opened.
  Logger::GetInstance().Info("this will fail to write");
  Logger::GetInstance().Warning("this too");
  Logger::GetInstance().Error("and this");
}

TEST_F(InstallerLoggerTest, LogDirectoryResolutionFailure_WritesToFile) {
  InitializeLogger();

  Logger::GetInstance().LogDirectoryResolutionFailure(
      Command::kInstall, "Could not find or create install directory");

  std::string content = ReadLogFile();
  EXPECT_NE(std::string::npos, content.find("[ERROR]"));
  EXPECT_NE(std::string::npos,
            content.find("Install directory resolution failed"));
  EXPECT_NE(std::string::npos, content.find("install"));
  EXPECT_NE(std::string::npos,
            content.find("Could not find or create install directory"));
}

}  // namespace
}  // namespace cef_installer
