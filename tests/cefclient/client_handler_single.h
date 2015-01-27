// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFCLIENT_CLIENT_HANDLER_SINGLE_H_
#define CEF_TESTS_CEFCLIENT_CLIENT_HANDLER_SINGLE_H_
#pragma once

#include <list>
#include <string>

#include "include/base/cef_lock.h"
#include "cefclient/client_handler.h"

namespace client {

// Client handler implementation that is used by a single browser.
class ClientHandlerSingle : public ClientHandler {
 public:
  // Implement this interface to receive notification of ClientHandlerSingle
  // events. The methods of this class will be called on the main thread.
  class Delegate {
   public:
    // Called when the browser is created.
    virtual void OnBrowserCreated(CefRefPtr<CefBrowser> browser) = 0;

    // Called when the browser is closing.
    virtual void OnBrowserClosing(CefRefPtr<CefBrowser> browser) = 0;

    // Called when the browser has been closed.
    virtual void OnBrowserClosed(CefRefPtr<CefBrowser> browser) = 0;

    // Set the window URL address.
    virtual void OnSetAddress(const std::string& url) = 0;

    // Set the window title.
    virtual void OnSetTitle(const std::string& title) = 0;

    // Set the loading state.
    virtual void OnSetLoadingState(bool isLoading,
                                   bool canGoBack,
                                   bool canGoForward) = 0;

   protected:
    virtual ~Delegate() {}
  };

  // This object may outlive the Delegate object so it's necessary for the
  // Delegate to detach itself before destruction.
  void DetachDelegate();

 protected:
  // Constructor may be called on any thread.
  // |delegate| must outlive this object or DetachDelegate() must be called.
  ClientHandlerSingle(Delegate* delegate,
                      bool is_osr,
                      const std::string& startup_url);

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

 private:
  void NotifyBrowserCreated(CefRefPtr<CefBrowser> browser);
  void NotifyBrowserClosing(CefRefPtr<CefBrowser> browser);
  void NotifyBrowserClosed(CefRefPtr<CefBrowser> browser);
  void NotifyAddress(const CefString& url);
  void NotifyTitle(const CefString& title);
  void NotifyLoadingState(bool isLoading,
                          bool canGoBack,
                          bool canGoForward);

  // Only accessed on the main thread.
  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(ClientHandlerSingle);
};

}  // namespace client

#endif  // CEF_TESTS_CEFCLIENT_CLIENT_HANDLER_SINGLE_H_
