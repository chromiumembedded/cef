// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/common/main_delegate.h"
#include "libcef/browser/content_browser_client.h"
#include "libcef/browser/context.h"
#include "libcef/common/cef_switches.h"
#include "libcef/common/command_line_impl.h"
#include "libcef/common/crash_reporting.h"
#include "libcef/common/extensions/extensions_util.h"
#include "libcef/renderer/content_renderer_client.h"
#include "libcef/utility/content_utility_client.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "chrome/browser/browser_process.h"
#include "chrome/child/pdf_child_init.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "content/public/browser/browser_main_runner.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/main_function_params.h"
#include "extensions/common/constants.h"
#include "pdf/pdf.h"
#include "ui/base/layout.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "ui/base/ui_base_switches.h"

#include "ipc/ipc_message.h"  // For IPC_MESSAGE_LOG_ENABLED.

#if defined(IPC_MESSAGE_LOG_ENABLED)
#define IPC_MESSAGE_MACROS_LOG_ENABLED
#include "content/public/common/content_ipc_logging.h"
#define IPC_LOG_TABLE_ADD_ENTRY(msg_id, logger) \
    content::RegisterIPCLogger(msg_id, logger)
#include "libcef/common/cef_message_generator.h"
#endif

#if defined(OS_WIN)
#include <Objbase.h>  // NOLINT(build/include_order)
#include "base/win/registry.h"
#endif

#if defined(OS_MACOSX)
#include "libcef/common/util_mac.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/foundation_util.h"
#include "content/public/common/content_paths.h"
#endif

#if defined(OS_LINUX)
#include "base/environment.h"
#include "base/nix/xdg_util.h"
#endif

namespace {

#if defined(OS_MACOSX)

base::FilePath GetResourcesFilePath() {
  return util_mac::GetFrameworkResourcesDirectory();
}

// Use a "~/Library/Logs/<app name>_debug.log" file where <app name> is the name
// of the running executable.
base::FilePath GetDefaultLogFile() {
  std::string exe_name = util_mac::GetMainProcessPath().BaseName().value();
  return base::mac::GetUserLibraryPath()
      .Append(FILE_PATH_LITERAL("Logs"))
      .Append(FILE_PATH_LITERAL(exe_name + "_debug.log"));
}

void OverrideFrameworkBundlePath() {
  base::FilePath framework_path = util_mac::GetFrameworkDirectory();
  DCHECK(!framework_path.empty());

  base::mac::SetOverrideFrameworkBundlePath(framework_path);
}

void OverrideChildProcessPath() {
  base::FilePath child_process_path =
      base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
          switches::kBrowserSubprocessPath);

  if (child_process_path.empty()) {
    child_process_path = util_mac::GetChildProcessPath();
    DCHECK(!child_process_path.empty());
  }

  // Used by ChildProcessHost::GetChildPath and PlatformCrashpadInitialization.
  PathService::Override(content::CHILD_PROCESS_EXE, child_process_path);
}

#else  // !defined(OS_MACOSX)

base::FilePath GetResourcesFilePath() {
  base::FilePath pak_dir;
  PathService::Get(base::DIR_MODULE, &pak_dir);
  return pak_dir;
}

// Use a "debug.log" file in the running executable's directory.
base::FilePath GetDefaultLogFile() {
  base::FilePath log_path;
  PathService::Get(base::DIR_EXE, &log_path);
  return log_path.Append(FILE_PATH_LITERAL("debug.log"));
}

#endif  // !defined(OS_MACOSX)

#if defined(OS_WIN)

// Gets the Flash path if installed on the system.
bool GetSystemFlashFilename(base::FilePath* out_path) {
  const wchar_t kPepperFlashRegistryRoot[] =
      L"SOFTWARE\\Macromedia\\FlashPlayerPepper";
  const wchar_t kFlashPlayerPathValueName[] = L"PlayerPath";

  base::win::RegKey path_key(
      HKEY_LOCAL_MACHINE, kPepperFlashRegistryRoot, KEY_READ);
  base::string16 path_str;
  if (FAILED(path_key.ReadValue(kFlashPlayerPathValueName, &path_str)))
    return false;

  *out_path = base::FilePath(path_str);
  return true;
}

