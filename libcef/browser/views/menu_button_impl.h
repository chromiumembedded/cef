// Copyright 2016 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_VIEWS_MENU_BUTTON_IMPL_H_
#define CEF_LIBCEF_BROWSER_VIEWS_MENU_BUTTON_IMPL_H_
#pragma once

#include "include/views/cef_menu_button.h"
#include "include/views/cef_menu_button_delegate.h"

#include "libcef/browser/menu_model_impl.h"
#include "libcef/browser/views/label_button_impl.h"

#include "ui/views/controls/button/menu_button.h"

class CefMenuButtonImpl :
    public CefLabelButtonImpl<views::MenuButton, CefMenuButton,
                              CefMenuButtonDelegate> {
 public:
  typedef CefLabelButtonImpl<views::MenuButton, CefMenuButton,
                             CefMenuButtonDelegate> ParentClass;

  // Create a new CefMenuButton instance. |delegate| must not be nullptr.
  static CefRefPtr<CefMenuButtonImpl> Create(
      CefRefPtr<CefMenuButtonDelegate> delegate,
      const CefString& text,
      bool with_frame,
      bool with_menu_marker);

  // CefMenuButton methods:
  void ShowMenu(CefRefPtr<CefMenuModel> menu_model,
                const CefPoint& screen_point,
                cef_menu_anchor_position_t anchor_position) override;
  void TriggerMenu() override;

  // CefLabelButton methods:
  CefRefPtr<CefMenuButton> AsMenuButton() override { return this; }

  // CefViewAdapter methods:
  std::string GetDebugType() override { return "MenuButton"; }

  // CefView methods:
  void SetFocusable(bool focusable) override;

 private:
  // Create a new implementation object.
  // Always call Initialize() after creation.
  // |delegate| must not be nullptr.
  explicit CefMenuButtonImpl(CefRefPtr<CefMenuButtonDelegate> delegate);

  // CefViewImpl methods:
  views::MenuButton* CreateRootView() override;
  void InitializeRootView() override;

  IMPLEMENT_REFCOUNTING_DELETE_ON_UIT(CefMenuButtonImpl);
  DISALLOW_COPY_AND_ASSIGN(CefMenuButtonImpl);
};

#endif  // CEF_LIBCEF_BROWSER_VIEWS_MENU_BUTTON_IMPL_H_
