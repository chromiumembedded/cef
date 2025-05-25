// Copyright 2020 The Chromium Embedded Framework Authors.
// Portions copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_CRASHPAD_RUNNER_H_
#define CEF_LIBCEF_BROWSER_CRASHPAD_RUNNER_H_

#include "base/command_line.h"

namespace crashpad_runner {

// Chrome uses an embedded crashpad handler on Windows only and imports this
// function via the existing "run_as_crashpad_handler" target defined in
// components/crash/core/app/BUILD.gn. CEF uses an embedded handler on all
// platforms so we define the function here instead of using the existing
// target (because we can't use that target on macOS).
int RunAsCrashpadHandler(const base::CommandLine& command_line);

}  // namespace crashpad_runner

#endif  // CEF_LIBCEF_BROWSER_CRASHPAD_RUNNER_H_
