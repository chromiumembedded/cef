// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/common/main_delegate.h"

#if defined(OS_LINUX)
#include <dlfcn.h>
#endif

#include "libcef/browser/browser_message_loop.h"
#include "libcef/browser/content_browser_client.h"
#include "libcef/browser/context.h"
#include "libcef/common/cef_switches.h"
#include "libcef/common/command_line_impl.h"
#include "libcef/common/crash_reporting.h"
#include "libcef/common/extensions/extensions_util.h"
#include "libcef/renderer/content_renderer_client.h"
#include "libcef/utility/content_utility_client.h"

#include "base/at_exit.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "chrome/browser/browser_process.h"
#include "chrome/child/pdf_child_init.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_switches.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/viz/common/features.h"
#include "content/browser/browser_process_sub_thread.h"
#include "content/browser/scheduler/browser_task_executor.h"
#include "content/public/browser/browser_main_runner.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/main_function_params.h"
#include "extensions/common/constants.h"
#include "ipc/ipc_buildflags.h"
#include "pdf/pdf_ppapi.h"
#include "services/network/public/cpp/features.h"
#include "services/service_manager/sandbox/switches.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "ui/base/ui_base_switches.h"

#if BUILDFLAG(IPC_MESSAGE_LOG_ENABLED)
#define IPC_MESSAGE_MACROS_LOG_ENABLED
#include "content/public/common/content_ipc_logging.h"
#define IPC_LOG_TABLE_ADD_ENTRY(msg_id, logger) \
  content::RegisterIPCLogger(msg_id, logger)
#include "libcef/common/cef_message_generator.h"
#endif

#if defined(OS_WIN)
#include <Objbase.h>
#include "base/win/registry.h"
#endif

#if defined(OS_MACOSX)
#include "base/mac/bundle_locations.h"
#include "base/mac/foundation_util.h"
#include "content/public/common/content_paths.h"
#include "libcef/common/util_mac.h"
#endif

#if defined(OS_LINUX)
#include "base/environment.h"
#include "base/nix/xdg_util.h"
#endif

namespace {

const char* const kNonWildcardDomainNonPortSchemes[] = {
    extensions::kExtensionScheme};
const size_t kNonWildcardDomainNonPortSchemesSize =
    base::size(kNonWildcardDomainNonPortSchemes);

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

void OverrideOuterBundlePath() {
  base::FilePath bundle_path = util_mac::GetMainBundlePath();
  DCHECK(!bundle_path.empty());

  base::mac::SetOverrideOuterBundlePath(bundle_path);
}

void OverrideBaseBundleID() {
  std::string bundle_id = util_mac::GetMainBundleID();
  DCHECK(!bundle_id.empty());

  base::mac::SetBaseBundleID(bundle_id.c_str());
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
  base::PathService::Override(content::CHILD_PROCESS_EXE, child_process_path);
}

#else  // !defined(OS_MACOSX)

base::FilePath GetResourcesFilePath() {
  base::FilePath pak_dir;
  base::PathService::Get(base::DIR_ASSETS, &pak_dir);
  return pak_dir;
}

// Use a "debug.log" file in the running executable's directory.
base::FilePath GetDefaultLogFile() {
  base::FilePath log_path;
  base::PathService::Get(base::DIR_EXE, &log_path);
  return log_path.Append(FILE_PATH_LITERAL("debug.log"));
}

#endif  // !defined(OS_MACOSX)

#if defined(OS_WIN)

// Gets the Flash path if installed on the system.
bool GetSystemFlashFilename(base::FilePath* out_path) {
  const wchar_t kPepperFlashRegistryRoot[] =
      L"SOFTWARE\\Macromedia\\FlashPlayerPepper";
  const wchar_t kFlashPlayerPathValueName[] = L"PlayerPath";

  base::win::RegKey path_key(HKEY_LOCAL_MACHINE, kPepperFlashRegistryRoot,
                             KEY_READ);
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
    base::PathService::Override(chrome::FILE_PEPPER_FLASH_SYSTEM_PLUGIN,
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
  base::FilePath config_dir(base::nix::GetXDGDirectory(
      env.get(), base::nix::kXdgConfigHomeEnvVar, base::nix::kDotConfigDir));
  *result = config_dir.Append(FILE_PATH_LITERAL("cef_user_data"));
  return true;
}

#elif defined(OS_MACOSX)

// Based on chrome/common/chrome_paths_mac.mm.
bool GetDefaultUserDataDirectory(base::FilePath* result) {
  if (!base::PathService::Get(base::DIR_APP_DATA, result))
    return false;
  *result = result->Append(FILE_PATH_LITERAL("CEF"));
  *result = result->Append(FILE_PATH_LITERAL("User Data"));
  return true;
}

#elif defined(OS_WIN)

// Based on chrome/common/chrome_paths_win.cc.
bool GetDefaultUserDataDirectory(base::FilePath* result) {
  if (!base::PathService::Get(base::DIR_LOCAL_APP_DATA, result))
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

  if (base::PathService::Get(base::DIR_TEMP, &result))
    return result;

  NOTREACHED();
  return result;
}

bool GetDefaultDownloadDirectory(base::FilePath* result) {
  base::FilePath cur;
  if (!chrome::GetUserDownloadsDirectory(&cur))
    return false;
  *result = cur;
  return true;
}

bool GetDefaultDownloadSafeDirectory(base::FilePath* result) {
  base::FilePath cur;
#if defined(OS_WIN) || defined(OS_LINUX)
  if (!chrome::GetUserDownloadsDirectorySafe(&cur))
    return false;
#else
  GetDefaultDownloadDirectory(&cur);
#endif
  *result = cur;
  return true;
}

// Returns true if |scale_factor| is supported by this platform.
// Same as ui::ResourceBundle::IsScaleFactorSupported.
bool IsScaleFactorSupported(ui::ScaleFactor scale_factor) {
  const std::vector<ui::ScaleFactor>& supported_scale_factors =
      ui::GetSupportedScaleFactors();
  return std::find(supported_scale_factors.begin(),
                   supported_scale_factors.end(),
                   scale_factor) != supported_scale_factors.end();
}

#if defined(OS_LINUX)
// Look for binary files (*.bin, *.dat, *.pak, chrome-sandbox, libGLESv2.so,
// libEGL.so, locales/*.pak, swiftshader/*.so) next to libcef instead of the exe
// on Linux. This is already the default on Windows.
void OverrideAssetPath() {
  Dl_info dl_info;
  if (dladdr(reinterpret_cast<const void*>(&OverrideAssetPath), &dl_info)) {
    base::FilePath path = base::FilePath(dl_info.dli_fname).DirName();
    base::PathService::Override(base::DIR_ASSETS, path);
  }
}
#endif

}  // namespace

// Used to run the UI on a separate thread.
class CefUIThread : public base::PlatformThread::Delegate {
 public:
  explicit CefUIThread(base::OnceClosure setup_callback)
      : setup_callback_(std::move(setup_callback)) {}
  ~CefUIThread() override { Stop(); }

