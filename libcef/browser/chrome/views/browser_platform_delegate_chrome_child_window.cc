// Copyright 2022 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef/browser/chrome/views/browser_platform_delegate_chrome_child_window.h"

#include "cef/include/views/cef_browser_view.h"
#include "cef/libcef/browser/chrome/views/chrome_child_window.h"

#if defined(USE_AURA)
#include "cef/libcef/browser/native/browser_platform_delegate_native_aura.h"
#endif

CefBrowserPlatformDelegateChromeChildWindow::
    CefBrowserPlatformDelegateChromeChildWindow(
        std::unique_ptr<CefBrowserPlatformDelegateNative> native_delegate,
        CefRefPtr<CefBrowserViewImpl> browser_view)
    : CefBrowserPlatformDelegateChromeViews(std::move(native_delegate),
                                            browser_view) {}

void CefBrowserPlatformDelegateChromeChildWindow::RenderViewReady() {
#if defined(USE_AURA)
  static_cast<CefBrowserPlatformDelegateNativeAura*>(native_delegate_.get())
      ->InstallRootWindowBoundsCallback();
#endif
}

void CefBrowserPlatformDelegateChromeChildWindow::CloseHostWindow() {
  native_delegate_->CloseHostWindow();
}

CefRefPtr<CefBrowserViewDelegate> CefBrowserPlatformDelegateChromeChildWindow::
    GetDefaultBrowserViewDelegateForPopupOpener() {
  return chrome_child_window::GetDefaultBrowserViewDelegateForPopupOpener();
}

void CefBrowserPlatformDelegateChromeChildWindow::SetFocus(bool focus) {
  native_delegate_->SetFocus(focus);
}

#if BUILDFLAG(IS_WIN) || (BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC))
void CefBrowserPlatformDelegateChromeChildWindow::NotifyMoveOrResizeStarted() {
  native_delegate_->NotifyMoveOrResizeStarted();
}
#endif
