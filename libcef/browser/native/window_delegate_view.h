// Copyright 2014 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_NATIVE_WINDOW_DELEGATE_VIEW_H_
#define CEF_LIBCEF_BROWSER_NATIVE_WINDOW_DELEGATE_VIEW_H_
#pragma once

#include "ui/views/widget/widget_delegate.h"

namespace content {
class WebContents;
}

namespace views {
class WebView;
}

// Manages the views-based root window that hosts the web contents. This object
// will be deleted automatically when the associated root window is destroyed.
class CefWindowDelegateView : public views::WidgetDelegateView {
 public:
  explicit CefWindowDelegateView(SkColor background_color);

  // Create the Widget and associated root window.
  void Init(gfx::AcceleratedWidget parent_widget,
            content::WebContents* web_contents,
            const gfx::Rect& bounds);

 private:
  // Initialize the Widget's content.
  void InitContent();

  // WidgetDelegateView methods:
  bool CanResize() const override { return true; }
  bool CanMaximize() const override { return true; }
  View* GetContentsView() override { return this; }

  // View methods:
  void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) override;

 private:
  SkColor background_color_;
  views::WebView* web_view_;

  DISALLOW_COPY_AND_ASSIGN(CefWindowDelegateView);
};

#endif  // CEF_LIBCEF_BROWSER_NATIVE_WINDOW_DELEGATE_VIEW_H_