  void Start() {
    base::AutoLock lock(thread_lock_);
    bool success = base::PlatformThread::CreateWithPriority(
        0, this, &thread_, base::ThreadPriority::NORMAL);
    if (!success) {
      LOG(FATAL) << "failed to UI create thread";
    }
  }

  void Stop() {
    base::AutoLock lock(thread_lock_);

    if (!stopping_) {
      stopping_ = true;
      base::PostTaskWithTraits(
          FROM_HERE, {content::BrowserThread::UI},
          base::BindOnce(&CefUIThread::ThreadQuitHelper, Unretained(this)));
    }

    // Can't join if the |thread_| is either already gone or is non-joinable.
    if (thread_.is_null())
      return;

    base::PlatformThread::Join(thread_);
    thread_ = base::PlatformThreadHandle();

    stopping_ = false;
  }

  bool WaitUntilThreadStarted() const {
    DCHECK(owning_sequence_checker_.CalledOnValidSequence());
    start_event_.Wait();
    return true;
  }

  void InitializeBrowserRunner(
      const content::MainFunctionParams& main_function_params) {
    // Use our own browser process runner.
    browser_runner_ = content::BrowserMainRunner::Create();

    // Initialize browser process state. Uses the current thread's message loop.
    int exit_code = browser_runner_->Initialize(main_function_params);
    CHECK_EQ(exit_code, -1);
  }

 protected:
  void ThreadMain() override {
    base::PlatformThread::SetName("CefUIThread");

#if defined(OS_WIN)
    // Initializes the COM library on the current thread.
    CoInitialize(NULL);
#endif

    start_event_.Signal();

    std::move(setup_callback_).Run();

    base::RunLoop run_loop;
    run_loop_ = &run_loop;
    run_loop.Run();

    browser_runner_->Shutdown();
    browser_runner_.reset(NULL);

    content::BrowserTaskExecutor::Shutdown();

    // Run exit callbacks on the UI thread to avoid sequence check failures.
    base::AtExitManager::ProcessCallbacksNow();

#if defined(OS_WIN)
    // Closes the COM library on the current thread. CoInitialize must
    // be balanced by a corresponding call to CoUninitialize.
    CoUninitialize();
#endif

    run_loop_ = nullptr;
  }

  void ThreadQuitHelper() {
    DCHECK(run_loop_);
    run_loop_->QuitWhenIdle();
  }

  std::unique_ptr<content::BrowserMainRunner> browser_runner_;
  base::OnceClosure setup_callback_;

  bool stopping_ = false;

