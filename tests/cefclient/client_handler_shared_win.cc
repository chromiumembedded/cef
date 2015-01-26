// Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "cefclient/client_handler_shared.h"

#include <string>
#include <windows.h>

#include "include/cef_browser.h"
#include "cefclient/resource.h"

namespace client {

void ClientHandlerShared::SetAddress(CefRefPtr<CefBrowser> browser,
                                     const CefString& url) {
  CEF_REQUIRE_UI_THREAD();

  if (browser_id_ == browser->GetIdentifier()) {
    // Set the edit window text for the main (top-level) browser.
    SetWindowText(edit_handle_, std::wstring(url).c_str());
  }
}

void ClientHandlerShared::SetTitle(CefRefPtr<CefBrowser> browser,
                                   const CefString& title) {
  CEF_REQUIRE_UI_THREAD();

  // Set the frame window title bar.
  CefWindowHandle hwnd = browser->GetHost()->GetWindowHandle();
  if (browser_id_ == browser->GetIdentifier()) {
    // For the main (top-level) browser the frame window will be the parent of
    // the browser window.
    hwnd = GetParent(hwnd);
  }
  SetWindowText(hwnd, std::wstring(title).c_str());
}

void ClientHandlerShared::SetLoadingState(CefRefPtr<CefBrowser> browser,
                                          bool isLoading,
                                          bool canGoBack,
                                          bool canGoForward) {
  CEF_REQUIRE_UI_THREAD();

  if (browser_id_ == browser->GetIdentifier()) {
    // Set UX control state for the main (top-level) browser.
    EnableWindow(edit_handle_, TRUE);
    EnableWindow(reload_handle_, !isLoading);
    EnableWindow(stop_handle_, isLoading);
    EnableWindow(back_handle_, canGoBack);
    EnableWindow(forward_handle_, canGoForward);
  }
}

}  // namespace client
