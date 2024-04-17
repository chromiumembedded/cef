// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/cefclient/browser/browser_window_std_mac.h"

#include <Cocoa/Cocoa.h>

#include "include/base/cef_logging.h"
#include "tests/cefclient/browser/client_handler_std.h"
#include "tests/shared/browser/main_message_loop.h"

namespace client {

BrowserWindowStdMac::BrowserWindowStdMac(Delegate* delegate,
                                         bool with_controls,
                                         const std::string& startup_url)
    : BrowserWindow(delegate) {
  client_handler_ = new ClientHandlerStd(this, with_controls, startup_url);
}

void BrowserWindowStdMac::CreateBrowser(
    ClientWindowHandle parent_handle,
    const CefRect& rect,
    const CefBrowserSettings& settings,
    CefRefPtr<CefDictionaryValue> extra_info,
    CefRefPtr<CefRequestContext> request_context) {
  REQUIRE_MAIN_THREAD();

  CefWindowInfo window_info;
  window_info.SetAsChild(parent_handle, rect);

  if (delegate_->UseAlloyStyle()) {
    window_info.runtime_style = CEF_RUNTIME_STYLE_ALLOY;
  }

  CefBrowserHost::CreateBrowser(window_info, client_handler_,
                                client_handler_->startup_url(), settings,
                                extra_info, request_context);
}

void BrowserWindowStdMac::GetPopupConfig(CefWindowHandle temp_handle,
                                         CefWindowInfo& windowInfo,
                                         CefRefPtr<CefClient>& client,
                                         CefBrowserSettings& settings) {
  CEF_REQUIRE_UI_THREAD();

  // The window will be properly sized after the browser is created.
  windowInfo.SetAsChild(temp_handle, CefRect());

  if (delegate_->UseAlloyStyle()) {
    windowInfo.runtime_style = CEF_RUNTIME_STYLE_ALLOY;
  }

  client = client_handler_;
}

void BrowserWindowStdMac::ShowPopup(ClientWindowHandle parent_handle,
                                    int x,
                                    int y,
                                    size_t width,
                                    size_t height) {
  REQUIRE_MAIN_THREAD();

  NSView* browser_view = CAST_CEF_WINDOW_HANDLE_TO_NSVIEW(GetWindowHandle());

  // Re-parent |browser_view| to |parent_handle|.
  [browser_view removeFromSuperview];
  [CAST_CEF_WINDOW_HANDLE_TO_NSVIEW(parent_handle) addSubview:browser_view];

  NSSize size = NSMakeSize(static_cast<int>(width), static_cast<int>(height));
  [browser_view setFrameSize:size];
}

void BrowserWindowStdMac::Show() {
  REQUIRE_MAIN_THREAD();
  // Nothing to do here. Chromium internally handles window show/hide.
}

void BrowserWindowStdMac::Hide() {
  REQUIRE_MAIN_THREAD();
  // Nothing to do here. Chromium internally handles window show/hide.
}

void BrowserWindowStdMac::SetBounds(int x, int y, size_t width, size_t height) {
  REQUIRE_MAIN_THREAD();
  // Nothing to do here. Cocoa will size the browser for us.
}

void BrowserWindowStdMac::SetFocus(bool focus) {
  REQUIRE_MAIN_THREAD();
  // Nothing to do here. Chromium internally handles window focus assignment.
}

ClientWindowHandle BrowserWindowStdMac::GetWindowHandle() const {
  REQUIRE_MAIN_THREAD();

  if (browser_) {
    return browser_->GetHost()->GetWindowHandle();
  }
  return nullptr;
}

}  // namespace client
