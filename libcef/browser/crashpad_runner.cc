// Copyright 2020 The Chromium Embedded Framework Authors.
// Portions copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef/browser/crashpad_runner.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/crashpad/crashpad/handler/handler_main.h"

namespace crashpad_runner {

// Based on components/crash/core/app/run_as_crashpad_handler_win.cc
int RunAsCrashpadHandler(const base::CommandLine& command_line) {
  // Remove the "--type=crashpad-handler" command-line flag that will otherwise
  // confuse the crashpad handler.
  base::CommandLine::StringVector argv = command_line.argv();
  const base::CommandLine::StringType process_type =
      FILE_PATH_LITERAL("--type=");
  argv.erase(
      std::remove_if(argv.begin(), argv.end(),
                     [&process_type](const base::CommandLine::StringType& str) {
                       return base::StartsWith(str, process_type,
                                               base::CompareCase::SENSITIVE) ||
                              (!str.empty() && str[0] == L'/');
                     }),
      argv.end());

#if BUILDFLAG(IS_POSIX)
  // HandlerMain on POSIX uses the system version of getopt_long which expects
  // the first argument to be the program name.
  argv.insert(argv.begin(), command_line.GetProgram().value());
#endif

  auto argv_as_utf8 = std::make_unique<char*[]>(argv.size() + 1);
  std::vector<std::string> storage;
  storage.reserve(argv.size());
  for (size_t i = 0; i < argv.size(); ++i) {
#if BUILDFLAG(IS_WIN)
    storage.push_back(base::WideToUTF8(argv[i]));
#else
    storage.push_back(argv[i]);
#endif
    argv_as_utf8[i] = &storage[i][0];
  }
  argv_as_utf8[argv.size()] = nullptr;
  argv.clear();
  return crashpad::HandlerMain(static_cast<int>(storage.size()),
                               argv_as_utf8.get(), nullptr);
}

}  // namespace crashpad_runner
