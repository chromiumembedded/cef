// Copyright 2016 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_VIEWS_TEXTFIELD_VIEW_H_
#define CEF_LIBCEF_BROWSER_VIEWS_TEXTFIELD_VIEW_H_
#pragma once

#include "include/views/cef_textfield_delegate.h"

#include "include/views/cef_textfield.h"
#include "libcef/browser/views/view_view.h"

#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_controller.h"

class CefTextfieldView :
    public CefViewView<views::Textfield, CefTextfieldDelegate>,
    public views::TextfieldController {
 public:
  typedef CefViewView<views::Textfield, CefTextfieldDelegate> ParentClass;

  // |cef_delegate| may be nullptr.
  explicit CefTextfieldView(CefTextfieldDelegate* cef_delegate);

  void Initialize() override;

  // Returns the CefTextfield associated with this view. See comments on
  // CefViewView::GetCefView.
  CefRefPtr<CefTextfield> GetCefTextfield() const {
    CefRefPtr<CefTextfield> textfield = GetCefView()->AsTextfield();
    DCHECK(textfield);
    return textfield;
  }

  // TextfieldController methods:
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override;
  void OnAfterUserAction(views::Textfield* sender) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(CefTextfieldView);
};

#endif  // CEF_LIBCEF_BROWSER_VIEWS_TEXTFIELD_VIEW_H_
