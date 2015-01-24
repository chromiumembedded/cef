// Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "cefclient/client_handler.h"

#include <string>
#include <windows.h>

#include "include/cef_browser.h"
#include "include/cef_frame.h"
#include "cefclient/resource.h"

namespace client {

void ClientHandler::OnAddressChange(CefRefPtr<CefBrowser> browser,
                                    CefRefPtr<CefFrame> frame,
                                    const CefString& url) {
  CEF_REQUIRE_UI_THREAD();

  if (GetBrowserId() == browser->GetIdentifier() && frame->IsMain()) {
    // Set the edit window text
    SetWindowText(edit_handle_, std::wstring(url).c_str());
  }
}

void ClientHandler::OnTitleChange(CefRefPtr<CefBrowser> browser,
                                  const CefString& title) {
  CEF_REQUIRE_UI_THREAD();

  // Set the frame window title bar
  CefWindowHandle hwnd = browser->GetHost()->GetWindowHandle();
  if (GetBrowserId() == browser->GetIdentifier()) {
    // The frame window will be the parent of the browser window
    hwnd = GetParent(hwnd);
  }
  SetWindowText(hwnd, std::wstring(title).c_str());
}

void ClientHandler::SetLoading(bool isLoading) {
  DCHECK(edit_handle_ != NULL && reload_handle_ != NULL &&
         stop_handle_ != NULL);
  EnableWindow(edit_handle_, TRUE);
  EnableWindow(reload_handle_, !isLoading);
  EnableWindow(stop_handle_, isLoading);
}

void ClientHandler::SetNavState(bool canGoBack, bool canGoForward) {
  DCHECK(back_handle_ != NULL && forward_handle_ != NULL);
  EnableWindow(back_handle_, canGoBack);
  EnableWindow(forward_handle_, canGoForward);
}

}  // namespace client
