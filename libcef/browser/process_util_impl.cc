// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/cef_process_util.h"
#include "libcef/common/command_line_impl.h"

#include "base/logging.h"
#include "base/notreached.h"
#include "base/process/launch.h"
#include "content/public/browser/child_process_launcher_utils.h"

bool CefLaunchProcess(CefRefPtr<CefCommandLine> command_line) {
  if (!command_line.get()) {
    DCHECK(false) << "invalid parameter";
    return false;
  }

  if (!content::CurrentlyOnProcessLauncherTaskRunner()) {
    DCHECK(false) << "called on invalid thread";
    return false;
  }

  CefCommandLineImpl* impl =
      static_cast<CefCommandLineImpl*>(command_line.get());

  CefValueController::AutoLock lock_scope(impl->controller());

  base::LaunchOptions options;
  return base::LaunchProcess(impl->command_line(), options).IsValid();
}
