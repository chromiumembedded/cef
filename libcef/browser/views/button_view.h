// Copyright 2016 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_VIEWS_BUTTON_VIEW_H_
#define CEF_LIBCEF_BROWSER_VIEWS_BUTTON_VIEW_H_
#pragma once

#include "include/views/cef_button_delegate.h"

#include "libcef/browser/thread_util.h"
#include "libcef/browser/views/view_view.h"

#include "base/logging.h"
#include "ui/views/controls/button/custom_button.h"

// Helpers for template boiler-plate.
#define CEF_BUTTON_VIEW_T CEF_VIEW_VIEW_T
#define CEF_BUTTON_VIEW_A CEF_VIEW_VIEW_A
#define CEF_BUTTON_VIEW_D CefButtonView<CEF_BUTTON_VIEW_A>

// Template for implementing views::CustomButton-derived classes. The
// views::CustomButton-derived type passed to this template must extend
// views::ButtonListener (for example, see LabelButtonEx from
// basic_label_button_view.h). See comments in view_impl.h for a usage overview.
CEF_BUTTON_VIEW_T class CefButtonView : public CEF_VIEW_VIEW_D {
 public:
  typedef CEF_VIEW_VIEW_D ParentClass;

  // |cef_delegate| may be nullptr.
  explicit CefButtonView(CefViewDelegateClass* cef_delegate)
      : ParentClass(cef_delegate) {
  }

  // Returns the CefButton associated with this view. See comments on
  // CefViewView::GetCefView.
  CefRefPtr<CefButton> GetCefButton() const {
    CefRefPtr<CefButton> button = ParentClass::GetCefView()->AsButton();
    DCHECK(button);
    return button;
  }

  // views::CustomButton methods:
  void StateChanged(views::CustomButton::ButtonState old_state) override;

  // views::ButtonListener methods:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;
};

CEF_BUTTON_VIEW_T void CEF_BUTTON_VIEW_D::StateChanged(
    views::CustomButton::ButtonState old_state) {
  ParentClass::StateChanged(old_state);
  if (ParentClass::cef_delegate())
    ParentClass::cef_delegate()->OnButtonStateChanged(GetCefButton());
}

CEF_BUTTON_VIEW_T void CEF_BUTTON_VIEW_D::ButtonPressed(
    views::Button* sender, const ui::Event& event) {
  if (ParentClass::cef_delegate())
    ParentClass::cef_delegate()->OnButtonPressed(GetCefButton());
  if (ParentClass::ink_drop_mode() != views::CustomButton::InkDropMode::OFF &&
      !ParentClass::IsFocusable()) {
    // When ink drop is enabled for non-focusable buttons the ink drop state
    // does not get reset properly on click, so we do it here explicitly.
    ParentClass::AnimateInkDrop(views::InkDropState::HIDDEN,
                                ui::LocatedEvent::FromIfValid(&event));
  }
}

#endif  // CEF_LIBCEF_BROWSER_VIEWS_BUTTON_VIEW_H_
