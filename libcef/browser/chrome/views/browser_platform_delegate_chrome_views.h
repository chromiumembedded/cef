// Copyright 2021 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_CHROME_VIEWS_BROWSER_PLATFORM_DELEGATE_CHROME_VIEWS_H_
#define CEF_LIBCEF_BROWSER_CHROME_VIEWS_BROWSER_PLATFORM_DELEGATE_CHROME_VIEWS_H_

#include "libcef/browser/chrome/browser_platform_delegate_chrome.h"
#include "libcef/browser/views/browser_view_impl.h"

class CefWindowImpl;

// Implementation of Chrome-based browser functionality.
class CefBrowserPlatformDelegateChromeViews
    : public CefBrowserPlatformDelegateChrome {
 public:
  explicit CefBrowserPlatformDelegateChromeViews(
      std::unique_ptr<CefBrowserPlatformDelegateNative> native_delegate,
      CefRefPtr<CefBrowserViewImpl> browser_view);

  // CefBrowserPlatformDelegate overrides.
  void WebContentsCreated(content::WebContents* web_contents,
                          bool owned) override;
  void BrowserCreated(CefBrowserHostBase* browser) override;
  void NotifyBrowserCreated() override;
  void NotifyBrowserDestroyed() override;
  void BrowserDestroyed(CefBrowserHostBase* browser) override;
  void CloseHostWindow() override;
  CefWindowHandle GetHostWindowHandle() const override;
  views::Widget* GetWindowWidget() const override;
  CefRefPtr<CefBrowserView> GetBrowserView() const override;
  void PopupWebContentsCreated(
      const CefBrowserSettings& settings,
      CefRefPtr<CefClient> client,
      content::WebContents* new_web_contents,
      CefBrowserPlatformDelegate* new_platform_delegate,
      bool is_devtools) override;
  void PopupBrowserCreated(CefBrowserHostBase* new_browser,
                           bool is_devtools) override;
  bool IsViewsHosted() const override;

  CefRefPtr<CefBrowserViewImpl> browser_view() const { return browser_view_; }

 private:
  void SetBrowserView(CefRefPtr<CefBrowserViewImpl> browser_view);

  CefWindowImpl* GetWindowImpl() const;

  CefRefPtr<CefBrowserViewImpl> browser_view_;
};

#endif  // CEF_LIBCEF_BROWSER_CHROME_VIEWS_BROWSER_PLATFORM_DELEGATE_CHROME_VIEWS_H_
