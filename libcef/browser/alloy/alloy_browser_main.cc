// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/alloy/alloy_browser_main.h"

#include <stdint.h>

#include <string>

#include "libcef/browser/browser_context.h"
#include "libcef/browser/browser_context_keyed_service_factories.h"
#include "libcef/browser/context.h"
#include "libcef/browser/devtools/devtools_manager_delegate.h"
#include "libcef/browser/extensions/extension_system_factory.h"
#include "libcef/browser/extensions/extensions_browser_client.h"
#include "libcef/browser/net/chrome_scheme_handler.h"
#include "libcef/browser/printing/constrained_window_views_client.h"
#include "libcef/browser/thread_util.h"
#include "libcef/common/app_manager.h"
#include "libcef/common/extensions/extensions_client.h"
#include "libcef/common/extensions/extensions_util.h"
#include "libcef/common/net/net_resource_provider.h"

#include "base/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/media/router/chrome_media_router_factory.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/plugins/plugin_finder.h"
#include "chrome/common/chrome_switches.h"
#include "components/constrained_window/constrained_window_views.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/common/result_codes.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/constants.h"
#include "net/base/net_module.h"
#include "third_party/widevine/cdm/buildflags.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(USE_AURA) && defined(USE_X11)
#include "ui/events/devices/x11/touch_factory_x11.h"
#endif

#if defined(USE_AURA)
#include "ui/aura/env.h"
#include "ui/display/screen.h"
#include "ui/views/widget/desktop_aura/desktop_screen.h"
#include "ui/wm/core/wm_state.h"

#if defined(OS_WIN)
#include "chrome/browser/chrome_browser_main_win.h"
#include "chrome/browser/win/parental_controls.h"
#include "components/os_crypt/os_crypt.h"
#endif
#endif  // defined(USE_AURA)

#if defined(TOOLKIT_VIEWS)
#if defined(OS_MAC)
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_views_delegate.h"
#else
#include "ui/views/test/desktop_test_views_delegate.h"
#endif
#endif  // defined(TOOLKIT_VIEWS)

#if defined(USE_AURA) && defined(OS_LINUX)
#include "ui/base/ime/init/input_method_initializer.h"
#endif

#if defined(OS_LINUX)
#include "libcef/browser/printing/print_dialog_linux.h"
#endif

#if BUILDFLAG(ENABLE_MEDIA_FOUNDATION_WIDEVINE_CDM)
#include "chrome/browser/component_updater/media_foundation_widevine_cdm_component_installer.h"
#endif

#if BUILDFLAG(ENABLE_WIDEVINE_CDM_COMPONENT)
#include "chrome/browser/component_updater/widevine_cdm_component_installer.h"
#endif

AlloyBrowserMainParts::AlloyBrowserMainParts(
    const content::MainFunctionParams& parameters)
    : BrowserMainParts(), devtools_delegate_(nullptr) {}

AlloyBrowserMainParts::~AlloyBrowserMainParts() {
  constrained_window::SetConstrainedWindowViewsClient(nullptr);
}

int AlloyBrowserMainParts::PreEarlyInitialization() {
#if defined(USE_AURA) && defined(OS_LINUX)
  // TODO(linux): Consider using a real input method or
  // views::LinuxUI::SetInstance.
  ui::InitializeInputMethodForTesting();
#endif

  return content::RESULT_CODE_NORMAL_EXIT;
}

void AlloyBrowserMainParts::ToolkitInitialized() {
  SetConstrainedWindowViewsClient(CreateCefConstrainedWindowViewsClient());
#if defined(USE_AURA)
  CHECK(aura::Env::GetInstance());

  wm_state_.reset(new wm::WMState);
#endif  // defined(USE_AURA)

#if defined(TOOLKIT_VIEWS)
#if defined(OS_MAC)
  views_delegate_ = std::make_unique<ChromeViewsDelegate>();
  layout_provider_ = ChromeLayoutProvider::CreateLayoutProvider();
#else
  views_delegate_ = std::make_unique<views::DesktopTestViewsDelegate>();
#endif
#endif  // defined(TOOLKIT_VIEWS)
}

void AlloyBrowserMainParts::PreCreateMainMessageLoop() {
#if defined(USE_AURA) && defined(USE_X11)
  ui::TouchFactory::SetTouchDeviceListFromCommandLine();
#endif

#if defined(OS_WIN)
  // Initialize the OSCrypt.
  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);
  bool os_crypt_init = OSCrypt::Init(local_state);
  DCHECK(os_crypt_init);

  // installer_util references strings that are normally compiled into
  // setup.exe.  In Chrome, these strings are in the locale files.
  ChromeBrowserMainPartsWin::SetupInstallerUtilStrings();
