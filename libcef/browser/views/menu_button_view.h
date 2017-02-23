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
#include "ui/views/controls/button/menu_button_listener.h"

// Extend views::LabelButton with a no-argument constructor as required by the
// CefViewView template and extend views::ButtonListener as required by the
// CefButtonView template.
class MenuButtonEx : public views::MenuButton,
                     public views::ButtonListener,
                     public views::MenuButtonListener {
 public:
  MenuButtonEx() : views::MenuButton(base::string16(), this, true) {
    // TODO(cef): MenuButton should not use ButtonListener. See
    // http://crbug.com/585252 for details.
    Button::listener_ = this;
  }
};

class CefMenuButtonView :
    public CefLabelButtonView<MenuButtonEx, CefMenuButtonDelegate> {
 public:
  typedef CefLabelButtonView<MenuButtonEx, CefMenuButtonDelegate> ParentClass;

  // |cef_delegate| must not be nullptr.
  explicit CefMenuButtonView(CefMenuButtonDelegate* cef_delegate);

  void Initialize() override;

  // Returns the CefMenuButton associated with this view. See comments on
  // CefViewView::GetCefView.
  CefRefPtr<CefMenuButton> GetCefMenuButton() const;

  // Set the flags that control display of accelerator characters.
  void SetDrawStringsFlags(int flags);

  // views::MenuButtonListener methods:
  void OnMenuButtonClicked(views::MenuButton* source,
                           const gfx::Point& point,
                           const ui::Event* event) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(CefMenuButtonView);
};

#endif  // CEF_LIBCEF_BROWSER_VIEWS_MENU_BUTTON_VIEW_H_
