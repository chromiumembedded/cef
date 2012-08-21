// Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "osrplugin_test.h"
#include "osrplugin.h"
#include "cefclient.h"
#include "client_handler.h"
#include "plugin_test.h"

void InitOSRPluginTest()
{
  // Structure providing information about the client plugin.
  CefPluginInfo plugin_info;
  CefString(&plugin_info.display_name).FromASCII("Client OSR Plugin");
  CefString(&plugin_info.unique_name).FromASCII("client_osr_plugin");
  CefString(&plugin_info.description).FromASCII("My Example Client OSR Plugin");
  CefString(&plugin_info.mime_types).FromASCII(
      "application/x-client-osr-plugin");

  plugin_info.np_getentrypoints = NP_OSRGetEntryPoints;
  plugin_info.np_initialize = NP_OSRInitialize;
  plugin_info.np_shutdown = NP_OSRShutdown;

  // Register the internal client plugin
  CefRegisterPlugin(plugin_info);
}

void RunOSRPluginTest(CefRefPtr<CefBrowser> browser, bool transparent)
{
  class Listener : public CefDOMEventListener
  {
  public:
    Listener() {}
    virtual void HandleEvent(CefRefPtr<CefDOMEvent> event)
    {
      CefRefPtr<CefBrowser> browser = GetOffScreenBrowser();
      
      CefRefPtr<CefDOMNode> element = event->GetTarget();
      ASSERT(element.get());
      std::string elementId = element->GetElementAttribute("id");

      if (elementId == "back") {
        browser->GoBack();
      } else if(elementId == "forward") {
        browser->GoForward();
      } else if(elementId == "stop") {
        browser->Reload();
      } else if(elementId == "reload") {
        browser->StopLoad();
      } else if (elementId == "go") {
        // Retrieve the value of the "url" field and load it in the off-screen
        // browser window.
        CefRefPtr<CefDOMDocument> document = event->GetDocument();
        ASSERT(document.get());
        CefRefPtr<CefDOMNode> url = document->GetElementById("url");
        ASSERT(url.get());
        CefString value = url->GetValue();
        if (!value.empty())
          browser->GetMainFrame()->LoadURL(value);
      } else if(elementId == "testTransparency") {
        // Transparency test.
        browser->GetMainFrame()->LoadURL(
            "http://tests/transparency");
      } else if(elementId == "testAnimation") {
        // requestAnimationFrame test.
        browser->GetMainFrame()->LoadURL(
            "http://mrdoob.com/lab/javascript/requestanimationframe/");
      } else if(elementId == "testWindowlessPlugin") {
        // Load flash, which is a windowless plugin.
        browser->GetMainFrame()->LoadURL(
            "http://www.adobe.com/software/flash/about/");
      } else if(elementId == "viewSource") {
        // View the page source for the host browser window.
        AppGetBrowser()->GetMainFrame()->ViewSource();
      } else {
        // Not reached.
        ASSERT(false);
      }
    }

    IMPLEMENT_REFCOUNTING(Listener);
  };

  class Visitor : public CefDOMVisitor
  {
  public:
    Visitor() {}

    void RegisterClickListener(CefRefPtr<CefDOMDocument> document,
                               CefRefPtr<CefDOMEventListener> listener,
                               const std::string& elementId)
    {
      CefRefPtr<CefDOMNode> element = document->GetElementById(elementId);
      ASSERT(element.get());
      element->AddEventListener("click", listener, false);
    }

    virtual void Visit(CefRefPtr<CefDOMDocument> document)
    {
      CefRefPtr<CefDOMEventListener> listener(new Listener());
      
      // Register click listeners for the various HTML elements.
      RegisterClickListener(document, listener, "back");
      RegisterClickListener(document, listener, "forward");
      RegisterClickListener(document, listener, "stop");
      RegisterClickListener(document, listener, "reload");
      RegisterClickListener(document, listener, "go");
      RegisterClickListener(document, listener, "testTransparency");
      RegisterClickListener(document, listener, "testAnimation");
      RegisterClickListener(document, listener, "testWindowlessPlugin");
      RegisterClickListener(document, listener, "viewSource");
    }

    IMPLEMENT_REFCOUNTING(Visitor);
  };

  // Center the window on the screen.
  int screenX = GetSystemMetrics(SM_CXFULLSCREEN);
  int screenY = GetSystemMetrics(SM_CYFULLSCREEN);
  int width = 1000, height = 780;
  int x = (screenX - width) / 2;
  int y = (screenY - height) / 2;

  SetWindowPos(AppGetMainHwnd(), NULL, x, y, width, height,
      SWP_NOZORDER | SWP_SHOWWINDOW);

  // The DOM visitor will be called after the path is loaded.
  CefRefPtr<CefClient> client = browser->GetClient();
  static_cast<ClientHandler*>(client.get())->AddDOMVisitor(
      "http://tests/osrapp", new Visitor());

  SetOffScreenTransparent(transparent);
  browser->GetMainFrame()->LoadURL("http://tests/osrapp");
}