#elif defined(OS_MACOSX)

const base::FilePath::CharType kPepperFlashSystemBaseDirectory[] =
    FILE_PATH_LITERAL("Internet Plug-Ins/PepperFlashPlayer");

#endif

void OverridePepperFlashSystemPluginPath() {
  base::FilePath plugin_filename;
#if defined(OS_WIN)
  if (!GetSystemFlashFilename(&plugin_filename))
    return;
#elif defined(OS_MACOSX)
  if (!util_mac::GetLocalLibraryDirectory(&plugin_filename))
    return;
  plugin_filename = plugin_filename.Append(kPepperFlashSystemBaseDirectory)
                                   .Append(chrome::kPepperFlashPluginFilename);
#else
  // A system plugin is not available on other platforms.
  return;
#endif

  if (!plugin_filename.empty()) {
    PathService::Override(chrome::FILE_PEPPER_FLASH_SYSTEM_PLUGIN,
                          plugin_filename);
  }
}

#if defined(OS_LINUX)

// Based on chrome/common/chrome_paths_linux.cc.
// See http://standards.freedesktop.org/basedir-spec/basedir-spec-latest.html
// for a spec on where config files go.  The net effect for most
// systems is we use ~/.config/chromium/ for Chromium and
// ~/.config/google-chrome/ for official builds.
// (This also helps us sidestep issues with other apps grabbing ~/.chromium .)
bool GetDefaultUserDataDirectory(base::FilePath* result) {
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  base::FilePath config_dir(
      base::nix::GetXDGDirectory(env.get(),
                                 base::nix::kXdgConfigHomeEnvVar,
                                 base::nix::kDotConfigDir));
  *result = config_dir.Append(FILE_PATH_LITERAL("cef_user_data"));
  return true;
}

#elif defined(OS_MACOSX)

// Based on chrome/common/chrome_paths_mac.mm.
bool GetDefaultUserDataDirectory(base::FilePath* result) {
  if (!PathService::Get(base::DIR_APP_DATA, result))
    return false;
  *result = result->Append(FILE_PATH_LITERAL("CEF"));
  *result = result->Append(FILE_PATH_LITERAL("User Data"));
  return true;
}

#elif defined(OS_WIN)

// Based on chrome/common/chrome_paths_win.cc.
bool GetDefaultUserDataDirectory(base::FilePath* result) {
  if (!PathService::Get(base::DIR_LOCAL_APP_DATA, result))
    return false;
  *result = result->Append(FILE_PATH_LITERAL("CEF"));
  *result = result->Append(FILE_PATH_LITERAL("User Data"));
  return true;
}

#endif

base::FilePath GetUserDataPath() {
  const CefSettings& settings = CefContext::Get()->settings();
  if (settings.user_data_path.length > 0)
    return base::FilePath(CefString(&settings.user_data_path));

  base::FilePath result;
  if (GetDefaultUserDataDirectory(&result))
    return result;

  if (PathService::Get(base::DIR_TEMP, &result))
    return result;

  NOTREACHED();
  return result;
}

// Returns true if |scale_factor| is supported by this platform.
// Same as ResourceBundle::IsScaleFactorSupported.
bool IsScaleFactorSupported(ui::ScaleFactor scale_factor) {
  const std::vector<ui::ScaleFactor>& supported_scale_factors =
      ui::GetSupportedScaleFactors();
  return std::find(supported_scale_factors.begin(),
                   supported_scale_factors.end(),
                   scale_factor) != supported_scale_factors.end();
}

// Used to run the UI on a separate thread.
class CefUIThread : public base::Thread {
 public:
  explicit CefUIThread(const content::MainFunctionParams& main_function_params)
    : base::Thread("CefUIThread"),
      main_function_params_(main_function_params) {
  }

  void Init() override {
#if defined(OS_WIN)
    // Initializes the COM library on the current thread.
    CoInitialize(NULL);
#endif

    // Use our own browser process runner.
    browser_runner_.reset(content::BrowserMainRunner::Create());

    // Initialize browser process state. Uses the current thread's mesage loop.
    int exit_code = browser_runner_->Initialize(main_function_params_);
    CHECK_EQ(exit_code, -1);
  }

