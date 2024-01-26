// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/common/alloy/alloy_main_delegate.h"

#include <memory>
#include <tuple>

#include "libcef/browser/alloy/alloy_browser_context.h"
#include "libcef/browser/alloy/alloy_content_browser_client.h"
#include "libcef/common/cef_switches.h"
#include "libcef/common/command_line_impl.h"
#include "libcef/common/crash_reporting.h"
#include "libcef/common/extensions/extensions_util.h"
#include "libcef/common/resource_util.h"
#include "libcef/renderer/alloy/alloy_content_renderer_client.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_process_singleton.h"
#include "chrome/child/pdf_child_init.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_result_codes.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/utility/chrome_content_utility_client.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/embedder_support/switches.h"
#include "components/metrics/persistent_histograms.h"
#include "components/viz/common/features.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/main_function_params.h"
#include "content/public/common/url_constants.h"
#include "extensions/common/constants.h"
#include "net/base/features.h"
#include "sandbox/policy/switches.h"
#include "services/network/public/cpp/features.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/switches.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/resource/scoped_startup_resource_bundle.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_paths.h"
#include "ui/base/ui_base_switches.h"

#if BUILDFLAG(IS_MAC)
#include "components/crash/core/common/objc_zombie.h"
#include "libcef/common/util_mac.h"
#elif BUILDFLAG(IS_POSIX)
#include "libcef/common/util_linux.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "ui/base/resource/resource_bundle_win.h"
#endif

namespace {

const char* const kNonWildcardDomainNonPortSchemes[] = {
    extensions::kExtensionScheme, content::kChromeDevToolsScheme,
    content::kChromeUIScheme, content::kChromeUIUntrustedScheme};
const size_t kNonWildcardDomainNonPortSchemesSize =
    std::size(kNonWildcardDomainNonPortSchemes);

std::optional<int> AcquireProcessSingleton(
    const base::FilePath& user_data_dir) {
  // Take the Chrome process singleton lock. The process can become the
  // Browser process if it succeed to take the lock. Otherwise, the
  // command-line is sent to the actual Browser process and the current
  // process can be exited.
  ChromeProcessSingleton::CreateInstance(user_data_dir);

  ProcessSingleton::NotifyResult notify_result =
      ChromeProcessSingleton::GetInstance()->NotifyOtherProcessOrCreate();
  switch (notify_result) {
    case ProcessSingleton::PROCESS_NONE:
      break;

    case ProcessSingleton::PROCESS_NOTIFIED: {
      // Ensure there is an instance of ResourceBundle that is initialized for
      // localized string resource accesses.
      ui::ScopedStartupResourceBundle startup_resource_bundle;
      printf("%s\n", base::SysWideToNativeMB(
                         base::UTF16ToWide(l10n_util::GetStringUTF16(
                             IDS_USED_EXISTING_BROWSER)))
                         .c_str());
      return chrome::RESULT_CODE_NORMAL_EXIT_PROCESS_NOTIFIED;
    }

    case ProcessSingleton::PROFILE_IN_USE:
      return chrome::RESULT_CODE_PROFILE_IN_USE;

    case ProcessSingleton::LOCK_ERROR:
      LOG(ERROR) << "Failed to create a ProcessSingleton for your profile "
                    "directory. This means that running multiple instances "
                    "would start multiple browser processes rather than "
                    "opening a new window in the existing process. Aborting "
                    "now to avoid profile corruption.";
      return chrome::RESULT_CODE_PROFILE_IN_USE;
  }

  return std::nullopt;
}

}  // namespace

AlloyMainDelegate::AlloyMainDelegate(CefMainRunnerHandler* runner,
                                     CefSettings* settings,
                                     CefRefPtr<CefApp> application)
    : runner_(runner), settings_(settings), application_(application) {
#if BUILDFLAG(IS_LINUX)
  resource_util::OverrideAssetPath();
#endif
}

AlloyMainDelegate::~AlloyMainDelegate() = default;

std::optional<int> AlloyMainDelegate::PreBrowserMain() {
  runner_->PreBrowserMain();
  return std::nullopt;
}

std::optional<int> AlloyMainDelegate::PostEarlyInitialization(
    InvokedIn invoked_in) {
  const auto* invoked_in_browser =
      absl::get_if<InvokedInBrowserProcess>(&invoked_in);
  if (!invoked_in_browser) {
    return std::nullopt;
  }

  // Based on ChromeMainDelegate::PostEarlyInitialization.
  // The User Data dir is guaranteed to be valid as per PreSandboxStartup.
  base::FilePath user_data_dir =
      base::PathService::CheckedGet(chrome::DIR_USER_DATA);

  // On platforms that support the process rendezvous, acquire the process
  // singleton. In case of failure, it means there is already a running browser
  // instance that handled the command-line.
  if (auto process_singleton_result = AcquireProcessSingleton(user_data_dir);
      process_singleton_result.has_value()) {
    // To ensure that the histograms emitted in this process are reported in
    // case of early exit, report the metrics accumulated this session with a
    // future session's metrics.
    DeferBrowserMetrics(user_data_dir);

    return process_singleton_result;
  }

  return std::nullopt;
}

