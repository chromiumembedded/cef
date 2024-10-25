// Copyright 2020 The Chromium Embedded Framework Authors.
// Portions copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef/common/chrome/chrome_main_delegate_cef.h"

#include <tuple>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/path_service.h"
#include "base/threading/threading_features.h"
#include "cef/libcef/browser/chrome/chrome_browser_context.h"
#include "cef/libcef/browser/chrome/chrome_content_browser_client_cef.h"
#include "cef/libcef/browser/main_runner.h"
#include "cef/libcef/common/cef_switches.h"
#include "cef/libcef/common/command_line_impl.h"
#include "cef/libcef/common/crash_reporting.h"
#include "cef/libcef/common/resource_util.h"
#include "cef/libcef/renderer/chrome/chrome_content_renderer_client_cef.h"
#include "chrome/browser/metrics/chrome_feature_list_creator.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/embedder_support/switches.h"
#include "components/variations/service/buildflags.h"
#include "content/public/common/content_switches.h"
#include "net/base/features.h"
#include "sandbox/policy/switches.h"
#include "third_party/blink/public/common/switches.h"
#include "ui/base/ui_base_paths.h"
#include "ui/base/ui_base_switches.h"

#if BUILDFLAG(IS_MAC)
#include "cef/libcef/common/util_mac.h"
#elif BUILDFLAG(IS_POSIX)
#include "cef/libcef/common/util_linux.h"
#endif

namespace {

base::LazyInstance<ChromeContentRendererClientCef>::DestructorAtExit
    g_chrome_content_renderer_client = LAZY_INSTANCE_INITIALIZER;

void InitLogging(const base::CommandLine* command_line) {
  logging::LogSeverity log_severity = logging::LOGGING_INFO;

  std::string log_severity_str =
      command_line->GetSwitchValueASCII(switches::kLogSeverity);
  if (!log_severity_str.empty()) {
    if (base::EqualsCaseInsensitiveASCII(log_severity_str,
                                         switches::kLogSeverity_Verbose)) {
      log_severity = logging::LOGGING_VERBOSE;
    } else if (base::EqualsCaseInsensitiveASCII(
                   log_severity_str, switches::kLogSeverity_Warning)) {
      log_severity = logging::LOGGING_WARNING;
    } else if (base::EqualsCaseInsensitiveASCII(log_severity_str,
                                                switches::kLogSeverity_Error)) {
      log_severity = logging::LOGGING_ERROR;
    } else if (base::EqualsCaseInsensitiveASCII(log_severity_str,
                                                switches::kLogSeverity_Fatal)) {
      log_severity = logging::LOGGING_FATAL;
    } else if (base::EqualsCaseInsensitiveASCII(
                   log_severity_str, switches::kLogSeverity_Disable)) {
      log_severity = LOGSEVERITY_DISABLE;
    }
  }

  if (log_severity == LOGSEVERITY_DISABLE) {
    // By default, ERROR and FATAL messages will always be output to stderr due
    // to the kAlwaysPrintErrorLevel value in base/logging.cc. We change the log
    // level here so that only FATAL messages are output.
    logging::SetMinLogLevel(logging::LOGGING_FATAL);
  } else {
    logging::SetMinLogLevel(log_severity);
  }

  // Customization of items automatically prepended to log lines.
  std::string log_items_str =
      command_line->GetSwitchValueASCII(switches::kLogItems);
  if (!log_items_str.empty()) {
    bool enable_log_of_process_id, enable_log_of_thread_id,
        enable_log_of_time_stamp, enable_log_of_tick_count;
    enable_log_of_process_id = enable_log_of_thread_id =
        enable_log_of_time_stamp = enable_log_of_tick_count = false;

    for (const auto& cur_item_to_log :
         base::SplitStringPiece(log_items_str, ",", base::TRIM_WHITESPACE,
                                base::SPLIT_WANT_NONEMPTY)) {
      // if "none" mode is present, all items are disabled.
      if (base::EqualsCaseInsensitiveASCII(cur_item_to_log,
                                           switches::kLogItems_None)) {
        enable_log_of_process_id = enable_log_of_thread_id =
            enable_log_of_time_stamp = enable_log_of_tick_count = false;
        break;
      } else if (base::EqualsCaseInsensitiveASCII(cur_item_to_log,
                                                  switches::kLogItems_PId)) {
        enable_log_of_process_id = true;
      } else if (base::EqualsCaseInsensitiveASCII(cur_item_to_log,
                                                  switches::kLogItems_TId)) {
        enable_log_of_thread_id = true;
      } else if (base::EqualsCaseInsensitiveASCII(
                     cur_item_to_log, switches::kLogItems_TimeStamp)) {
        enable_log_of_time_stamp = true;
      } else if (base::EqualsCaseInsensitiveASCII(
                     cur_item_to_log, switches::kLogItems_TickCount)) {
        enable_log_of_tick_count = true;
      }
    }
    logging::SetLogItems(enable_log_of_process_id, enable_log_of_thread_id,
                         enable_log_of_time_stamp, enable_log_of_tick_count);
  }
}

}  // namespace

