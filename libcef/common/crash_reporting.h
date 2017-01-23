// Copyright 2016 The Chromium Embedded Framework Authors. Portions copyright
// 2016 The Chromium Authors. All rights reserved. Use of this source code is
// governed by a BSD-style license that can be found in the LICENSE file.

#include <string>

#include "build/build_config.h"

namespace base {
class CommandLine;
}

namespace crash_reporting {

// Returns true if crash reporting is enabled.
bool Enabled();

// Functions are called from similarly named methods in CefMainDelegate.

#if defined(OS_POSIX)
void BasicStartupComplete(base::CommandLine* command_line);
#endif

void PreSandboxStartup(const base::CommandLine& command_line,
                       const std::string& process_type);

#if defined(OS_POSIX) && !defined(OS_ANDROID) && !defined(OS_MACOSX)
void ZygoteForked(base::CommandLine* command_line,
                  const std::string& process_type);
#endif

}  // namespace crash_reporting
