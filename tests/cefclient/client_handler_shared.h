// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFCLIENT_CLIENT_HANDLER_SHARED_H_
#define CEF_TESTS_CEFCLIENT_CLIENT_HANDLER_SHARED_H_
#pragma once

#include <list>
#include <string>

#include "include/base/cef_lock.h"
#include "cefclient/client_handler.h"

namespace client {

// Client handler implementation that is shared by all existing browsers.
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

  // Get the main (non-popup) browser associated with this client. Safe to call
  // on any thread.
  CefRefPtr<CefBrowser> GetBrowser() const;

  // Get the main (non-popup) browser ID. Will return non-0 if the main browser
  // currently exists. Should only be called on the CEF UI thread.
  int GetBrowserId() const;

  // Request that all existing browser windows close.
  void CloseAllBrowsers(bool force_close);

  // Returns true if the main browser window is currently closing. Used in
  // combination with DoClose() and the OS close notification to properly handle
  // 'onbeforeunload' JavaScript events during window close. Safe to call on any
  // thread.
  bool IsClosing() const;

 private:
  // Lock used to protect members accessed on multiple threads. Make it mutable
  // so that it can be used from const methods.
  mutable base::Lock lock_;

  // LOCK PROTECTED MEMBERS
  // Setting the following members or accessing them from a thread other than
  // the CEF UI thread must be protected by |lock_|. Most platforms will only
  // access them from the UI thread but on Windows they will be accessed from
  // the main application thread when using using multi-threaded message loop
  // mode.

  // The handler for off-screen rendering, if any.
  CefRefPtr<RenderHandler> osr_handler_;

  // The child browser window.
  CefRefPtr<CefBrowser> browser_;

  // True if the main browser window is currently closing.
  bool is_closing_;

  // UI THREAD MEMBERS
  // The following members will only be accessed on the CEF UI thread.

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
