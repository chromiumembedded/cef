// Copyright 2021 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_CHROME_VIEWS_BROWSER_PLATFORM_DELEGATE_CHROME_VIEWS_H_
#define CEF_LIBCEF_BROWSER_CHROME_VIEWS_BROWSER_PLATFORM_DELEGATE_CHROME_VIEWS_H_

#include "base/memory/weak_ptr.h"
#include "cef/libcef/browser/chrome/browser_platform_delegate_chrome.h"
#include "cef/libcef/browser/views/browser_view_impl.h"

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
  void WebContentsDestroyed(content::WebContents* web_contents) override;
  void BrowserCreated(CefBrowserHostBase* browser) override;
  void NotifyBrowserCreated() override;
  void NotifyBrowserDestroyed() override;
  void BrowserDestroyed(CefBrowserHostBase* browser) override;
  void CloseHostWindow() override;
  CefWindowHandle GetHostWindowHandle() const override;
  views::Widget* GetWindowWidget() const override;
  CefRefPtr<CefBrowserView> GetBrowserView() const override;
  void SetBrowserView(CefRefPtr<CefBrowserView> browser_view) override;
  void SetFocus(bool setFocus) override;
  bool IsViewsHosted() const override;

  CefBrowserViewImpl* browser_view() const { return browser_view_.get(); }

 private:
  CefWindowImpl* GetWindowImpl() const;

  // Holding a weak reference here because we want the CefBrowserViewImpl to be
  // destroyed first if all references are released by the client.
  // CefBrowserViewImpl destruction will then trigger destruction of any
  // associated CefBrowserHostBase (which owns this CefBrowserPlatformDelegate
  // object).
  base::WeakPtr<CefBrowserViewImpl> browser_view_;
};

#endif  // CEF_LIBCEF_BROWSER_CHROME_VIEWS_BROWSER_PLATFORM_DELEGATE_CHROME_VIEWS_H_
