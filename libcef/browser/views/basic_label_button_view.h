// Copyright 2016 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_VIEWS_BASIC_LABEL_BUTTON_VIEW_H_
#define CEF_LIBCEF_BROWSER_VIEWS_BASIC_LABEL_BUTTON_VIEW_H_
#pragma once

#include "cef/include/views/cef_button_delegate.h"
#include "cef/libcef/browser/views/label_button_view.h"
#include "ui/views/controls/button/label_button.h"

// Extend views::LabelButton with a no-argument constructor as required by the
// CefViewView template and extend views::ButtonListener as required by the
// CefButtonView template.
class LabelButtonEx : public views::LabelButton {
  METADATA_HEADER(LabelButtonEx, views::LabelButton)

 public:
  LabelButtonEx()
      : views::LabelButton(base::BindRepeating(
                               [](LabelButtonEx* self, const ui::Event& event) {
                                 self->ButtonPressed(event);
                               },
                               base::Unretained(this)),
                           std::u16string()) {}

  virtual void ButtonPressed(const ui::Event& event) = 0;
};

class CefBasicLabelButtonView
    : public CefLabelButtonView<LabelButtonEx, CefButtonDelegate> {
  METADATA_HEADER(CefBasicLabelButtonView, LabelButtonEx)

 public:
  using ParentClass = CefLabelButtonView<LabelButtonEx, CefButtonDelegate>;

  // |cef_delegate| may be nullptr.
  explicit CefBasicLabelButtonView(CefButtonDelegate* cef_delegate);

  CefBasicLabelButtonView(const CefBasicLabelButtonView&) = delete;
  CefBasicLabelButtonView& operator=(const CefBasicLabelButtonView&) = delete;
};

#endif  // CEF_LIBCEF_BROWSER_VIEWS_BASIC_LABEL_BUTTON_VIEW_H_
