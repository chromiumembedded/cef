// Copyright 2016 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_VIEWS_TEXTFIELD_IMPL_H_
#define CEF_LIBCEF_BROWSER_VIEWS_TEXTFIELD_IMPL_H_
#pragma once

#include "include/views/cef_textfield.h"
#include "include/views/cef_textfield_delegate.h"

#include "libcef/browser/views/view_impl.h"
#include "libcef/browser/views/textfield_view.h"

class CefTextfieldImpl :
    public CefViewImpl<CefTextfieldView, CefTextfield, CefTextfieldDelegate> {
 public:
  typedef CefViewImpl<CefTextfieldView, CefTextfield, CefTextfieldDelegate>
      ParentClass;

  // Create a new CefTextfield instance. |delegate| may be nullptr.
  static CefRefPtr<CefTextfieldImpl> Create(
      CefRefPtr<CefTextfieldDelegate> delegate);

  // CefTextfield methods:
  void SetPasswordInput(bool password_input) override;
  bool IsPasswordInput() override;
  void SetReadOnly(bool read_only) override;
  bool IsReadOnly() override;
  CefString GetText() override;
  void SetText(const CefString& text) override;
  void AppendText(const CefString& text) override;
  void InsertOrReplaceText(const CefString& text) override;
  bool HasSelection() override;
  CefString GetSelectedText() override;
  void SelectAll(bool reversed) override;
  void ClearSelection() override;
  CefRange GetSelectedRange() override;
  void SelectRange(const CefRange& range) override;
  size_t GetCursorPosition() override;
  void SetTextColor(cef_color_t color) override;
  cef_color_t GetTextColor() override;
  void SetSelectionTextColor(cef_color_t color) override;
  cef_color_t GetSelectionTextColor() override;
  void SetSelectionBackgroundColor(cef_color_t color) override;
  cef_color_t GetSelectionBackgroundColor() override;
  void SetFontList(const CefString& font_list) override;
  void ApplyTextColor(cef_color_t color,
                      const CefRange& range) override;
  void ApplyTextStyle(cef_text_style_t style,
                      bool add,
                      const CefRange& range) override;
  bool IsCommandEnabled(int command_id) override;
  void ExecuteCommand(int command_id) override;
  void ClearEditHistory() override;
  void SetPlaceholderText(const CefString& text) override;
  CefString GetPlaceholderText() override;
  void SetPlaceholderTextColor(cef_color_t color) override;
  void SetAccessibleName(const CefString& name) override;

  // CefView methods:
  CefRefPtr<CefTextfield> AsTextfield() override { return this; }
  void SetBackgroundColor(cef_color_t color) override;
  cef_color_t GetBackgroundColor() override;

  // CefViewAdapter methods:
  std::string GetDebugType() override { return "Textfield"; }

 private:
  // Create a new implementation object.
  // Always call Initialize() after creation.
  // |delegate| may be nullptr.
  explicit CefTextfieldImpl(CefRefPtr<CefTextfieldDelegate> delegate);

  // CefViewImpl methods:
  CefTextfieldView* CreateRootView() override;
  void InitializeRootView() override;

  IMPLEMENT_REFCOUNTING_DELETE_ON_UIT(CefTextfieldImpl);
  DISALLOW_COPY_AND_ASSIGN(CefTextfieldImpl);
};

#endif  // CEF_LIBCEF_BROWSER_VIEWS_TEXTFIELD_IMPL_H_
