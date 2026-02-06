// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "cef/libcef/browser/context.h"

#include <memory>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/notimplemented.h"
#include "base/run_loop.h"
#include "base/task/current_thread.h"
#include "base/threading/thread_restrictions.h"
#include "cef/libcef/browser/browser_info_manager.h"
#include "cef/libcef/browser/prefs/pref_helper.h"
#include "cef/libcef/browser/request_context_impl.h"
#include "cef/libcef/browser/thread_util.h"
#include "cef/libcef/browser/trace_subscriber.h"
#include "cef/libcef/common/cef_switches.h"
#include "chrome/browser/browser_process_impl.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "ui/base/ui_base_switches.h"

#if BUILDFLAG(IS_WIN)
#include "base/strings/utf_string_conversions.h"
#include "cef/include/internal/cef_win.h"
#include "cef/libcef/browser/preferred_stack_size_win.inc"
#include "chrome/chrome_elf/chrome_elf_main.h"
#include "chrome/install_static/initialize_from_primary_module.h"
#endif

namespace {

CefContext* g_context = nullptr;

// Invalid value before CefInitialize is called.
int g_exit_code = -1;

#if DCHECK_IS_ON()
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wexit-time-destructors"
// When the process terminates check if CefShutdown() has been called.
class CefShutdownChecker {
 public:
  ~CefShutdownChecker() { DCHECK(!g_context) << "CefShutdown was not called"; }
} g_shutdown_checker;
#pragma clang diagnostic pop
#endif  // DCHECK_IS_ON()

#if BUILDFLAG(IS_WIN)

// Transfer state from chrome_elf.dll to the libcef.dll. Accessed when
// loading chrome://system.
void InitInstallDetails() {
  static bool initialized = false;
  if (initialized) {
    return;
  }
  initialized = true;
  install_static::InitializeFromPrimaryModule();
}

// Signal chrome_elf to initialize crash reporting, rather than doing it in
// DllMain. See https://crbug.com/656800 for details.
void InitCrashReporter() {
  static bool initialized = false;
  if (initialized) {
    return;
  }
  initialized = true;
  SignalInitializeCrashReporting();
}

#endif  // BUILDFLAG(IS_WIN)

bool GetColor(const cef_color_t cef_in, bool is_transparent, SkColor* sk_out) {
  // transparent unsupported browser colors must be fully opaque.
  if (!is_transparent && CefColorGetA(cef_in) != SK_AlphaOPAQUE) {
    return false;
  }

  // transparent supported browser colors may be fully transparent.
  if (is_transparent && CefColorGetA(cef_in) == SK_AlphaTRANSPARENT) {
    *sk_out = SK_ColorTRANSPARENT;
    return true;
  }

  // Ignore the alpha component.
  *sk_out = SkColorSetRGB(CefColorGetR(cef_in), CefColorGetG(cef_in),
                          CefColorGetB(cef_in));
  return true;
}

// Convert |path_str| to a normalized FilePath.
base::FilePath NormalizePath(const cef_string_t& path_str,
                             const char* name,
                             bool* has_error = nullptr) {
  if (has_error) {
    *has_error = false;
  }

  base::FilePath path = base::FilePath(CefString(&path_str));
  if (path.EndsWithSeparator()) {
    // Remove the trailing separator because it will interfere with future
    // equality checks.
    path = path.StripTrailingSeparators();
  }

  if (!path.empty()) {
    if (!path.IsAbsolute()) {
      LOG(ERROR) << "The " << name << " directory (" << path.value()
                 << ") is not an absolute path. Defaulting to empty.";
      if (has_error) {
        *has_error = true;
      }
      return base::FilePath();
    }

#if BUILDFLAG(IS_POSIX)
    // Always resolve symlinks to absolute paths. This avoids issues with
    // mismatched paths when mixing Chromium and OS filesystem functions.
    // See https://crbug.com/40229712.
    base::ScopedAllowBlockingForTesting allow_blocking;
    const base::FilePath& resolved_path = base::MakeAbsoluteFilePath(path);
    if (!resolved_path.empty()) {
      return resolved_path;
    } else if (errno != 0 && errno != ENOENT) {
      PLOG(ERROR) << "realpath(" << path.value() << ") failed";
    }
#endif  // BUILDFLAG(IS_POSIX)
  }

  return path;
}

void SetPath(cef_string_t& path_str, const base::FilePath& path) {
#if BUILDFLAG(IS_WIN)
  CefString(&path_str).FromWString(path.value());
#else
  CefString(&path_str).FromString(path.value());
#endif
}

// Convert |path_str| to a normalized FilePath and update the |path_str| value.
base::FilePath NormalizePathAndSet(cef_string_t& path_str, const char* name) {
  const base::FilePath& path = NormalizePath(path_str, name);
  SetPath(path_str, path);
  return path;
}

// Verify that |cache_path| is valid and create it if necessary.
bool ValidateCachePath(const base::FilePath& cache_path,
                       const base::FilePath& root_cache_path) {
  if (cache_path.empty()) {
    return true;
  }

  if (!root_cache_path.empty() && root_cache_path != cache_path &&
      !root_cache_path.IsParent(cache_path)) {
    LOG(ERROR) << "The cache_path directory (" << cache_path.value()
               << ") is not a child of the root_cache_path directory ("
               << root_cache_path.value() << ")";
    return false;
  }

  base::ScopedAllowBlockingForTesting allow_blocking;
  if (!base::DirectoryExists(cache_path) &&
      !base::CreateDirectory(cache_path)) {
    LOG(ERROR) << "The cache_path directory (" << cache_path.value()
               << ") could not be created.";
    return false;
  }

  return true;
}

// Like NormalizePathAndSet but with additional checks specific to the
// cache_path value.
base::FilePath NormalizeCachePathAndSet(cef_string_t& path_str,
                                        const base::FilePath& root_cache_path) {
  bool has_error = false;
  base::FilePath path = NormalizePath(path_str, "cache_path", &has_error);
  if (has_error || !ValidateCachePath(path, root_cache_path)) {
    LOG(ERROR) << "The cache_path is invalid. Defaulting to in-memory storage.";
    path = base::FilePath();
  }
  SetPath(path_str, path);
  return path;
}

}  // namespace

