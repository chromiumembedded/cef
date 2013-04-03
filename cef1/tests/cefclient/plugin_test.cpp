// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/cef_base.h"
#include "include/cef_browser.h"
#include "include/cef_frame.h"
#include "include/cef_nplugin.h"
#include "cefclient/clientplugin.h"
#include "cefclient/client_handler.h"

namespace plugin_test {

namespace {

const char* kTestUrl = "http://tests/plugins";

// Handle resource loading in the browser process.
class RequestDelegate: public ClientHandler::RequestDelegate {
 public:
  RequestDelegate() {
  }

  // From ClientHandler::RequestDelegate.
  virtual bool OnBeforeResourceLoad(
      CefRefPtr<ClientHandler> handler,
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefRequest> request,
      CefString& redirectUrl,
      CefRefPtr<CefStreamReader>& resourceStream,
      CefRefPtr<CefResponse> response,
      int loadFlags) OVERRIDE {
    std::string url = request->GetURL();
    if (url == kTestUrl) {
      std::string html =
          "<html><body>\n"
          "Client Plugin loaded by Mime Type:<br>\n"
          "<embed type=\"application/x-client-plugin\" width=600 height=40>\n"
          "<br><br>Client Plugin loaded by File Extension:<br>\n"
          "<embed src=\"test.xcp\" width=600 height=40>\n"
          // Add some extra space below the plugin to allow scrolling.
          "<div style=\"height:1000px;\">&nbsp;</div>\n"
          "</body></html>";

      resourceStream = CefStreamReader::CreateForData(
          static_cast<void*>(const_cast<char*>(html.c_str())),
          html.size());
      response->SetMimeType("text/html");
      response->SetStatus(200);
    }

    return false;
  }

  IMPLEMENT_REFCOUNTING(RequestDelegate);
};

}  // namespace

void InitTest() {
  // Structure providing information about the client plugin.
  CefPluginInfo plugin_info;
  CefString(&plugin_info.display_name).FromASCII("Client Plugin");
  CefString(&plugin_info.unique_name).FromASCII("client_plugin");
  CefString(&plugin_info.description).FromASCII("My Example Client Plugin");
  CefString(&plugin_info.mime_types).FromASCII("application/x-client-plugin");
  CefString(&plugin_info.file_extensions).FromASCII("xcp");

  plugin_info.np_getentrypoints = NP_ClientGetEntryPoints;
  plugin_info.np_initialize = NP_ClientInitialize;
  plugin_info.np_shutdown = NP_ClientShutdown;

  // Register the internal client plugin
  CefRegisterPlugin(plugin_info);
}

void CreateRequestDelegates(ClientHandler::RequestDelegateSet& delegates) {
  delegates.insert(new RequestDelegate);
}

void RunTest(CefRefPtr<CefBrowser> browser) {
  // Page content is provided in ClientHandler::OnBeforeResourceLoad().
  browser->GetMainFrame()->LoadURL(kTestUrl);
}

}  // namespace plugin_test