  void CleanUp() override {
    browser_runner_->Shutdown();
    browser_runner_.reset(NULL);

#if defined(OS_WIN)
    // Closes the COM library on the current thread. CoInitialize must
    // be balanced by a corresponding call to CoUninitialize.
    CoUninitialize();
#endif
  }

 protected:
  content::MainFunctionParams main_function_params_;
  std::unique_ptr<content::BrowserMainRunner> browser_runner_;
};

}  // namespace

CefMainDelegate::CefMainDelegate(CefRefPtr<CefApp> application)
    : content_client_(application) {
  // Necessary so that exported functions from base_impl.cc will be included
  // in the binary.
  extern void base_impl_stub();
  base_impl_stub();
}

CefMainDelegate::~CefMainDelegate() {
}

bool CefMainDelegate::BasicStartupComplete(int* exit_code) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  std::string process_type =
      command_line->GetSwitchValueASCII(switches::kProcessType);

#if defined(OS_POSIX)
  // Read the crash configuration file. Platforms using Breakpad also add a
  // command-line switch. On Windows this is done from chrome_elf.
  crash_reporting::BasicStartupComplete(command_line);
#endif

  if (process_type.empty()) {
    // In the browser process. Populate the global command-line object.
    const CefSettings& settings = CefContext::Get()->settings();

    if (settings.command_line_args_disabled) {
      // Remove any existing command-line arguments.
      base::CommandLine::StringVector argv;
      argv.push_back(command_line->GetProgram().value());
      command_line->InitFromArgv(argv);

      const base::CommandLine::SwitchMap& map = command_line->GetSwitches();
      const_cast<base::CommandLine::SwitchMap*>(&map)->clear();
    }

    if (settings.single_process)
      command_line->AppendSwitch(switches::kSingleProcess);

    bool no_sandbox = settings.no_sandbox ? true : false;

    if (settings.browser_subprocess_path.length > 0) {
      base::FilePath file_path =
          base::FilePath(CefString(&settings.browser_subprocess_path));
      if (!file_path.empty()) {
        command_line->AppendSwitchPath(switches::kBrowserSubprocessPath,
                                       file_path);

#if defined(OS_WIN)
        // The sandbox is not supported when using a separate subprocess
        // executable on Windows.
        no_sandbox = true;
#endif
      }
    }

#if defined(OS_MACOSX)
    if (settings.framework_dir_path.length > 0) {
      base::FilePath file_path =
          base::FilePath(CefString(&settings.framework_dir_path));
      if (!file_path.empty())
        command_line->AppendSwitchPath(switches::kFrameworkDirPath, file_path);
    }
#endif

    if (no_sandbox)
      command_line->AppendSwitch(switches::kNoSandbox);

    if (settings.user_agent.length > 0) {
      command_line->AppendSwitchASCII(switches::kUserAgent,
          CefString(&settings.user_agent));
    } else if (settings.product_version.length > 0) {
      command_line->AppendSwitchASCII(switches::kProductVersion,
          CefString(&settings.product_version));
    }

    if (settings.locale.length > 0) {
      command_line->AppendSwitchASCII(switches::kLang,
          CefString(&settings.locale));
    } else if (!command_line->HasSwitch(switches::kLang)) {
      command_line->AppendSwitchASCII(switches::kLang, "en-US");
    }

    base::FilePath log_file;
    bool has_log_file_cmdline = false;
    if (settings.log_file.length > 0)
      log_file = base::FilePath(CefString(&settings.log_file));
    if (log_file.empty() && command_line->HasSwitch(switches::kLogFile)) {
      log_file = command_line->GetSwitchValuePath(switches::kLogFile);
      if (!log_file.empty())
        has_log_file_cmdline = true;
    }
    if (log_file.empty())
      log_file = GetDefaultLogFile();
    DCHECK(!log_file.empty());
    if (!has_log_file_cmdline)
      command_line->AppendSwitchPath(switches::kLogFile, log_file);

    if (settings.log_severity != LOGSEVERITY_DEFAULT) {
      std::string log_severity;
      switch (settings.log_severity) {
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
        case LOGSEVERITY_DISABLE:
          log_severity = switches::kLogSeverity_Disable;
          break;
        default:
          break;
      }
      if (!log_severity.empty())
        command_line->AppendSwitchASCII(switches::kLogSeverity, log_severity);
    }

    if (settings.javascript_flags.length > 0) {
      command_line->AppendSwitchASCII(switches::kJavaScriptFlags,
          CefString(&settings.javascript_flags));
    }

    if (settings.pack_loading_disabled) {
      command_line->AppendSwitch(switches::kDisablePackLoading);
    } else {
      if (settings.resources_dir_path.length > 0) {
        base::FilePath file_path =
            base::FilePath(CefString(&settings.resources_dir_path));
        if (!file_path.empty()) {
          command_line->AppendSwitchPath(switches::kResourcesDirPath,
                                         file_path);
        }
      }

      if (settings.locales_dir_path.length > 0) {
        base::FilePath file_path =
            base::FilePath(CefString(&settings.locales_dir_path));
        if (!file_path.empty())
          command_line->AppendSwitchPath(switches::kLocalesDirPath, file_path);
      }
    }

    if (settings.remote_debugging_port >= 1024 &&
        settings.remote_debugging_port <= 65535) {
      command_line->AppendSwitchASCII(switches::kRemoteDebuggingPort,
          base::IntToString(settings.remote_debugging_port));
    }

    if (settings.uncaught_exception_stack_size > 0) {
      command_line->AppendSwitchASCII(switches::kUncaughtExceptionStackSize,
        base::IntToString(settings.uncaught_exception_stack_size));
    }

    if (settings.context_safety_implementation != 0) {
      command_line->AppendSwitchASCII(switches::kContextSafetyImplementation,
          base::IntToString(settings.context_safety_implementation));
    }
  }

  if (content_client_.application().get()) {
    // Give the application a chance to view/modify the command line.
    CefRefPtr<CefCommandLineImpl> commandLinePtr(
        new CefCommandLineImpl(command_line, false, false));
    content_client_.application()->OnBeforeCommandLineProcessing(
        CefString(process_type), commandLinePtr.get());
    commandLinePtr->Detach(NULL);
  }

  // Initialize logging.
  logging::LoggingSettings log_settings;

  const base::FilePath& log_file =
      command_line->GetSwitchValuePath(switches::kLogFile);
  DCHECK(!log_file.empty());
  log_settings.log_file = log_file.value().c_str();

  log_settings.lock_log = logging::DONT_LOCK_LOG_FILE;
  log_settings.delete_old = logging::APPEND_TO_OLD_LOG_FILE;

  logging::LogSeverity log_severity = logging::LOG_INFO;

  std::string log_severity_str =
      command_line->GetSwitchValueASCII(switches::kLogSeverity);
  if (!log_severity_str.empty()) {
    if (base::LowerCaseEqualsASCII(log_severity_str,
                                   switches::kLogSeverity_Verbose)) {
      log_severity = logging::LOG_VERBOSE;
    } else if (base::LowerCaseEqualsASCII(log_severity_str,
                                          switches::kLogSeverity_Warning)) {
      log_severity = logging::LOG_WARNING;
    } else if (base::LowerCaseEqualsASCII(log_severity_str,
                                          switches::kLogSeverity_Error)) {
      log_severity = logging::LOG_ERROR;
    } else if (base::LowerCaseEqualsASCII(log_severity_str,
                                          switches::kLogSeverity_Disable)) {
      log_severity = LOGSEVERITY_DISABLE;
    }
  }

  if (log_severity == LOGSEVERITY_DISABLE) {
    log_settings.logging_dest = logging::LOG_NONE;
  } else {
    log_settings.logging_dest = logging::LOG_TO_ALL;
    logging::SetMinLogLevel(log_severity);
  }

  logging::InitLogging(log_settings);

  ContentSettingsPattern::SetNonWildcardDomainNonPortScheme(
      extensions::kExtensionScheme);

  content::SetContentClient(&content_client_);

