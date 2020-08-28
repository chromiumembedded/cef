// Copyright 2020 The Chromium Embedded Framework Authors.
// Portions copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/common/chrome/chrome_main_delegate_cef.h"

#include "libcef/browser/chrome/chrome_browser_context.h"
#include "libcef/browser/chrome/chrome_content_browser_client_cef.h"
#include "libcef/common/crash_reporting.h"
#include "libcef/common/resource_util.h"

#include "base/command_line.h"
#include "content/public/common/content_switches.h"

#if defined(OS_MAC)
#include "libcef/common/util_mac.h"
#endif

ChromeMainDelegateCef::ChromeMainDelegateCef(CefMainRunnerHandler* runner,
                                             CefSettings* settings,
                                             CefRefPtr<CefApp> application)
    : ChromeMainDelegate(base::TimeTicks::Now()),
      runner_(runner),
      settings_(settings),
      application_(application) {
#if defined(OS_LINUX)
  resource_util::OverrideAssetPath();
#endif
}

ChromeMainDelegateCef::~ChromeMainDelegateCef() = default;

bool ChromeMainDelegateCef::BasicStartupComplete(int* exit_code) {
  // Returns false if startup should proceed.
  bool result = ChromeMainDelegate::BasicStartupComplete(exit_code);

  if (!result) {
#if defined(OS_POSIX)
    // Read the crash configuration file. Platforms using Breakpad also add a
    // command-line switch. On Windows this is done from chrome_elf.
    crash_reporting::BasicStartupComplete(
        base::CommandLine::ForCurrentProcess());
#endif

#if defined(OS_MAC)
    util_mac::BasicStartupComplete();
#endif
  }

  return result;
}

void ChromeMainDelegateCef::PreSandboxStartup() {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  const std::string& process_type =
      command_line->GetSwitchValueASCII(switches::kProcessType);

#if defined(OS_MAC)
  if (process_type.empty()) {
    util_mac::PreSandboxStartup();
  }
#endif  // defined(OS_MAC)

  // Since this may be configured via CefSettings we override the value on
  // all platforms. We can't use the default implementation on macOS because
  // chrome::GetDefaultUserDataDirectory expects to find the Chromium version
  // number in the app bundle path.
  resource_util::OverrideUserDataDir(settings_, command_line);

  ChromeMainDelegate::PreSandboxStartup();

  // Initialize crash reporting state for this process/module.
  // chrome::DIR_CRASH_DUMPS must be configured before calling this function.
  crash_reporting::PreSandboxStartup(*command_line, process_type);
}

void ChromeMainDelegateCef::PreCreateMainMessageLoop() {
  // The parent ChromeMainDelegate implementation creates the NSApplication
  // instance on macOS, and we intentionally don't want to do that here.
  // TODO(macos): Do we need l10n_util::OverrideLocaleWithCocoaLocale()?
  runner_->PreCreateMainMessageLoop();
}

int ChromeMainDelegateCef::RunProcess(
    const std::string& process_type,
    const content::MainFunctionParams& main_function_params) {
  if (process_type.empty()) {
    return runner_->RunMainProcess(main_function_params);
  }

  return ChromeMainDelegate::RunProcess(process_type, main_function_params);
}

#if defined(OS_LINUX)
void ChromeMainDelegateCef::ZygoteForked() {
  ChromeMainDelegate::ZygoteForked();

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  const std::string& process_type =
      command_line->GetSwitchValueASCII(switches::kProcessType);

  // Initialize crash reporting state for the newly forked process.
  crash_reporting::ZygoteForked(command_line, process_type);
}
#endif  // defined(OS_LINUX)

content::ContentClient* ChromeMainDelegateCef::CreateContentClient() {
  return &chrome_content_client_cef_;
}

content::ContentBrowserClient*
ChromeMainDelegateCef::CreateContentBrowserClient() {
  // Match the logic in the parent ChromeMainDelegate implementation, but create
  // our own object type.
  if (chrome_content_browser_client_ == nullptr) {
    DCHECK(!startup_data_);
    startup_data_ = std::make_unique<StartupData>();

    chrome_content_browser_client_ =
        std::make_unique<ChromeContentBrowserClientCef>(startup_data_.get());
  }
  return chrome_content_browser_client_.get();
}

CefRefPtr<CefRequestContext> ChromeMainDelegateCef::GetGlobalRequestContext() {
  auto browser_client = content_browser_client();
  if (browser_client)
    return browser_client->request_context();
  return nullptr;
}

CefBrowserContext* ChromeMainDelegateCef::CreateNewBrowserContext(
    const CefRequestContextSettings& settings) {
  auto context = new ChromeBrowserContext(settings);
  context->Initialize();
  return context;
}

scoped_refptr<base::SingleThreadTaskRunner>
ChromeMainDelegateCef::GetBackgroundTaskRunner() {
  auto browser_client = content_browser_client();
  if (browser_client)
    return browser_client->background_task_runner();
  return nullptr;
}

scoped_refptr<base::SingleThreadTaskRunner>
ChromeMainDelegateCef::GetUserVisibleTaskRunner() {
  auto browser_client = content_browser_client();
  if (browser_client)
    return browser_client->user_visible_task_runner();
  return nullptr;
}

scoped_refptr<base::SingleThreadTaskRunner>
ChromeMainDelegateCef::GetUserBlockingTaskRunner() {
  auto browser_client = content_browser_client();
  if (browser_client)
    return browser_client->user_blocking_task_runner();
  return nullptr;
}

scoped_refptr<base::SingleThreadTaskRunner>
ChromeMainDelegateCef::GetRenderTaskRunner() {
  // TODO: Implement.
  NOTREACHED();
  return nullptr;
}

scoped_refptr<base::SingleThreadTaskRunner>
ChromeMainDelegateCef::GetWebWorkerTaskRunner() {
  // TODO: Implement.
  NOTREACHED();
  return nullptr;
}

ChromeContentBrowserClientCef* ChromeMainDelegateCef::content_browser_client()
    const {
  return static_cast<ChromeContentBrowserClientCef*>(
      chrome_content_browser_client_.get());
}
