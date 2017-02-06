// Copyright 2016 The Chromium Embedded Framework Authors. Portions copyright
// 2016 The Chromium Authors. All rights reserved. Use of this source code is
// governed by a BSD-style license that can be found in the LICENSE file.

#ifndef CEF_LIBCEF_COMMON_CRASH_REPORTER_CLIENT_H_
#define CEF_LIBCEF_COMMON_CRASH_REPORTER_CLIENT_H_

#include <vector>
#include <string>

#include "include/cef_version.h"

#include "base/macros.h"
#include "build/build_config.h"
#include "components/crash/content/app/crash_reporter_client.h"

// Global object that is instantiated in each process and configures crash
// reporting. On Windows this is created by the
// InitializeCrashReportingForProcess() method called from chrome_elf. On
// Linux and macOS this is created by crash_reporting::BasicStartupComplete().
class CefCrashReporterClient : public crash_reporter::CrashReporterClient {
 public:
  CefCrashReporterClient();
  ~CefCrashReporterClient() override;

  // Reads the crash config file and returns true on success. Failure to read
  // the crash config file will disable crash reporting. This method should be
  // called immediately after the CefCrashReporterClient instance is created.
  bool ReadCrashConfigFile();
  bool HasCrashConfigFile() const;

#if defined(OS_WIN)
  // Called from chrome_elf (chrome_elf/crash/crash_helper.cc) to instantiate
  // a process wide instance of CefCrashReporterClient and initialize crash
  // reporting for the process. The instance is leaked.
  // crash_reporting_win::InitializeCrashReportingForModule() will be called
  // later from crash_reporting::PreSandboxStartup() to read global state into
  // the module address space.
  static void InitializeCrashReportingForProcess();

  bool GetAlternativeCrashDumpLocation(base::string16* crash_dir) override;
  void GetProductNameAndVersion(const base::string16& exe_path,
                                base::string16* product_name,
                                base::string16* version,
                                base::string16* special_build,
                                base::string16* channel_name) override;
  bool GetCrashDumpLocation(base::string16* crash_dir) override;
  bool GetCrashMetricsLocation(base::string16* metrics_dir) override;
#elif defined(OS_POSIX)
  void GetProductNameAndVersion(const char** product_name,
                                const char** version) override;
#if !defined(OS_MACOSX)
  base::FilePath GetReporterLogFilename() override;
  bool EnableBreakpadForProcess(const std::string& process_type) override;
#endif
  bool GetCrashDumpLocation(base::FilePath* crash_dir) override;
#endif  // defined(OS_POSIX)

  // All of these methods must return true to enable crash report upload.
  bool GetCollectStatsConsent() override;
  bool GetCollectStatsInSample() override;
#if defined(OS_WIN) || defined(OS_MACOSX)
  bool ReportingIsEnforcedByPolicy(bool* crashpad_enabled) override;
#endif

  size_t RegisterCrashKeys() override;

#if defined(OS_POSIX) && !defined(OS_MACOSX)
  bool IsRunningUnattended() override;
#endif

#if defined(OS_WIN)
  size_t GetCrashKeyCount() const;
  bool GetCrashKey(size_t index,
                   const char** key_name,
                   size_t* max_length) const;
#endif

  std::string GetCrashServerURL() override;
  void GetCrashOptionalArguments(std::vector<std::string>* arguments) override;

#if defined(OS_WIN)
  base::string16 GetCrashExternalHandler(
      const base::string16& exe_dir) override;
  bool HasCrashExternalHandler() const;
#endif

#if defined(OS_MACOSX)
  bool EnableBrowserCrashForwarding() override;
#endif

private:
  bool has_crash_config_file_ = false;

  // Values that will persist until the end of the program.
  // Matches the members of base::debug::CrashKey.
  struct StoredCrashKey {
    std::string key_name_;
    size_t max_length_;
  };
  std::vector<StoredCrashKey> crash_keys_;
  std::string server_url_;
  bool rate_limit_ = true;
  int max_uploads_ = 5;
  int max_db_size_ = 20;
  int max_db_age_ = 5;

  std::string product_name_ = "cef";
  std::string product_version_ = CEF_VERSION;

#if defined(OS_WIN)
  std::string app_name_ = "CEF";
  std::string external_handler_;
#endif

#if defined(OS_MACOSX)
  bool enable_browser_crash_forwarding_ = false;
#endif

  DISALLOW_COPY_AND_ASSIGN(CefCrashReporterClient);
};

#endif  // CEF_LIBCEF_COMMON_CRASH_REPORTER_CLIENT_H_