  // The thread's handle.
  base::PlatformThreadHandle thread_;
  mutable base::Lock thread_lock_;  // Protects |thread_|.

  base::RunLoop* run_loop_ = nullptr;

  mutable base::WaitableEvent start_event_;

  // This class is not thread-safe, use this to verify access from the owning
  // sequence of the Thread.
  base::SequenceChecker owning_sequence_checker_;
};

CefMainDelegate::CefMainDelegate(CefRefPtr<CefApp> application)
    : content_client_(application) {
  // Necessary so that exported functions from base_impl.cc will be included
  // in the binary.
  extern void base_impl_stub();
  base_impl_stub();

#if defined(OS_LINUX)
  OverrideAssetPath();
#endif
}

CefMainDelegate::~CefMainDelegate() {}

void CefMainDelegate::PreCreateMainMessageLoop() {
  InitMessagePumpFactoryForUI();
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

    if (settings.main_bundle_path.length > 0) {
      base::FilePath file_path =
          base::FilePath(CefString(&settings.main_bundle_path));
      if (!file_path.empty())
        command_line->AppendSwitchPath(switches::kMainBundlePath, file_path);
    }
#endif

    if (no_sandbox)
      command_line->AppendSwitch(service_manager::switches::kNoSandbox);

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
        case LOGSEVERITY_FATAL:
          log_severity = switches::kLogSeverity_Fatal;
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
      command_line->AppendSwitchASCII(
          switches::kRemoteDebuggingPort,
          base::NumberToString(settings.remote_debugging_port));
    }

    if (settings.uncaught_exception_stack_size > 0) {
      command_line->AppendSwitchASCII(
          switches::kUncaughtExceptionStackSize,
          base::NumberToString(settings.uncaught_exception_stack_size));
    }

    std::vector<std::string> disable_features;

    if (network::features::kOutOfBlinkCors.default_state ==
        base::FEATURE_ENABLED_BY_DEFAULT) {
      // TODO: Add support for out-of-Blink CORS (see issue #2716)
      disable_features.push_back(network::features::kOutOfBlinkCors.name);
    }

    if (features::kMimeHandlerViewInCrossProcessFrame.default_state ==
        base::FEATURE_ENABLED_BY_DEFAULT) {
      // TODO: Add support for cross-process mime handler view (see issue #2727)
      disable_features.push_back(
          features::kMimeHandlerViewInCrossProcessFrame.name);
    }

    if (features::kAudioServiceAudioStreams.default_state ==
        base::FEATURE_ENABLED_BY_DEFAULT) {
      // TODO: Add support for audio service (see issue #2755)
      disable_features.push_back(
          features::kAudioServiceAudioStreams.name);
    }

    if (!disable_features.empty()) {
      DCHECK(!base::FeatureList::GetInstance());
      std::string disable_features_str =
          command_line->GetSwitchValueASCII(switches::kDisableFeatures);
      for (auto feature_str : disable_features) {
        if (!disable_features_str.empty())
          disable_features_str += ",";
        disable_features_str += feature_str;
      }
      command_line->AppendSwitchASCII(switches::kDisableFeatures,
                                      disable_features_str);
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
                                          switches::kLogSeverity_Fatal)) {
      log_severity = logging::LOG_FATAL;
    } else if (base::LowerCaseEqualsASCII(log_severity_str,
                                          switches::kLogSeverity_Disable)) {
      log_severity = LOGSEVERITY_DISABLE;
    }
  }

  if (log_severity == LOGSEVERITY_DISABLE) {
    log_settings.logging_dest = logging::LOG_NONE;
    // By default, ERROR and FATAL messages will always be output to stderr due
    // to the kAlwaysPrintErrorLevel value in base/logging.cc. We change the log
    // level here so that only FATAL messages are output.
    logging::SetMinLogLevel(logging::LOG_FATAL);
  } else {
    log_settings.logging_dest = logging::LOG_TO_ALL;
    logging::SetMinLogLevel(log_severity);
  }

  logging::InitLogging(log_settings);

  ContentSettingsPattern::SetNonWildcardDomainNonPortSchemes(
      kNonWildcardDomainNonPortSchemes, kNonWildcardDomainNonPortSchemesSize);

  content::SetContentClient(&content_client_);

#if defined(OS_MACOSX)
  OverrideFrameworkBundlePath();
  OverrideOuterBundlePath();
  OverrideBaseBundleID();
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

    base::FilePath dir_default_download;
    base::FilePath dir_default_download_safe;
    if (GetDefaultDownloadDirectory(&dir_default_download)) {
      base::PathService::Override(chrome::DIR_DEFAULT_DOWNLOADS,
                                  dir_default_download);
    }
    if (GetDefaultDownloadSafeDirectory(&dir_default_download_safe)) {
      base::PathService::Override(chrome::DIR_DEFAULT_DOWNLOADS_SAFE,
                                  dir_default_download_safe);
    }

    const base::FilePath& user_data_path = GetUserDataPath();
    base::PathService::Override(chrome::DIR_USER_DATA, user_data_path);

    // Path used for crash dumps.
    base::PathService::Override(chrome::DIR_CRASH_DUMPS, user_data_path);

    // Path used for spell checking dictionary files.
    base::PathService::OverrideAndCreateIfNeeded(
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
  InitializePDF();
}

