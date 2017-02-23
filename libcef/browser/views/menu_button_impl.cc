// Copyright 2016 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "libcef/browser/views/menu_button_impl.h"

#include "libcef/browser/views/menu_button_view.h"
#include "libcef/browser/views/window_impl.h"

#include "ui/gfx/canvas.h"

// static
CefRefPtr<CefMenuButton> CefMenuButton::CreateMenuButton(
    CefRefPtr<CefMenuButtonDelegate> delegate,
    const CefString& text,
    bool with_frame,
    bool with_menu_marker) {
  return CefMenuButtonImpl::Create(delegate, text, with_frame,
                                   with_menu_marker);
}

// static
CefRefPtr<CefMenuButtonImpl> CefMenuButtonImpl::Create(
    CefRefPtr<CefMenuButtonDelegate> delegate,
    const CefString& text,
    bool with_frame,
    bool with_menu_marker) {
  CEF_REQUIRE_UIT_RETURN(nullptr);
  DCHECK(delegate);
  if (!delegate)
    return nullptr;
  CefRefPtr<CefMenuButtonImpl> menu_button = new CefMenuButtonImpl(delegate);
  menu_button->Initialize();
  if (!text.empty())
    menu_button->SetText(text);
  if (with_frame)
    menu_button->root_view()->SetStyle(views::CustomButton::STYLE_BUTTON);
  menu_button->root_view()->set_show_menu_marker(with_menu_marker);
  return menu_button;
}

void CefMenuButtonImpl::ShowMenu(CefRefPtr<CefMenuModel> menu_model,
                                 const CefPoint& screen_point,
                                 cef_menu_anchor_position_t anchor_position) {
  CEF_REQUIRE_VALID_RETURN_VOID();
  CefRefPtr<CefWindow> window =
      view_util::GetWindowFor(root_view()->GetWidget());
  CefWindowImpl* window_impl = static_cast<CefWindowImpl*>(window.get());
  if (window_impl) {
    window_impl->ShowMenu(root_view(), menu_model, screen_point,
                          anchor_position);
  }
}

void CefMenuButtonImpl::TriggerMenu() {
  CEF_REQUIRE_VALID_RETURN_VOID();
  root_view()->Activate(nullptr);
}

void CefMenuButtonImpl::SetFocusable(bool focusable) {
  CEF_REQUIRE_VALID_RETURN_VOID();
  static_cast<CefMenuButtonView*>(root_view())->SetDrawStringsFlags(
      focusable ? gfx::Canvas::SHOW_PREFIX : gfx::Canvas::HIDE_PREFIX);
  ParentClass::SetFocusable(focusable);
}

CefMenuButtonImpl::CefMenuButtonImpl(CefRefPtr<CefMenuButtonDelegate> delegate)
    : ParentClass(delegate) {
  DCHECK(delegate);
}

views::MenuButton* CefMenuButtonImpl::CreateRootView() {
  return new CefMenuButtonView(delegate());
}

void CefMenuButtonImpl::InitializeRootView() {
  static_cast<CefMenuButtonView*>(root_view())->Initialize();
}
