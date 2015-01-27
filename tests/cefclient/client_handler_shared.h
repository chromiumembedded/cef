// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFCLIENT_CLIENT_HANDLER_SHARED_H_
#define CEF_TESTS_CEFCLIENT_CLIENT_HANDLER_SHARED_H_
#pragma once

#include <list>
#include <string>

#include "cefclient/client_handler.h"

namespace client {

// Client handler implementation that is shared by all existing browsers. All
// methods must be called on the CEF UI thread unless otherwise indicated.
class ClientHandlerShared : public ClientHandler {
 public:
  // Interface implemented to handle off-screen rendering.
  class RenderHandler : public CefRenderHandler {
   public:
    virtual void OnBeforeClose(CefRefPtr<CefBrowser> browser) =0;
  };

  ClientHandlerShared();
  ~ClientHandlerShared();
  
  // CefClient methods
  CefRefPtr<CefRenderHandler> GetRenderHandler() OVERRIDE {
    return osr_handler_;
  }

  // ClientHandler methods
  void BrowserCreated(CefRefPtr<CefBrowser> browser) OVERRIDE;
  void BrowserClosing(CefRefPtr<CefBrowser> browser) OVERRIDE;
  void BrowserClosed(CefRefPtr<CefBrowser> browser) OVERRIDE;
  void SetAddress(CefRefPtr<CefBrowser> browser,
                  const CefString& url) OVERRIDE;
  void SetTitle(CefRefPtr<CefBrowser> browser,
                const CefString& title) OVERRIDE;
  void SetLoadingState(CefRefPtr<CefBrowser> browser,
                       bool isLoading,
                       bool canGoBack,
                       bool canGoForward) OVERRIDE;
  bool CreatePopupWindow(
      CefRefPtr<CefBrowser> browser,
      bool is_devtools,
      const CefPopupFeatures& popupFeatures,
      CefWindowInfo& windowInfo,
      CefRefPtr<CefClient>& client,
      CefBrowserSettings& settings) OVERRIDE;

  void SetUXWindowHandles(ClientWindowHandle editHandle,
                          ClientWindowHandle backHandle,
                          ClientWindowHandle forwardHandle,
                          ClientWindowHandle reloadHandle,
                          ClientWindowHandle stopHandle);

  CefRefPtr<RenderHandler> GetOSRHandler() const;
  void SetOSRHandler(CefRefPtr<RenderHandler> handler);

  // Get the main (non-popup) browser associated with this client.
  CefRefPtr<CefBrowser> GetBrowser() const;

  // Get the main (non-popup) browser ID. Will return non-0 if the main browser
  // currently exists.
  int GetBrowserId() const;

  // Request that all existing browser windows close.
  void CloseAllBrowsers(bool force_close);

  // Returns true if the main browser window is currently closing. Used in
  // combination with DoClose() and the OS close notification to properly handle
  // 'onbeforeunload' JavaScript events during window close.
  bool IsClosing() const;

 private:
  // The following members will only be accessed on the CEF UI thread.

  // The handler for off-screen rendering, if any.
  CefRefPtr<RenderHandler> osr_handler_;

  // The child browser window.
  CefRefPtr<CefBrowser> browser_;

  // True if the main browser window is currently closing.
  bool is_closing_;

  // The child browser id.
  int browser_id_;

  // List of any popup browser windows.
  typedef std::list<CefRefPtr<CefBrowser> > BrowserList;
  BrowserList popup_browsers_;

  // The edit window handle.
  ClientWindowHandle edit_handle_;

  // The button window handles.
  ClientWindowHandle back_handle_;
  ClientWindowHandle forward_handle_;
  ClientWindowHandle stop_handle_;
  ClientWindowHandle reload_handle_;

  // Include the default reference counting implementation.
  IMPLEMENT_REFCOUNTING(ClientHandlerShared);
  DISALLOW_COPY_AND_ASSIGN(ClientHandlerShared);
};

}  // namespace client

#endif  // CEF_TESTS_CEFCLIENT_CLIENT_HANDLER_SHARED_H_
