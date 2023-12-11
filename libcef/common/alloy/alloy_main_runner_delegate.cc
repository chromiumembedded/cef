// Copyright 2020 The Chromium Embedded Framework Authors.
// Portions copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/common/alloy/alloy_main_runner_delegate.h"

#include "libcef/browser/alloy/chrome_browser_process_alloy.h"
#include "libcef/common/alloy/alloy_main_delegate.h"
#include "libcef/renderer/alloy/alloy_content_renderer_client.h"

#include "chrome/browser/chrome_process_singleton.h"
#include "content/public/browser/render_process_host.h"
#include "ui/base/resource/resource_bundle.h"

AlloyMainRunnerDelegate::AlloyMainRunnerDelegate(CefMainRunnerHandler* runner,
                                                 CefSettings* settings,
                                                 CefRefPtr<CefApp> application)
    : runner_(runner), settings_(settings), application_(application) {}
AlloyMainRunnerDelegate::~AlloyMainRunnerDelegate() = default;

content::ContentMainDelegate*
AlloyMainRunnerDelegate::GetContentMainDelegate() {
  if (!main_delegate_) {
    main_delegate_ =
        std::make_unique<AlloyMainDelegate>(runner_, settings_, application_);
  }
  return main_delegate_.get();
}

void AlloyMainRunnerDelegate::BeforeMainThreadInitialize(
    const CefMainArgs& args) {
  g_browser_process = new ChromeBrowserProcessAlloy();
}

void AlloyMainRunnerDelegate::BeforeMainThreadRun(
    bool multi_threaded_message_loop) {
  static_cast<ChromeBrowserProcessAlloy*>(g_browser_process)->Initialize();
}

void AlloyMainRunnerDelegate::AfterUIThreadInitialize() {
  static_cast<ChromeBrowserProcessAlloy*>(g_browser_process)
      ->OnContextInitialized();
}

void AlloyMainRunnerDelegate::BeforeUIThreadShutdown() {
  static_cast<ChromeBrowserProcessAlloy*>(g_browser_process)
      ->CleanupOnUIThread();

  ui::ResourceBundle::GetSharedInstance().CleanupOnUIThread();
}

void AlloyMainRunnerDelegate::AfterUIThreadShutdown() {
  ChromeProcessSingleton::DeleteInstance();
}

void AlloyMainRunnerDelegate::BeforeMainThreadShutdown() {
  if (content::RenderProcessHost::run_renderer_in_process()) {
    // Blocks until RenderProcess cleanup is complete.
    AlloyContentRendererClient::Get()->RunSingleProcessCleanup();
  }
}

void AlloyMainRunnerDelegate::AfterMainThreadShutdown() {
  if (g_browser_process) {
    delete g_browser_process;
    g_browser_process = nullptr;
  }
}
