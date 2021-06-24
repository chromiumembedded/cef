// Copyright 2016 The Chromium Embedded Framework Authors. Postions copyright
// 2012 The Chromium Authors. All rights reserved. Use of this source code is
// governed by a BSD-style license that can be found in the LICENSE file.

#include "tests/ceftests/test_suite.h"

#include "include/cef_file_util.h"
#include "include/wrapper/cef_scoped_temp_dir.h"
#include "tests/gtest/include/gtest/gtest.h"
#include "tests/gtest/teamcity/include/teamcity_gtest.h"
#include "tests/shared/common/client_switches.h"

namespace {

CefTestSuite* g_test_suite = nullptr;

#if defined(OS_WIN)

// From base/process/launch_win.cc.
void RouteStdioToConsole(bool create_console_if_not_found) {
  // Don't change anything if stdout or stderr already point to a
  // valid stream.
  //
  // If we are running under Buildbot or under Cygwin's default
  // terminal (mintty), stderr and stderr will be pipe handles.  In
  // that case, we don't want to open CONOUT$, because its output
  // likely does not go anywhere.
  //
  // We don't use GetStdHandle() to check stdout/stderr here because
  // it can return dangling IDs of handles that were never inherited
  // by this process.  These IDs could have been reused by the time
  // this function is called.  The CRT checks the validity of
  // stdout/stderr on startup (before the handle IDs can be reused).
  // _fileno(stdout) will return -2 (_NO_CONSOLE_FILENO) if stdout was
  // invalid.
  if (_fileno(stdout) >= 0 || _fileno(stderr) >= 0) {
    return;
  }

  if (!AttachConsole(ATTACH_PARENT_PROCESS)) {
    unsigned int result = GetLastError();
    // Was probably already attached.
    if (result == ERROR_ACCESS_DENIED)
      return;
    // Don't bother creating a new console for each child process if the
    // parent process is invalid (eg: crashed).
    if (result == ERROR_GEN_FAILURE)
      return;
    if (create_console_if_not_found) {
      // Make a new console if attaching to parent fails with any other error.
      // It should be ERROR_INVALID_HANDLE at this point, which means the
      // browser was likely not started from a console.
      AllocConsole();
    } else {
      return;
    }
  }

  // Arbitrary byte count to use when buffering output lines.  More
  // means potential waste, less means more risk of interleaved
  // log-lines in output.
  enum { kOutputBufferSize = 64 * 1024 };

  if (freopen("CONOUT$", "w", stdout)) {
    setvbuf(stdout, nullptr, _IOLBF, kOutputBufferSize);
    // Overwrite FD 1 for the benefit of any code that uses this FD
    // directly.  This is safe because the CRT allocates FDs 0, 1 and
    // 2 at startup even if they don't have valid underlying Windows
    // handles.  This means we won't be overwriting an FD created by
    // _open() after startup.
    _dup2(_fileno(stdout), 1);
  }
  if (freopen("CONOUT$", "w", stderr)) {
    setvbuf(stderr, nullptr, _IOLBF, kOutputBufferSize);
    _dup2(_fileno(stderr), 2);
  }

  // Fix all cout, wcout, cin, wcin, cerr, wcerr, clog and wclog.
  std::ios::sync_with_stdio();
}

#endif  // defined(OS_WIN)

}  // namespace

CefTestSuite::CefTestSuite(int argc, char** argv)
    : argc_(argc), argv_(argc, argv), retval_(0) {
  g_test_suite = this;

  // Keep a representation of the original command-line.
  command_line_ = CefCommandLine::CreateCommandLine();
#if defined(OS_WIN)
  command_line_->InitFromString(::GetCommandLineW());
#else
  command_line_->InitFromArgv(argc, argv);
#endif

  if (!command_line_->HasSwitch("type")) {
    // Initialize in the main process only.
    root_cache_path_ =
        command_line_->GetSwitchValue(client::switches::kCachePath);
    if (root_cache_path_.empty()) {
      CefScopedTempDir temp_dir;
      CHECK(temp_dir.CreateUniqueTempDir());
      root_cache_path_ = temp_dir.Take();
      RegisterTempDirectory(root_cache_path_);
    }
  }
}

CefTestSuite::~CefTestSuite() {
  g_test_suite = nullptr;
}

// static
CefTestSuite* CefTestSuite::GetInstance() {
  return g_test_suite;
}

void CefTestSuite::InitMainProcess() {
  PreInitialize();

  // This will modify |argc_| and |argv_|.
  testing::InitGoogleTest(&argc_, argv_.array());

  if (jetbrains::teamcity::underTeamcity()) {
    auto& listeners = ::testing::UnitTest::GetInstance()->listeners();
    listeners.Append(
        new jetbrains::teamcity::TeamcityGoogleTestEventListener());
  }
}

// Don't add additional code to this method. Instead add it to Initialize().
int CefTestSuite::Run() {
  Initialize();
  retval_ = RUN_ALL_TESTS();
  Shutdown();
  return retval_;
}

void CefTestSuite::GetSettings(CefSettings& settings) const {
  // Enable the experimental Chrome runtime. See issue #2969 for details.
  settings.chrome_runtime =
      command_line_->HasSwitch(client::switches::kEnableChromeRuntime);

  CefString(&settings.cache_path) = root_cache_path_;
  CefString(&settings.root_cache_path) = root_cache_path_;
  CefString(&settings.user_data_path) = root_cache_path_;

  // Always expose the V8 gc() function to give tests finer-grained control over
  // memory management.
  std::string javascript_flags = "--expose-gc";
  // Value of kJavascriptFlags switch.
  std::string other_javascript_flags =
      command_line_->GetSwitchValue("js-flags");
  if (!other_javascript_flags.empty())
    javascript_flags += " " + other_javascript_flags;
  CefString(&settings.javascript_flags) = javascript_flags;

  // Necessary for V8Test.OnUncaughtException tests.
  settings.uncaught_exception_stack_size = 10;

  // Necessary for the OSRTest tests.
  settings.windowless_rendering_enabled = true;

  // For Accept-Language test
  CefString(&settings.accept_language_list) = CEF_SETTINGS_ACCEPT_LANGUAGE;
}

void CefTestSuite::RegisterTempDirectory(const CefString& directory) {
  base::AutoLock lock_scope(temp_directories_lock_);
  temp_directories_.push_back(directory);
}

void CefTestSuite::DeleteTempDirectories() {
  base::AutoLock lock_scope(temp_directories_lock_);
  for (size_t i = 0U; i < temp_directories_.size(); ++i) {
    CefDeleteFile(temp_directories_[i], true);
  }
  temp_directories_.clear();
}

void CefTestSuite::PreInitialize() {
#if defined(OS_WIN)
  testing::GTEST_FLAG(catch_exceptions) = false;

  // Enable termination on heap corruption.
  // Ignore the result code. Supported starting with XP SP3 and Vista.
  HeapSetInformation(nullptr, HeapEnableTerminationOnCorruption, nullptr, 0);
#endif

#if defined(OS_LINUX)
  // When calling native char conversion functions (e.g wrctomb) we need to
  // have the locale set. In the absence of such a call the "C" locale is the
  // default. In the gtk code (below) gtk_init() implicitly sets a locale.
  setlocale(LC_ALL, "");
#endif  // defined(OS_LINUX)

  // Don't add additional code to this function. Instead add it to Initialize().
}

void CefTestSuite::Initialize() {
#if defined(OS_WIN)
  RouteStdioToConsole(true);
#endif  // defined(OS_WIN)
}

void CefTestSuite::Shutdown() {}
