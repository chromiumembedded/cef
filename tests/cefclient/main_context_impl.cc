// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "cefclient/main_context_impl.h"

#include "cefclient/client_switches.h"

#if defined(OS_WIN)
#include "cefclient/root_window_manager.h"
#endif
namespace client {

namespace {

// The default URL to load in a browser window.
const char kDefaultUrl[] = "http://www.google.com";

}  // namespace

MainContextImpl::MainContextImpl(int argc,
                                 const char* const* argv
#if defined(OS_WIN)
                                 , bool terminate_when_all_windows_closed
#endif
                  ) {
  // Parse the command line.
  command_line_ = CefCommandLine::CreateCommandLine();
#if defined(OS_WIN)
  command_line_->InitFromString(::GetCommandLineW());
#else
  command_line_->InitFromArgv(argc, argv);
#endif

  // Set the main URL.
  if (command_line_->HasSwitch(switches::kUrl))
    main_url_ = command_line_->GetSwitchValue(switches::kUrl);
  if (main_url_.empty())
    main_url_ = kDefaultUrl;

#if defined(OS_WIN)
  root_window_manager_.reset(
      new RootWindowManager(terminate_when_all_windows_closed));
#endif
}

std::string MainContextImpl::GetConsoleLogPath() {
  return GetAppWorkingDirectory() + "console.log";
}

std::string MainContextImpl::GetMainURL() {
  return main_url_;
}

void MainContextImpl::PopulateSettings(CefSettings* settings) {
#if defined(OS_WIN)
  settings->multi_threaded_message_loop =
      command_line_->HasSwitch(switches::kMultiThreadedMessageLoop);
#endif

  CefString(&settings->cache_path) =
      command_line_->GetSwitchValue(switches::kCachePath);

  if (command_line_->HasSwitch(switches::kOffScreenRenderingEnabled))
    settings->windowless_rendering_enabled = true;
}

void MainContextImpl::PopulateBrowserSettings(CefBrowserSettings* settings) {
  if (command_line_->HasSwitch(switches::kOffScreenFrameRate)) {
    settings->windowless_frame_rate = atoi(command_line_->
        GetSwitchValue(switches::kOffScreenFrameRate).ToString().c_str());
  }
}

#if defined(OS_WIN)
RootWindowManager* MainContextImpl::GetRootWindowManager() {
  return root_window_manager_.get();
}
#endif

}  // namespace client