ChromeMainDelegateCef::ChromeMainDelegateCef(CefMainRunner* runner,
                                             CefSettings* settings,
                                             CefRefPtr<CefApp> application)
    : ChromeMainDelegate({.exe_entry_point_ticks = base::TimeTicks::Now()}),
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

    if (settings_->browser_subprocess_path.length > 0) {
      base::FilePath file_path =
          base::FilePath(CefString(&settings_->browser_subprocess_path));
      if (!file_path.empty()) {
        command_line->AppendSwitchPath(switches::kBrowserSubprocessPath,
                                       file_path);

#if BUILDFLAG(IS_WIN)
        // The sandbox is not supported when using a separate subprocess
        // executable on Windows.
        no_sandbox = true;
#endif
      }
    }

#if BUILDFLAG(IS_MAC)
    if (settings_->framework_dir_path.length > 0) {
      base::FilePath file_path =
          base::FilePath(CefString(&settings_->framework_dir_path));
      if (!file_path.empty()) {
        command_line->AppendSwitchPath(switches::kFrameworkDirPath, file_path);
      }
    }

    if (settings_->main_bundle_path.length > 0) {
      base::FilePath file_path =
          base::FilePath(CefString(&settings_->main_bundle_path));
      if (!file_path.empty()) {
        command_line->AppendSwitchPath(switches::kMainBundlePath, file_path);
      }
    }
