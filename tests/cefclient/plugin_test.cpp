// Copyright (c) 2008-2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/cef.h"
#include "clientplugin.h"

void InitPluginTest()
{
  // Structure providing information about the client plugin.
  CefPluginInfo plugin_info;
  plugin_info.display_name = "Client Plugin";
  plugin_info.unique_name = "client_plugin";
  plugin_info.version = "1, 0, 0, 1";
  plugin_info.description = "My Example Client Plugin";

  CefPluginMimeType mime_type;
  mime_type.mime_type = "application/x-client-plugin";
  mime_type.file_extensions.push_back("*");
  plugin_info.mime_types.push_back(mime_type);

  plugin_info.np_getentrypoints = NP_GetEntryPoints;
  plugin_info.np_initialize = NP_Initialize;
  plugin_info.np_shutdown = NP_Shutdown;

  // Register the internal client plugin
  CefRegisterPlugin(plugin_info);
}

void RunPluginTest(CefRefPtr<CefBrowser> browser)
{
  std::string html =
    "<html><body>Client Plugin:<br>"
    "<embed type=\"application/x-client-plugin\""
    "width=600 height=40>"
    "</body></html>";
  browser->GetMainFrame()->LoadString(html, "about:blank");
}