#endif  // defined(OS_WIN)

  media_router::ChromeMediaRouterFactory::DoPlatformInit();
}

void AlloyBrowserMainParts::PostCreateMainMessageLoop() {
#if defined(OS_LINUX)
  printing::PrintingContextLinux::SetCreatePrintDialogFunction(
      &CefPrintDialogLinux::CreatePrintDialog);
  printing::PrintingContextLinux::SetPdfPaperSizeFunction(
      &CefPrintDialogLinux::GetPdfPaperSize);
#endif
}

int AlloyBrowserMainParts::PreCreateThreads() {
#if defined(OS_WIN)
  PlatformInitialize();
#endif

  net::NetModule::SetResourceProvider(&NetResourceProvider);

  // Initialize these objects before IO access restrictions are applied and
  // before the IO thread is started.
  content::GpuDataManager::GetInstance();
  SystemNetworkContextManager::CreateInstance(g_browser_process->local_state());

  return 0;
}

int AlloyBrowserMainParts::PreMainMessageLoopRun() {
#if defined(USE_AURA)
  screen_ = views::CreateDesktopScreen();
  display::Screen::SetScreenInstance(screen_.get());
#endif

  if (extensions::ExtensionsEnabled()) {
    // Initialize extension global objects before creating the global
    // BrowserContext.
    extensions_client_.reset(new extensions::CefExtensionsClient());
    extensions::ExtensionsClient::Set(extensions_client_.get());
    extensions_browser_client_.reset(
        new extensions::CefExtensionsBrowserClient);
    extensions::ExtensionsBrowserClient::Set(extensions_browser_client_.get());

    extensions::CefExtensionSystemFactory::GetInstance();
  }

  // Register additional KeyedService factories here. See
  // ChromeBrowserMainExtraPartsProfiles for details.
  cef::EnsureBrowserContextKeyedServiceFactoriesBuilt();

  background_task_runner_ = base::ThreadPool::CreateSingleThreadTaskRunner(
      {base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN, base::MayBlock()});
  user_visible_task_runner_ = base::ThreadPool::CreateSingleThreadTaskRunner(
      {base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN, base::MayBlock()});
  user_blocking_task_runner_ = base::ThreadPool::CreateSingleThreadTaskRunner(
      {base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN, base::MayBlock()});

  CefRequestContextSettings settings;
  CefContext::Get()->PopulateGlobalRequestContextSettings(&settings);

  // Create the global RequestContext.
  global_request_context_ =
      CefRequestContextImpl::CreateGlobalRequestContext(settings);
  auto browser_context =
      global_request_context_->GetBrowserContext()->AsBrowserContext();

  CefDevToolsManagerDelegate::StartHttpHandler(browser_context);

#if defined(OS_WIN)
  // Windows parental controls calls can be slow, so we do an early init here
  // that calculates this value off of the UI thread.
  InitializeWinParentalControls();
#endif

  // Triggers initialization of the singleton instance on UI thread.
  PluginFinder::GetInstance()->Init();

  scheme::RegisterWebUIControllerFactory();

#if BUILDFLAG(ENABLE_MEDIA_FOUNDATION_WIDEVINE_CDM) || \
    BUILDFLAG(ENABLE_WIDEVINE_CDM_COMPONENT)
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kDisableComponentUpdate)) {
    auto* const cus = g_browser_process->component_updater();

#if BUILDFLAG(ENABLE_MEDIA_FOUNDATION_WIDEVINE_CDM)
    RegisterMediaFoundationWidevineCdmComponent(cus);
#endif

#if BUILDFLAG(ENABLE_WIDEVINE_CDM_COMPONENT)
    RegisterWidevineCdmComponent(cus);
#endif
  }
#endif

  return content::RESULT_CODE_NORMAL_EXIT;
}

void AlloyBrowserMainParts::PostMainMessageLoopRun() {
  // NOTE: Destroy objects in reverse order of creation.
  CefDevToolsManagerDelegate::StopHttpHandler();

  // There should be no additional references to the global CefRequestContext
  // during shutdown. Did you forget to release a CefBrowser reference?
  DCHECK(global_request_context_->HasOneRef());
  global_request_context_ = nullptr;
}

void AlloyBrowserMainParts::PostDestroyThreads() {
  if (extensions::ExtensionsEnabled()) {
    extensions::ExtensionsBrowserClient::Set(nullptr);
    extensions_browser_client_.reset();
  }

#if defined(TOOLKIT_VIEWS)
  views_delegate_.reset();
#if defined(OS_MAC)
  layout_provider_.reset();
#endif
#endif  // defined(TOOLKIT_VIEWS)
}
