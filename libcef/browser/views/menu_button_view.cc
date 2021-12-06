// Copyright 2016 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "libcef/browser/views/menu_button_view.h"

#include "libcef/browser/thread_util.h"

#include "ui/gfx/canvas.h"
#include "ui/views/controls/button/menu_button_controller.h"
#include "ui/views/controls/menu/menu_config.h"

namespace {

class ButtonPressedLock : public CefMenuButtonPressedLock {
 public:
  explicit ButtonPressedLock(views::MenuButton* menu_button)
      : pressed_lock_(menu_button->button_controller()) {}

  ButtonPressedLock(const ButtonPressedLock&) = delete;
  ButtonPressedLock& operator=(const ButtonPressedLock&) = delete;

 private:
  views::MenuButtonController::PressedLock pressed_lock_;

  IMPLEMENT_REFCOUNTING_DELETE_ON_UIT(ButtonPressedLock);
};

}  // namespace

CefMenuButtonView::CefMenuButtonView(CefMenuButtonDelegate* cef_delegate)
    : ParentClass(cef_delegate) {
  DCHECK(cef_delegate);
}

void CefMenuButtonView::Initialize() {
  ParentClass::Initialize();

  SetDrawStringsFlags(IsFocusable() ? gfx::Canvas::SHOW_PREFIX
                                    : gfx::Canvas::HIDE_PREFIX);

  // Use the same default font as MenuItemView.
  SetFontList(views::MenuConfig::instance().font_list);
}

CefRefPtr<CefMenuButton> CefMenuButtonView::GetCefMenuButton() const {
  CefRefPtr<CefMenuButton> menu_button = GetCefLabelButton()->AsMenuButton();
  DCHECK(menu_button);
  return menu_button;
}

void CefMenuButtonView::SetDrawStringsFlags(int flags) {
  label()->SetDrawStringsFlags(flags);
}

void CefMenuButtonView::ButtonPressed(const ui::Event& event) {
  auto position = GetMenuPosition();
  cef_delegate()->OnMenuButtonPressed(GetCefMenuButton(),
                                      CefPoint(position.x(), position.y()),
                                      new ButtonPressedLock(this));
}