#if defined(OS_MACOSX)
  OverrideFrameworkBundlePath();
#endif

  return false;
}

void CefMainDelegate::PreSandboxStartup() {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  const std::string& process_type =
      command_line->GetSwitchValueASCII(switches::kProcessType);

  if (process_type.empty()) {
    // Only override these paths when executing the main process.
#if defined(OS_MACOSX)
    OverrideChildProcessPath();
#endif

    OverridePepperFlashSystemPluginPath();

    const base::FilePath& user_data_path = GetUserDataPath();
    PathService::Override(chrome::DIR_USER_DATA, user_data_path);

    // Path used for crash dumps.
    PathService::Override(chrome::DIR_CRASH_DUMPS, user_data_path);

    // Path used for spell checking dictionary files.
    PathService::OverrideAndCreateIfNeeded(
        chrome::DIR_APP_DICTIONARIES,
        user_data_path.AppendASCII("Dictionaries"),
        false,  // May not be an absolute path.
        true);  // Create if necessary.
  }

  if (command_line->HasSwitch(switches::kDisablePackLoading))
    content_client_.set_pack_loading_disabled(true);

  // Initialize crash reporting state for this process/module.
  // chrome::DIR_CRASH_DUMPS must be configured before calling this function.
  crash_reporting::PreSandboxStartup(*command_line, process_type);

  InitializeResourceBundle();
  chrome::InitializePDF();
}

