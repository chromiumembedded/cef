// Copyright 2021 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_CHROME_VIEWS_CHROME_BROWSER_VIEW_H_
#define CEF_LIBCEF_BROWSER_CHROME_VIEWS_CHROME_BROWSER_VIEW_H_
#pragma once

#include "include/views/cef_browser_view.h"
#include "include/views/cef_browser_view_delegate.h"
#include "libcef/browser/chrome/views/toolbar_view_impl.h"
#include "libcef/browser/views/browser_view_view.h"
#include "libcef/browser/views/view_view.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_view.h"

namespace views {
class WebView;
}  // namespace views

// A variant of CefBrowserViewView that extends BrowserView instead of
// views::WebView. See chrome_browser_frame.h for related documentation.
class ChromeBrowserView
    : public CefViewView<BrowserView, CefBrowserViewDelegate> {
 public:
  using ParentClass = CefViewView<BrowserView, CefBrowserViewDelegate>;
  using Delegate = CefBrowserViewView::Delegate;

  // |cef_delegate| may be nullptr.
  // |browser_view_delegate| must be non-nullptr.
  ChromeBrowserView(CefBrowserViewDelegate* cef_delegate,
                    Delegate* browser_view_delegate);

  ChromeBrowserView(const ChromeBrowserView&) = delete;
  ChromeBrowserView& operator=(const ChromeBrowserView&) = delete;

  // Called by ChromeBrowserHostImpl.
  void InitBrowser(std::unique_ptr<Browser> browser,
                   CefRefPtr<CefBrowserView> browser_view);
  void Destroyed();

  // View methods:
  void ViewHierarchyChanged(
      const views::ViewHierarchyChangedDetails& details) override;
  void AddedToWidget() override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

  // BrowserView methods:
  ToolbarView* OverrideCreateToolbar(Browser* browser,
                                     BrowserView* browser_view) override;

  CefRefPtr<CefToolbarViewImpl> cef_toolbar() const { return cef_toolbar_; }

 private:
  // Not owned by this object.
  Delegate* browser_view_delegate_;

  Browser* browser_ = nullptr;
  views::WebView* web_view_ = nullptr;

  bool destroyed_ = false;

  CefRefPtr<CefToolbarViewImpl> cef_toolbar_;
};

#endif  // CEF_LIBCEF_BROWSER_CHROME_VIEWS_CHROME_BROWSER_VIEW_H_
