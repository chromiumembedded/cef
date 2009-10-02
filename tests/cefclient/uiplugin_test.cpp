// Copyright (c) 2008-2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "uiplugin_test.h"
#include "uiplugin.h"
#include "cefclient.h"


void InitUIPluginTest()
{
  // Structure providing information about the client plugin.
  CefPluginInfo plugin_info;
  plugin_info.display_name = L"Client UI Plugin";
  plugin_info.unique_name = L"client_ui_plugin";
  plugin_info.version = L"1, 0, 0, 1";
  plugin_info.description = L"My Example Client UI Plugin";

  CefPluginMimeType mime_type;
  mime_type.mime_type = L"application/x-client-ui-plugin";
  mime_type.file_extensions.push_back(L"*");
  plugin_info.mime_types.push_back(mime_type);

  plugin_info.np_getentrypoints = NP_UIGetEntryPoints;
  plugin_info.np_initialize = NP_UIInitialize;
  plugin_info.np_shutdown = NP_UIShutdown;

  // Register the internal client plugin
  CefRegisterPlugin(plugin_info);
}

// Implementation of the V8 handler class for the "window.uiapp" functions.
class ClientV8UIHandler : public CefThreadSafeBase<CefV8Handler>
{
public:
  ClientV8UIHandler() {}

  // Execute with the specified argument list and return value.  Return true if
  // the method was handled.
  virtual bool Execute(const std::wstring& name,
                       CefRefPtr<CefV8Value> object,
                       const CefV8ValueList& arguments,
                       CefRefPtr<CefV8Value>& retval,
                       std::wstring& exception)
  {
    if(name == L"modifyRotation") {
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
    } else if(name == L"resetRotation") {
      // Reset the rotation value.
      ResetRotation();
      return true;
    } else if(name == L"viewSource") {
      // View the page source.
      AppGetBrowser()->GetMainFrame()->ViewSource();
      return true;
    }

    return false;
  }
};

void InitUIBindingTest(CefRefPtr<CefBrowser> browser,
                       CefRefPtr<CefFrame> frame,
                       CefRefPtr<CefV8Value> object)
{
  if(frame->GetURL().find(L"http://tests/uiapp") != 0)
    return;
      
  // Create the new V8 object.
  CefRefPtr<CefV8Value> testObjPtr = CefV8Value::CreateObject(NULL);
  // Add the new V8 object to the global window object with the name "uiapp".
  object->SetValue(L"uiapp", testObjPtr);

  // Create an instance of ClientV8UIHandler as the V8 handler.
  CefRefPtr<CefV8Handler> handlerPtr = new ClientV8UIHandler();

  // Add a new V8 function to the uiapp object with the name "modifyRotation".
  testObjPtr->SetValue(L"modifyRotation",
      CefV8Value::CreateFunction(L"modifyRotation", handlerPtr));
  // Add a new V8 function to the uiapp object with the name "resetRotation".
  testObjPtr->SetValue(L"resetRotation",
      CefV8Value::CreateFunction(L"resetRotation", handlerPtr));
  // Add a new V8 function to the uiapp object with the name "viewSource".
  testObjPtr->SetValue(L"viewSource",
      CefV8Value::CreateFunction(L"viewSource", handlerPtr));
}

void RunUIPluginTest(CefRefPtr<CefBrowser> browser)
{
  browser->GetMainFrame()->LoadURL(L"http://tests/uiapp");
}
