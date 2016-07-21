// Copyright 2016 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_VIEWS_BASIC_LABEL_BUTTON_IMPL_H_
#define CEF_LIBCEF_BROWSER_VIEWS_BASIC_LABEL_BUTTON_IMPL_H_
#pragma once

#include "include/views/cef_label_button.h"
#include "include/views/cef_button_delegate.h"

#include "libcef/browser/views/label_button_impl.h"

#include "ui/views/controls/button/label_button.h"

class CefBasicLabelButtonImpl :
    public CefLabelButtonImpl<views::LabelButton, CefLabelButton,
                              CefButtonDelegate> {
 public:
  typedef CefLabelButtonImpl<views::LabelButton, CefLabelButton,
                             CefButtonDelegate> ParentClass;

  // Create a new CefLabelButton instance. |delegate| may be nullptr.
  static CefRefPtr<CefBasicLabelButtonImpl> Create(
      CefRefPtr<CefButtonDelegate> delegate,
      const CefString& text,
      bool with_frame);

  // CefViewAdapter methods:
  std::string GetDebugType() override { return "LabelButton"; }

 private:
  // Create a new implementation object.
  // Always call Initialize() after creation.
  // |delegate| may be nullptr.
  explicit CefBasicLabelButtonImpl(CefRefPtr<CefButtonDelegate> delegate);

  // CefViewImpl methods:
  views::LabelButton* CreateRootView() override;
  void InitializeRootView() override;

  IMPLEMENT_REFCOUNTING_DELETE_ON_UIT(CefBasicLabelButtonImpl);
  DISALLOW_COPY_AND_ASSIGN(CefBasicLabelButtonImpl);
};

#endif  // CEF_LIBCEF_BROWSER_VIEWS_BASIC_LABEL_BUTTON_IMPL_H_
