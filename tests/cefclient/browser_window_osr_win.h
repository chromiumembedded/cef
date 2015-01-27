// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFCLIENT_BROWSER_WINDOW_OSR_WIN_H_
#define CEF_TESTS_CEFCLIENT_BROWSER_WINDOW_OSR_WIN_H_

#include "cefclient/browser_window_win.h"
#include "cefclient/osr_window_win.h"

namespace client {

// Represents a native child window hosting a single off-screen browser
// instance. The methods of this class must be called on the main thread unless
// otherwise indicated.
class BrowserWindowOsrWin : public BrowserWindowWin,
                                   OsrWindowWin::Delegate {
 public:
  // Constructor may be called on any thread.
  // |delegate| must outlive this object.
  BrowserWindowOsrWin(BrowserWindowWin::Delegate* delegate,
                      const std::string& startup_url,
                      bool transparent,
                      bool show_update_rect);

  // BrowserWindowWin methods.
  void CreateBrowser(HWND parent_hwnd,
                     const RECT& rect,
                     const CefBrowserSettings& settings) OVERRIDE;
  void GetPopupConfig(HWND temp_hwnd,
                      CefWindowInfo& windowInfo,
                      CefRefPtr<CefClient>& client,
                      CefBrowserSettings& settings) OVERRIDE;
  void ShowPopup(HWND parent_hwnd,
                 int x, int y, size_t width, size_t height) OVERRIDE;
  void Show() OVERRIDE;
  void Hide() OVERRIDE;
  void SetBounds(int x, int y, size_t width, size_t height) OVERRIDE;
  void SetFocus() OVERRIDE;
  HWND GetHWND() const OVERRIDE;

 private:
  // ClientHandler::Delegate methods.
  void OnBrowserClosed(CefRefPtr<CefBrowser> browser) OVERRIDE;

  // OsrWindowWin::Delegate methods.
  void OnOsrNativeWindowCreated(HWND hwnd) OVERRIDE;

  const bool transparent_;

  // The below members are only accessed on the main thread.
  scoped_refptr<OsrWindowWin> osr_window_;
  HWND osr_hwnd_;

  DISALLOW_COPY_AND_ASSIGN(BrowserWindowOsrWin);
};

}  // namespace client

#endif  // CEF_TESTS_CEFCLIENT_BROWSER_WINDOW_OSR_WIN_H_
