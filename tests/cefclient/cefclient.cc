// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "cefclient/cefclient.h"
#include <stdio.h>
#include <cstdlib>
#include <sstream>
#include <string>
#include "include/cef_app.h"
#include "include/cef_browser.h"
#include "include/cef_command_line.h"
#include "include/cef_frame.h"
#include "include/cef_web_plugin.h"
#include "include/wrapper/cef_helpers.h"
#include "cefclient/client_handler.h"
#include "cefclient/client_switches.h"

CefRefPtr<ClientHandler> g_handler;

namespace {

CefRefPtr<CefCommandLine> g_command_line;
int g_offscreen_state = 0;

}  // namespace

CefRefPtr<CefBrowser> AppGetBrowser() {
  if (!g_handler.get())
    return NULL;
  return g_handler->GetBrowser();
}

ClientWindowHandle AppGetMainWindowHandle() {
  if (!g_handler.get())
    return kNullWindowHandle;
  return g_handler->GetMainWindowHandle();
}

void AppInitCommandLine(int argc, const char* const* argv) {
  g_command_line = CefCommandLine::CreateCommandLine();
#if defined(OS_WIN)
  g_command_line->InitFromString(::GetCommandLineW());
#else
  g_command_line->InitFromArgv(argc, argv);
#endif
}

// Returns the application command line object.
CefRefPtr<CefCommandLine> AppGetCommandLine() {
  return g_command_line;
}

// Returns the application settings based on command line arguments.
void AppGetSettings(CefSettings& settings) {
  DCHECK(g_command_line.get());
  if (!g_command_line.get())
    return;

  CefString str;

#if defined(OS_WIN)
  settings.multi_threaded_message_loop =
      g_command_line->HasSwitch(cefclient::kMultiThreadedMessageLoop);
#endif

  CefString(&settings.cache_path) =
      g_command_line->GetSwitchValue(cefclient::kCachePath);

  if (g_command_line->HasSwitch(cefclient::kOffScreenRenderingEnabled))
    settings.windowless_rendering_enabled = true;
}

void AppGetBrowserSettings(CefBrowserSettings& settings) {
  DCHECK(g_command_line.get());
  if (!g_command_line.get())
    return;

  if (g_command_line->HasSwitch(cefclient::kOffScreenFrameRate)) {
    settings.windowless_frame_rate = atoi(g_command_line->
        GetSwitchValue(cefclient::kOffScreenFrameRate).ToString().c_str());
  }
}

bool AppIsOffScreenRenderingEnabled() {
  if (g_offscreen_state == 0) {
    // Store the value so it isn't queried multiple times.
    DCHECK(g_command_line.get());
    g_offscreen_state =
        g_command_line->HasSwitch(cefclient::kOffScreenRenderingEnabled) ?
            1 : 2;
  }

  return (g_offscreen_state == 1);
}
