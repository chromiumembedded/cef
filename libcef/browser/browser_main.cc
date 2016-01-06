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
#include "libcef/browser/devtools_delegate.h"
#include "libcef/browser/extensions/browser_context_keyed_service_factories.h"
#include "libcef/browser/extensions/extensions_browser_client.h"
#include "libcef/browser/extensions/extension_system_factory.h"
#include "libcef/browser/thread_util.h"
#include "libcef/common/extensions/extensions_client.h"
#include "libcef/common/extensions/extensions_util.h"
#include "libcef/common/net/net_resource_provider.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/plugins/plugin_finder.h"
#include "content/browser/webui/content_web_ui_controller_factory.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/browser/web_ui_controller_factory.h"
#include "content/public/common/content_switches.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/constants.h"
#include "net/base/net_module.h"
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
  gfx::Screen::SetScreenInstance(gfx::SCREEN_TYPE_NATIVE,
                                 views::CreateDesktopScreen());
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

  CefRequestContextSettings settings;
  CefContext::Get()->PopulateRequestContextSettings(&settings);

  // Create the global BrowserContext.
  global_browser_context_ = new CefBrowserContextImpl(settings);
  global_browser_context_->Initialize();

  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kRemoteDebuggingPort)) {
    std::string port_str =
        command_line->GetSwitchValueASCII(switches::kRemoteDebuggingPort);
    int port;
    if (base::StringToInt(port_str, &port) && port > 0 && port < 65535) {
      devtools_delegate_ =
          new CefDevToolsDelegate(static_cast<uint16_t>(port));
    } else {
      LOG(WARNING) << "Invalid http debugger port number " << port;
    }
  }

  // Triggers initialization of the singleton instance on UI thread.
  PluginFinder::GetInstance()->Init();

#if defined(OS_WIN)
  PlatformPreMainMessageLoopRun();
#endif
}

void CefBrowserMainParts::PostMainMessageLoopRun() {
  if (extensions::ExtensionsEnabled()) {
    extensions::ExtensionsBrowserClient::Set(NULL);
    extensions_browser_client_.reset();
  }

  if (devtools_delegate_) {
    devtools_delegate_->Stop();
    devtools_delegate_ = NULL;
  }

  global_browser_context_ = NULL;

#ifndef NDEBUG
  // No CefBrowserContext instances should exist at this point.
  DCHECK_EQ(0, CefBrowserContext::DebugObjCt);
#endif
}

void CefBrowserMainParts::PostDestroyThreads() {
#if defined(USE_AURA)
  aura::Env::DeleteInstance();

  // Delete the DesktopTestViewsDelegate.
  delete views::ViewsDelegate::GetInstance();
#endif

#ifndef NDEBUG
  // No CefURLRequestContext instances should exist at this point.
  DCHECK_EQ(0, CefURLRequestContext::DebugObjCt);
#endif
}
