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
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/button/button.h"

// Helpers for template boiler-plate.
#define CEF_BUTTON_VIEW_T CEF_VIEW_VIEW_T
#define CEF_BUTTON_VIEW_A CEF_VIEW_VIEW_A
#define CEF_BUTTON_VIEW_D CefButtonView<CEF_BUTTON_VIEW_A>

// Template for implementing views::Button-derived classes. The
// views::Button-derived type passed to this template must extend
// views::ButtonListener (for example, see LabelButtonEx from
// basic_label_button_view.h). See comments in view_impl.h for a usage overview.
CEF_BUTTON_VIEW_T class CefButtonView : public CEF_VIEW_VIEW_D {
 public:
  using ParentClass = CEF_VIEW_VIEW_D;

  // |cef_delegate| may be nullptr.
  template <typename... Args>
  explicit CefButtonView(CefViewDelegateClass* cef_delegate, Args... args)
      : ParentClass(cef_delegate, args...) {}

  // Returns the CefButton associated with this view. See comments on
  // CefViewView::GetCefView.
  CefRefPtr<CefButton> GetCefButton() const {
    CefRefPtr<CefButton> button = ParentClass::GetCefView()->AsButton();
    DCHECK(button);
    return button;
  }

  // views::Button methods:
  void StateChanged(views::Button::ButtonState old_state) override;

  // LabelButtonEx methods:
  void ButtonPressed(const ui::Event& event) override;
};

CEF_BUTTON_VIEW_T void CEF_BUTTON_VIEW_D::StateChanged(
    views::Button::ButtonState old_state) {
  ParentClass::StateChanged(old_state);
  if (ParentClass::cef_delegate()) {
    ParentClass::cef_delegate()->OnButtonStateChanged(GetCefButton());
  }
}

CEF_BUTTON_VIEW_T void CEF_BUTTON_VIEW_D::ButtonPressed(
    const ui::Event& event) {
  // Callback may trigger new animation state.
  if (ParentClass::cef_delegate()) {
    ParentClass::cef_delegate()->OnButtonPressed(GetCefButton());
  }
  if (views::InkDrop::Get(this)->ink_drop_mode() !=
          views::InkDropHost::InkDropMode::OFF &&
      !ParentClass::IsFocusable() &&
      ParentClass::GetState() != views::Button::STATE_PRESSED) {
    // Ink drop state does not get reset properly on click when the button is
    // non-focusable. Reset the ink drop state here if the state has not been
    // explicitly set to pressed by the OnButtonPressed callback calling
    // SetState (which also sets the ink drop state).
    views::InkDrop::Get(this)->AnimateToState(
        views::InkDropState::HIDDEN, ui::LocatedEvent::FromIfValid(&event));
  }
}

#endif  // CEF_LIBCEF_BROWSER_VIEWS_BUTTON_VIEW_H_
