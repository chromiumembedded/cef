// Copyright 2016 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "libcef/browser/views/textfield_impl.h"

#include "libcef/browser/thread_util.h"

// static
CefRefPtr<CefTextfield> CefTextfield::CreateTextfield(
    CefRefPtr<CefTextfieldDelegate> delegate) {
  return CefTextfieldImpl::Create(delegate);
}

// static
CefRefPtr<CefTextfieldImpl> CefTextfieldImpl::Create(
    CefRefPtr<CefTextfieldDelegate> delegate) {
  CEF_REQUIRE_UIT_RETURN(nullptr);
  CefRefPtr<CefTextfieldImpl> textfield = new CefTextfieldImpl(delegate);
  textfield->Initialize();
  return textfield;
}

void CefTextfieldImpl::SetPasswordInput(bool password_input) {
  CEF_REQUIRE_VALID_RETURN_VOID();
  root_view()->SetTextInputType(password_input ? ui::TEXT_INPUT_TYPE_PASSWORD :
                                                 ui::TEXT_INPUT_TYPE_TEXT);
}

bool CefTextfieldImpl::IsPasswordInput() {
  CEF_REQUIRE_VALID_RETURN(false);
  return (root_view()->GetTextInputType() == ui::TEXT_INPUT_TYPE_PASSWORD);
}

void CefTextfieldImpl::SetReadOnly(bool read_only) {
  CEF_REQUIRE_VALID_RETURN_VOID();
  root_view()->SetReadOnly(read_only);
}

bool CefTextfieldImpl::IsReadOnly() {
  CEF_REQUIRE_VALID_RETURN(false);
  return root_view()->read_only();
}

CefString CefTextfieldImpl::GetText() {
  CEF_REQUIRE_VALID_RETURN(CefString());
  return root_view()->text();
}

void CefTextfieldImpl::SetText(const CefString& text) {
  CEF_REQUIRE_VALID_RETURN_VOID();
  root_view()->SetText(text);
}

void CefTextfieldImpl::AppendText(const CefString& text) {
  CEF_REQUIRE_VALID_RETURN_VOID();
  root_view()->AppendText(text);
}

void CefTextfieldImpl::InsertOrReplaceText(const CefString& text) {
  CEF_REQUIRE_VALID_RETURN_VOID();
  root_view()->InsertOrReplaceText(text);
}

bool CefTextfieldImpl::HasSelection() {
  CEF_REQUIRE_VALID_RETURN(false);
  return root_view()->HasSelection();
}

CefString CefTextfieldImpl::GetSelectedText() {
  CEF_REQUIRE_VALID_RETURN(CefString());
  return root_view()->GetSelectedText();
}

void CefTextfieldImpl::SelectAll(bool reversed) {
  CEF_REQUIRE_VALID_RETURN_VOID();
  root_view()->SelectAll(reversed);
}

void CefTextfieldImpl::ClearSelection() {
  CEF_REQUIRE_VALID_RETURN_VOID();
  root_view()->ClearSelection();
}

CefRange CefTextfieldImpl::GetSelectedRange() {
  CEF_REQUIRE_VALID_RETURN(CefRange());
  const gfx::Range& range = root_view()->GetSelectedRange();
  return CefRange(range.start(), range.end());
}

void CefTextfieldImpl::SelectRange(const CefRange& range) {
  CEF_REQUIRE_VALID_RETURN_VOID();
  root_view()->SelectRange(gfx::Range(range.from, range.to));
}

size_t CefTextfieldImpl::GetCursorPosition() {
  CEF_REQUIRE_VALID_RETURN(0U);
  return root_view()->GetCursorPosition();
}

void CefTextfieldImpl::SetTextColor(cef_color_t color) {
  CEF_REQUIRE_VALID_RETURN_VOID();
  root_view()->SetTextColor(color);
}

cef_color_t CefTextfieldImpl::GetTextColor() {
  CEF_REQUIRE_VALID_RETURN(0U);
  return root_view()->GetTextColor();
}

