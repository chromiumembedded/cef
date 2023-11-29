// Copyright (c) 2023 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/browser/chrome/chrome_startup_browser_creator.h"

#include <tuple>

#include "libcef/browser/browser_context.h"
#include "libcef/common/app_manager.h"
#include "libcef/common/command_line_impl.h"

#include "chrome/browser/ui/startup/startup_browser_creator.h"

namespace startup_browser_creator {

namespace {

bool ProcessCommandLineCallback(const base::CommandLine& command_line,
                                const base::FilePath& cur_dir) {
  bool handled = false;

  if (auto app = CefAppManager::Get()->GetApplication()) {
    if (auto handler = app->GetBrowserProcessHandler()) {
      CefRefPtr<CefCommandLineImpl> commandLinePtr(
          new CefCommandLineImpl(command_line));
      handled = handler->OnAlreadyRunningAppRelaunch(commandLinePtr.get(),
                                                     cur_dir.value());
      std::ignore = commandLinePtr->Detach(nullptr);
    }
  }

  return handled;
}

}  // namespace

void RegisterProcessCommandLineCallback() {
  StartupBrowserCreator::RegisterProcessCommandLineCallback(
      base::BindRepeating(&ProcessCommandLineCallback));
}

}  // namespace startup_browser_creator
