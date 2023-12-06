// Copyright 2016 The Chromium Embedded Framework Authors. Portions copyright
// 2016 The Chromium Authors. All rights reserved. Use of this source code is
// governed by a BSD-style license that can be found in the LICENSE file.

#include <string>

#include "base/strings/string_piece.h"
#include "build/build_config.h"

namespace base {
class CommandLine;
}

namespace crash_reporting {

// Returns true if crash reporting is enabled.
bool Enabled();

// Set or clear a crash key value.
bool SetCrashKeyValue(const base::StringPiece& key,
                      const base::StringPiece& value);

// Functions are called from similarly named methods in AlloyMainDelegate.

#if BUILDFLAG(IS_POSIX)
void BasicStartupComplete(base::CommandLine* command_line);
#endif

void PreSandboxStartup(const base::CommandLine& command_line,
                       const std::string& process_type);

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_MAC)
void ZygoteForked(base::CommandLine* command_line,
                  const std::string& process_type);
#endif

}  // namespace crash_reporting
