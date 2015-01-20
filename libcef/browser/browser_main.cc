// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/browser_main.h"

#include <string>

#include "libcef/browser/browser_context_impl.h"
#include "libcef/browser/browser_message_loop.h"
#include "libcef/browser/content_browser_client.h"
#include "libcef/browser/devtools_delegate.h"
#include "libcef/common/net_resource_provider.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/net/proxy_service_factory.h"
#include "components/user_prefs/user_prefs.h"
#include "content/browser/webui/content_web_ui_controller_factory.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/browser/web_ui_controller_factory.h"
#include "content/public/common/content_switches.h"
#include "net/base/net_module.h"
#include "net/proxy/proxy_resolver_v8.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(USE_AURA)
#include "ui/aura/env.h"
#include "ui/gfx/screen.h"
#include "ui/views/test/desktop_test_views_delegate.h"
#include "ui/views/widget/desktop_aura/desktop_screen.h"

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
    message_loop_->set_thread_name("CrBrowserMain");
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

  DCHECK(!views::ViewsDelegate::views_delegate);
  new views::DesktopTestViewsDelegate;

#if defined(OS_WIN)
  ui::CursorLoaderWin::SetCursorResourceModule(
      CefContentBrowserClient::Get()->GetResourceDllName());
#endif
#endif  // defined(USE_AURA)
}

void CefBrowserMainParts::PostMainMessageLoopStart() {
  // Don't use the default WebUI controller factory because is conflicts with
  // CEF's internal handling of "chrome://tracing".
  content::WebUIControllerFactory::UnregisterFactoryForTesting(
      content::ContentWebUIControllerFactory::GetInstance());

#if defined(OS_LINUX)
  printing::PrintingContextLinux::SetCreatePrintDialogFunction(
      &CefPrintDialogLinux::CreatePrintDialog);
#endif
}

int CefBrowserMainParts::PreCreateThreads() {
  PlatformInitialize();
  net::NetModule::SetResourceProvider(&NetResourceProvider);

  // Initialize the GpuDataManager before IO access restrictions are applied and
  // before the IO thread is started.
  content::GpuDataManager::GetInstance();

#if defined(USE_AURA)
  gfx::Screen::SetScreenInstance(gfx::SCREEN_TYPE_NATIVE,
                                 views::CreateDesktopScreen());
#endif

  // Initialize user preferences.
  pref_store_ = new CefBrowserPrefStore();
  pref_store_->SetInitializationCompleted();
  pref_service_ = pref_store_->CreateService().Pass();

  // Initialize the V8 proxy integration.
  net::ProxyResolverV8::EnsureIsolateCreated();

  // Initialize proxy configuration tracker.
  pref_proxy_config_tracker_.reset(
      ProxyServiceFactory::CreatePrefProxyConfigTrackerOfLocalState(
          pref_service_.get()));

  return 0;
}

void CefBrowserMainParts::PreMainMessageLoopRun() {
  // Create the global browser context.
  global_browser_context_.reset(new CefBrowserContextImpl());

  // Initialize the proxy configuration service. This needs to occur before
  // CefURLRequestContextGetter::GetURLRequestContext() is called for the
  // first time.
  proxy_config_service_.reset(
      ProxyServiceFactory::CreateProxyConfigService(
          pref_proxy_config_tracker_.get()));

  // Initialize the request context getter. This indirectly triggers a call
  // to CefURLRequestContextGetter::GetURLRequestContext() on the IO thread.
  global_request_context_ = global_browser_context_->GetRequestContext();

  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kRemoteDebuggingPort)) {
    std::string port_str =
        command_line->GetSwitchValueASCII(switches::kRemoteDebuggingPort);
    int port;
    if (base::StringToInt(port_str, &port) && port > 0 && port < 65535) {
      devtools_delegate_ =
          new CefDevToolsDelegate(static_cast<uint16>(port));
    } else {
      LOG(WARNING) << "Invalid http debugger port number " << port;
    }
  }

  // Spell checking support and possibly other subsystems retrieve the
  // PrefService associated with a BrowserContext via UserPrefs::Get().
  user_prefs::UserPrefs::Set(browser_context(), pref_service());
}

void CefBrowserMainParts::PostMainMessageLoopRun() {
  if (devtools_delegate_) {
    devtools_delegate_->Stop();
    devtools_delegate_ = NULL;
  }
  pref_proxy_config_tracker_->DetachFromPrefService();

  // Only the global browser context should still exist.
  DCHECK(browser_contexts_.empty());
  browser_contexts_.clear();

  global_request_context_ = NULL;
  global_browser_context_.reset();
}

void CefBrowserMainParts::PostDestroyThreads() {
  pref_proxy_config_tracker_.reset(NULL);

#if defined(USE_AURA)
  aura::Env::DeleteInstance();
  delete views::ViewsDelegate::views_delegate;
#endif

  PlatformCleanup();
}

void CefBrowserMainParts::AddBrowserContext(CefBrowserContext* context) {
  // Spell checking support and possibly other subsystems retrieve the
  // PrefService associated with a BrowserContext via UserPrefs::Get().
  user_prefs::UserPrefs::Set(context, pref_service());

  browser_contexts_.push_back(context);
}

void CefBrowserMainParts::RemoveBrowserContext(CefBrowserContext* context) {
  ScopedVector<CefBrowserContext>::iterator it = browser_contexts_.begin();
  for (; it != browser_contexts_.end(); ++it) {
    if (*it == context) {
      browser_contexts_.erase(it);
      return;
    }
  }

  NOTREACHED();
}
