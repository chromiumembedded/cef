// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/cefclient/browser/browser_window_std_win.h"

#include "tests/cefclient/browser/client_handler_std.h"
#include "tests/shared/browser/main_message_loop.h"

namespace client {

BrowserWindowStdWin::BrowserWindowStdWin(Delegate* delegate,
                                         const std::string& startup_url)
    : BrowserWindow(delegate) {
  client_handler_ = new ClientHandlerStd(this, startup_url);
}

void BrowserWindowStdWin::CreateBrowser(
    ClientWindowHandle parent_handle,
    const CefRect& rect,
    const CefBrowserSettings& settings,
    CefRefPtr<CefDictionaryValue> extra_info,
    CefRefPtr<CefRequestContext> request_context) {
  REQUIRE_MAIN_THREAD();

  CefWindowInfo window_info;
  window_info.SetAsChild(parent_handle, rect);

  if (GetWindowLongPtr(parent_handle, GWL_EXSTYLE) & WS_EX_NOACTIVATE) {
    // Don't activate the browser window on creation.
    window_info.ex_style |= WS_EX_NOACTIVATE;
  }

  CefBrowserHost::CreateBrowser(window_info, client_handler_,
                                client_handler_->startup_url(), settings,
                                extra_info, request_context);
}

void BrowserWindowStdWin::GetPopupConfig(CefWindowHandle temp_handle,
                                         CefWindowInfo& windowInfo,
                                         CefRefPtr<CefClient>& client,
                                         CefBrowserSettings& settings) {
  CEF_REQUIRE_UI_THREAD();

  // The window will be properly sized after the browser is created.
  windowInfo.SetAsChild(temp_handle, CefRect());

  // Don't activate the hidden browser window on creation.
  windowInfo.ex_style |= WS_EX_NOACTIVATE;

  client = client_handler_;
}

void BrowserWindowStdWin::ShowPopup(ClientWindowHandle parent_handle,
                                    int x,
                                    int y,
                                    size_t width,
                                    size_t height) {
  REQUIRE_MAIN_THREAD();

  HWND hwnd = GetWindowHandle();
  if (hwnd) {
    SetParent(hwnd, parent_handle);
    SetWindowPos(hwnd, nullptr, x, y, static_cast<int>(width),
                 static_cast<int>(height), SWP_NOZORDER | SWP_NOACTIVATE);

    const bool no_activate =
        GetWindowLongPtr(parent_handle, GWL_EXSTYLE) & WS_EX_NOACTIVATE;
    ShowWindow(hwnd, no_activate ? SW_SHOWNOACTIVATE : SW_SHOW);
  }
}

void BrowserWindowStdWin::Show() {
  REQUIRE_MAIN_THREAD();

  HWND hwnd = GetWindowHandle();
  if (hwnd && !::IsWindowVisible(hwnd))
    ShowWindow(hwnd, SW_SHOW);
}

void BrowserWindowStdWin::Hide() {
  REQUIRE_MAIN_THREAD();

  HWND hwnd = GetWindowHandle();
  if (hwnd) {
    // When the frame window is minimized set the browser window size to 0x0 to
    // reduce resource usage.
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                 SWP_NOZORDER | SWP_NOMOVE | SWP_NOACTIVATE);
  }
}

void BrowserWindowStdWin::SetBounds(int x, int y, size_t width, size_t height) {
  REQUIRE_MAIN_THREAD();

  HWND hwnd = GetWindowHandle();
  if (hwnd) {
    // Set the browser window bounds.
    SetWindowPos(hwnd, nullptr, x, y, static_cast<int>(width),
                 static_cast<int>(height), SWP_NOZORDER);
  }
}

void BrowserWindowStdWin::SetFocus(bool focus) {
  REQUIRE_MAIN_THREAD();

  if (browser_)
    browser_->GetHost()->SetFocus(focus);
}

ClientWindowHandle BrowserWindowStdWin::GetWindowHandle() const {
  REQUIRE_MAIN_THREAD();

  if (browser_)
    return browser_->GetHost()->GetWindowHandle();
  return nullptr;
}

}  // namespace client
