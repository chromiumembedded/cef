// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/browser_main.h"

#include <stdint.h>

#include <string>

#include "libcef/browser/browser_context_impl.h"
#include "libcef/browser/browser_context_proxy.h"
#include "libcef/browser/browser_message_loop.h"
#include "libcef/browser/content_browser_client.h"
#include "libcef/browser/context.h"
#include "libcef/browser/devtools_manager_delegate.h"
#include "libcef/browser/extensions/browser_context_keyed_service_factories.h"
#include "libcef/browser/extensions/extensions_browser_client.h"
#include "libcef/browser/extensions/extension_system_factory.h"
#include "libcef/browser/net/chrome_scheme_handler.h"
#include "libcef/browser/printing/printing_message_filter.h"
#include "libcef/browser/thread_util.h"
#include "libcef/common/extensions/extensions_client.h"
#include "libcef/common/extensions/extensions_util.h"
#include "libcef/common/net/net_resource_provider.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/plugins/plugin_finder.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/common/content_switches.h"
#include "device/geolocation/access_token_store.h"
#include "device/geolocation/geolocation_delegate.h"
#include "device/geolocation/geolocation_provider.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/constants.h"
#include "net/base/net_module.h"
#include "ui/base/resource/resource_bundle.h"

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

namespace {

// In-memory store for access tokens used by geolocation.
class CefAccessTokenStore : public device::AccessTokenStore {
 public:
  // |system_context| is used by NetworkLocationProvider to communicate with a
  // remote geolocation service.
  explicit CefAccessTokenStore(net::URLRequestContextGetter* system_context)
      : system_context_(system_context) {}

  void LoadAccessTokens(const LoadAccessTokensCallback& callback) override {
    callback.Run(access_token_map_, system_context_);
  }

  void SaveAccessToken(
      const GURL& server_url, const base::string16& access_token) override {
    access_token_map_[server_url] = access_token;
  }

 private:
  net::URLRequestContextGetter* system_context_;
  AccessTokenMap access_token_map_;

  DISALLOW_COPY_AND_ASSIGN(CefAccessTokenStore);
};

// A provider of services for geolocation.
class CefGeolocationDelegate : public device::GeolocationDelegate {
 public:
  explicit CefGeolocationDelegate(net::URLRequestContextGetter* system_context)
      : system_context_(system_context) {}

  scoped_refptr<device::AccessTokenStore> CreateAccessTokenStore() override {
    return new CefAccessTokenStore(system_context_);
  }

 private:
  net::URLRequestContextGetter* system_context_;

  DISALLOW_COPY_AND_ASSIGN(CefGeolocationDelegate);
};
  
}  // namespace

CefBrowserMainParts::CefBrowserMainParts(
    const content::MainFunctionParams& parameters)
    : BrowserMainParts(),
      devtools_delegate_(NULL) {
}

CefBrowserMainParts::~CefBrowserMainParts() {
}

void CefBrowserMainParts::PreMainMessageLoopStart() {
  if (!base::MessageLoop::current()) {
    // Create the browser message loop.
    message_loop_.reset(new CefBrowserMessageLoop());
  }
}

void CefBrowserMainParts::PreEarlyInitialization() {
#if defined(USE_AURA) && defined(OS_LINUX)
  // TODO(linux): Consider using a real input method or
  // views::LinuxUI::SetInstance.
  ui::InitializeInputMethodForTesting();
#endif
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

  // Initialize the GpuDataManager before IO access restrictions are applied and
  // before the IO thread is started.
  content::GpuDataManager::GetInstance();

#if defined(USE_AURA)
  display::Screen::SetScreenInstance(views::CreateDesktopScreen());
#endif

  return 0;
}

void CefBrowserMainParts::PreMainMessageLoopRun() {
  if (extensions::ExtensionsEnabled()) {
    // Initialize extension global objects before creating the global
    // BrowserContext.
    extensions_client_.reset(new extensions::CefExtensionsClient());
    extensions::ExtensionsClient::Set(extensions_client_.get());
    extensions_browser_client_.reset(new extensions::CefExtensionsBrowserClient);
    extensions::ExtensionsBrowserClient::Set(extensions_browser_client_.get());

    // Register additional KeyedService factories here. See
    // ChromeBrowserMainExtraPartsProfiles for details.
    extensions::cef::EnsureBrowserContextKeyedServiceFactoriesBuilt();
    extensions::CefExtensionSystemFactory::GetInstance();
  }

  printing::CefPrintingMessageFilter::EnsureShutdownNotifierFactoryBuilt();

  CefRequestContextSettings settings;
  CefContext::Get()->PopulateRequestContextSettings(&settings);

  // Create the global BrowserContext.
  global_browser_context_.reset(new CefBrowserContextImpl(settings));
  global_browser_context_->Initialize();

  CefDevToolsManagerDelegate::StartHttpHandler(global_browser_context_.get());

  // Triggers initialization of the singleton instance on UI thread.
  PluginFinder::GetInstance()->Init();

  device::GeolocationProvider::SetGeolocationDelegate(
      new CefGeolocationDelegate(
          global_browser_context_->request_context().get()));

  scheme::RegisterWebUIControllerFactory();
}

void CefBrowserMainParts::PostMainMessageLoopRun() {
  // NOTE: Destroy objects in reverse order of creation.
  CefDevToolsManagerDelegate::StopHttpHandler();

  global_browser_context_.reset(nullptr);

  if (extensions::ExtensionsEnabled()) {
    extensions::ExtensionsBrowserClient::Set(NULL);
    extensions_browser_client_.reset();
  }
}

void CefBrowserMainParts::PostDestroyThreads() {
#if defined(USE_AURA)
  // Delete the DesktopTestViewsDelegate.
  delete views::ViewsDelegate::GetInstance();
#endif
}
