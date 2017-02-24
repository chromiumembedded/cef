// Copyright 2016 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_VIEWS_BUTTON_IMPL_H_
#define CEF_LIBCEF_BROWSER_VIEWS_BUTTON_IMPL_H_
#pragma once

#include "include/views/cef_button.h"
#include "include/views/cef_label_button.h"

#include "libcef/browser/views/view_impl.h"

#include "base/logging.h"
#include "ui/gfx/color_utils.h"
#include "ui/views/controls/button/custom_button.h"

// Helpers for template boiler-plate.
#define CEF_BUTTON_IMPL_T CEF_VIEW_IMPL_T
#define CEF_BUTTON_IMPL_A CEF_VIEW_IMPL_A
#define CEF_BUTTON_IMPL_D CefButtonImpl<CEF_BUTTON_IMPL_A>

// Template for implementing CefButton-derived classes. See comments in
// view_impl.h for a usage overview.
CEF_BUTTON_IMPL_T class CefButtonImpl : public CEF_VIEW_IMPL_D {
 public:
  typedef CEF_VIEW_IMPL_D ParentClass;

  // CefButton methods. When adding new As*() methods make sure to update
  // CefViewAdapter::GetFor() in view_adapter.cc.
  CefRefPtr<CefLabelButton> AsLabelButton() override { return nullptr; }
  void SetState(cef_button_state_t state) override;
  cef_button_state_t GetState() override;
  void SetInkDropEnabled(bool enabled) override;
  void SetTooltipText(const CefString& tooltip_text) override;
  void SetAccessibleName(const CefString& name) override;

  // CefView methods:
  CefRefPtr<CefButton> AsButton() override { return this; }
  void SetFocusable(bool focusable) override;

 protected:
  // Create a new implementation object.
  // Always call Initialize() after creation.
  // |delegate| may be nullptr.
  explicit CefButtonImpl(CefRefPtr<CefViewDelegateClass> delegate)
      : ParentClass(delegate) {
  }
};

CEF_BUTTON_IMPL_T void CEF_BUTTON_IMPL_D::SetState(cef_button_state_t state) {
  CEF_REQUIRE_VALID_RETURN_VOID();
  ParentClass::root_view()->SetState(
      static_cast<views::Button::ButtonState>(state));
}

CEF_BUTTON_IMPL_T cef_button_state_t CEF_BUTTON_IMPL_D::GetState() {
  CEF_REQUIRE_VALID_RETURN(CEF_BUTTON_STATE_NORMAL);
  return static_cast<cef_button_state_t>(ParentClass::root_view()->state());
}

CEF_BUTTON_IMPL_T void CEF_BUTTON_IMPL_D::SetInkDropEnabled(bool enabled) {
  CEF_REQUIRE_VALID_RETURN_VOID();
  ParentClass::root_view()->SetInkDropMode(
      enabled ? views::InkDropHostView::InkDropMode::ON :
                views::InkDropHostView::InkDropMode::OFF);
  if (enabled) {
    ParentClass::root_view()->set_ink_drop_base_color(
        color_utils::BlendTowardOppositeLuma(
            ParentClass::root_view()->background()->get_color(), 0x61));
  }
}

CEF_BUTTON_IMPL_T void CEF_BUTTON_IMPL_D::SetTooltipText(
    const CefString& tooltip_text) {
  CEF_REQUIRE_VALID_RETURN_VOID();
  ParentClass::root_view()->SetTooltipText(tooltip_text);
}

CEF_BUTTON_IMPL_T void CEF_BUTTON_IMPL_D::SetAccessibleName(
    const CefString& name) {
  CEF_REQUIRE_VALID_RETURN_VOID();
  ParentClass::root_view()->SetAccessibleName(name);
}

CEF_BUTTON_IMPL_T void CEF_BUTTON_IMPL_D::SetFocusable(
    bool focusable) {
  CEF_REQUIRE_VALID_RETURN_VOID();
  ParentClass::root_view()->set_request_focus_on_press(focusable);
  ParentClass::SetFocusable(focusable);
}

#endif  // CEF_LIBCEF_BROWSER_VIEWS_BUTTON_IMPL_H_
