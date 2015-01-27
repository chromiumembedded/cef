// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "cefclient/browser_window_std_win.h"

#include "cefclient/client_handler_std.h"
#include "cefclient/main_message_loop.h"

namespace client {

BrowserWindowStdWin::BrowserWindowStdWin(Delegate* delegate,
                                         const std::string& startup_url)
    : BrowserWindowWin(delegate) {
  client_handler_ = new ClientHandlerStd(this, startup_url);
}

void BrowserWindowStdWin::CreateBrowser(HWND parent_hwnd,
                                        const RECT& rect,
                                        const CefBrowserSettings& settings) {
  REQUIRE_MAIN_THREAD();

  CefWindowInfo window_info;
  window_info.SetAsChild(parent_hwnd, rect);

  CefBrowserHost::CreateBrowser(window_info, client_handler_,
                                client_handler_->startup_url(),
                                settings, NULL);
}

void BrowserWindowStdWin::GetPopupConfig(HWND temp_hwnd,
                                         CefWindowInfo& windowInfo,
                                         CefRefPtr<CefClient>& client,
                                         CefBrowserSettings& settings) {
  // Note: This method may be called on any thread.
  // The window will be properly sized after the browser is created.
  windowInfo.SetAsChild(temp_hwnd, RECT());
  client = client_handler_;
}

void BrowserWindowStdWin::ShowPopup(HWND parent_hwnd,
                                    int x, int y, size_t width, size_t height) {
  REQUIRE_MAIN_THREAD();

  HWND hwnd = GetHWND();
  if (hwnd) {
    SetParent(hwnd, parent_hwnd);
    SetWindowPos(hwnd, NULL, x, y, width, height, SWP_NOZORDER);
    ShowWindow(hwnd, SW_SHOW);
  }
}

void BrowserWindowStdWin::Show() {
  REQUIRE_MAIN_THREAD();

  HWND hwnd = GetHWND();
  if (hwnd && !::IsWindowVisible(hwnd))
    ShowWindow(hwnd, SW_SHOW);
}

void BrowserWindowStdWin::Hide() {
  REQUIRE_MAIN_THREAD();

  HWND hwnd = GetHWND();
  if (hwnd) {
    // When the frame window is minimized set the browser window size to 0x0 to
    // reduce resource usage.
    SetWindowPos(hwnd, NULL,
        0, 0, 0, 0, SWP_NOZORDER | SWP_NOMOVE | SWP_NOACTIVATE);
  }
}

void BrowserWindowStdWin::SetBounds(int x, int y, size_t width, size_t height) {
  REQUIRE_MAIN_THREAD();

  HWND hwnd = GetHWND();
  if (hwnd) {
    // Set the browser window bounds.
    SetWindowPos(hwnd, NULL, x, y, width, height, SWP_NOZORDER);
  }
}

void BrowserWindowStdWin::SetFocus() {
  REQUIRE_MAIN_THREAD();

  if (browser_) {
    // Give focus to the browser window.
    browser_->GetHost()->SetFocus(true);
  }
}

HWND BrowserWindowStdWin::GetHWND() const {
  REQUIRE_MAIN_THREAD();

  if (browser_)
    return browser_->GetHost()->GetWindowHandle();
  return NULL;
}


}  // namespace client
