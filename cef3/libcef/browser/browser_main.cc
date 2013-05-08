// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/browser_main.h"

#include <string>

#include "libcef/browser/browser_context.h"
#include "libcef/browser/browser_message_loop.h"
#include "libcef/browser/context.h"
#include "libcef/browser/devtools_delegate.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/string_number_conversions.h"
#include "chrome/browser/net/proxy_service_factory.h"
#include "content/browser/webui/content_web_ui_controller_factory.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/browser/web_ui_controller_factory.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "net/base/net_module.h"
#include "net/proxy/proxy_resolver_v8.h"
#include "ui/base/resource/resource_bundle.h"
#include "v8/include/v8.h"

namespace {

base::StringPiece ResourceProvider(int resource_id) {
  return content::GetContentClient()->GetDataResource(resource_id,
                                                      ui::SCALE_FACTOR_NONE);
}

}  // namespace

CefBrowserMainParts::CefBrowserMainParts(
    const content::MainFunctionParams& parameters)
    : BrowserMainParts(),
      devtools_delegate_(NULL),
      proxy_v8_isolate_(NULL) {
}

CefBrowserMainParts::~CefBrowserMainParts() {
}

void CefBrowserMainParts::PreMainMessageLoopStart() {
  if (!MessageLoop::current()) {
    // Create the browser message loop.
    message_loop_.reset(new CefBrowserMessageLoop());
    message_loop_->set_thread_name("CrBrowserMain");
  }
}

void CefBrowserMainParts::PostMainMessageLoopStart() {
  // Don't use the default WebUI controller factory because is conflicts with
  // CEF's internal handling of "chrome://tracing".
  content::WebUIControllerFactory::UnregisterFactoryForTesting(
      content::ContentWebUIControllerFactory::GetInstance());
}

int CefBrowserMainParts::PreCreateThreads() {
  PlatformInitialize();
  net::NetModule::SetResourceProvider(&ResourceProvider);

  // Initialize the GpuDataManager before IO access restrictions are applied and
  // before the IO thread is started.
  content::GpuDataManager::GetInstance();

  // Initialize user preferences.
  pref_store_ = new CefBrowserPrefStore();
  pref_store_->SetInitializationCompleted();
  pref_service_.reset(pref_store_->CreateService());

  // Create a v8::Isolate for the current thread if it doesn't already exist.
  if (!v8::Isolate::GetCurrent()) {
    proxy_v8_isolate_ = v8::Isolate::New();
    proxy_v8_isolate_->Enter();
  }

  // Initialize the V8 proxy integration.
  net::ProxyResolverV8::RememberDefaultIsolate();

  // Initialize proxy configuration tracker.
  pref_proxy_config_tracker_.reset(
      ProxyServiceFactory::CreatePrefProxyConfigTracker(
          pref_service_.get()));

  return 0;
}

void CefBrowserMainParts::PreMainMessageLoopRun() {
  browser_context_.reset(new CefBrowserContext());

  // Initialize the proxy configuration service. This needs to occur before
  // CefURLRequestContextGetter::GetURLRequestContext() is called for the
  // first time.
  ChromeProxyConfigService* chrome_proxy_config_service =
      ProxyServiceFactory::CreateProxyConfigService();
  proxy_config_service_.reset(chrome_proxy_config_service);
  pref_proxy_config_tracker_->SetChromeProxyConfigService(
      chrome_proxy_config_service);

  // Initialize the request context getter. This indirectly triggers a call
  // to CefURLRequestContextGetter::GetURLRequestContext() on the IO thread.
  _Context->set_request_context(browser_context_->GetRequestContext());

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kRemoteDebuggingPort)) {
    std::string port_str =
        command_line.GetSwitchValueASCII(switches::kRemoteDebuggingPort);
    int port;
    if (base::StringToInt(port_str, &port) && port > 0 && port < 65535) {
      devtools_delegate_ = new CefDevToolsDelegate(port);
    } else {
      LOG(WARNING) << "Invalid http debugger port number " << port;
    }
  }
}

void CefBrowserMainParts::PostMainMessageLoopRun() {
  if (devtools_delegate_)
    devtools_delegate_->Stop();
  pref_proxy_config_tracker_->DetachFromPrefService();
  _Context->set_request_context(NULL);
  browser_context_.reset();
}

void CefBrowserMainParts::PostDestroyThreads() {
  pref_proxy_config_tracker_.reset(NULL);

  if (proxy_v8_isolate_) {
    proxy_v8_isolate_->Exit();
    proxy_v8_isolate_->Dispose();
  }

  PlatformCleanup();
}
