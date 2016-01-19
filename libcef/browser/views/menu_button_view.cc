// Copyright 2016 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "libcef/browser/views/menu_button_view.h"

CefMenuButtonView::CefMenuButtonView(
    CefMenuButtonDelegate* cef_delegate)
    : ParentClass(cef_delegate) {
  DCHECK(cef_delegate);
}

CefRefPtr<CefMenuButton> CefMenuButtonView::GetCefMenuButton() const {
  CefRefPtr<CefMenuButton> menu_button = GetCefLabelButton()->AsMenuButton();
  DCHECK(menu_button);
  return menu_button;
}

void CefMenuButtonView::OnMenuButtonClicked(views::MenuButton* source,
                                            const gfx::Point& point,
                                            const ui::Event* event) {
  cef_delegate()->OnMenuButtonPressed(GetCefMenuButton(),
                                      CefPoint(point.x(), point.y()));
}
