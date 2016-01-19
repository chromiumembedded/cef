// Copyright 2016 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_VIEWS_BASIC_LABEL_BUTTON_VIEW_H_
#define CEF_LIBCEF_BROWSER_VIEWS_BASIC_LABEL_BUTTON_VIEW_H_
#pragma once

#include "include/views/cef_button_delegate.h"

#include "libcef/browser/views/label_button_view.h"

#include "ui/views/controls/button/label_button.h"

// Extend views::LabelButton with a no-argument constructor as required by the
// CefViewView template and extend views::ButtonListener as required by the
// CefButtonView template.
class LabelButtonEx : public views::LabelButton,
                      public views::ButtonListener {
 public:
  LabelButtonEx() : views::LabelButton(this, base::string16()) {
  }
};

class CefBasicLabelButtonView :
    public CefLabelButtonView<LabelButtonEx, CefButtonDelegate> {
 public:
  typedef CefLabelButtonView<LabelButtonEx, CefButtonDelegate> ParentClass;

  // |cef_delegate| may be nullptr.
  explicit CefBasicLabelButtonView(CefButtonDelegate* cef_delegate);

 private:
  DISALLOW_COPY_AND_ASSIGN(CefBasicLabelButtonView);
};

#endif  // CEF_LIBCEF_BROWSER_VIEWS_BASIC_LABEL_BUTTON_VIEW_H_
