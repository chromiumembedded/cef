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
  CefWindowDelegateView(SkColor background_color,
                        bool always_on_top,
                        base::RepeatingClosure on_bounds_changed,
                        base::OnceClosure on_delete);

  CefWindowDelegateView(const CefWindowDelegateView&) = delete;
  CefWindowDelegateView& operator=(const CefWindowDelegateView&) = delete;

  // Create the Widget and associated root window.
  void Init(gfx::AcceleratedWidget parent_widget,
            content::WebContents* web_contents,
            const gfx::Rect& bounds);

 private:
  // Initialize the Widget's content.
  void InitContent();

  void DeleteDelegate();

  // WidgetDelegateView methods:
  bool CanMaximize() const override { return true; }
  View* GetContentsView() override { return this; }

  // View methods:
  void ViewHierarchyChanged(
      const views::ViewHierarchyChangedDetails& details) override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

 private:
  const SkColor background_color_;
  const bool always_on_top_;
  base::RepeatingClosure on_bounds_changed_;
  base::OnceClosure on_delete_;

  views::WebView* web_view_ = nullptr;
};

#endif  // CEF_LIBCEF_BROWSER_NATIVE_WINDOW_DELEGATE_VIEW_H_
