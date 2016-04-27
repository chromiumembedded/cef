// Copyright 2016 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_VIEWS_LABEL_BUTTON_IMPL_H_
#define CEF_LIBCEF_BROWSER_VIEWS_LABEL_BUTTON_IMPL_H_
#pragma once

#include "include/views/cef_button.h"
#include "include/views/cef_label_button.h"
#include "include/views/cef_menu_button.h"

#include "libcef/browser/image_impl.h"
#include "libcef/browser/views/button_impl.h"

#include "base/logging.h"
#include "ui/views/controls/button/label_button.h"

// Helpers for template boiler-plate.
#define CEF_LABEL_BUTTON_IMPL_T CEF_BUTTON_IMPL_T
#define CEF_LABEL_BUTTON_IMPL_A CEF_BUTTON_IMPL_A
#define CEF_LABEL_BUTTON_IMPL_D CefLabelButtonImpl<CEF_LABEL_BUTTON_IMPL_A>

// Template for implementing CefLabelButton-derived classes. See comments in
// view_impl.h for a usage overview.
CEF_LABEL_BUTTON_IMPL_T class CefLabelButtonImpl : public CEF_BUTTON_IMPL_D {
 public:
  typedef CEF_BUTTON_IMPL_D ParentClass;

  // CefLabelButton methods. When adding new As*() methods make sure to update
  // CefViewAdapter::GetFor() in view_adapter.cc.
  void SetText(const CefString& text) override;
  CefString GetText() override;
  void SetImage(cef_button_state_t button_state,
                CefRefPtr<CefImage> image) override;
  CefRefPtr<CefImage> GetImage(cef_button_state_t button_state) override;
  void SetTextColor(cef_button_state_t for_state, cef_color_t color) override;
  void SetEnabledTextColors(cef_color_t color) override;
  void SetFontList(const CefString& font_list) override;
  void SetHorizontalAlignment(cef_horizontal_alignment_t alignment) override;
  void SetMinimumSize(const CefSize& size) override;
  void SetMaximumSize(const CefSize& size) override;

  // CefLabelButton methods:
  CefRefPtr<CefMenuButton> AsMenuButton() override { return nullptr; }

  // CefButton methods:
  CefRefPtr<CefLabelButton> AsLabelButton() override { return this; }

  // CefViewAdapter methods:
  void GetDebugInfo(base::DictionaryValue* info,
                    bool include_children) override {
    ParentClass::GetDebugInfo(info, include_children);
    info->SetString("text", ParentClass::root_view()->GetText());
  }

 protected:
  // Create a new implementation object.
  // Always call Initialize() after creation.
  // |delegate| may be nullptr.
  explicit CefLabelButtonImpl(CefRefPtr<CefViewDelegateClass> delegate)
      : ParentClass(delegate) {
  }
};

CEF_LABEL_BUTTON_IMPL_T void CEF_LABEL_BUTTON_IMPL_D::SetText(
    const CefString& text) {
  CEF_REQUIRE_VALID_RETURN_VOID();
  ParentClass::root_view()->SetText(text);
}

CEF_LABEL_BUTTON_IMPL_T CefString CEF_LABEL_BUTTON_IMPL_D::GetText() {
  CEF_REQUIRE_VALID_RETURN(CefString());
  return ParentClass::root_view()->GetText();
}

CEF_LABEL_BUTTON_IMPL_T void CEF_LABEL_BUTTON_IMPL_D::SetImage(
    cef_button_state_t button_state,
    CefRefPtr<CefImage> image) {
  CEF_REQUIRE_VALID_RETURN_VOID();
  gfx::ImageSkia image_skia;
  if (image)
    image_skia = static_cast<CefImageImpl*>(image.get())->image().AsImageSkia();
  ParentClass::root_view()->SetImage(
      static_cast<views::Button::ButtonState>(button_state), image_skia);
}

CEF_LABEL_BUTTON_IMPL_T CefRefPtr<CefImage> CEF_LABEL_BUTTON_IMPL_D::GetImage(
    cef_button_state_t button_state) {
  CEF_REQUIRE_VALID_RETURN(nullptr);
  const gfx::ImageSkia& image_skia = ParentClass::root_view()->GetImage(
      static_cast<views::Button::ButtonState>(button_state));
  if (image_skia.isNull())
    return nullptr;
  return new CefImageImpl(image_skia);
}

CEF_LABEL_BUTTON_IMPL_T void CEF_LABEL_BUTTON_IMPL_D::SetTextColor(
    cef_button_state_t for_state, cef_color_t color) {
  CEF_REQUIRE_VALID_RETURN_VOID();
  ParentClass::root_view()->SetTextColor(
      static_cast<views::Button::ButtonState>(for_state), color);
}

CEF_LABEL_BUTTON_IMPL_T void CEF_LABEL_BUTTON_IMPL_D::SetEnabledTextColors(
    cef_color_t color) {
  CEF_REQUIRE_VALID_RETURN_VOID();
  ParentClass::root_view()->SetEnabledTextColors(color);
}

CEF_LABEL_BUTTON_IMPL_T void CEF_LABEL_BUTTON_IMPL_D::SetFontList(
    const CefString& font_list) {
  CEF_REQUIRE_VALID_RETURN_VOID();
  ParentClass::root_view()->SetFontList(gfx::FontList(font_list));
}

CEF_LABEL_BUTTON_IMPL_T void CEF_LABEL_BUTTON_IMPL_D::SetHorizontalAlignment(
    cef_horizontal_alignment_t alignment) {
  CEF_REQUIRE_VALID_RETURN_VOID();
  ParentClass::root_view()->SetHorizontalAlignment(
      static_cast<gfx::HorizontalAlignment>(alignment));
}

CEF_LABEL_BUTTON_IMPL_T void CEF_LABEL_BUTTON_IMPL_D::SetMinimumSize(
    const CefSize& size) {
  CEF_REQUIRE_VALID_RETURN_VOID();
  ParentClass::root_view()->SetMinSize(gfx::Size(size.width, size.height));
}

CEF_LABEL_BUTTON_IMPL_T void CEF_LABEL_BUTTON_IMPL_D::SetMaximumSize(
    const CefSize& size) {
  CEF_REQUIRE_VALID_RETURN_VOID();
  ParentClass::root_view()->SetMaxSize(gfx::Size(size.width, size.height));
}

#endif  // CEF_LIBCEF_BROWSER_VIEWS_LABEL_BUTTON_IMPL_H_
