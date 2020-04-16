// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/browser_main.h"

#include <stdint.h>

#include <string>

#include "libcef/browser/browser_context.h"
#include "libcef/browser/browser_context_keyed_service_factories.h"
#include "libcef/browser/content_browser_client.h"
#include "libcef/browser/context.h"
#include "libcef/browser/devtools/devtools_manager_delegate.h"
#include "libcef/browser/extensions/extension_system_factory.h"
#include "libcef/browser/extensions/extensions_browser_client.h"
#include "libcef/browser/net/chrome_scheme_handler.h"
#include "libcef/browser/printing/constrained_window_views_client.h"
#include "libcef/browser/printing/printing_message_filter.h"
#include "libcef/browser/thread_util.h"
#include "libcef/common/extensions/extensions_client.h"
#include "libcef/common/extensions/extensions_util.h"
#include "libcef/common/net/net_resource_provider.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/plugins/plugin_finder.h"
#include "components/constrained_window/constrained_window_views.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/browser/network_service_instance.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/constants.h"
#include "net/base/net_module.h"
#include "services/service_manager/embedder/result_codes.h"
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
#include "ui/base/cursor/cursor_loader_win.h"
#endif
#endif  // defined(USE_AURA)

#if defined(TOOLKIT_VIEWS)
#if defined(OS_MACOSX)
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

CefBrowserMainParts::CefBrowserMainParts(
    const content::MainFunctionParams& parameters)
    : BrowserMainParts(), devtools_delegate_(nullptr) {}

CefBrowserMainParts::~CefBrowserMainParts() {
  constrained_window::SetConstrainedWindowViewsClient(nullptr);
}

int CefBrowserMainParts::PreEarlyInitialization() {
#if defined(USE_AURA) && defined(OS_LINUX)
  // TODO(linux): Consider using a real input method or
  // views::LinuxUI::SetInstance.
  ui::InitializeInputMethodForTesting();
#endif

  return service_manager::RESULT_CODE_NORMAL_EXIT;
}

void CefBrowserMainParts::ToolkitInitialized() {
  SetConstrainedWindowViewsClient(CreateCefConstrainedWindowViewsClient());
#if defined(USE_AURA)
  CHECK(aura::Env::GetInstance());

  wm_state_.reset(new wm::WMState);

#if defined(OS_WIN)
  ui::CursorLoaderWin::SetCursorResourceModule(
      CefContentBrowserClient::Get()->GetResourceDllName());
#endif
#endif  // defined(USE_AURA)

#if defined(TOOLKIT_VIEWS)
#if defined(OS_MACOSX)
  views_delegate_ = std::make_unique<ChromeViewsDelegate>();
  layout_provider_ = ChromeLayoutProvider::CreateLayoutProvider();
#else
  views_delegate_ = std::make_unique<views::DesktopTestViewsDelegate>();
#endif
#endif  // defined(TOOLKIT_VIEWS)
}

void CefBrowserMainParts::PreMainMessageLoopStart() {
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
}

void CefBrowserMainParts::PostMainMessageLoopStart() {
#if defined(OS_LINUX)
  printing::PrintingContextLinux::SetCreatePrintDialogFunction(
      &CefPrintDialogLinux::CreatePrintDialog);
  printing::PrintingContextLinux::SetPdfPaperSizeFunction(
      &CefPrintDialogLinux::GetPdfPaperSize);
#endif
}

int CefBrowserMainParts::PreCreateThreads() {
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

void CefBrowserMainParts::PreMainMessageLoopRun() {
#if defined(USE_AURA)
  display::Screen::SetScreenInstance(views::CreateDesktopScreen());
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

  printing::CefPrintingMessageFilter::EnsureShutdownNotifierFactoryBuilt();

  background_task_runner_ = base::CreateSingleThreadTaskRunner(
      {base::ThreadPool(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN, base::MayBlock()});
  user_visible_task_runner_ = base::CreateSingleThreadTaskRunner(
      {base::ThreadPool(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN, base::MayBlock()});
  user_blocking_task_runner_ = base::CreateSingleThreadTaskRunner(
      {base::ThreadPool(), base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN, base::MayBlock()});

  CefRequestContextSettings settings;
  CefContext::Get()->PopulateRequestContextSettings(&settings);

  // Create the global RequestContext.
  global_request_context_ =
      CefRequestContextImpl::CreateGlobalRequestContext(settings);
  CefBrowserContext* browser_context = static_cast<CefBrowserContext*>(
      global_request_context_->GetBrowserContext());

  CefDevToolsManagerDelegate::StartHttpHandler(browser_context);

#if defined(OS_WIN)
  // Windows parental controls calls can be slow, so we do an early init here
  // that calculates this value off of the UI thread.
  InitializeWinParentalControls();
#endif

  // Triggers initialization of the singleton instance on UI thread.
  PluginFinder::GetInstance()->Init();

  scheme::RegisterWebUIControllerFactory();
}

void CefBrowserMainParts::PostMainMessageLoopRun() {
  // NOTE: Destroy objects in reverse order of creation.
  CefDevToolsManagerDelegate::StopHttpHandler();

  // There should be no additional references to the global CefRequestContext
  // during shutdown. Did you forget to release a CefBrowser reference?
  DCHECK(global_request_context_->HasOneRef());
  global_request_context_ = nullptr;
}

void CefBrowserMainParts::PostDestroyThreads() {
  if (extensions::ExtensionsEnabled()) {
    extensions::ExtensionsBrowserClient::Set(nullptr);
    extensions_browser_client_.reset();
  }

#if defined(TOOLKIT_VIEWS)
  views_delegate_.reset();
#if defined(OS_MACOSX)
  layout_provider_.reset();
#endif
#endif  // defined(TOOLKIT_VIEWS)
}
