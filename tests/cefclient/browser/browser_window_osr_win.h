// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFCLIENT_BROWSER_BROWSER_WINDOW_OSR_WIN_H_
#define CEF_TESTS_CEFCLIENT_BROWSER_BROWSER_WINDOW_OSR_WIN_H_
#pragma once

#include "tests/cefclient/browser/browser_window.h"
#include "tests/cefclient/browser/osr_window_win.h"

namespace client {

// Represents a native child window hosting a single off-screen browser
// instance. The methods of this class must be called on the main thread unless
// otherwise indicated.
class BrowserWindowOsrWin : public BrowserWindow,
                            public OsrWindowWin::Delegate {
 public:
  // Constructor may be called on any thread.
  // |delegate| must outlive this object.
  BrowserWindowOsrWin(BrowserWindow::Delegate* delegate,
                      const std::string& startup_url,
                      const OsrRendererSettings& settings);

  // BrowserWindow methods.
  void CreateBrowser(ClientWindowHandle parent_handle,
                     const CefRect& rect,
                     const CefBrowserSettings& settings,
                     CefRefPtr<CefDictionaryValue> extra_info,
                     CefRefPtr<CefRequestContext> request_context) override;
  void GetPopupConfig(CefWindowHandle temp_handle,
                      CefWindowInfo& windowInfo,
                      CefRefPtr<CefClient>& client,
                      CefBrowserSettings& settings) override;
  void ShowPopup(ClientWindowHandle parent_handle,
                 int x,
                 int y,
                 size_t width,
                 size_t height) override;
  void Show() override;
  void Hide() override;
  void SetBounds(int x, int y, size_t width, size_t height) override;
  void SetFocus(bool focus) override;
  void SetDeviceScaleFactor(float device_scale_factor) override;
  float GetDeviceScaleFactor() const override;
  ClientWindowHandle GetWindowHandle() const override;

 private:
  // ClienHandler::Delegate methods.
  void OnBrowserClosed(CefRefPtr<CefBrowser> browser) override;

  // OsrWindowWin::Delegate methods.
  void OnOsrNativeWindowCreated(HWND hwnd) override;

  // The below members are only accessed on the main thread.
  scoped_refptr<OsrWindowWin> osr_window_;
  HWND osr_hwnd_;

  float device_scale_factor_;

  DISALLOW_COPY_AND_ASSIGN(BrowserWindowOsrWin);
};

}  // namespace client

#endif  // CEF_TESTS_CEFCLIENT_BROWSER_BROWSER_WINDOW_OSR_WIN_H_
