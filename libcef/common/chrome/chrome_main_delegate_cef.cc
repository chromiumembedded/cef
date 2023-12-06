// Copyright 2020 The Chromium Embedded Framework Authors.
// Portions copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/common/chrome/chrome_main_delegate_cef.h"

#include <tuple>

#include "libcef/browser/chrome/chrome_browser_context.h"
#include "libcef/browser/chrome/chrome_content_browser_client_cef.h"
#include "libcef/common/cef_switches.h"
#include "libcef/common/command_line_impl.h"
#include "libcef/common/crash_reporting.h"
#include "libcef/common/resource_util.h"
#include "libcef/renderer/chrome/chrome_content_renderer_client_cef.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/threading/threading_features.h"
#include "chrome/browser/metrics/chrome_feature_list_creator.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/embedder_support/switches.h"
#include "content/public/common/content_switches.h"
#include "sandbox/policy/switches.h"
#include "third_party/blink/public/common/switches.h"
#include "ui/base/ui_base_switches.h"

#if BUILDFLAG(IS_MAC)
#include "libcef/common/util_mac.h"
#elif BUILDFLAG(IS_POSIX)
#include "libcef/common/util_linux.h"
#endif

namespace {

base::LazyInstance<ChromeContentRendererClientCef>::DestructorAtExit
    g_chrome_content_renderer_client = LAZY_INSTANCE_INITIALIZER;

}  // namespace

ChromeMainDelegateCef::ChromeMainDelegateCef(CefMainRunnerHandler* runner,
                                             CefSettings* settings,
                                             CefRefPtr<CefApp> application)
    : ChromeMainDelegate(base::TimeTicks::Now()),
      runner_(runner),
      settings_(settings),
      application_(application) {
#if BUILDFLAG(IS_LINUX)
  resource_util::OverrideAssetPath();
#endif
}

ChromeMainDelegateCef::~ChromeMainDelegateCef() = default;