void CefMainDelegate::SandboxInitialized(const std::string& process_type) {
  CefContentClient::SetPDFEntryFunctions(
      chrome_pdf::PPP_GetInterface,
      chrome_pdf::PPP_InitializeModule,
      chrome_pdf::PPP_ShutdownModule);
}

int CefMainDelegate::RunProcess(
    const std::string& process_type,
    const content::MainFunctionParams& main_function_params) {
  if (process_type.empty()) {
    const CefSettings& settings = CefContext::Get()->settings();
    if (!settings.multi_threaded_message_loop) {
      // Use our own browser process runner.
      browser_runner_.reset(content::BrowserMainRunner::Create());

      // Initialize browser process state. Results in a call to
      // CefBrowserMain::PreMainMessageLoopStart() which creates the UI message
      // loop.
      int exit_code = browser_runner_->Initialize(main_function_params);
      if (exit_code >= 0)
        return exit_code;
    } else {
      // Run the UI on a separate thread.
      std::unique_ptr<base::Thread> thread;
      thread.reset(new CefUIThread(main_function_params));
      base::Thread::Options options;
      options.message_loop_type = base::MessageLoop::TYPE_UI;
      if (!thread->StartWithOptions(options)) {
        NOTREACHED() << "failed to start UI thread";
        return 1;
      }
      thread->WaitUntilThreadStarted();
      ui_thread_.swap(thread);
    }

    return 0;
  }

  return -1;
}

void CefMainDelegate::ProcessExiting(const std::string& process_type) {
  ResourceBundle::CleanupSharedInstance();
}

#if defined(OS_POSIX) && !defined(OS_ANDROID) && !defined(OS_MACOSX)
void CefMainDelegate::ZygoteForked() {
  base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  const std::string& process_type = command_line->GetSwitchValueASCII(
        switches::kProcessType);
  // Initialize crash reporting state for the newly forked process.
  crash_reporting::ZygoteForked(command_line, process_type);
}
#endif

content::ContentBrowserClient* CefMainDelegate::CreateContentBrowserClient() {
  browser_client_.reset(new CefContentBrowserClient);
  return browser_client_.get();
}

content::ContentRendererClient*
    CefMainDelegate::CreateContentRendererClient() {
  renderer_client_.reset(new CefContentRendererClient);
  return renderer_client_.get();
}

content::ContentUtilityClient* CefMainDelegate::CreateContentUtilityClient() {
  utility_client_.reset(new CefContentUtilityClient);
  return utility_client_.get();
}

void CefMainDelegate::ShutdownBrowser() {
  if (browser_runner_.get()) {
    browser_runner_->Shutdown();
    browser_runner_.reset(NULL);
  }
  if (ui_thread_.get()) {
    // Blocks until the thread has stopped.
    ui_thread_->Stop();
    ui_thread_.reset();
  }
}

