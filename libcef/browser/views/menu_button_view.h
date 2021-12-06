// Copyright 2016 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_VIEWS_MENU_BUTTON_VIEW_H_
#define CEF_LIBCEF_BROWSER_VIEWS_MENU_BUTTON_VIEW_H_
#pragma once

#include "include/views/cef_menu_button.h"
#include "include/views/cef_menu_button_delegate.h"

#include "libcef/browser/views/label_button_view.h"

#include "ui/views/controls/button/menu_button.h"

// Extend views::LabelButton with a no-argument constructor as required by the
// CefViewView template and extend views::ButtonListener as required by the
// CefButtonView template.
class MenuButtonEx : public views::MenuButton {
 public:
  MenuButtonEx()
      : views::MenuButton(base::BindRepeating(
            [](MenuButtonEx* self, const ui::Event& event) {
              self->ButtonPressed(event);
            },
            base::Unretained(this))) {}

  virtual void ButtonPressed(const ui::Event& event) = 0;
};

class CefMenuButtonView
    : public CefLabelButtonView<MenuButtonEx, CefMenuButtonDelegate> {
 public:
  using ParentClass = CefLabelButtonView<MenuButtonEx, CefMenuButtonDelegate>;

  // |cef_delegate| must not be nullptr.
  explicit CefMenuButtonView(CefMenuButtonDelegate* cef_delegate);

  CefMenuButtonView(const CefMenuButtonView&) = delete;
  CefMenuButtonView& operator=(const CefMenuButtonView&) = delete;

  void Initialize() override;

  // Returns the CefMenuButton associated with this view. See comments on
  // CefViewView::GetCefView.
  CefRefPtr<CefMenuButton> GetCefMenuButton() const;

  // Set the flags that control display of accelerator characters.
  void SetDrawStringsFlags(int flags);

  // MenuButtonEx methods:
  void ButtonPressed(const ui::Event& event) override;
};

#endif  // CEF_LIBCEF_BROWSER_VIEWS_MENU_BUTTON_VIEW_H_