std::optional<int> ChromeMainDelegateCef::BasicStartupComplete() {
  // Returns no value if startup should proceed.
  auto result = ChromeMainDelegate::BasicStartupComplete();
  if (result.has_value()) {
    return result;
  }

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

#if BUILDFLAG(IS_POSIX)
  // Read the crash configuration file. On Windows this is done from chrome_elf.
  crash_reporting::BasicStartupComplete(command_line);
#endif

  const std::string& process_type =
      command_line->GetSwitchValueASCII(switches::kProcessType);
  if (process_type.empty()) {
    // In the browser process. Populate the global command-line object.
    // TODO(chrome-runtime): Copy more settings from AlloyMainDelegate and test.
    if (settings_->command_line_args_disabled) {
      // Remove any existing command-line arguments.
      base::CommandLine::StringVector argv;
      argv.push_back(command_line->GetProgram().value());
      command_line->InitFromArgv(argv);

      const base::CommandLine::SwitchMap& map = command_line->GetSwitches();
      const_cast<base::CommandLine::SwitchMap*>(&map)->clear();
    }

    bool no_sandbox = settings_->no_sandbox ? true : false;

    if (no_sandbox) {
      command_line->AppendSwitch(sandbox::policy::switches::kNoSandbox);
    }

    if (settings_->user_agent.length > 0) {
      command_line->AppendSwitchASCII(
          embedder_support::kUserAgent,
          CefString(&settings_->user_agent).ToString());
    } else if (settings_->user_agent_product.length > 0) {
      command_line->AppendSwitchASCII(
          switches::kUserAgentProductAndVersion,
          CefString(&settings_->user_agent_product).ToString());
    }

    if (settings_->locale.length > 0) {
      command_line->AppendSwitchASCII(switches::kLang,
                                      CefString(&settings_->locale).ToString());
    } else if (!command_line->HasSwitch(switches::kLang)) {
      command_line->AppendSwitchASCII(switches::kLang, "en-US");
    }

    if (settings_->javascript_flags.length > 0) {
      command_line->AppendSwitchASCII(
          blink::switches::kJavaScriptFlags,
          CefString(&settings_->javascript_flags).ToString());
    }

    if (settings_->remote_debugging_port >= 1024 &&
        settings_->remote_debugging_port <= 65535) {
      command_line->AppendSwitchASCII(
          switches::kRemoteDebuggingPort,
          base::NumberToString(settings_->remote_debugging_port));
    }

    if (settings_->uncaught_exception_stack_size > 0) {
      command_line->AppendSwitchASCII(
          switches::kUncaughtExceptionStackSize,
          base::NumberToString(settings_->uncaught_exception_stack_size));
    }

    std::vector<std::string> disable_features;

    if (!!settings_->multi_threaded_message_loop &&
        base::kEnableHangWatcher.default_state ==
            base::FEATURE_ENABLED_BY_DEFAULT) {
      // Disable EnableHangWatcher when running with multi-threaded-message-loop
      // to avoid shutdown crashes (see issue #3403).
      disable_features.push_back(base::kEnableHangWatcher.name);
    }

    if (!disable_features.empty()) {
      DCHECK(!base::FeatureList::GetInstance());
      std::string disable_features_str =
          command_line->GetSwitchValueASCII(switches::kDisableFeatures);
      for (auto feature_str : disable_features) {
        if (!disable_features_str.empty()) {
          disable_features_str += ",";
        }
        disable_features_str += feature_str;
      }
      command_line->AppendSwitchASCII(switches::kDisableFeatures,
                                      disable_features_str);
    }
  }

  if (application_) {
    // Give the application a chance to view/modify the command line.
    CefRefPtr<CefCommandLineImpl> commandLinePtr(
        new CefCommandLineImpl(command_line, false, false));
    application_->OnBeforeCommandLineProcessing(process_type,
                                                commandLinePtr.get());
    std::ignore = commandLinePtr->Detach(nullptr);
  }

#if BUILDFLAG(IS_MAC)
  util_mac::BasicStartupComplete();
#endif

  return std::nullopt;
}