void CefMainDelegate::InitializeResourceBundle() {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  base::FilePath cef_pak_file, cef_100_percent_pak_file,
                 cef_200_percent_pak_file, cef_extensions_pak_file,
                 devtools_pak_file, locales_dir;

  base::FilePath resources_dir;
  if (command_line->HasSwitch(switches::kResourcesDirPath)) {
    resources_dir =
        command_line->GetSwitchValuePath(switches::kResourcesDirPath);
  }
  if (resources_dir.empty())
    resources_dir = GetResourcesFilePath();
  if (!resources_dir.empty())
    PathService::Override(chrome::DIR_RESOURCES, resources_dir);

  if (!content_client_.pack_loading_disabled()) {
    if (!resources_dir.empty()) {
      CHECK(resources_dir.IsAbsolute());
      cef_pak_file = resources_dir.Append(FILE_PATH_LITERAL("cef.pak"));
      cef_100_percent_pak_file =
          resources_dir.Append(FILE_PATH_LITERAL("cef_100_percent.pak"));
      cef_200_percent_pak_file =
          resources_dir.Append(FILE_PATH_LITERAL("cef_200_percent.pak"));
      cef_extensions_pak_file =
          resources_dir.Append(FILE_PATH_LITERAL("cef_extensions.pak"));
      devtools_pak_file =
          resources_dir.Append(FILE_PATH_LITERAL("devtools_resources.pak"));
    }

    if (command_line->HasSwitch(switches::kLocalesDirPath))
      locales_dir = command_line->GetSwitchValuePath(switches::kLocalesDirPath);

    if (!locales_dir.empty())
      PathService::Override(ui::DIR_LOCALES, locales_dir);
  }

  std::string locale = command_line->GetSwitchValueASCII(switches::kLang);
  DCHECK(!locale.empty());

  // Avoid DCHECK() in ResourceBundle::LoadChromeResources().
  ui::MaterialDesignController::Initialize();

  const std::string loaded_locale =
      ui::ResourceBundle::InitSharedInstanceWithLocale(
          locale,
          &content_client_,
          ui::ResourceBundle::LOAD_COMMON_RESOURCES);
  if (!loaded_locale.empty() && g_browser_process)
    g_browser_process->SetApplicationLocale(loaded_locale);

  ResourceBundle& resource_bundle = ResourceBundle::GetSharedInstance();

  if (!content_client_.pack_loading_disabled()) {
    if (loaded_locale.empty())
      LOG(ERROR) << "Could not load locale pak for " << locale;

    content_client_.set_allow_pack_file_load(true);

    if (base::PathExists(cef_pak_file)) {
      resource_bundle.AddDataPackFromPath(cef_pak_file, ui::SCALE_FACTOR_NONE);
    } else {
      LOG(ERROR) << "Could not load cef.pak";
    }

    // On OS X and Linux/Aura always load the 1x data pack first as the 2x data
    // pack contains both 1x and 2x images.
    const bool load_100_percent =
#if defined(OS_WIN)
        IsScaleFactorSupported(ui::SCALE_FACTOR_100P);
#else
        true;
#endif

    if (load_100_percent) {
      if (base::PathExists(cef_100_percent_pak_file)) {
        resource_bundle.AddDataPackFromPath(
            cef_100_percent_pak_file, ui::SCALE_FACTOR_100P);
      } else {
        LOG(ERROR) << "Could not load cef_100_percent.pak";
      }
    }

    if (IsScaleFactorSupported(ui::SCALE_FACTOR_200P)) {
      if (base::PathExists(cef_200_percent_pak_file)) {
        resource_bundle.AddDataPackFromPath(
            cef_200_percent_pak_file, ui::SCALE_FACTOR_200P);
      } else {
        LOG(ERROR) << "Could not load cef_200_percent.pak";
      }
    }

    if (extensions::ExtensionsEnabled()) {
      if (base::PathExists(cef_extensions_pak_file)) {
        resource_bundle.AddDataPackFromPath(
            cef_extensions_pak_file, ui::SCALE_FACTOR_NONE);
      } else {
        LOG(ERROR) << "Could not load cef_extensions.pak";
      }
    }

    if (base::PathExists(devtools_pak_file)) {
      resource_bundle.AddDataPackFromPath(
          devtools_pak_file, ui::SCALE_FACTOR_NONE);
    }

    content_client_.set_allow_pack_file_load(false);
  }
}
