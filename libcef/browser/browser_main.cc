// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/browser_main.h"
#include "libcef/browser/browser_context.h"
#include "libcef/browser/browser_message_loop.h"
#include "libcef/browser/content_browser_client.h"
#include "libcef/browser/devtools_delegate.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/string_number_conversions.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "content/browser/browser_process_sub_thread.h"
#include "content/browser/download/download_file_manager.h"
#include "content/browser/download/save_file_manager.h"
#include "content/browser/plugin_service_impl.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "net/base/net_module.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

base::StringPiece ResourceProvider(int resource_id) {
  return content::GetContentClient()->GetDataResource(resource_id);
}

}

CefBrowserMainParts::CefBrowserMainParts(
    const content::MainFunctionParams& parameters)
    : BrowserMainParts(),
      devtools_delegate_(NULL) {
  CefContentBrowserClient* browser_client =
      static_cast<CefContentBrowserClient*>(
          content::GetContentClient()->browser());
  browser_client->set_browser_main_parts(this);
}

CefBrowserMainParts::~CefBrowserMainParts() {
}

MessageLoop* CefBrowserMainParts::GetMainMessageLoop() {
  return new CefBrowserMessageLoop();
}

int CefBrowserMainParts::PreCreateThreads() {
  return 0;
}

void CefBrowserMainParts::PreMainMessageLoopRun() {
  browser_context_.reset(new CefBrowserContext(this));
  
  PlatformInitialize();
  net::NetModule::SetResourceProvider(&ResourceProvider);

  // Initialize the GpuDataManager before IO access restrictions are applied.
  content::GpuDataManager::GetInstance();

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kRemoteDebuggingPort)) {
    std::string port_str =
        command_line.GetSwitchValueASCII(switches::kRemoteDebuggingPort);
    int port;
    if (base::StringToInt(port_str, &port) && port > 0 && port < 65535) {
      devtools_delegate_ = new CefDevToolsDelegate(
          port,
          browser_context_->GetRequestContext());
    } else {
      DLOG(WARNING) << "Invalid http debugger port number " << port;
    }
  }
}

void CefBrowserMainParts::PostMainMessageLoopRun() {
  PlatformCleanup();

  if (devtools_delegate_)
    devtools_delegate_->Stop();
  browser_context_.reset();
}

bool CefBrowserMainParts::MainMessageLoopRun(int* result_code) {
  return false;
}

ui::Clipboard* CefBrowserMainParts::GetClipboard() {
  if (!clipboard_.get())
    clipboard_.reset(new ui::Clipboard());
  return clipboard_.get();
}
