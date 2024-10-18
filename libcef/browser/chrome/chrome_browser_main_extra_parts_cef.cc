// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef/browser/chrome/chrome_browser_main_extra_parts_cef.h"

#include "base/task/thread_pool.h"
#include "cef/libcef/browser/alloy/dialogs/alloy_constrained_window_views_client.h"
#include "cef/libcef/browser/chrome/chrome_context_menu_handler.h"
#include "cef/libcef/browser/chrome/chrome_startup_browser_creator.h"
#include "cef/libcef/browser/context.h"
#include "cef/libcef/browser/file_dialog_runner.h"
#include "cef/libcef/browser/permission_prompt.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/chrome_constrained_window_views_client.h"
#include "components/constrained_window/constrained_window_views.h"

#if BUILDFLAG(IS_LINUX)
#include "base/linux_util.h"
#include "cef/libcef/browser/printing/print_dialog_linux.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/win/app_icon.h"
#endif

ChromeBrowserMainExtraPartsCef::ChromeBrowserMainExtraPartsCef() = default;

ChromeBrowserMainExtraPartsCef::~ChromeBrowserMainExtraPartsCef() = default;

void ChromeBrowserMainExtraPartsCef::PostProfileInit(Profile* profile,
                                                     bool is_initial_profile) {
  if (!is_initial_profile) {
    return;
  }

  CefRequestContextSettings settings;
  CefContext::Get()->PopulateGlobalRequestContextSettings(&settings);

  // Use the existing path for the initial profile.
  CefString(&settings.cache_path) = profile->GetPath().value();

  // Create the global RequestContext.
  global_request_context_ =
      CefRequestContextImpl::CreateGlobalRequestContext(settings);
}

void ChromeBrowserMainExtraPartsCef::PostBrowserStart() {
  // Register the callback before ChromeBrowserMainParts::PostBrowserStart
  // allows ProcessSingleton to begin processing messages.
  startup_browser_creator::RegisterProcessCommandLineCallback();

#if BUILDFLAG(IS_LINUX)
  // This may be called indirectly via StartupBrowserCreator::LaunchBrowser.
  // Call it here before blocking is disallowed to avoid assertions.
  base::GetLinuxDistro();
#endif
}

void ChromeBrowserMainExtraPartsCef::PreMainMessageLoopRun() {
  background_task_runner_ = base::ThreadPool::CreateSingleThreadTaskRunner(
      {base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN, base::MayBlock()});
  user_visible_task_runner_ = base::ThreadPool::CreateSingleThreadTaskRunner(
      {base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN, base::MayBlock()});
  user_blocking_task_runner_ = base::ThreadPool::CreateSingleThreadTaskRunner(
      {base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN, base::MayBlock()});

  context_menu::RegisterCallbacks();
  file_dialog_runner::RegisterFactory();
  permission_prompt::RegisterCreateCallback();

#if BUILDFLAG(IS_WIN)
  const auto& settings = CefContext::Get()->settings();
  if (settings.chrome_app_icon_id > 0) {
    SetExeAppIconResourceId(settings.chrome_app_icon_id);
  }
#endif
}

void ChromeBrowserMainExtraPartsCef::ToolkitInitialized() {
  // Override the default Chrome client.
  SetConstrainedWindowViewsClient(CreateAlloyConstrainedWindowViewsClient(
      CreateChromeConstrainedWindowViewsClient()));

#if BUILDFLAG(IS_LINUX)
  auto printing_delegate = new CefPrintingContextLinuxDelegate();
  auto default_delegate =
      ui::PrintingContextLinuxDelegate::SetInstance(printing_delegate);
  printing_delegate->SetDefaultDelegate(default_delegate);
#endif  // BUILDFLAG(IS_LINUX)
}
