// Copyright (c) 2008-2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/cef.h"
#include "clientplugin.h"

void InitPluginTest()
{
  // Structure providing information about the client plugin.
  CefPluginInfo plugin_info;
  CefString(&plugin_info.display_name).FromASCII("Client Plugin");
  CefString(&plugin_info.unique_name).FromASCII("client_plugin");
  CefString(&plugin_info.description).FromASCII("My Example Client Plugin");
  CefString(&plugin_info.mime_type).FromASCII("application/x-client-plugin");

  plugin_info.np_getentrypoints = NP_ClientGetEntryPoints;
  plugin_info.np_initialize = NP_ClientInitialize;
  plugin_info.np_shutdown = NP_ClientShutdown;

  // Register the internal client plugin
  CefRegisterPlugin(plugin_info);
}

void RunPluginTest(CefRefPtr<CefBrowser> browser)
{
  // Add some extra space below the plugin to allow scrolling.
  std::string html =
    "<html><body>Client Plugin:<br>"
    "<embed type=\"application/x-client-plugin\" width=600 height=40>"
    "<div style=\"height:1000px;\">&nbsp;</div>"
    "</body></html>";
  browser->GetMainFrame()->LoadString(html, "about:blank");
}
