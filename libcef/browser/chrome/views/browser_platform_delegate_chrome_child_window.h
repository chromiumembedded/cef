// Copyright 2022 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_CHROME_VIEWS_BROWSER_PLATFORM_DELEGATE_CHROME_CHILD_WINDOW_H_
#define CEF_LIBCEF_BROWSER_CHROME_VIEWS_BROWSER_PLATFORM_DELEGATE_CHROME_CHILD_WINDOW_H_

#include "libcef/browser/chrome/views/browser_platform_delegate_chrome_views.h"

// Implementation of Chrome-based browser functionality.
class CefBrowserPlatformDelegateChromeChildWindow
    : public CefBrowserPlatformDelegateChromeViews {
 public:
  CefBrowserPlatformDelegateChromeChildWindow(
      std::unique_ptr<CefBrowserPlatformDelegateNative> native_delegate,
      CefRefPtr<CefBrowserViewImpl> browser_view);

  // CefBrowserPlatformDelegate overrides.
  void CloseHostWindow() override;
  void SetFocus(bool focus) override;

#if BUILDFLAG(IS_WIN) || (BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC))
  void NotifyMoveOrResizeStarted() override;
#endif

  bool HasExternalParent() const override { return true; }
};

#endif  // CEF_LIBCEF_BROWSER_CHROME_VIEWS_BROWSER_PLATFORM_DELEGATE_CHROME_CHILD_WINDOW_H_
