// Copyright 2022 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/chrome/views/browser_platform_delegate_chrome_child_window.h"

#include "include/views/cef_browser_view.h"
#include "libcef/browser/chrome/views/chrome_child_window.h"

CefBrowserPlatformDelegateChromeChildWindow::
    CefBrowserPlatformDelegateChromeChildWindow(
        std::unique_ptr<CefBrowserPlatformDelegateNative> native_delegate,
        CefRefPtr<CefBrowserViewImpl> browser_view)
    : CefBrowserPlatformDelegateChromeViews(std::move(native_delegate),
                                            browser_view) {}

void CefBrowserPlatformDelegateChromeChildWindow::RenderViewCreated(
    content::RenderViewHost* render_view_host) {
  CefBrowserPlatformDelegateChromeViews::RenderViewCreated(render_view_host);

  // Calls SetHasExternalParent(true). This setting is required for proper
  // focus handling on Windows and Linux.
  native_delegate_->RenderViewCreated(render_view_host);
}

void CefBrowserPlatformDelegateChromeChildWindow::CloseHostWindow() {
  native_delegate_->CloseHostWindow();
}

void CefBrowserPlatformDelegateChromeChildWindow::SetFocus(bool focus) {
  native_delegate_->SetFocus(focus);
}

#if BUILDFLAG(IS_WIN) || (BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC))
void CefBrowserPlatformDelegateChromeChildWindow::NotifyMoveOrResizeStarted() {
  native_delegate_->NotifyMoveOrResizeStarted();
}
#endif
