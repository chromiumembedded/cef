// Copyright 2016 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_VIEWS_LABEL_BUTTON_VIEW_H_
#define CEF_LIBCEF_BROWSER_VIEWS_LABEL_BUTTON_VIEW_H_
#pragma once

#include "include/views/cef_label_button.h"
#include "libcef/browser/views/button_view.h"

#include "base/logging.h"
#include "ui/gfx/font_list.h"

// Helpers for template boiler-plate.
#define CEF_LABEL_BUTTON_VIEW_T CEF_BUTTON_VIEW_T
#define CEF_LABEL_BUTTON_VIEW_A CEF_BUTTON_VIEW_A
#define CEF_LABEL_BUTTON_VIEW_D CefLabelButtonView<CEF_LABEL_BUTTON_VIEW_A>

// Template for implementing views::View-derived classes that support adding and
// removing children (called a Panel in CEF terminology). See comments in
// view_impl.h for a usage overview.
CEF_LABEL_BUTTON_VIEW_T class CefLabelButtonView : public CEF_BUTTON_VIEW_D {
 public:
  typedef CEF_BUTTON_VIEW_D ParentClass;

  // |cef_delegate| may be nullptr.
  explicit CefLabelButtonView(CefViewDelegateClass* cef_delegate)
      : ParentClass(cef_delegate) {
  }

  void Initialize() override {
    ParentClass::Initialize();

    // Use our defaults instead of the Views framework defaults.
    ParentClass::SetFontList(gfx::FontList(view_util::kDefaultFontList));
  }

  // Returns the CefLabelButton associated with this view. See comments on
  // CefViewView::GetCefView.
  CefRefPtr<CefLabelButton> GetCefLabelButton() const {
    CefRefPtr<CefLabelButton> label_button =
        ParentClass::GetCefButton()->AsLabelButton();
    DCHECK(label_button);
    return label_button;
  }

  // CefViewView methods:
  bool HasMinimumSize() const { return true; }
};

#endif  // CEF_LIBCEF_BROWSER_VIEWS_LABEL_BUTTON_VIEW_H_
