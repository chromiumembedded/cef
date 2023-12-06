// Copyright 2016 The Chromium Embedded Framework Authors. Portions copyright
// 2016 The Chromium Authors. All rights reserved. Use of this source code is
// governed by a BSD-style license that can be found in the LICENSE file.

#ifndef CEF_LIBCEF_COMMON_CRASH_REPORTER_CLIENT_H_
#define CEF_LIBCEF_COMMON_CRASH_REPORTER_CLIENT_H_

#include <string>
#include <vector>

#include "include/cef_version.h"

#include "base/strings/string_piece.h"
#include "base/synchronization/lock.h"
#include "build/build_config.h"
#include "components/crash/core/app/crash_reporter_client.h"

// Global object that is instantiated in each process and configures crash
// reporting. On Windows this is created by the
// InitializeCrashReportingForProcess() method called from chrome_elf. On
// Linux and macOS this is created by crash_reporting::BasicStartupComplete().
class CefCrashReporterClient : public crash_reporter::CrashReporterClient {
 public:
  CefCrashReporterClient();

  CefCrashReporterClient(const CefCrashReporterClient&) = delete;
  CefCrashReporterClient& operator=(const CefCrashReporterClient&) = delete;

  ~CefCrashReporterClient() override;

  // Reads the crash config file and returns true on success. Failure to read
  // the crash config file will disable crash reporting. This method should be
  // called immediately after the CefCrashReporterClient instance is created.
  bool ReadCrashConfigFile();
  bool HasCrashConfigFile() const;

#if BUILDFLAG(IS_WIN)
  // Called from chrome_elf (chrome_elf/crash/crash_helper.cc) to instantiate
  // a process wide instance of CefCrashReporterClient and initialize crash
  // reporting for the process. The instance is leaked.
  // crash_reporting_win::InitializeCrashReportingForModule() will be called
  // later from crash_reporting::PreSandboxStartup() to read global state into
  // the module address space.
  static void InitializeCrashReportingForProcess();

  bool GetAlternativeCrashDumpLocation(std::wstring* crash_dir) override;
  void GetProductNameAndVersion(const std::wstring& exe_path,
                                std::wstring* product_name,
                                std::wstring* version,
                                std::wstring* special_build,
                                std::wstring* channel_name) override;
  bool GetCrashDumpLocation(std::wstring* crash_dir) override;
  bool GetCrashMetricsLocation(std::wstring* metrics_dir) override;
#elif BUILDFLAG(IS_POSIX)
  void GetProductNameAndVersion(const char** product_name,
                                const char** version) override;
  void GetProductNameAndVersion(std::string* product_name,
                                std::string* version,
                                std::string* channel) override;
  bool GetCrashDumpLocation(base::FilePath* crash_dir) override;
#endif  // BUILDFLAG(IS_POSIX)

  // All of these methods must return true to enable crash report upload.
  bool GetCollectStatsConsent() override;
  bool GetCollectStatsInSample() override;
  bool ReportingIsEnforcedByPolicy(bool* crashpad_enabled) override;

  std::string GetUploadUrl() override;
  void GetCrashOptionalArguments(std::vector<std::string>* arguments) override;

#if BUILDFLAG(IS_WIN)
  std::wstring GetCrashExternalHandler(const std::wstring& exe_dir) override;
  bool HasCrashExternalHandler() const;
#endif

#if BUILDFLAG(IS_MAC)
  bool EnableBrowserCrashForwarding() override;
#endif

  // Set or clear a crash key value.
  bool SetCrashKeyValue(const base::StringPiece& key,
                        const base::StringPiece& value);

 private:
  bool has_crash_config_file_ = false;

  enum KeySize { SMALL_SIZE, MEDIUM_SIZE, LARGE_SIZE };

  // Map of crash key name to (KeySize, index).
  // Const access to |crash_keys_| is thread-safe after initialization.
  using KeyMap = std::map<std::string, std::pair<KeySize, size_t>>;
  KeyMap crash_keys_;

  // Modification of CrashKeyString values must be synchronized.
  base::Lock crash_key_lock_;

  std::string server_url_;
  bool rate_limit_ = true;
  int max_uploads_ = 5;
  int max_db_size_ = 20;
  int max_db_age_ = 5;

  std::string product_name_ = "cef";
  std::string product_version_ = CEF_VERSION;

#if BUILDFLAG(IS_WIN)
  std::string app_name_ = "CEF";
  std::string external_handler_;
#endif

#if BUILDFLAG(IS_MAC)
  bool enable_browser_crash_forwarding_ = false;
#endif
};

#endif  // CEF_LIBCEF_COMMON_CRASH_REPORTER_CLIENT_H_
