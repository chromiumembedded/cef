// Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "test_suite.h"
#include "../cefclient/cefclient_switches.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/test/test_suite.h"

#if defined(OS_MACOSX)
#include "base/file_path.h"
#include "base/i18n/icu_util.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/test/test_timeouts.h"
#endif

CommandLine* CefTestSuite::commandline_ = NULL;

CefTestSuite::CefTestSuite(int argc, char** argv)
  : TestSuite(argc, argv) {
}

// static
void CefTestSuite::InitCommandLine(int argc, const char* const* argv)
{
  if (commandline_) {
    // If this is intentional, Reset() must be called first. If we are using
    // the shared build mode, we have to share a single object across multiple
    // shared libraries.
    return;
  }

  commandline_ = new CommandLine(CommandLine::NO_PROGRAM);
#if defined(OS_WIN)
  commandline_->ParseFromString(::GetCommandLineW());
#elif defined(OS_POSIX)
  commandline_->InitFromArgv(argc, argv);
#endif
}

// static
void CefTestSuite::GetSettings(CefSettings& settings) {
#if defined(OS_WIN)
  settings.multi_threaded_message_loop =
      commandline_->HasSwitch(cefclient::kMultiThreadedMessageLoop);
#endif

  CefString(&settings.cache_path) =
      commandline_->GetSwitchValueASCII(cefclient::kCachePath);
  CefString(&settings.user_agent) =
      commandline_->GetSwitchValueASCII(cefclient::kUserAgent);
  CefString(&settings.product_version) =
      commandline_->GetSwitchValueASCII(cefclient::kProductVersion);
  CefString(&settings.locale) =
      commandline_->GetSwitchValueASCII(cefclient::kLocale);
  CefString(&settings.log_file) =
      commandline_->GetSwitchValueASCII(cefclient::kLogFile);

  {
    std::string str =
        commandline_->GetSwitchValueASCII(cefclient::kLogSeverity);
    bool invalid = false;
    if (!str.empty()) {
      if (str == cefclient::kLogSeverity_Verbose)
        settings.log_severity = LOGSEVERITY_VERBOSE;
      else if (str == cefclient::kLogSeverity_Info)
        settings.log_severity = LOGSEVERITY_INFO;
      else if (str == cefclient::kLogSeverity_Warning)
        settings.log_severity = LOGSEVERITY_WARNING;
      else if (str == cefclient::kLogSeverity_Error)
        settings.log_severity = LOGSEVERITY_ERROR;
      else if (str == cefclient::kLogSeverity_ErrorReport)
        settings.log_severity = LOGSEVERITY_ERROR_REPORT;
      else if (str == cefclient::kLogSeverity_Disable)
        settings.log_severity = LOGSEVERITY_DISABLE;
      else
        invalid = true;
    }
    if (str.empty() || invalid) {
#ifdef NDEBUG
      // Only log error messages and higher in release build.
      settings.log_severity = LOGSEVERITY_ERROR;
#endif
    }
  }

  {
    std::string str =
        commandline_->GetSwitchValueASCII(cefclient::kGraphicsImpl);
    if (!str.empty()) {
#if defined(OS_WIN)
      if (str == cefclient::kGraphicsImpl_Angle)
        settings.graphics_implementation = ANGLE_IN_PROCESS;
      else if (str == cefclient::kGraphicsImpl_AngleCmdBuffer)
        settings.graphics_implementation = ANGLE_IN_PROCESS_COMMAND_BUFFER;
      else
#endif
      if (str == cefclient::kGraphicsImpl_Desktop)
        settings.graphics_implementation = DESKTOP_IN_PROCESS;
      else if (str == cefclient::kGraphicsImpl_DesktopCmdBuffer)
        settings.graphics_implementation = DESKTOP_IN_PROCESS_COMMAND_BUFFER;
    }
  }

  settings.local_storage_quota = atoi(commandline_->GetSwitchValueASCII(
      cefclient::kLocalStorageQuota).c_str());
  settings.session_storage_quota = atoi(commandline_->GetSwitchValueASCII(
      cefclient::kSessionStorageQuota).c_str());

  CefString(&settings.javascript_flags) =
      commandline_->GetSwitchValueASCII(cefclient::kJavascriptFlags);
}

// static
bool CefTestSuite::GetCachePath(std::string& path) {
  DCHECK(commandline_);

  if (commandline_->HasSwitch(cefclient::kCachePath)) {
    // Set the cache_path value.
    path = commandline_->GetSwitchValueASCII(cefclient::kCachePath);
    return true;
  }

  return false;
}

#if defined(OS_MACOSX)
void CefTestSuite::Initialize() {
  // The below code is copied from base/test/test_suite.cc to avoid calling
  // RegisterMockCrApp() on Mac.
  
  // Initialize logging.
  FilePath exe;
  PathService::Get(base::FILE_EXE, &exe);
  FilePath log_filename = exe.ReplaceExtension(FILE_PATH_LITERAL("log"));
  logging::InitLogging(
                       log_filename.value().c_str(),
                       logging::LOG_TO_BOTH_FILE_AND_SYSTEM_DEBUG_LOG,
                       logging::LOCK_LOG_FILE,
                       logging::DELETE_OLD_LOG_FILE,
                       logging::DISABLE_DCHECK_FOR_NON_OFFICIAL_RELEASE_BUILDS);
  // We want process and thread IDs because we may have multiple processes.
  // Note: temporarily enabled timestamps in an effort to catch bug 6361.
  logging::SetLogItems(true, true, true, true);
  
  CHECK(base::EnableInProcessStackDumping());
  
  // In some cases, we do not want to see standard error dialogs.
  if (!base::debug::BeingDebugged() &&
      !CommandLine::ForCurrentProcess()->HasSwitch("show-error-dialogs")) {
    SuppressErrorDialogs();
    base::debug::SetSuppressDebugUI(true);
    logging::SetLogAssertHandler(UnitTestAssertHandler);
  }
  
  icu_util::Initialize();
  
  CatchMaybeTests();
  ResetCommandLine();
  
  TestTimeouts::Initialize();
}
#endif // defined(OS_MACOSX)
