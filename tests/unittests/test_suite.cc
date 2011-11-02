// Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/cef.h"
#include "test_suite.h"
#include "base/command_line.h"
#include "build/build_config.h"
#include "base/threading/platform_thread.h"
#include "base/test/test_suite.h"

CefTestSuite::CefTestSuite(int argc, char** argv) : TestSuite(argc, argv) {
}

void CefTestSuite::Initialize() {
  TestSuite::Initialize();
    
  CefSettings settings;
  settings.multi_threaded_message_loop = true;

  CommandLine* command_line = CommandLine::ForCurrentProcess();

  std::string cache_path;
  if (GetCachePath(cache_path)) {
    // Set the cache_path value.
    CefString(&settings.cache_path).FromASCII(cache_path.c_str());
  }

  CefInitialize(settings);
}

void CefTestSuite::Shutdown() {
  // Delay a bit so that the system has a chance to finish destroying windows
  // before CefShutdown() checks for memory leaks.
  base::PlatformThread::Sleep(500);

  CefShutdown();
  TestSuite::Shutdown();
}

// static
bool CefTestSuite::GetCachePath(std::string& path) {
  CommandLine* command_line = CommandLine::ForCurrentProcess();

  if (command_line->HasSwitch("cache_path")) {
    // Set the cache_path value.
    path = command_line->GetSwitchValueASCII("cache_path");
    return true;
  }

  return false;
}
