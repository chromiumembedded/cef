// Copyright 2020 The Chromium Embedded Framework Authors.
// Portions copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/chrome/chrome_content_browser_client_cef.h"

#include "libcef/browser/chrome/chrome_browser_main_extra_parts_cef.h"
#include "libcef/common/cef_switches.h"

#include "base/command_line.h"
#include "chrome/browser/chrome_browser_main.h"

ChromeContentBrowserClientCef::ChromeContentBrowserClientCef(
    StartupData* startup_data)
    : ChromeContentBrowserClient(startup_data) {}
ChromeContentBrowserClientCef::~ChromeContentBrowserClientCef() = default;

std::unique_ptr<content::BrowserMainParts>
ChromeContentBrowserClientCef::CreateBrowserMainParts(
    const content::MainFunctionParams& parameters) {
  auto main_parts =
      ChromeContentBrowserClient::CreateBrowserMainParts(parameters);
  browser_main_parts_ = new ChromeBrowserMainExtraPartsCef;
  static_cast<ChromeBrowserMainParts*>(main_parts.get())
      ->AddParts(browser_main_parts_);
  return main_parts;
}

void ChromeContentBrowserClientCef::AppendExtraCommandLineSwitches(
    base::CommandLine* command_line,
    int child_process_id) {
  ChromeContentBrowserClient::AppendExtraCommandLineSwitches(command_line,
                                                             child_process_id);

  // Necessary to launch sub-processes in the correct mode.
  command_line->AppendSwitch(switches::kEnableChromeRuntime);
}

CefRefPtr<CefRequestContextImpl>
ChromeContentBrowserClientCef::request_context() const {
  return browser_main_parts_->request_context();
}

scoped_refptr<base::SingleThreadTaskRunner>
ChromeContentBrowserClientCef::background_task_runner() const {
  return browser_main_parts_->background_task_runner();
}

scoped_refptr<base::SingleThreadTaskRunner>
ChromeContentBrowserClientCef::user_visible_task_runner() const {
  return browser_main_parts_->user_visible_task_runner();
}

scoped_refptr<base::SingleThreadTaskRunner>
ChromeContentBrowserClientCef::user_blocking_task_runner() const {
  return browser_main_parts_->user_blocking_task_runner();
}