void CefTextfieldImpl::SetSelectionTextColor(cef_color_t color) {
  CEF_REQUIRE_VALID_RETURN_VOID();
  root_view()->SetSelectionTextColor(color);
}

cef_color_t CefTextfieldImpl::GetSelectionTextColor() {
  CEF_REQUIRE_VALID_RETURN(0U);
  return root_view()->GetSelectionTextColor();
}

void CefTextfieldImpl::SetSelectionBackgroundColor(cef_color_t color) {
  CEF_REQUIRE_VALID_RETURN_VOID();
  root_view()->SetSelectionBackgroundColor(color);
}

cef_color_t CefTextfieldImpl::GetSelectionBackgroundColor() {
  CEF_REQUIRE_VALID_RETURN(0U);
  return root_view()->GetSelectionBackgroundColor();
}

void CefTextfieldImpl::SetFontList(const CefString& font_list) {
  CEF_REQUIRE_VALID_RETURN_VOID();
  root_view()->SetFontList(gfx::FontList(font_list));
}

void CefTextfieldImpl::ApplyTextColor(cef_color_t color,
                                      const CefRange& range) {
  CEF_REQUIRE_VALID_RETURN_VOID();
  if (range.from == range.to)
    root_view()->SetColor(color);
  else
    root_view()->ApplyColor(color, gfx::Range(range.from, range.to));
}

void CefTextfieldImpl::ApplyTextStyle(cef_text_style_t style,
                                      bool add,
                                      const CefRange& range) {
  CEF_REQUIRE_VALID_RETURN_VOID();
  if (range.from == range.to) {
    root_view()->SetStyle(static_cast<gfx::TextStyle>(style), add);
  } else {
    root_view()->ApplyStyle(static_cast<gfx::TextStyle>(style), add,
                            gfx::Range(range.from, range.to));
  }
}

bool CefTextfieldImpl::IsCommandEnabled(int command_id) {
  CEF_REQUIRE_VALID_RETURN(false);
  return root_view()->IsCommandIdEnabled(command_id);
}

void CefTextfieldImpl::ExecuteCommand(int command_id) {
  CEF_REQUIRE_VALID_RETURN_VOID();
  if (root_view()->IsCommandIdEnabled(command_id))
    root_view()->ExecuteCommand(command_id, ui::EF_NONE);
}

void CefTextfieldImpl::ClearEditHistory() {
  CEF_REQUIRE_VALID_RETURN_VOID();
  root_view()->ClearEditHistory();
}

void CefTextfieldImpl::SetPlaceholderText(const CefString& text) {
  CEF_REQUIRE_VALID_RETURN_VOID();
  root_view()->set_placeholder_text(text);
}

CefString CefTextfieldImpl::GetPlaceholderText() {
  CEF_REQUIRE_VALID_RETURN(CefString());
  return root_view()->GetPlaceholderText();
}

void CefTextfieldImpl::SetPlaceholderTextColor(cef_color_t color) {
  CEF_REQUIRE_VALID_RETURN_VOID();
  root_view()->set_placeholder_text_color(color);
}

void CefTextfieldImpl::SetBackgroundColor(cef_color_t color) {
  CEF_REQUIRE_VALID_RETURN_VOID();
  root_view()->SetBackgroundColor(color);
}

cef_color_t CefTextfieldImpl::GetBackgroundColor() {
  CEF_REQUIRE_VALID_RETURN(0U);
  return root_view()->GetBackgroundColor();
}

void CefTextfieldImpl::SetAccessibleName(const CefString& name) {
  CEF_REQUIRE_VALID_RETURN_VOID();
  root_view()->SetAccessibleName(name);
}

CefTextfieldImpl::CefTextfieldImpl(CefRefPtr<CefTextfieldDelegate> delegate)
    : ParentClass(delegate) {
}

CefTextfieldView* CefTextfieldImpl::CreateRootView() {
  return new CefTextfieldView(delegate());
}

void CefTextfieldImpl::InitializeRootView() {
  static_cast<CefTextfieldView*>(root_view())->Initialize();
}
