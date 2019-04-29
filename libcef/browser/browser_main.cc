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
#include "libcef/browser/printing/printing_message_filter.h"
#include "libcef/browser/thread_util.h"
#include "libcef/common/extensions/extensions_client.h"
#include "libcef/common/extensions/extensions_util.h"
#include "libcef/common/net/net_resource_provider.h"
#include "libcef/common/net_service/util.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"
#include "chrome/browser/plugins/plugin_finder.h"
#include "content/public/browser/gpu_data_manager.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/constants.h"
#include "net/base/net_module.h"
#include "services/service_manager/embedder/result_codes.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(USE_AURA) && defined(USE_X11)
#include "ui/events/devices/x11/touch_factory_x11.h"
#endif

#if defined(USE_AURA)
#include "ui/aura/env.h"
#include "ui/display/screen.h"
#include "ui/views/test/desktop_test_views_delegate.h"
#include "ui/views/widget/desktop_aura/desktop_screen.h"
#include "ui/wm/core/wm_state.h"

#if defined(OS_WIN)
#include "ui/base/cursor/cursor_loader_win.h"
#endif
#endif  // defined(USE_AURA)

#if defined(USE_AURA) && defined(OS_LINUX)
#include "ui/base/ime/input_method_initializer.h"
#endif

#if defined(OS_LINUX)
#include "libcef/browser/printing/print_dialog_linux.h"
#endif

#if defined(OS_MACOSX)
#include "chrome/browser/browser_process.h"
#include "components/os_crypt/os_crypt.h"
#endif

CefBrowserMainParts::CefBrowserMainParts(
    const content::MainFunctionParams& parameters)
    : BrowserMainParts(), devtools_delegate_(NULL) {}

CefBrowserMainParts::~CefBrowserMainParts() {
  for (int i = static_cast<int>(chrome_extra_parts_.size()) - 1; i >= 0; --i)
    delete chrome_extra_parts_[i];
  chrome_extra_parts_.clear();
}

void CefBrowserMainParts::AddParts(ChromeBrowserMainExtraParts* parts) {
  chrome_extra_parts_.push_back(parts);
}

int CefBrowserMainParts::PreEarlyInitialization() {
#if defined(USE_AURA) && defined(OS_LINUX)
  // TODO(linux): Consider using a real input method or
  // views::LinuxUI::SetInstance.
  ui::InitializeInputMethodForTesting();
#endif

  for (size_t i = 0; i < chrome_extra_parts_.size(); ++i)
    chrome_extra_parts_[i]->PreEarlyInitialization();

  return service_manager::RESULT_CODE_NORMAL_EXIT;
}

void CefBrowserMainParts::PostEarlyInitialization() {
  for (size_t i = 0; i < chrome_extra_parts_.size(); ++i)
    chrome_extra_parts_[i]->PostEarlyInitialization();
}

void CefBrowserMainParts::ToolkitInitialized() {
#if defined(USE_AURA)
  CHECK(aura::Env::GetInstance());

  new views::DesktopTestViewsDelegate;

  wm_state_.reset(new wm::WMState);

#if defined(OS_WIN)
  ui::CursorLoaderWin::SetCursorResourceModule(
      CefContentBrowserClient::Get()->GetResourceDllName());
#endif
#endif  // defined(USE_AURA)

  for (size_t i = 0; i < chrome_extra_parts_.size(); ++i)
    chrome_extra_parts_[i]->ToolkitInitialized();
}

void CefBrowserMainParts::PreMainMessageLoopStart() {
#if defined(USE_AURA) && defined(USE_X11)
  ui::TouchFactory::SetTouchDeviceListFromCommandLine();
#endif

#if defined(OS_MACOSX)
  if (net_service::IsEnabled()) {
    // Initialize the OSCrypt.
    PrefService* local_state = g_browser_process->local_state();
    DCHECK(local_state);
    OSCrypt::Init(local_state);
  }
#endif

  for (size_t i = 0; i < chrome_extra_parts_.size(); ++i)
    chrome_extra_parts_[i]->PreMainMessageLoopStart();
}

void CefBrowserMainParts::PostMainMessageLoopStart() {
#if defined(OS_LINUX)
  printing::PrintingContextLinux::SetCreatePrintDialogFunction(
      &CefPrintDialogLinux::CreatePrintDialog);
  printing::PrintingContextLinux::SetPdfPaperSizeFunction(
      &CefPrintDialogLinux::GetPdfPaperSize);
#endif

  for (size_t i = 0; i < chrome_extra_parts_.size(); ++i)
    chrome_extra_parts_[i]->PostMainMessageLoopStart();
}

