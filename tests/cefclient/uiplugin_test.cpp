// Copyright (c) 2008-2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "uiplugin_test.h"
#include "uiplugin.h"
#include "cefclient.h"


// Implementation of the V8 handler class for the "window.uiapp" functions.
class ClientV8UIHandler : public CefV8Handler
{
public:
  ClientV8UIHandler() {}

  // Execute with the specified argument list and return value.  Return true if
  // the method was handled.
  virtual bool Execute(const CefString& name,
                       CefRefPtr<CefV8Value> object,
                       const CefV8ValueList& arguments,
                       CefRefPtr<CefV8Value>& retval,
                       CefString& exception)
  {
    if(name == "modifyRotation") {
      // This function requires one argument.
      if(arguments.size() != 1)
        return false;

      float increment = 0.;
      if(arguments[0]->IsInt()) {
        // The argument is an integer value.
        increment = static_cast<float>(arguments[0]->GetIntValue());
      } else if(arguments[0]->IsDouble()) {
        // The argument is an double value.
        increment = static_cast<float>(arguments[0]->GetDoubleValue());
      }

      if(increment != 0.) {
        // Modify the rotation accordingly.
        ModifyRotation(increment);
        return true;
      }
    } else if(name == "resetRotation") {
      // Reset the rotation value.
      ResetRotation();
      return true;
    } else if(name == "viewSource") {
      // View the page source.
      AppGetBrowser()->GetMainFrame()->ViewSource();
      return true;
    }

    return false;
  }

  IMPLEMENT_REFCOUNTING(ClientV8UIHandler);
};

void InitUIPluginTest()
{
  // Structure providing information about the client plugin.
  CefPluginInfo plugin_info;
  CefString(&plugin_info.display_name).FromASCII("Client UI Plugin");
  CefString(&plugin_info.unique_name).FromASCII("client_ui_plugin");
  CefString(&plugin_info.description).FromASCII("My Example Client UI Plugin");
  CefString(&plugin_info.mime_type).FromASCII("application/x-client-ui-plugin");

  plugin_info.np_getentrypoints = NP_UIGetEntryPoints;
  plugin_info.np_initialize = NP_UIInitialize;
  plugin_info.np_shutdown = NP_UIShutdown;

  // Register the internal client plugin
  CefRegisterPlugin(plugin_info);

  // Register a V8 extension with the below JavaScript code that calls native
  // methods implemented in ClientV8UIHandler.
  std::string code = "var cef;"
    "if (!cef)"
    "  cef = {};"
    "if (!cef.uiapp)"
    "  cef.uiapp = {};"
    "(function() {"
    "  cef.uiapp.modifyRotation = function(val) {"
    "    native function modifyRotation();"
    "    return modifyRotation(val);"
    "  };"
    "  cef.uiapp.resetRotation = function() {"
    "    native function resetRotation();"
    "    return resetRotation();"
    "  };"
    "  cef.uiapp.viewSource = function() {"
    "    native function viewSource();"
    "    return viewSource();"
    "  };"
    "})();";
  CefRegisterExtension("uiplugin/test", code, new ClientV8UIHandler());
}

void RunUIPluginTest(CefRefPtr<CefBrowser> browser)
{
  browser->GetMainFrame()->LoadURL("http://tests/uiapp");
}