void CefMainDelegate::SandboxInitialized(const std::string& process_type) {
  CefContentClient::SetPDFEntryFunctions(chrome_pdf::PPP_GetInterface,
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
      browser_runner_ = content::BrowserMainRunner::Create();

      // Initialize browser process state. Results in a call to
      // CefBrowserMain::PreMainMessageLoopStart() which creates the UI message
      // loop.
      int exit_code = browser_runner_->Initialize(main_function_params);
      if (exit_code >= 0)
        return exit_code;
    } else {
      // Running on the separate UI thread.
      DCHECK(ui_thread_);
      ui_thread_->InitializeBrowserRunner(main_function_params);
    }

    return 0;
  }

  return -1;
}

bool CefMainDelegate::CreateUIThread(base::OnceClosure setup_callback) {
  DCHECK(!ui_thread_);

  ui_thread_.reset(new CefUIThread(std::move(setup_callback)));
  ui_thread_->Start();
  ui_thread_->WaitUntilThreadStarted();

  InitMessagePumpFactoryForUI();
  return true;
}

void CefMainDelegate::ProcessExiting(const std::string& process_type) {
  ui::ResourceBundle::CleanupSharedInstance();
}

#if defined(OS_LINUX)
void CefMainDelegate::ZygoteForked() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  const std::string& process_type =
      command_line->GetSwitchValueASCII(switches::kProcessType);
  // Initialize crash reporting state for the newly forked process.
  crash_reporting::ZygoteForked(command_line, process_type);
}
#endif

content::ContentBrowserClient* CefMainDelegate::CreateContentBrowserClient() {
  browser_client_.reset(new CefContentBrowserClient);
  return browser_client_.get();
}

content::ContentRendererClient* CefMainDelegate::CreateContentRendererClient() {
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
      cef_200_percent_pak_file, cef_extensions_pak_file, devtools_pak_file,
      locales_dir;

  base::FilePath resources_dir;
  if (command_line->HasSwitch(switches::kResourcesDirPath)) {
    resources_dir =
        command_line->GetSwitchValuePath(switches::kResourcesDirPath);
  }
  if (resources_dir.empty())
    resources_dir = GetResourcesFilePath();
  if (!resources_dir.empty())
    base::PathService::Override(chrome::DIR_RESOURCES, resources_dir);

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
      base::PathService::Override(ui::DIR_LOCALES, locales_dir);
  }

  std::string locale = command_line->GetSwitchValueASCII(switches::kLang);
  DCHECK(!locale.empty());

  const std::string loaded_locale =
      ui::ResourceBundle::InitSharedInstanceWithLocale(
          locale, content_client_.GetCefResourceBundleDelegate(),
          ui::ResourceBundle::LOAD_COMMON_RESOURCES);
  if (!loaded_locale.empty() && g_browser_process)
    g_browser_process->SetApplicationLocale(loaded_locale);

  ui::ResourceBundle& resource_bundle = ui::ResourceBundle::GetSharedInstance();

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
        resource_bundle.AddDataPackFromPath(cef_100_percent_pak_file,
                                            ui::SCALE_FACTOR_100P);
      } else {
        LOG(ERROR) << "Could not load cef_100_percent.pak";
      }
    }

    if (IsScaleFactorSupported(ui::SCALE_FACTOR_200P)) {
      if (base::PathExists(cef_200_percent_pak_file)) {
        resource_bundle.AddDataPackFromPath(cef_200_percent_pak_file,
                                            ui::SCALE_FACTOR_200P);
      } else {
        LOG(ERROR) << "Could not load cef_200_percent.pak";
      }
    }

    if (extensions::ExtensionsEnabled() ||
        !command_line->HasSwitch(switches::kDisablePlugins)) {
      if (base::PathExists(cef_extensions_pak_file)) {
        resource_bundle.AddDataPackFromPath(cef_extensions_pak_file,
                                            ui::SCALE_FACTOR_NONE);
      } else {
        LOG(ERROR) << "Could not load cef_extensions.pak";
      }
    }

    if (base::PathExists(devtools_pak_file)) {
      resource_bundle.AddDataPackFromPath(devtools_pak_file,
                                          ui::SCALE_FACTOR_NONE);
    }

    content_client_.set_allow_pack_file_load(false);
  }
}