void ChromeMainDelegateCef::PreSandboxStartup() {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  const std::string& process_type =
      command_line->GetSwitchValueASCII(switches::kProcessType);

  if (process_type.empty()) {
#if BUILDFLAG(IS_MAC)
    util_mac::PreSandboxStartup();
#elif BUILDFLAG(IS_POSIX)
    util_linux::PreSandboxStartup();
#endif
  }

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

std::optional<int> ChromeMainDelegateCef::PreBrowserMain() {
  // The parent ChromeMainDelegate implementation creates the NSApplication
  // instance on macOS, and we intentionally don't want to do that here.
  // TODO(macos): Do we need l10n_util::OverrideLocaleWithCocoaLocale()?
  runner_->PreBrowserMain();
  return std::nullopt;
}

std::optional<int> ChromeMainDelegateCef::PostEarlyInitialization(
    InvokedIn invoked_in) {
  // Configure this before ChromeMainDelegate::PostEarlyInitialization triggers
  // ChromeBrowserPolicyConnector creation.
  if (settings_ && settings_->chrome_policy_id.length > 0) {
    policy::ChromeBrowserPolicyConnector::EnablePlatformPolicySupport(
        CefString(&settings_->chrome_policy_id).ToString());
  }

  const auto result = ChromeMainDelegate::PostEarlyInitialization(invoked_in);
  if (!result) {
    const auto* invoked_in_browser =
        absl::get_if<InvokedInBrowserProcess>(&invoked_in);
    if (invoked_in_browser) {
      // At this point local_state has been created but ownership has not yet
      // been passed to BrowserProcessImpl (g_browser_process is nullptr).
      auto* local_state = chrome_content_browser_client_->startup_data()
                              ->chrome_feature_list_creator()
                              ->local_state();

      // Don't show the profile picker on startup (see issue #3440).
      local_state->SetBoolean(prefs::kBrowserShowProfilePickerOnStartup, false);
    }
  }

  return result;
}

absl::variant<int, content::MainFunctionParams>
ChromeMainDelegateCef::RunProcess(
    const std::string& process_type,
    content::MainFunctionParams main_function_params) {
  if (process_type.empty()) {
    return runner_->RunMainProcess(std::move(main_function_params));
  }

  return ChromeMainDelegate::RunProcess(process_type,
                                        std::move(main_function_params));
}

#if BUILDFLAG(IS_LINUX)
void ChromeMainDelegateCef::ZygoteForked() {
  ChromeMainDelegate::ZygoteForked();

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  const std::string& process_type =
      command_line->GetSwitchValueASCII(switches::kProcessType);

  // Initialize crash reporting state for the newly forked process.
  crash_reporting::ZygoteForked(command_line, process_type);
}
#endif  // BUILDFLAG(IS_LINUX)

content::ContentClient* ChromeMainDelegateCef::CreateContentClient() {
  return &chrome_content_client_cef_;
}

content::ContentBrowserClient*
ChromeMainDelegateCef::CreateContentBrowserClient() {
  // Match the logic in the parent ChromeMainDelegate implementation, but create
  // our own object type.
  chrome_content_browser_client_ =
      std::make_unique<ChromeContentBrowserClientCef>();
  return chrome_content_browser_client_.get();
}

content::ContentRendererClient*
ChromeMainDelegateCef::CreateContentRendererClient() {
  return g_chrome_content_renderer_client.Pointer();
}

CefRefPtr<CefRequestContext> ChromeMainDelegateCef::GetGlobalRequestContext() {
  auto browser_client = content_browser_client();
  if (browser_client) {
    return browser_client->request_context();
  }
  return nullptr;
}

CefBrowserContext* ChromeMainDelegateCef::CreateNewBrowserContext(
    const CefRequestContextSettings& settings,
    base::OnceClosure initialized_cb) {
  auto context = new ChromeBrowserContext(settings);
  context->InitializeAsync(std::move(initialized_cb));
  return context;
}

scoped_refptr<base::SingleThreadTaskRunner>
ChromeMainDelegateCef::GetBackgroundTaskRunner() {
  auto browser_client = content_browser_client();
  if (browser_client) {
    return browser_client->background_task_runner();
  }
  return nullptr;
}

scoped_refptr<base::SingleThreadTaskRunner>
ChromeMainDelegateCef::GetUserVisibleTaskRunner() {
  auto browser_client = content_browser_client();
  if (browser_client) {
    return browser_client->user_visible_task_runner();
  }
  return nullptr;
}

scoped_refptr<base::SingleThreadTaskRunner>
ChromeMainDelegateCef::GetUserBlockingTaskRunner() {
  auto browser_client = content_browser_client();
  if (browser_client) {
    return browser_client->user_blocking_task_runner();
  }
  return nullptr;
}

scoped_refptr<base::SingleThreadTaskRunner>
ChromeMainDelegateCef::GetRenderTaskRunner() {
  auto renderer_client = content_renderer_client();
  if (renderer_client) {
    return renderer_client->render_task_runner();
  }
  return nullptr;
}

scoped_refptr<base::SingleThreadTaskRunner>
ChromeMainDelegateCef::GetWebWorkerTaskRunner() {
  auto renderer_client = content_renderer_client();
  if (renderer_client) {
    return renderer_client->GetCurrentTaskRunner();
  }
  return nullptr;
}

ChromeContentBrowserClientCef* ChromeMainDelegateCef::content_browser_client()
    const {
  return static_cast<ChromeContentBrowserClientCef*>(
      chrome_content_browser_client_.get());
}

ChromeContentRendererClientCef* ChromeMainDelegateCef::content_renderer_client()
    const {
  if (!g_chrome_content_renderer_client.IsCreated()) {
    return nullptr;
  }
  return g_chrome_content_renderer_client.Pointer();
}
