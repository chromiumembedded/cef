// Copyright 2016 The Chromium Embedded Framework Authors. Portions copyright
// 2016 The Chromium Authors. All rights reserved. Use of this source code is
// governed by a BSD-style license that can be found in the LICENSE file.

#include "libcef/common/crash_reporting.h"

#include "include/cef_crash_util.h"
#include "libcef/common/cef_switches.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/debug/crash_logging.h"
#include "base/strings/string_util.h"
#include "chrome/common/crash_keys.h"
#include "content/public/common/content_switches.h"

#if defined(OS_MACOSX)
#include "base/mac/foundation_util.h"
#include "components/crash/content/app/crashpad.h"
#include "components/crash/core/common/crash_keys.h"
#include "content/public/common/content_paths.h"
#endif

#if defined(OS_POSIX)
#include "base/lazy_instance.h"
#include "libcef/common/crash_reporter_client.h"
#endif

#if defined(OS_POSIX) && !defined(OS_MACOSX)
#include "components/crash/content/app/breakpad_linux.h"
#endif

#if defined(OS_WIN)
#include "libcef/common/crash_reporting_win.h"
#endif

namespace crash_reporting {

namespace {

bool g_crash_reporting_enabled = false;

#if defined(OS_POSIX)
base::LazyInstance<CefCrashReporterClient>::Leaky g_crash_reporter_client =
    LAZY_INSTANCE_INITIALIZER;

void InitCrashReporter(const base::CommandLine& command_line,
                       const std::string& process_type) {
  CefCrashReporterClient* crash_client = g_crash_reporter_client.Pointer();
  if (!crash_client->HasCrashConfigFile())
    return;

  crash_reporter::SetCrashReporterClient(crash_client);

#if defined(OS_MACOSX)
  // TODO(mark): Right now, InitializeCrashpad() needs to be called after
  // CommandLine::Init() and configuration of chrome::DIR_CRASH_DUMPS. Ideally,
  // Crashpad initialization could occur sooner, preferably even before the
  // framework dylib is even loaded, to catch potential early crashes.
  crash_reporter::InitializeCrashpad(process_type.empty(), process_type);

  if (base::mac::AmIBundled()) {
    // Mac Chrome is packaged with a main app bundle and a helper app bundle.
    // The main app bundle should only be used for the browser process, so it
    // should never see a --type switch (switches::kProcessType).  Likewise,
    // the helper should always have a --type switch.
    //
    // This check is done this late so there is already a call to
    // base::mac::IsBackgroundOnlyProcess(), so there is no change in
    // startup/initialization order.

    // The helper's Info.plist marks it as a background only app.
    if (base::mac::IsBackgroundOnlyProcess()) {
      CHECK(command_line.HasSwitch(switches::kProcessType) &&
            !process_type.empty())
          << "Helper application requires --type.";
    } else {
      CHECK(!command_line.HasSwitch(switches::kProcessType) &&
            process_type.empty())
          << "Main application forbids --type, saw " << process_type;
    }
  }

  g_crash_reporting_enabled = true;
#else  // !defined(OS_MACOSX)
  breakpad::SetCrashServerURL(crash_client->GetCrashServerURL());

  if (process_type != switches::kZygoteProcess) {
    // Crash reporting for subprocesses created using the zygote will be
    // initialized in ZygoteForked.
    breakpad::InitCrashReporter(process_type);

    g_crash_reporting_enabled = true;
  }
#endif  // !defined(OS_MACOSX)
}
#endif  // defined(OS_POSIX)

// Used to exclude command-line flags from crash reporting.
bool IsBoringCEFSwitch(const std::string& flag) {
  if (crash_keys::IsBoringChromeSwitch(flag))
    return true;

  static const char* const kIgnoreSwitches[] = {
    // CEF internals.
    switches::kLogFile,

    // Chromium internals.
    "content-image-texture-target",
    "mojo-platform-channel-handle",
    "primordial-pipe-token",
    "service-request-channel-token",
  };

  if (!base::StartsWith(flag, "--", base::CompareCase::SENSITIVE))
    return false;

  size_t end = flag.find("=");
  size_t len = (end == std::string::npos) ? flag.length() - 2 : end - 2;
  for (size_t i = 0; i < arraysize(kIgnoreSwitches); ++i) {
    if (flag.compare(2, len, kIgnoreSwitches[i]) == 0)
      return true;
  }
  return false;
}

}  // namespace

bool Enabled() {
  return g_crash_reporting_enabled;
}

#if defined(OS_POSIX)
// Be aware that logging is not initialized at the time this method is called.
void BasicStartupComplete(base::CommandLine* command_line) {
  CefCrashReporterClient* crash_client = g_crash_reporter_client.Pointer();
  if (crash_client->ReadCrashConfigFile()) {
#if !defined(OS_MACOSX)
    // Breakpad requires this switch.
    command_line->AppendSwitch(switches::kEnableCrashReporter);
#endif
  }
}
#endif

void PreSandboxStartup(const base::CommandLine& command_line,
                       const std::string& process_type) {
#if defined(OS_POSIX)
  // Initialize crash reporting here on macOS and Linux. Crash reporting on
  // Windows is initialized from context.cc.
  InitCrashReporter(command_line, process_type);
#elif defined(OS_WIN)
  // Initialize crash key globals in the module (libcef) address space.
  g_crash_reporting_enabled =
      crash_reporting_win::InitializeCrashReportingForModule();
#endif

  if (g_crash_reporting_enabled) {
    LOG(INFO) << "Crash reporting enabled for process: " <<
                 (process_type.empty() ? "browser" : process_type.c_str());
  }

  // After platform crash reporting have been initialized, store the command
  // line for crash reporting.
  crash_keys::SetSwitchesFromCommandLine(command_line, &IsBoringCEFSwitch);
}

#if defined(OS_POSIX) && !defined(OS_ANDROID) && !defined(OS_MACOSX)
void ZygoteForked(base::CommandLine* command_line,
                  const std::string& process_type) {
  CefCrashReporterClient* crash_client = g_crash_reporter_client.Pointer();
  if (crash_client->HasCrashConfigFile()) {
    // Breakpad requires this switch.
    command_line->AppendSwitch(switches::kEnableCrashReporter);
  }

  InitCrashReporter(*command_line, process_type);

  if (g_crash_reporting_enabled) {
    LOG(INFO) << "Crash reporting enabled for process: " << process_type;
  }

  // Reset the command line for the newly spawned process.
  crash_keys::SetSwitchesFromCommandLine(*command_line, &IsBoringCEFSwitch);
}
#endif

}  // namespace crash_reporting

bool CefCrashReportingEnabled() {
  return crash_reporting::Enabled();
}

void CefSetCrashKeyValue(const CefString& key, const CefString& value) {
  base::debug::SetCrashKeyValue(key.ToString(), value.ToString());
}
