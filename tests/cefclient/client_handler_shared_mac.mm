// Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "cefclient/client_handler_shared.h"
#include "include/cef_browser.h"

namespace client {

void ClientHandlerShared::SetAddress(CefRefPtr<CefBrowser> browser,
                                     const CefString& url) {
  CEF_REQUIRE_UI_THREAD();

  if (browser_id_ == browser->GetIdentifier()) {
    // Set edit window text for the main window.
    NSTextField* textField = (NSTextField*)edit_handle_;
    std::string urlStr(url);
    NSString* str = [NSString stringWithUTF8String:urlStr.c_str()];
    [textField setStringValue:str];
  }
}

void ClientHandlerShared::SetTitle(CefRefPtr<CefBrowser> browser,
                                   const CefString& title) {
  CEF_REQUIRE_UI_THREAD();

  // Set the frame window title bar.
  NSView* view = (NSView*)browser->GetHost()->GetWindowHandle();
  NSWindow* window = [view window];
  std::string titleStr(title);
  NSString* str = [NSString stringWithUTF8String:titleStr.c_str()];
  [window setTitle:str];
}

void ClientHandlerShared::SetLoadingState(CefRefPtr<CefBrowser> browser,
                                          bool isLoading,
                                          bool canGoBack,
                                          bool canGoForward) {
  CEF_REQUIRE_UI_THREAD();

  if (browser_id_ == browser->GetIdentifier()) {
    // Set UX control state for the main window.
    [(NSTextField*)edit_handle_ setEnabled:YES];
    [(NSButton*)reload_handle_ setEnabled:!isLoading];
    [(NSButton*)stop_handle_ setEnabled:isLoading];
    [(NSButton*)back_handle_ setEnabled:canGoBack];
    [(NSButton*)forward_handle_ setEnabled:canGoForward];
  }
}

}  // namespace client