#endif

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

    if (!command_line->HasSwitch(switches::kLogFile) &&
        settings_->log_file.length > 0) {
      auto log_file = base::FilePath(CefString(&settings_->log_file));
      command_line->AppendSwitchPath(switches::kLogFile, log_file);
    }

    if (!command_line->HasSwitch(switches::kLogSeverity) &&
        settings_->log_severity != LOGSEVERITY_DEFAULT) {
      std::string log_severity;
      switch (settings_->log_severity) {
        case LOGSEVERITY_VERBOSE:
          log_severity = switches::kLogSeverity_Verbose;
          break;
        case LOGSEVERITY_INFO:
          log_severity = switches::kLogSeverity_Info;
          break;
        case LOGSEVERITY_WARNING:
          log_severity = switches::kLogSeverity_Warning;
          break;
        case LOGSEVERITY_ERROR:
          log_severity = switches::kLogSeverity_Error;
          break;
        case LOGSEVERITY_FATAL:
          log_severity = switches::kLogSeverity_Fatal;
          break;
        case LOGSEVERITY_DISABLE:
          log_severity = switches::kLogSeverity_Disable;
          break;
        default:
          break;
      }
      if (!log_severity.empty()) {
        command_line->AppendSwitchASCII(switches::kLogSeverity, log_severity);
      }
    }

    if (!command_line->HasSwitch(switches::kLogItems) &&
        settings_->log_items != LOG_ITEMS_DEFAULT) {
      std::string log_items_str;
      if (settings_->log_items == LOG_ITEMS_NONE) {
        log_items_str = std::string(switches::kLogItems_None);
      } else {
        std::vector<std::string_view> added_items;
        if (settings_->log_items & LOG_ITEMS_FLAG_PROCESS_ID) {
          added_items.emplace_back(switches::kLogItems_PId);
        }
        if (settings_->log_items & LOG_ITEMS_FLAG_THREAD_ID) {
          added_items.emplace_back(switches::kLogItems_TId);
        }
        if (settings_->log_items & LOG_ITEMS_FLAG_TIME_STAMP) {
          added_items.emplace_back(switches::kLogItems_TimeStamp);
        }
        if (settings_->log_items & LOG_ITEMS_FLAG_TICK_COUNT) {
          added_items.emplace_back(switches::kLogItems_TickCount);
        }
        if (!added_items.empty()) {
          log_items_str = base::JoinString(added_items, ",");
        }
      }
      if (!log_items_str.empty()) {
        command_line->AppendSwitchASCII(switches::kLogItems, log_items_str);
      }
    }

    if (settings_->javascript_flags.length > 0) {
      command_line->AppendSwitchASCII(
          blink::switches::kJavaScriptFlags,
          CefString(&settings_->javascript_flags).ToString());
    }

    if (settings_->resources_dir_path.length > 0) {
      base::FilePath file_path =
          base::FilePath(CefString(&settings_->resources_dir_path));
      if (!file_path.empty()) {
        command_line->AppendSwitchPath(switches::kResourcesDirPath, file_path);
      }
    }

    if (settings_->locales_dir_path.length > 0) {
      base::FilePath file_path =
          base::FilePath(CefString(&settings_->locales_dir_path));
      if (!file_path.empty()) {
        command_line->AppendSwitchPath(switches::kLocalesDirPath, file_path);
      }
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

#if BUILDFLAG(IS_WIN)
    {
      const bool feature_enabled =
#if BUILDFLAG(FIELDTRIAL_TESTING_ENABLED)
          // May be enabled via the experiments platform is non-Official builds.
          true;
#else
          net::features::kTcpSocketIoCompletionPortWin.default_state ==
          base::FEATURE_ENABLED_BY_DEFAULT;
#endif

      if (feature_enabled) {
        // Disable TcpSocketIoCompletionPortWin which breaks embedded test
        // servers. See https://crbug.com/40287434#comment36
        disable_features.push_back(
            net::features::kTcpSocketIoCompletionPortWin.name);
      }
    }
#endif  // BUILDFLAG(IS_WIN)

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

  // Call as early as possible.
  InitLogging(command_line);

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

  base::FilePath resources_dir;
  if (command_line->HasSwitch(switches::kResourcesDirPath)) {
    resources_dir =
        command_line->GetSwitchValuePath(switches::kResourcesDirPath);
  }
  if (resources_dir.empty()) {
    resources_dir = resource_util::GetResourcesDir();
  }
  if (!resources_dir.empty()) {
    base::PathService::Override(chrome::DIR_RESOURCES, resources_dir);
  }

  if (command_line->HasSwitch(switches::kLocalesDirPath)) {
    const auto& locales_dir =
        command_line->GetSwitchValuePath(switches::kLocalesDirPath);
    if (!locales_dir.empty()) {
      base::PathService::Override(ui::DIR_LOCALES, locales_dir);
    }
  }

  ChromeMainDelegate::PreSandboxStartup();

  // Initialize crash reporting state for this process/module.
  // chrome::DIR_CRASH_DUMPS must be configured before calling this function.
  crash_reporting::PreSandboxStartup(*command_line, process_type);

#if !BUILDFLAG(IS_WIN)
  // Call after InitLogging() potentially changes values in
  // chrome/app/chrome_main_delegate.cc.
  InitLogging(command_line);
#endif
}

void ChromeMainDelegateCef::SandboxInitialized(
    const std::string& process_type) {
  ChromeMainDelegate::SandboxInitialized(process_type);

#if BUILDFLAG(IS_WIN)
  // Call after InitLogging() potentially changes values in
  // chrome/app/chrome_main_delegate.cc.
  InitLogging(base::CommandLine::ForCurrentProcess());
#endif
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
  // Configure this before ChromeMainDelegate::PostEarlyInitialization
  // triggers ChromeBrowserPolicyConnector creation.
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
  // Match the logic in the parent ChromeMainDelegate implementation, but
  // create our own object type.
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