std::optional<int> AlloyMainDelegate::BasicStartupComplete() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  std::string process_type =
      command_line->GetSwitchValueASCII(switches::kProcessType);

#if BUILDFLAG(IS_POSIX)
  // Read the crash configuration file. On Windows this is done from chrome_elf.
  crash_reporting::BasicStartupComplete(command_line);
#endif

  const bool is_browser = process_type.empty();
  if (is_browser) {
    // In the browser process. Populate the global command-line object.
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

    base::FilePath log_file;
    bool has_log_file_cmdline = false;
    if (settings_->log_file.length > 0) {
      log_file = base::FilePath(CefString(&settings_->log_file));
    }
    if (log_file.empty() && command_line->HasSwitch(switches::kLogFile)) {
      log_file = command_line->GetSwitchValuePath(switches::kLogFile);
      if (!log_file.empty()) {
        has_log_file_cmdline = true;
      }
    }
    if (log_file.empty()) {
      log_file = resource_util::GetDefaultLogFilePath();
    }
    DCHECK(!log_file.empty());
    if (!has_log_file_cmdline) {
      command_line->AppendSwitchPath(switches::kLogFile, log_file);
    }

    if (settings_->log_severity != LOGSEVERITY_DEFAULT) {
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

    if (settings_->log_items != LOG_ITEMS_DEFAULT) {
      std::string log_items_str;
      if (settings_->log_items == LOG_ITEMS_NONE) {
        log_items_str = std::string(switches::kLogItems_None);
      } else {
        std::vector<base::StringPiece> added_items;
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

    if (settings_->pack_loading_disabled) {
      command_line->AppendSwitch(switches::kDisablePackLoading);
    } else {
      if (settings_->resources_dir_path.length > 0) {
        base::FilePath file_path =
            base::FilePath(CefString(&settings_->resources_dir_path));
        if (!file_path.empty()) {
          command_line->AppendSwitchPath(switches::kResourcesDirPath,
                                         file_path);
        }
      }

      if (settings_->locales_dir_path.length > 0) {
        base::FilePath file_path =
            base::FilePath(CefString(&settings_->locales_dir_path));
        if (!file_path.empty()) {
          command_line->AppendSwitchPath(switches::kLocalesDirPath, file_path);
        }
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

#if BUILDFLAG(IS_WIN)
    if (features::kCalculateNativeWinOcclusion.default_state ==
        base::FEATURE_ENABLED_BY_DEFAULT) {
      // TODO: Add support for occlusion detection in combination with native
      // parent windows (see issue #2805).
      disable_features.push_back(features::kCalculateNativeWinOcclusion.name);
    }
#endif  // BUILDFLAG(IS_WIN)

    if (features::kBackForwardCache.default_state ==
        base::FEATURE_ENABLED_BY_DEFAULT) {
      // Disable BackForwardCache globally so that
      // blink::RuntimeEnabledFeatures::BackForwardCacheEnabled reports the
      // correct value in the renderer process (see issue #3374).
      disable_features.push_back(features::kBackForwardCache.name);
    }

    if (blink::features::kDocumentPictureInPictureAPI.default_state ==
        base::FEATURE_ENABLED_BY_DEFAULT) {
      // Disable DocumentPictureInPictureAPI globally so that
      // blink::RuntimeEnabledFeatures::DocumentPictureInPictureAPIEnabled
      // reports the correct value in the renderer process (see issue #3448).
      disable_features.push_back(
          blink::features::kDocumentPictureInPictureAPI.name);
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
    application_->OnBeforeCommandLineProcessing(CefString(process_type),
                                                commandLinePtr.get());
    std::ignore = commandLinePtr->Detach(nullptr);
  }

#if BUILDFLAG(IS_MAC)
  // Turns all deallocated Objective-C objects into zombies. Give the browser
  // process a longer treadmill, since crashes there have more impact.
  ObjcEvilDoers::ZombieEnable(true, is_browser ? 10000 : 1000);
#endif

  // Initialize logging.
  logging::LoggingSettings log_settings;

  const base::FilePath& log_file =
      command_line->GetSwitchValuePath(switches::kLogFile);
  DCHECK(!log_file.empty());
  log_settings.log_file_path = log_file.value().c_str();

  log_settings.lock_log = logging::DONT_LOCK_LOG_FILE;
  log_settings.delete_old = logging::APPEND_TO_OLD_LOG_FILE;

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
    log_settings.logging_dest = logging::LOG_NONE;
    // By default, ERROR and FATAL messages will always be output to stderr due
    // to the kAlwaysPrintErrorLevel value in base/logging.cc. We change the log
    // level here so that only FATAL messages are output.
    logging::SetMinLogLevel(logging::LOGGING_FATAL);
  } else {
    log_settings.logging_dest = logging::LOG_TO_ALL;
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

  logging::InitLogging(log_settings);

  ContentSettingsPattern::SetNonWildcardDomainNonPortSchemes(
      kNonWildcardDomainNonPortSchemes, kNonWildcardDomainNonPortSchemesSize);

  content::SetContentClient(&content_client_);

#if BUILDFLAG(IS_MAC)
  util_mac::BasicStartupComplete();
#endif

  return std::nullopt;
}

void AlloyMainDelegate::PreSandboxStartup() {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  const std::string& process_type =
      command_line->GetSwitchValueASCII(switches::kProcessType);

  if (process_type.empty()) {
// Only override these paths when executing the main process.
#if BUILDFLAG(IS_MAC)
    util_mac::PreSandboxStartup();
#elif BUILDFLAG(IS_POSIX)
    util_linux::PreSandboxStartup();
#endif

    resource_util::OverrideDefaultDownloadDir();
  }

  resource_util::OverrideUserDataDir(settings_, command_line);

  if (command_line->HasSwitch(switches::kDisablePackLoading)) {
    resource_bundle_delegate_.set_pack_loading_disabled(true);
  }

  // Initialize crash reporting state for this process/module.
  // chrome::DIR_CRASH_DUMPS must be configured before calling this function.
  crash_reporting::PreSandboxStartup(*command_line, process_type);

  // Register the component_updater PathProvider.
  component_updater::RegisterPathProvider(chrome::DIR_COMPONENTS,
                                          chrome::DIR_INTERNAL_PLUGINS,
                                          chrome::DIR_USER_DATA);

  InitializeResourceBundle();
  MaybePatchGdiGetFontData();
}

absl::variant<int, content::MainFunctionParams> AlloyMainDelegate::RunProcess(
    const std::string& process_type,
    content::MainFunctionParams main_function_params) {
  if (process_type.empty()) {
    return runner_->RunMainProcess(std::move(main_function_params));
  }

  return std::move(main_function_params);
}

void AlloyMainDelegate::ProcessExiting(const std::string& process_type) {
  ui::ResourceBundle::CleanupSharedInstance();
}

#if BUILDFLAG(IS_LINUX)
void AlloyMainDelegate::ZygoteForked() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  const std::string& process_type =
      command_line->GetSwitchValueASCII(switches::kProcessType);
  // Initialize crash reporting state for the newly forked process.
  crash_reporting::ZygoteForked(command_line, process_type);
}
#endif

content::ContentBrowserClient* AlloyMainDelegate::CreateContentBrowserClient() {
  browser_client_ = std::make_unique<AlloyContentBrowserClient>();
  return browser_client_.get();
}

content::ContentRendererClient*
AlloyMainDelegate::CreateContentRendererClient() {
  renderer_client_ = std::make_unique<AlloyContentRendererClient>();
  return renderer_client_.get();
}

content::ContentUtilityClient* AlloyMainDelegate::CreateContentUtilityClient() {
  utility_client_ = std::make_unique<ChromeContentUtilityClient>();
  return utility_client_.get();
}

CefRefPtr<CefRequestContext> AlloyMainDelegate::GetGlobalRequestContext() {
  if (!browser_client_) {
    return nullptr;
  }
  return browser_client_->request_context();
}

CefBrowserContext* AlloyMainDelegate::CreateNewBrowserContext(
    const CefRequestContextSettings& settings,
    base::OnceClosure initialized_cb) {
  auto context = new AlloyBrowserContext(settings);
  context->Initialize();
  std::move(initialized_cb).Run();
  return context;
}

scoped_refptr<base::SingleThreadTaskRunner>
AlloyMainDelegate::GetBackgroundTaskRunner() {
  if (browser_client_) {
    return browser_client_->background_task_runner();
  }
  return nullptr;
}

scoped_refptr<base::SingleThreadTaskRunner>
AlloyMainDelegate::GetUserVisibleTaskRunner() {
  if (browser_client_) {
    return browser_client_->user_visible_task_runner();
  }
  return nullptr;
}

scoped_refptr<base::SingleThreadTaskRunner>
AlloyMainDelegate::GetUserBlockingTaskRunner() {
  if (browser_client_) {
    return browser_client_->user_blocking_task_runner();
  }
  return nullptr;
}

scoped_refptr<base::SingleThreadTaskRunner>
AlloyMainDelegate::GetRenderTaskRunner() {
  if (renderer_client_) {
    return renderer_client_->render_task_runner();
  }
  return nullptr;
}

scoped_refptr<base::SingleThreadTaskRunner>
AlloyMainDelegate::GetWebWorkerTaskRunner() {
  if (renderer_client_) {
    return renderer_client_->GetCurrentTaskRunner();
  }
  return nullptr;
}

void AlloyMainDelegate::InitializeResourceBundle() {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  base::FilePath resources_pak_file, chrome_100_percent_pak_file,
      chrome_200_percent_pak_file, locales_dir;

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

  if (!resource_bundle_delegate_.pack_loading_disabled()) {
    if (!resources_dir.empty()) {
      CHECK(resources_dir.IsAbsolute());
      resources_pak_file =
          resources_dir.Append(FILE_PATH_LITERAL("resources.pak"));
      chrome_100_percent_pak_file =
          resources_dir.Append(FILE_PATH_LITERAL("chrome_100_percent.pak"));
      chrome_200_percent_pak_file =
          resources_dir.Append(FILE_PATH_LITERAL("chrome_200_percent.pak"));
    }

    if (command_line->HasSwitch(switches::kLocalesDirPath)) {
      locales_dir = command_line->GetSwitchValuePath(switches::kLocalesDirPath);
    }

    if (!locales_dir.empty()) {
      base::PathService::Override(ui::DIR_LOCALES, locales_dir);
    }
  }

#if BUILDFLAG(IS_WIN)
  // From chrome/app/chrome_main_delegate.cc
  // Throbber icons and cursors are still stored in chrome.dll,
  // this can be killed once those are merged into resources.pak. See
  // GlassBrowserFrameView::InitThrobberIcons(), https://crbug.com/368327 and
  // https://crbug.com/1178117.
  auto module_handle =
      ::GetModuleHandle(CefAppManager::Get()->GetResourceDllName());
  if (!module_handle) {
    module_handle = ::GetModuleHandle(nullptr);
  }

  ui::SetResourcesDataDLL(module_handle);
#endif

  std::string locale = command_line->GetSwitchValueASCII(switches::kLang);
  DCHECK(!locale.empty());

  const std::string loaded_locale =
      ui::ResourceBundle::InitSharedInstanceWithLocale(
          locale, &resource_bundle_delegate_,
          ui::ResourceBundle::LOAD_COMMON_RESOURCES);
  if (!loaded_locale.empty() && g_browser_process) {
    g_browser_process->SetApplicationLocale(loaded_locale);
  }

  ui::ResourceBundle& resource_bundle = ui::ResourceBundle::GetSharedInstance();

  if (!resource_bundle_delegate_.pack_loading_disabled()) {
    if (loaded_locale.empty()) {
      LOG(ERROR) << "Could not load locale pak for " << locale;
    }

    resource_bundle_delegate_.set_allow_pack_file_load(true);

    if (base::PathExists(resources_pak_file)) {
      resource_bundle.AddDataPackFromPath(resources_pak_file,
                                          ui::kScaleFactorNone);
    } else {
      LOG(ERROR) << "Could not load resources.pak";
    }

    // Always load the 1x data pack first as the 2x data pack contains both 1x
    // and 2x images. The 1x data pack only has 1x images, thus passes in an
    // accurate scale factor to gfx::ImageSkia::AddRepresentation.
    if (resource_util::IsScaleFactorSupported(ui::k100Percent)) {
      if (base::PathExists(chrome_100_percent_pak_file)) {
        resource_bundle.AddDataPackFromPath(chrome_100_percent_pak_file,
                                            ui::k100Percent);
      } else {
        LOG(ERROR) << "Could not load chrome_100_percent.pak";
      }
    }

    if (resource_util::IsScaleFactorSupported(ui::k200Percent)) {
      if (base::PathExists(chrome_200_percent_pak_file)) {
        resource_bundle.AddDataPackFromPath(chrome_200_percent_pak_file,
                                            ui::k200Percent);
      } else {
        LOG(ERROR) << "Could not load chrome_200_percent.pak";
      }
    }

    // Skip the default pak file loading that would otherwise occur in
    // ResourceBundle::LoadChromeResources().
    resource_bundle_delegate_.set_allow_pack_file_load(false);
  }
}