int CefBrowserMainParts::PreCreateThreads() {
#if defined(OS_WIN)
  PlatformInitialize();
#endif

  net::NetModule::SetResourceProvider(&NetResourceProvider);

  // Initialize the GpuDataManager before IO access restrictions are applied and
  // before the IO thread is started.
  content::GpuDataManager::GetInstance();

  for (size_t i = 0; i < chrome_extra_parts_.size(); ++i)
    chrome_extra_parts_[i]->PreCreateThreads();

  return 0;
}

void CefBrowserMainParts::ServiceManagerConnectionStarted(
    content::ServiceManagerConnection* connection) {
  for (size_t i = 0; i < chrome_extra_parts_.size(); ++i)
    chrome_extra_parts_[i]->ServiceManagerConnectionStarted(connection);
}

void CefBrowserMainParts::PreMainMessageLoopRun() {
#if defined(USE_AURA)
  display::Screen::SetScreenInstance(views::CreateDesktopScreen());
#endif

  ui::MaterialDesignController::Initialize();

  // CEF's profile is a BrowserContext.
  PreProfileInit();

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

  background_task_runner_ = base::CreateSingleThreadTaskRunnerWithTraits(
      {base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN, base::MayBlock()});
  user_visible_task_runner_ = base::CreateSingleThreadTaskRunnerWithTraits(
      {base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN, base::MayBlock()});
  user_blocking_task_runner_ = base::CreateSingleThreadTaskRunnerWithTraits(
      {base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN, base::MayBlock()});

  CefRequestContextSettings settings;
  CefContext::Get()->PopulateRequestContextSettings(&settings);

  // Create the global RequestContext.
  global_request_context_ =
      CefRequestContextImpl::CreateGlobalRequestContext(settings);
  CefBrowserContext* browser_context = static_cast<CefBrowserContext*>(
      global_request_context_->GetBrowserContext());

  PostProfileInit();

  CefDevToolsManagerDelegate::StartHttpHandler(browser_context);

  // Triggers initialization of the singleton instance on UI thread.
  PluginFinder::GetInstance()->Init();

  scheme::RegisterWebUIControllerFactory();

  // These have no equivalent in CEF.
  PreBrowserStart();
  PostBrowserStart();

  for (size_t i = 0; i < chrome_extra_parts_.size(); ++i)
    chrome_extra_parts_[i]->PreMainMessageLoopRun();
}

void CefBrowserMainParts::PostMainMessageLoopRun() {
  // NOTE: Destroy objects in reverse order of creation.
  CefDevToolsManagerDelegate::StopHttpHandler();

  // There should be no additional references to the global CefRequestContext
  // during shutdown. Did you forget to release a CefBrowser reference?
  DCHECK(global_request_context_->HasOneRef());
  global_request_context_ = NULL;

  for (size_t i = 0; i < chrome_extra_parts_.size(); ++i)
    chrome_extra_parts_[i]->PostMainMessageLoopRun();
}

void CefBrowserMainParts::PostDestroyThreads() {
  if (extensions::ExtensionsEnabled()) {
    extensions::ExtensionsBrowserClient::Set(NULL);
    extensions_browser_client_.reset();
  }

#if defined(USE_AURA)
  // Delete the DesktopTestViewsDelegate.
  delete views::ViewsDelegate::GetInstance();
#endif
}

void CefBrowserMainParts::PreProfileInit() {
  for (size_t i = 0; i < chrome_extra_parts_.size(); ++i)
    chrome_extra_parts_[i]->PreProfileInit();
}

void CefBrowserMainParts::PostProfileInit() {
  for (size_t i = 0; i < chrome_extra_parts_.size(); ++i)
    chrome_extra_parts_[i]->PostProfileInit();
}

void CefBrowserMainParts::PreBrowserStart() {
  for (size_t i = 0; i < chrome_extra_parts_.size(); ++i)
    chrome_extra_parts_[i]->PreBrowserStart();
}

void CefBrowserMainParts::PostBrowserStart() {
  for (size_t i = 0; i < chrome_extra_parts_.size(); ++i)
    chrome_extra_parts_[i]->PostBrowserStart();
}
