// Copyright 2016 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_VIEWS_PANEL_VIEW_H_
#define CEF_LIBCEF_BROWSER_VIEWS_PANEL_VIEW_H_
#pragma once

#include "include/views/cef_panel_delegate.h"

#include "libcef/browser/thread_util.h"
#include "libcef/browser/views/view_view.h"

#include "base/logging.h"

// Helpers for template boiler-plate.
#define CEF_PANEL_VIEW_T CEF_VIEW_VIEW_T
#define CEF_PANEL_VIEW_A CEF_VIEW_VIEW_A
#define CEF_PANEL_VIEW_D CefPanelView<CEF_PANEL_VIEW_A>

// Template for implementing views::View-derived classes that support adding and
// removing children (called a Panel in CEF terminology). See comments in
// view_impl.h for a usage overview.
CEF_PANEL_VIEW_T class CefPanelView : public CEF_VIEW_VIEW_D {
 public:
  typedef CEF_VIEW_VIEW_D ParentClass;

  // |cef_delegate| may be nullptr.
  explicit CefPanelView(CefViewDelegateClass* cef_delegate)
      : ParentClass(cef_delegate) {
  }

  // Returns the CefPanel associated with this view. See comments on
  // CefViewView::GetCefView.
  CefRefPtr<CefPanel> GetCefPanel() const {
    CefRefPtr<CefPanel> panel = ParentClass::GetCefView()->AsPanel();
    DCHECK(panel);
    return panel;
  }
};

#endif  // CEF_LIBCEF_BROWSER_VIEWS_PANEL_VIEW_H_