NO_STACK_PROTECTOR
int CefExecuteProcess(const CefMainArgs& args,
                      CefRefPtr<CefApp> application,
                      void* windows_sandbox_info) {
#if BUILDFLAG(IS_WIN)
  InitInstallDetails();
  InitCrashReporter();
#endif

  return CefMainRunner::RunAsHelperProcess(args, application,
                                           windows_sandbox_info);
}

bool CefInitialize(const CefMainArgs& args,
                   const CefSettings& settings,
                   CefRefPtr<CefApp> application,
                   void* windows_sandbox_info) {
#if BUILDFLAG(IS_WIN)
  InitInstallDetails();
  InitCrashReporter();
#endif

  // Return true if the global context already exists.
  if (g_context) {
    return true;
  }

  if (!CEF_MEMBER_EXISTS(&settings, disable_signal_handlers)) {
    DCHECK(false) << "invalid CefSettings structure size";
    return false;
  }

  // Create the new global context object.
  g_context = new CefContext();

  // Initialize the global context.
  const bool initialized =
      g_context->Initialize(args, settings, application, windows_sandbox_info);
  g_exit_code = g_context->exit_code();

  if (!initialized) {
    // Initialization failed. Delete the global context object.
    delete g_context;
    g_context = nullptr;
    return false;
  }

  return true;
}

int CefGetExitCode() {
  DCHECK_NE(g_exit_code, -1) << "invalid call to CefGetExitCode";
  return g_exit_code;
}

void CefShutdown() {
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    DCHECK(false) << "context not valid";
    return;
  }

  // Must always be called on the same thread as Initialize.
  if (!g_context->OnInitThread()) {
    DCHECK(false) << "called on invalid thread";
    return;
  }

  // Shut down the global context. This will block until shutdown is complete.
  g_context->Shutdown();

  // Delete the global context object.
  delete g_context;
  g_context = nullptr;
}

void CefDoMessageLoopWork() {
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    DCHECK(false) << "context not valid";
    return;
  }

  // Must always be called on the same thread as Initialize.
  if (!g_context->OnInitThread()) {
    DCHECK(false) << "called on invalid thread";
    return;
  }

  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
}

void CefRunMessageLoop() {
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    DCHECK(false) << "context not valid";
    return;
  }

  // Must always be called on the same thread as Initialize.
  if (!g_context->OnInitThread()) {
    DCHECK(false) << "called on invalid thread";
    return;
  }

  g_context->RunMessageLoop();
}

