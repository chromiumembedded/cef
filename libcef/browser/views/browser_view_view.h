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
    // Mouse events on draggable regions will not be handled by the WebView.
    // Avoid the resulting DCHECK in NativeViewHost::OnMousePressed by
    // configuring the NativeViewHost not to process events via the view
    // hierarchy.
    holder()->SetCanProcessEventsWithinSubtree(false);
  }
};

class CefBrowserViewView
    : public CefViewView<WebViewEx, CefBrowserViewDelegate> {
 public:
  using ParentClass = CefViewView<WebViewEx, CefBrowserViewDelegate>;

  CefBrowserViewView(const CefBrowserViewView&) = delete;
  CefBrowserViewView& operator=(const CefBrowserViewView&) = delete;

  class Delegate {
   public:
    // Called when the BrowserView has been added to a parent view.
    virtual void OnBrowserViewAdded() = 0;

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

 private:
  // Not owned by this object.
  Delegate* browser_view_delegate_;
};

#endif  // CEF_LIBCEF_BROWSER_VIEWS_BROWSER_VIEW_VIEW_H_
