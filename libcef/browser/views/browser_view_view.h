// Copyright 2016 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_VIEWS_BROWSER_VIEW_VIEW_H_
#define CEF_LIBCEF_BROWSER_VIEWS_BROWSER_VIEW_VIEW_H_
#pragma once

#include "base/memory/raw_ptr.h"
#include "cef/include/views/cef_browser_view_delegate.h"
#include "cef/libcef/browser/views/view_view.h"
#include "ui/views/controls/webview/webview.h"

// Extend views::WebView with a no-argument constructor as required by the
// CefViewView template.
class WebViewEx : public views::WebView {
  METADATA_HEADER(WebViewEx, views::WebView)

 public:
  WebViewEx() : views::WebView(nullptr) {}
};

class CefBrowserViewView
    : public CefViewView<WebViewEx, CefBrowserViewDelegate> {
  METADATA_HEADER(CefBrowserViewView, WebViewEx)

 public:
  using ParentClass = CefViewView<WebViewEx, CefBrowserViewDelegate>;

  CefBrowserViewView(const CefBrowserViewView&) = delete;
  CefBrowserViewView& operator=(const CefBrowserViewView&) = delete;

  class Delegate {
   public:
    // Called when the BrowserView is added or removed from a Widget.
    virtual void AddedToWidget() = 0;
    virtual void RemovedFromWidget() = 0;

    // Called when the BrowserView bounds have changed.
    virtual void OnBoundsChanged() = 0;

    // Called when the BrowserView receives a gesture event.
    // Returns true if the gesture was handled.
    virtual bool OnGestureEvent(ui::GestureEvent* event) = 0;

   protected:
    virtual ~Delegate() = default;
  };

  // |cef_delegate| may be nullptr.
  // |browser_view_delegate| must be non-nullptr.
  CefBrowserViewView(CefBrowserViewDelegate* cef_delegate,
                     Delegate* browser_view_delegate);

  // View methods:
  void ViewHierarchyChanged(
      const views::ViewHierarchyChangedDetails& details) override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void AddedToWidget() override;
  void RemovedFromWidget() override;

 private:
  // Not owned by this object.
  raw_ptr<Delegate> browser_view_delegate_;
};

#endif  // CEF_LIBCEF_BROWSER_VIEWS_BROWSER_VIEW_VIEW_H_