void CefQuitMessageLoop() {
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    DCHECK(false) << "context not valid";
    return;
  }

  // Must always be called on the same thread as Initialize.
  if (!g_context->OnInitThread()) {
    DCHECK(false) << "called on invalid thread";
    return;
  }

  g_context->QuitMessageLoop();
}

#if BUILDFLAG(IS_WIN)

void CefSetOSModalLoop(bool osModalLoop) {
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    DCHECK(false) << "context not valid";
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT, base::BindOnce(CefSetOSModalLoop, osModalLoop));
    return;
  }

  base::CurrentThread::Get()->set_os_modal_loop(osModalLoop);
}

#endif  // BUILDFLAG(IS_WIN)

void CefSetNestableTasksAllowed(bool allowed) {
  if (!CONTEXT_STATE_VALID()) {
    DCHECK(false) << "context not valid";
    return;
  }

  g_context->SetNestableTasksAllowed(allowed);
}

// CefContext

CefContext::CefContext() = default;

CefContext::~CefContext() = default;

// static
CefContext* CefContext::Get() {
  return g_context;
}

bool CefContext::Initialize(const CefMainArgs& args,
                            const CefSettings& settings,
                            CefRefPtr<CefApp> application,
                            void* windows_sandbox_info) {
  init_thread_id_ = base::PlatformThread::CurrentId();
  settings_ = settings;
  application_ = application;

#if !(BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX))
  if (settings.multi_threaded_message_loop) {
    NOTIMPLEMENTED() << "multi_threaded_message_loop is not supported.";
    return false;
  }
#endif

#if BUILDFLAG(IS_WIN)
  // Signal Chrome Elf that Chrome has begun to start.
  SignalChromeElf();
#endif

  const base::FilePath& root_cache_path =
      NormalizePathAndSet(settings_.root_cache_path, "root_cache_path");
  const base::FilePath& cache_path =
      NormalizeCachePathAndSet(settings_.cache_path, root_cache_path);
  if (root_cache_path.empty() && !cache_path.empty()) {
    CefString(&settings_.root_cache_path) = cache_path.value();
  }

  // All other paths that need to be normalized.
  NormalizePathAndSet(settings_.browser_subprocess_path,
                      "browser_subprocess_path");
  NormalizePathAndSet(settings_.framework_dir_path, "framework_dir_path");
  NormalizePathAndSet(settings_.main_bundle_path, "main_bundle_path");
  NormalizePathAndSet(settings_.resources_dir_path, "resources_dir_path");
  NormalizePathAndSet(settings_.locales_dir_path, "locales_dir_path");

  browser_info_manager_ = std::make_unique<CefBrowserInfoManager>();

  main_runner_ = std::make_unique<CefMainRunner>(
      settings_.multi_threaded_message_loop, settings_.external_message_pump);

  const bool initialized = main_runner_->Initialize(
      &settings_, application, args, windows_sandbox_info, &initialized_,
      base::BindOnce(&CefContext::OnContextInitialized,
                     base::Unretained(this)));
  exit_code_ = main_runner_->exit_code();

  CHECK_EQ(initialized, initialized_);

  if (!initialized) {
    Shutdown();
    return false;
  }

  return true;
}

void CefContext::RunMessageLoop() {
  // Must always be called on the same thread as Initialize.
  DCHECK(OnInitThread());

  // Blocks until QuitMessageLoop() is called.
  main_runner_->RunMessageLoop();
}

void CefContext::QuitMessageLoop() {
  // Must always be called on the same thread as Initialize.
  DCHECK(OnInitThread());

  main_runner_->QuitMessageLoop();
}

void CefContext::Shutdown() {
  // Must always be called on the same thread as Initialize.
  DCHECK(OnInitThread());

  shutting_down_ = true;

  main_runner_->Shutdown(
      base::BindOnce(&CefContext::ShutdownOnUIThread, base::Unretained(this)),
      base::BindOnce(&CefContext::FinalizeShutdown, base::Unretained(this)));
}

bool CefContext::OnInitThread() {
  return (base::PlatformThread::CurrentId() == init_thread_id_);
}

