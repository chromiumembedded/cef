// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "cefclient/client_handler_single.h"

#include <stdio.h>
#include <algorithm>
#include <string>

#include "include/base/cef_bind.h"
#include "include/cef_command_line.h"
#include "include/wrapper/cef_closure_task.h"
#include "cefclient/main_context.h"
#include "cefclient/main_message_loop.h"
#include "cefclient/root_window_manager.h"

namespace client {

ClientHandlerSingle::ClientHandlerSingle(Delegate* delegate,
                                         bool is_osr,
                                         const std::string& startup_url)
    : ClientHandler(startup_url, is_osr),
      delegate_(delegate) {
  DCHECK(delegate_);
}

void ClientHandlerSingle::DetachDelegate() {
  if (!CURRENTLY_ON_MAIN_THREAD()) {
    // Execute this method on the main thread.
    MAIN_POST_CLOSURE(base::Bind(&ClientHandlerSingle::DetachDelegate, this));
    return;
  }

  DCHECK(delegate_);
  delegate_ = NULL;
}

void ClientHandlerSingle::BrowserCreated(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();
  NotifyBrowserCreated(browser);
}

void ClientHandlerSingle::BrowserClosing(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();
  NotifyBrowserClosing(browser);
}

void ClientHandlerSingle::BrowserClosed(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();
  NotifyBrowserClosed(browser);
}

void ClientHandlerSingle::SetAddress(CefRefPtr<CefBrowser> browser,
                                     const CefString& url) {
  CEF_REQUIRE_UI_THREAD();
  NotifyAddress(url);
}

void ClientHandlerSingle::SetTitle(CefRefPtr<CefBrowser> browser,
                                   const CefString& title) {
  CEF_REQUIRE_UI_THREAD();
  NotifyTitle(title);
}

void ClientHandlerSingle::SetLoadingState(CefRefPtr<CefBrowser> browser,
                                          bool isLoading,
                                          bool canGoBack,
                                          bool canGoForward) {
  CEF_REQUIRE_UI_THREAD();
  NotifyLoadingState(isLoading, canGoBack, canGoForward);
}

bool ClientHandlerSingle::CreatePopupWindow(
    CefRefPtr<CefBrowser> browser,
    bool is_devtools,
    const CefPopupFeatures& popupFeatures,
    CefWindowInfo& windowInfo,
    CefRefPtr<CefClient>& client,
    CefBrowserSettings& settings) {
  // Note: This method will be called on multiple threads.

  // The popup browser will be parented to a new native window.
  // Don't show URL bar and navigation buttons on DevTools windows.
  MainContext::Get()->GetRootWindowManager()->CreateRootWindowAsPopup(
      !is_devtools, is_osr(), popupFeatures, windowInfo, client, settings);

  return true;
}

void ClientHandlerSingle::NotifyBrowserCreated(CefRefPtr<CefBrowser> browser) {
  if (!CURRENTLY_ON_MAIN_THREAD()) {
    // Execute this method on the main thread.
    MAIN_POST_CLOSURE(
        base::Bind(&ClientHandlerSingle::NotifyBrowserCreated, this, browser));
    return;
  }

  if (delegate_)
    delegate_->OnBrowserCreated(browser);
}

void ClientHandlerSingle::NotifyBrowserClosing(CefRefPtr<CefBrowser> browser) {
  if (!CURRENTLY_ON_MAIN_THREAD()) {
    // Execute this method on the main thread.
    MAIN_POST_CLOSURE(
        base::Bind(&ClientHandlerSingle::NotifyBrowserClosing, this, browser));
    return;
  }

  if (delegate_)
    delegate_->OnBrowserClosing(browser);
}

void ClientHandlerSingle::NotifyBrowserClosed(CefRefPtr<CefBrowser> browser) {
  if (!CURRENTLY_ON_MAIN_THREAD()) {
    // Execute this method on the main thread.
    MAIN_POST_CLOSURE(
        base::Bind(&ClientHandlerSingle::NotifyBrowserClosed, this, browser));
    return;
  }

  if (delegate_)
    delegate_->OnBrowserClosed(browser);
}

void ClientHandlerSingle::NotifyAddress(const CefString& url) {
  if (!CURRENTLY_ON_MAIN_THREAD()) {
    // Execute this method on the main thread.
    MAIN_POST_CLOSURE(
        base::Bind(&ClientHandlerSingle::NotifyAddress, this, url));
    return;
  }

  if (delegate_)
    delegate_->OnSetAddress(url);
}

void ClientHandlerSingle::NotifyTitle(const CefString& title) {
  if (!CURRENTLY_ON_MAIN_THREAD()) {
    // Execute this method on the main thread.
    MAIN_POST_CLOSURE(
        base::Bind(&ClientHandlerSingle::NotifyTitle, this, title));
    return;
  }

  if (delegate_)
    delegate_->OnSetTitle(title);
}

void ClientHandlerSingle::NotifyLoadingState(bool isLoading,
                                             bool canGoBack,
                                             bool canGoForward) {
  if (!CURRENTLY_ON_MAIN_THREAD()) {
    // Execute this method on the main thread.
    MAIN_POST_CLOSURE(
        base::Bind(&ClientHandlerSingle::NotifyLoadingState, this,
                   isLoading, canGoBack, canGoForward));
    return;
  }

  if (delegate_)
    delegate_->OnSetLoadingState(isLoading, canGoBack, canGoForward);
}

}  // namespace client
