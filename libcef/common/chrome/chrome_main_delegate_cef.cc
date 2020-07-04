// Copyright 2020 The Chromium Embedded Framework Authors.
// Portions copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/common/chrome/chrome_main_delegate_cef.h"

#include "libcef/browser/chrome/chrome_browser_context.h"
#include "libcef/browser/chrome/chrome_content_browser_client_cef.h"

ChromeMainDelegateCef::ChromeMainDelegateCef(CefMainRunnerHandler* runner,
                                             CefRefPtr<CefApp> application)
    : ChromeMainDelegate(base::TimeTicks::Now()),
      runner_(runner),
      application_(application) {}
ChromeMainDelegateCef::~ChromeMainDelegateCef() = default;

void ChromeMainDelegateCef::PreCreateMainMessageLoop() {
  // The parent ChromeMainDelegate implementation creates the NSApplication
  // instance on macOS, and we intentionally don't want to do that here.
  runner_->PreCreateMainMessageLoop();
}

int ChromeMainDelegateCef::RunProcess(
    const std::string& process_type,
    const content::MainFunctionParams& main_function_params) {
  if (process_type.empty()) {
    return runner_->RunMainProcess(main_function_params);
  }

  return ChromeMainDelegate::RunProcess(process_type, main_function_params);
}

content::ContentClient* ChromeMainDelegateCef::CreateContentClient() {
  return &chrome_content_client_cef_;
}

content::ContentBrowserClient*
ChromeMainDelegateCef::CreateContentBrowserClient() {
  // Match the logic in the parent ChromeMainDelegate implementation, but create
  // our own object type.
  if (chrome_content_browser_client_ == nullptr) {
    DCHECK(!startup_data_);
    startup_data_ = std::make_unique<StartupData>();

    chrome_content_browser_client_ =
        std::make_unique<ChromeContentBrowserClientCef>(startup_data_.get());
  }
  return chrome_content_browser_client_.get();
}

CefRefPtr<CefRequestContext> ChromeMainDelegateCef::GetGlobalRequestContext() {
  auto browser_client = content_browser_client();
  if (browser_client)
    return browser_client->request_context();
  return nullptr;
}

CefBrowserContext* ChromeMainDelegateCef::CreateNewBrowserContext(
    const CefRequestContextSettings& settings) {
  auto context = new ChromeBrowserContext(settings);
  context->Initialize();
  return context;
}

scoped_refptr<base::SingleThreadTaskRunner>
ChromeMainDelegateCef::GetBackgroundTaskRunner() {
  auto browser_client = content_browser_client();
  if (browser_client)
    return browser_client->background_task_runner();
  return nullptr;
}

scoped_refptr<base::SingleThreadTaskRunner>
ChromeMainDelegateCef::GetUserVisibleTaskRunner() {
  auto browser_client = content_browser_client();
  if (browser_client)
    return browser_client->user_visible_task_runner();
  return nullptr;
}

scoped_refptr<base::SingleThreadTaskRunner>
ChromeMainDelegateCef::GetUserBlockingTaskRunner() {
  auto browser_client = content_browser_client();
  if (browser_client)
    return browser_client->user_blocking_task_runner();
  return nullptr;
}

scoped_refptr<base::SingleThreadTaskRunner>
ChromeMainDelegateCef::GetRenderTaskRunner() {
  // TODO: Implement.
  NOTREACHED();
  return nullptr;
}

scoped_refptr<base::SingleThreadTaskRunner>
ChromeMainDelegateCef::GetWebWorkerTaskRunner() {
  // TODO: Implement.
  NOTREACHED();
  return nullptr;
}

ChromeContentBrowserClientCef* ChromeMainDelegateCef::content_browser_client()
    const {
  return static_cast<ChromeContentBrowserClientCef*>(
      chrome_content_browser_client_.get());
}
