// Copyright 2016 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_VIEWS_BROWSER_VIEW_VIEW_H_
#define CEF_LIBCEF_BROWSER_VIEWS_BROWSER_VIEW_VIEW_H_
#pragma once

#include "include/views/cef_browser_view_delegate.h"

#include "libcef/browser/views/view_view.h"

#include "ui/views/controls/webview/webview.h"

// Extend views::WebView with a no-argument constructor as required by the
// CefViewView template.
class WebViewEx : public views::WebView {
 public:
  WebViewEx() : views::WebView(nullptr) {
  }
};

class CefBrowserViewView :
    public CefViewView<WebViewEx, CefBrowserViewDelegate> {
 public:
  typedef CefViewView<WebViewEx, CefBrowserViewDelegate> ParentClass;

  class Delegate {
   public:
    // Called when the BrowserView has been added to a parent view.
    virtual void OnBrowserViewAdded() = 0;

   protected:
    virtual ~Delegate() {}
  };

  // |cef_delegate| may be nullptr.
  // |browser_view_delegate| must be non-nullptr.
  CefBrowserViewView(CefBrowserViewDelegate* cef_delegate,
                     Delegate* browser_view_delegate);

  // View methods:
  void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) override;

 private:
  // Not owned by this object.
  Delegate* browser_view_delegate_;

  DISALLOW_COPY_AND_ASSIGN(CefBrowserViewView);
};

#endif  // CEF_LIBCEF_BROWSER_VIEWS_BROWSER_VIEW_VIEW_H_