SkColor CefContext::GetBackgroundColor(
    const CefBrowserSettings* browser_settings,
    cef_state_t transparent_state) const {
  bool is_transparent = transparent_state == STATE_ENABLED
                           ? true
                           : (transparent_state == STATE_DISABLED
                                  ? false
                                  : !!settings_.windowless_rendering_enabled);

  // Default to opaque white if no acceptable color values are found.
  SkColor sk_color = SK_ColorWHITE;

  if (!browser_settings ||
      !GetColor(browser_settings->background_color, is_transparent, &sk_color)) {
    GetColor(settings_.background_color, is_transparent, &sk_color);
  }
  return sk_color;
}

CefTraceSubscriber* CefContext::GetTraceSubscriber() {
  CEF_REQUIRE_UIT();
  if (shutting_down_) {
    return nullptr;
  }
  if (!trace_subscriber_) {
    trace_subscriber_ = std::make_unique<CefTraceSubscriber>();
  }
  return trace_subscriber_.get();
}

pref_helper::Registrar* CefContext::GetPrefRegistrar() {
  CEF_REQUIRE_UIT();
  if (shutting_down_) {
    return nullptr;
  }
  if (!pref_registrar_) {
    pref_registrar_ = std::make_unique<pref_helper::Registrar>();
    pref_registrar_->Init(g_browser_process->local_state());
  }
  return pref_registrar_.get();
}

void CefContext::PopulateGlobalRequestContextSettings(
    CefRequestContextSettings* settings) {
  CefRefPtr<CefCommandLine> command_line =
      CefCommandLine::GetGlobalCommandLine();

  // This value was already normalized in Initialize.
  CefString(&settings->cache_path) = CefString(&settings_.cache_path);

  settings->persist_session_cookies =
      settings_.persist_session_cookies ||
      command_line->HasSwitch(switches::kPersistSessionCookies);

  CefString(&settings->cookieable_schemes_list) =
      CefString(&settings_.cookieable_schemes_list);
  settings->cookieable_schemes_exclude_defaults =
      settings_.cookieable_schemes_exclude_defaults;
}

void CefContext::NormalizeRequestContextSettings(
    CefRequestContextSettings* settings) {
  // The |root_cache_path| value was already normalized in Initialize.
  const base::FilePath& root_cache_path = CefString(&settings_.root_cache_path);
  NormalizeCachePathAndSet(settings->cache_path, root_cache_path);
}

void CefContext::SetNestableTasksAllowed(bool allowed) {
  CEF_REQUIRE_UIT();
  CHECK(allowed != nestable_tasks_allowed_.has_value())
      << "Invalid attempt at CefSetNestableTasksAllowed reentrancy";
  if (allowed) {
    nestable_tasks_allowed_.emplace();
  } else {
    nestable_tasks_allowed_.reset();
  }
}

void CefContext::AddObserver(Observer* observer) {
  CEF_REQUIRE_UIT();
  observers_.AddObserver(observer);
}

void CefContext::RemoveObserver(Observer* observer) {
  CEF_REQUIRE_UIT();
  observers_.RemoveObserver(observer);
}

bool CefContext::HasObserver(Observer* observer) const {
  CEF_REQUIRE_UIT();
  return observers_.HasObserver(observer);
}

void CefContext::OnContextInitialized() {
  CEF_REQUIRE_UIT();

  if (application_) {
    // Notify the handler after the global browser context has initialized.
    CefRefPtr<CefRequestContext> request_context =
        CefRequestContext::GetGlobalContext();
    auto impl = static_cast<CefRequestContextImpl*>(request_context.get());
    impl->ExecuteWhenBrowserContextInitialized(base::BindOnce(
        [](CefRefPtr<CefApp> app) {
          CefRefPtr<CefBrowserProcessHandler> handler =
              app->GetBrowserProcessHandler();
          if (handler) {
            handler->OnContextInitialized();
          }
        },
        application_));
  }
}

void CefContext::ShutdownOnUIThread() {
  // |initialized_| will be false if shutting down after early exit.
  if (!initialized_) {
    return;
  }

  CEF_REQUIRE_UIT();

  browser_info_manager_->DestroyAllBrowsers();

  for (auto& observer : observers_) {
    observer.OnContextDestroyed();
  }

  if (trace_subscriber_) {
    trace_subscriber_.reset();
  }
  if (pref_registrar_) {
    pref_registrar_.reset();
  }
}

void CefContext::FinalizeShutdown() {
  browser_info_manager_.reset();
  application_ = nullptr;
}
